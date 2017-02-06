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

#include "qchangearbiter_p.h"
#include "qcomponent.h"
#include "qabstractaspectjobmanager_p.h"

#include "qsceneobserverinterface_p.h"
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/corelogging_p.h>
#include <QMutexLocker>
#include <QReadLocker>
#include <QThread>
#include <QWriteLocker>
#include <private/qpostman_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DCore {

/* !\internal
    \class Qt3DCore::QChangeArbiter
    \inmodule Qt3DCore
    \since 5.5

    \brief Acts as a message router between observables and observers.

    Observables can be of two types: QNode observables and \l {QObservableInterface}s.
    QNode notifications are sent from the frontend QNode and delivered to the backend observers.
    QObservableInterface notifications are sent from backend nodes to backend observers and/or to the
    registered QPostman, which in turn delivers the notifications to the target frontend QNode.

    QNode observables are registered automatically. However, QObservableInterface object have to be registered manually
    by providing the QNodeId of the corresponding frontend QNode.

    Observers can be registered to receive messages from a QObservableInterface/QNode observable by providing a QNode NodeUuid.
    When a notification from a QObservableInterface is received, it is then sent to all observers observing the
    QNode NodeUuid as well as the QPostman to update the frontend QNode.
*/
QChangeArbiter::QChangeArbiter(QObject *parent)
    : QObject(parent)
    , m_mutex(QMutex::Recursive)
    , m_jobManager(nullptr)
    , m_postman(nullptr)
    , m_scene(nullptr)
{
    // The QMutex has to be recursive to handle the case where :
    // 1) SyncChanges is called, mutex is locked
    // 2) Changes are distributed
    // 3) An observer decides to register a new observable upon receiving notification
    // 4) registerObserver locks the mutex once again -> we need recursion otherwise deadlock
    // 5) Mutex is unlocked - leaving registerObserver
    // 6) Mutex is unlocked - leaving SyncChanges
}

QChangeArbiter::~QChangeArbiter()
{
    if (m_jobManager != nullptr)
        m_jobManager->waitForPerThreadFunction(QChangeArbiter::destroyThreadLocalChangeQueue, this);
    m_lockingChangeQueues.clear();
    m_changeQueues.clear();
}

void QChangeArbiter::initialize(QAbstractAspectJobManager *jobManager)
{
    Q_CHECK_PTR(jobManager);
    m_jobManager = jobManager;

    // Init TLS for the change queues
    m_jobManager->waitForPerThreadFunction(QChangeArbiter::createThreadLocalChangeQueue, this);
}

void QChangeArbiter::distributeQueueChanges(QChangeQueue *changeQueue)
{
    for (int i = 0, n = int(changeQueue->size()); i < n; i++) {
        QSceneChangePtr& change = (*changeQueue)[i];
        // Lookup which observers care about the subject this change came from
        // and distribute the change to them
        if (change.isNull())
            continue;

        if (change->type() == NodeCreated) {
            for (QSceneObserverInterface *observer : qAsConst(m_sceneObservers))
                observer->sceneNodeAdded(change);
        } else if (change->type() == NodeDeleted) {
            for (QSceneObserverInterface *observer : qAsConst(m_sceneObservers))
                observer->sceneNodeRemoved(change);
        }

        const QNodeId nodeId = change->subjectId();
        const auto it = m_nodeObservations.constFind(nodeId);
        if (it != m_nodeObservations.cend()) {
            const QObserverList &observers = it.value();
            for (const QObserverPair &observer : observers) {
                if ((change->type() & observer.first) &&
                        (change->deliveryFlags() & QSceneChange::BackendNodes))
                    observer.second->sceneChangeEvent(change);
            }
            if (change->deliveryFlags() & QSceneChange::Nodes) {
                // Also send change to the postman
                m_postman->sceneChangeEvent(change);
            }
        }
    }
    changeQueue->clear();
}

QThreadStorage<QChangeArbiter::QChangeQueue *> *QChangeArbiter::tlsChangeQueue()
{
    return &(m_tlsChangeQueue);
}

void QChangeArbiter::appendChangeQueue(QChangeArbiter::QChangeQueue *queue)
{
    QMutexLocker locker(&m_mutex);
    m_changeQueues.append(queue);
}

void QChangeArbiter::removeChangeQueue(QChangeArbiter::QChangeQueue *queue)
{
    QMutexLocker locker(&m_mutex);
    m_changeQueues.removeOne(queue);
}

void QChangeArbiter::appendLockingChangeQueue(QChangeArbiter::QChangeQueue *queue)
{
    QMutexLocker locker(&m_mutex);
    m_lockingChangeQueues.append(queue);
}

void QChangeArbiter::removeLockingChangeQueue(QChangeArbiter::QChangeQueue *queue)
{
    QMutexLocker locker(&m_mutex);
    m_lockingChangeQueues.removeOne(queue);
}

void QChangeArbiter::syncChanges()
{
    QMutexLocker locker(&m_mutex);
    for (QChangeArbiter::QChangeQueue *changeQueue : qAsConst(m_changeQueues))
        distributeQueueChanges(changeQueue);

    for (QChangeQueue *changeQueue : qAsConst(m_lockingChangeQueues))
        distributeQueueChanges(changeQueue);
}

void QChangeArbiter::setScene(QScene *scene)
{
    m_scene = scene;
}

QAbstractPostman *QChangeArbiter::postman() const
{
    return m_postman;
}

