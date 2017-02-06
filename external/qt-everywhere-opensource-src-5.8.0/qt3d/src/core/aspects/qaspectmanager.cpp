/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qaspectmanager_p.h"
#include "qabstractaspect.h"
#include "qabstractaspect_p.h"
#include "qchangearbiter_p.h"
// TODO Make the kind of job manager configurable (e.g. ThreadWeaver vs Intel TBB)
#include "qaspectjobmanager_p.h"
#include "qabstractaspectjobmanager_p.h"
#include "qentity.h"

#include <Qt3DCore/private/qservicelocator_p.h>

#include <Qt3DCore/private/qtickclockservice_p.h>
#include <Qt3DCore/private/corelogging_p.h>
#include <Qt3DCore/private/qscheduler_p.h>
#include <Qt3DCore/private/qtickclock_p.h>
#include <Qt3DCore/private/qabstractframeadvanceservice_p.h>
#include <QEventLoop>
#include <QThread>
#include <QWaitCondition>
#include <QSurface>

#if defined(QT3D_CORE_JOB_TIMING)
#include <QElapsedTimer>
#endif

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

QAspectManager::QAspectManager(QObject *parent)
    : QObject(parent)
    , m_root(nullptr)
    , m_scheduler(new QScheduler(this))
    , m_jobManager(new QAspectJobManager(this))
    , m_changeArbiter(new QChangeArbiter(this))
    , m_serviceLocator(new QServiceLocator())
    , m_waitForEndOfSimulationLoop(0)
    , m_waitForEndOfExecLoop(0)
    , m_waitForQuit(0)
{
    qRegisterMetaType<QSurface *>("QSurface*");
    m_runSimulationLoop.fetchAndStoreOrdered(0);
    m_runMainLoop.fetchAndStoreOrdered(1);
    qCDebug(Aspects) << Q_FUNC_INFO;
}

QAspectManager::~QAspectManager()
{
    delete m_changeArbiter;
    delete m_jobManager;
    delete m_scheduler;
}

void QAspectManager::enterSimulationLoop()
{
    qCDebug(Aspects) << Q_FUNC_INFO;
    m_runSimulationLoop.fetchAndStoreOrdered(1);
}

void QAspectManager::exitSimulationLoop()
{
    qCDebug(Aspects) << Q_FUNC_INFO;

    // If this fails, simulation loop is already exited so nothing to do
    if (!m_runSimulationLoop.testAndSetOrdered(1, 0)) {
        qCDebug(Aspects) << "Simulation loop was not running. Nothing to do";
        return;
    }

    QAbstractFrameAdvanceService *frameAdvanceService =
            m_serviceLocator->service<QAbstractFrameAdvanceService>(QServiceLocator::FrameAdvanceService);
    if (frameAdvanceService)
        frameAdvanceService->stop();

    // Give any aspects a chance to unqueue any asynchronous work they
    // may have scheduled that would otherwise potentially deadlock or
    // cause races. For example, the QLogicAspect queues up a vector of
    // QNodeIds to be processed by a callback on the main thread. However,
    // if we don't unqueue this work and release its semaphore, the logic
    // aspect would cause a deadlock when trying to exit the inner loop.
    // This is because we call this function from the main thread and the
    // logic aspect is waiting for the main thread to execute the
    // QLogicComponent::onFrameUpdate() callback.
    for (QAbstractAspect *aspect : qAsConst(m_aspects))
        aspect->d_func()->onEngineAboutToShutdown();

    // Wait until the simulation loop is fully exited and the aspects are done
    // processing any final changes and have had onEngineShutdown() called on them
    m_waitForEndOfSimulationLoop.acquire(1);
}

bool QAspectManager::isShuttingDown() const
{
    return !m_runSimulationLoop.load();
}

/*!
    \internal

    Called by the QAspectThread's run() method immediately after the manager
    has been created
*/
void QAspectManager::initialize()
{
    qCDebug(Aspects) << Q_FUNC_INFO;
    m_jobManager->initialize();
    m_scheduler->setAspectManager(this);
    m_changeArbiter->initialize(m_jobManager);
}

/*!
    \internal

    Called by the QAspectThread's run() method immediately after the manager's
    exec() function has returned.
*/
void QAspectManager::shutdown()
{
    qCDebug(Aspects) << Q_FUNC_INFO;

    for (QAbstractAspect *aspect : qAsConst(m_aspects))
        m_changeArbiter->unregisterSceneObserver(aspect->d_func());

    // Aspects must be deleted in the Thread they were created in
}

void QAspectManager::setRootEntity(Qt3DCore::QEntity *root, const QVector<Qt3DCore::QNodeCreatedChangeBasePtr> &changes)
{
    qCDebug(Aspects) << Q_FUNC_INFO;

    if (root == m_root)
        return;

    if (m_root) {
        // TODO: Delete all backend nodes. This is to be symmetric with how
        // we create them below in the call to setRootAndCreateNodes
    }

    m_root = root;

    if (m_root) {
        for (QAbstractAspect *aspect : qAsConst(m_aspects))
            aspect->d_func()->setRootAndCreateNodes(m_root, changes);
    }
}

/*!
 * Registers a new \a aspect.
 */
void QAspectManager::registerAspect(QAbstractAspect *aspect)
{
    qCDebug(Aspects) << "Registering aspect";

    if (aspect != nullptr) {
        m_aspects.append(aspect);
        QAbstractAspectPrivate::get(aspect)->m_aspectManager = this;
        QAbstractAspectPrivate::get(aspect)->m_jobManager = m_jobManager;
        QAbstractAspectPrivate::get(aspect)->m_arbiter = m_changeArbiter;
        // Register sceneObserver with the QChangeArbiter
        m_changeArbiter->registerSceneObserver(aspect->d_func());

        // Allow the aspect to do some work now that it is registered
        aspect->onRegistered();
    }
    else {
        qCWarning(Aspects) << "Failed to register aspect";
    }
    qCDebug(Aspects) << "Completed registering aspect";
}

/*!
 * \internal
 *
 * Calls QAbstractAspect::onUnregistered(), unregisters the aspect from the
 * change arbiter and unsets the arbiter, job manager and aspect manager.
 * Operations are performed in the reverse order to registerAspect.
 */
void QAspectManager::unregisterAspect(Qt3DCore::QAbstractAspect *aspect)
{
    qCDebug(Aspects) << "Unregistering aspect";
    Q_ASSERT(aspect);
    aspect->onUnregistered();
    m_changeArbiter->unregisterSceneObserver(aspect->d_func());
    QAbstractAspectPrivate::get(aspect)->m_arbiter = nullptr;
    QAbstractAspectPrivate::get(aspect)->m_jobManager = nullptr;
    QAbstractAspectPrivate::get(aspect)->m_aspectManager = nullptr;
    m_aspects.removeOne(aspect);
    qCDebug(Aspects) << "Completed unregistering aspect";
}

void QAspectManager::exec()
{
    // Gentlemen, start your engines
    QEventLoop eventLoop;

    // Enter the engine loop
    qCDebug(Aspects) << Q_FUNC_INFO << "***** Entering main loop *****";
    while (m_runMainLoop.load()) {
        // Process any pending events, waiting for more to arrive if queue is empty
        eventLoop.processEvents(QEventLoop::WaitForMoreEvents, 16);

        // Retrieve the frame advance service. Defaults to timer based if there is no renderer.
        QAbstractFrameAdvanceService *frameAdvanceService =
                m_serviceLocator->service<QAbstractFrameAdvanceService>(QServiceLocator::FrameAdvanceService);

        // Start the frameAdvanceService if we're about to enter the simulation loop
        bool needsShutdown = false;
        if (m_runSimulationLoop.load()) {
            needsShutdown = true;
            frameAdvanceService->start();

            // We are about to enter the simulation loop. Give aspects a chance to do any last
            // pieces of initialization
            qCDebug(Aspects) << "Calling onEngineStartup() for each aspect";
            for (QAbstractAspect *aspect : qAsConst(m_aspects)) {
                qCDebug(Aspects) << "\t" << aspect->objectName();
                aspect->onEngineStartup();
            }
            qCDebug(Aspects) << "Done calling onEngineStartup() for each aspect";
        }

        // Only enter main simulation loop once the renderer and other aspects are initialized
        while (m_runSimulationLoop.load()) {
            qint64 t = frameAdvanceService->waitForNextFrame();

            // Distribute accumulated changes. This includes changes sent from the frontend
            // to the backend nodes. We call this before the call to m_scheduler->update() to ensure
            // that any property changes do not set dirty flags in a data race with the renderer's
            // submission thread which may be looking for dirty flags, acting upon them and then
            // clearing the dirty flags.
            //
            // Doing this as the first call in the new frame ensures the lock free approach works
            // without any such data race.
            m_changeArbiter->syncChanges();

            // For each Aspect
            // Ask them to launch set of jobs for the current frame
            // Updates matrices, bounding volumes, render bins ...
#if defined(QT3D_CORE_JOB_TIMING)
            QElapsedTimer timer;
            timer.start();
#endif
            m_scheduler->scheduleAndWaitForFrameAspectJobs(t);
#if defined(QT3D_CORE_JOB_TIMING)
            qDebug() << "Jobs took" << timer.nsecsElapsed() / 1.0e6;
#endif

            // Process any pending events
            eventLoop.processEvents();
        } // End of simulation loop

        if (needsShutdown) {
            // Process any pending changes from the frontend before we shut the aspects down
            m_changeArbiter->syncChanges();

            // Give aspects a chance to perform any shutdown actions. This may include unqueuing
            // any blocking work on the main thread that could potentially deadlock during shutdown.
            qCDebug(Aspects) << "Calling onEngineShutdown() for each aspect";
            for (QAbstractAspect *aspect : qAsConst(m_aspects)) {
                qCDebug(Aspects) << "\t" << aspect->objectName();
                aspect->onEngineShutdown();
            }
            qCDebug(Aspects) << "Done calling onEngineShutdown() for each aspect";

            // Wake up the main thread which is waiting for us inside of exitSimulationLoop()
            m_waitForEndOfSimulationLoop.release(1);
        }
    } // End of main loop
    qCDebug(Aspects) << Q_FUNC_INFO << "***** Exited main loop *****";

    m_waitForEndOfExecLoop.release(1);
    m_waitForQuit.acquire(1);
}

void QAspectManager::quit()
{
    qCDebug(Aspects) << Q_FUNC_INFO;

    Q_ASSERT_X(m_runSimulationLoop.load() == 0, "QAspectManagr::quit()", "Inner loop is still running");
    m_runMainLoop.fetchAndStoreOrdered(0);

    // We need to wait for the QAspectManager exec loop to terminate
    m_waitForEndOfExecLoop.acquire(1);
    m_waitForQuit.release(1);

    qCDebug(Aspects) << Q_FUNC_INFO << "Exiting";
}

const QVector<QAbstractAspect *> &QAspectManager::aspects() const
{
    return m_aspects;
}

QAbstractAspectJobManager *QAspectManager::jobManager() const
{
    return m_jobManager;
}

QChangeArbiter *QAspectManager::changeArbiter() const
{
    return m_changeArbiter;
}

QServiceLocator *QAspectManager::serviceLocator() const
{
    return m_serviceLocator.data();
}

} // namespace Qt3DCore

QT_END_NAMESPACE