QScene *QChangeArbiter::scene() const
{
    return m_scene;
}

void QChangeArbiter::registerObserver(QObserverInterface *observer,
                                      QNodeId nodeId,
                                      ChangeFlags changeFlags)
{
    QMutexLocker locker(&m_mutex);
    QObserverList &observerList = m_nodeObservations[nodeId];
    observerList.append(QObserverPair(changeFlags, observer));
}

// Called from the QAspectThread context, no need to lock
void QChangeArbiter::registerSceneObserver(QSceneObserverInterface *observer)
{
    if (!m_sceneObservers.contains(observer))
        m_sceneObservers << observer;
}

void QChangeArbiter::unregisterObserver(QObserverInterface *observer, QNodeId nodeId)
{
    QMutexLocker locker(&m_mutex);
    if (m_nodeObservations.contains(nodeId)) {
        QObserverList &observers = m_nodeObservations[nodeId];
        for (int i = observers.count() - 1; i >= 0; i--) {
            if (observers[i].second == observer)
                observers.removeAt(i);
        }
    }
}

// Called from the QAspectThread context, no need to lock
void QChangeArbiter::unregisterSceneObserver(QSceneObserverInterface *observer)
{
    if (observer != nullptr)
        m_sceneObservers.removeOne(observer);
}

void QChangeArbiter::sceneChangeEvent(const QSceneChangePtr &e)
{
    //    qCDebug(ChangeArbiter) << Q_FUNC_INFO << QThread::currentThread();

    // Add the change to the thread local storage queue - no locking required => yay!
    QChangeQueue *localChangeQueue = m_tlsChangeQueue.localData();
    localChangeQueue->push_back(e);

    //    qCDebug(ChangeArbiter) << "Change queue for thread" << QThread::currentThread() << "now contains" << localChangeQueue->count() << "items";
}

void QChangeArbiter::sceneChangeEventWithLock(const QSceneChangePtr &e)
{
    QMutexLocker locker(&m_mutex);
    sceneChangeEvent(e);
}

void QChangeArbiter::sceneChangeEventWithLock(const QSceneChangeList &e)
{
    QMutexLocker locker(&m_mutex);
    QChangeQueue *localChangeQueue = m_tlsChangeQueue.localData();
    qCDebug(ChangeArbiter) << Q_FUNC_INFO << "Handles " << e.size() << " changes at once";
    localChangeQueue->insert(localChangeQueue->end(), e.begin(), e.end());
}

// Either we have the postman or we could make the QChangeArbiter agnostic to the postman
// but that would require adding it to every QObserverList in m_aspectObservations.
void QChangeArbiter::setPostman(QAbstractPostman *postman)
{
    if (m_postman != postman) {
        // Unregister old postman here if needed
        m_postman = postman;
    }
}

void QChangeArbiter::createUnmanagedThreadLocalChangeQueue(void *changeArbiter)
{
    Q_ASSERT(changeArbiter);

    QChangeArbiter *arbiter = static_cast<QChangeArbiter *>(changeArbiter);

    qCDebug(ChangeArbiter) << Q_FUNC_INFO << QThread::currentThread();
    if (!arbiter->tlsChangeQueue()->hasLocalData()) {
        QChangeQueue *localChangeQueue = new QChangeQueue;
        arbiter->tlsChangeQueue()->setLocalData(localChangeQueue);
        arbiter->appendLockingChangeQueue(localChangeQueue);
    }
}

void QChangeArbiter::destroyUnmanagedThreadLocalChangeQueue(void *changeArbiter)
{
    Q_ASSERT(changeArbiter);

    QChangeArbiter *arbiter = static_cast<QChangeArbiter *>(changeArbiter);
    qCDebug(ChangeArbiter) << Q_FUNC_INFO << QThread::currentThread();
    if (arbiter->tlsChangeQueue()->hasLocalData()) {
        QChangeQueue *localChangeQueue = arbiter->tlsChangeQueue()->localData();
        arbiter->removeLockingChangeQueue(localChangeQueue);
        arbiter->tlsChangeQueue()->setLocalData(nullptr);
    }
}

void QChangeArbiter::createThreadLocalChangeQueue(void *changeArbiter)
{
    Q_CHECK_PTR(changeArbiter);

    QChangeArbiter *arbiter = static_cast<QChangeArbiter *>(changeArbiter);

    qCDebug(ChangeArbiter) << Q_FUNC_INFO << QThread::currentThread();
    if (!arbiter->tlsChangeQueue()->hasLocalData()) {
        QChangeQueue *localChangeQueue = new QChangeQueue;
        arbiter->tlsChangeQueue()->setLocalData(localChangeQueue);
        arbiter->appendChangeQueue(localChangeQueue);
    }
}

void QChangeArbiter::destroyThreadLocalChangeQueue(void *changeArbiter)
{
    // TODO: Implement me!
    Q_UNUSED(changeArbiter);
    QChangeArbiter *arbiter = static_cast<QChangeArbiter *>(changeArbiter);
    if (arbiter->tlsChangeQueue()->hasLocalData()) {
        QChangeQueue *localChangeQueue = arbiter->tlsChangeQueue()->localData();
        arbiter->removeChangeQueue(localChangeQueue);
        arbiter->tlsChangeQueue()->setLocalData(nullptr);
    }
}

} // namespace Qt3DCore

QT_END_NAMESPACE
