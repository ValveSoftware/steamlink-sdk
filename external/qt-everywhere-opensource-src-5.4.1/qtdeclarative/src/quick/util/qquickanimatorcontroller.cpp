/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickanimatorcontroller_p.h"

#include <private/qquickwindow_p.h>
#include <private/qsgrenderloop_p.h>

#include <private/qanimationgroupjob_p.h>

#include <QtGui/qscreen.h>

#include <QtCore/qcoreapplication.h>

QT_BEGIN_NAMESPACE

QQuickAnimatorController::QQuickAnimatorController(QQuickWindow *window)
    : m_window(window)
    , m_nodesAreInvalid(false)
{
    m_guiEntity = new QQuickAnimatorControllerGuiThreadEntity();
    m_guiEntity->controller = this;
    connect(window, SIGNAL(frameSwapped()), m_guiEntity, SLOT(frameSwapped()));
}

void QQuickAnimatorControllerGuiThreadEntity::frameSwapped()
{
    if (!controller.isNull())
        controller->stopProxyJobs();
}

QQuickAnimatorController::~QQuickAnimatorController()
{
    // The proxy job might already have been deleted, in which case we
    // need to avoid calling functions on them. Then delete the job.
    foreach (QAbstractAnimationJob *job, m_deleting) {
        m_starting.take(job);
        m_stopping.take(job);
        m_animatorRoots.take(job);
        delete job;
    }

    foreach (QQuickAnimatorProxyJob *proxy, m_animatorRoots)
        proxy->controllerWasDeleted();
    qDeleteAll(m_animatorRoots.keys());

    // Delete those who have been started, stopped and are now still
    // pending for restart.
    foreach (QAbstractAnimationJob *job, m_starting.keys()) {
        if (!m_animatorRoots.contains(job))
            delete job;
    }

    delete m_guiEntity;
}

static void qquickanimator_invalidate_node(QAbstractAnimationJob *job)
{
    if (job->isRenderThreadJob()) {
        static_cast<QQuickAnimatorJob *>(job)->nodeWasDestroyed();
    } else if (job->isGroup()) {
        QAnimationGroupJob *g = static_cast<QAnimationGroupJob *>(job);
        for (QAbstractAnimationJob *a = g->firstChild(); a; a = a->nextSibling())
            qquickanimator_invalidate_node(a);
    }
}

void QQuickAnimatorController::windowNodesDestroyed()
{
    m_nodesAreInvalid = true;
    for (QHash<QAbstractAnimationJob *, QQuickAnimatorProxyJob *>::const_iterator it = m_animatorRoots.constBegin();
         it != m_animatorRoots.constEnd(); ++it) {
        qquickanimator_invalidate_node(it.key());
    }
}

void QQuickAnimatorController::itemDestroyed(QObject *o)
{
    m_deletedSinceLastFrame << (QQuickItem *) o;
}

void QQuickAnimatorController::advance()
{
    bool running = false;
    for (QHash<QAbstractAnimationJob *, QQuickAnimatorProxyJob *>::const_iterator it = m_animatorRoots.constBegin();
         !running && it != m_animatorRoots.constEnd(); ++it) {
        if (it.key()->isRunning())
            running = true;
    }

    // It was tempting to only run over the active animations, but we need to push
    // the values for the transforms that finished in the last frame and those will
    // have been removed already...
    lock();
    for (QHash<QQuickItem *, QQuickTransformAnimatorJob::Helper *>::const_iterator it = m_transforms.constBegin();
         it != m_transforms.constEnd(); ++it) {
        QQuickTransformAnimatorJob::Helper *xform = *it;
        // Set to zero when the item was deleted in beforeNodeSync().
        if (!xform->item)
            continue;
        (*it)->apply();
    }
    unlock();

    if (running)
        m_window->update();
}

static void qquick_initialize_helper(QAbstractAnimationJob *job, QQuickAnimatorController *c, bool attachListener)
{
    if (job->isRenderThreadJob()) {
        QQuickAnimatorJob *j = static_cast<QQuickAnimatorJob *>(job);
        if (!j->target()) {
            return;
        } else if (c->m_deletedSinceLastFrame.contains(j->target())) {
            j->targetWasDeleted();
        } else {
            if (attachListener)
                j->addAnimationChangeListener(c, QAbstractAnimationJob::StateChange);
            j->initialize(c);
        }
    } else if (job->isGroup()) {
        QAnimationGroupJob *g = static_cast<QAnimationGroupJob *>(job);
        for (QAbstractAnimationJob *a = g->firstChild(); a; a = a->nextSibling())
            qquick_initialize_helper(a, c, attachListener);
    }
}

void QQuickAnimatorController::beforeNodeSync()
{
    foreach (QAbstractAnimationJob *job, m_deleting) {
        m_starting.take(job);
        m_stopping.take(job);
        m_animatorRoots.take(job);
        job->stop();
        delete job;
    }
    m_deleting.clear();

    if (m_starting.size())
        m_window->update();
    foreach (QQuickAnimatorProxyJob *proxy, m_starting) {
        QAbstractAnimationJob *job = proxy->job();
        job->addAnimationChangeListener(this, QAbstractAnimationJob::Completion);
        qquick_initialize_helper(job, this, true);
        m_animatorRoots[job] = proxy;
        job->start();
        proxy->startedByController();
    }
    m_starting.clear();

    foreach (QQuickAnimatorProxyJob *proxy, m_stopping) {
        QAbstractAnimationJob *job = proxy->job();
        job->stop();
    }
    m_stopping.clear();

    // First sync after a window was hidden or otherwise invalidated.
    // call initialize again to pick up new nodes..
    if (m_nodesAreInvalid) {
        for (QHash<QAbstractAnimationJob *, QQuickAnimatorProxyJob *>::const_iterator it = m_animatorRoots.constBegin();
             it != m_animatorRoots.constEnd(); ++it) {
            qquick_initialize_helper(it.key(), this, false);
        }
        m_nodesAreInvalid = false;
    }

    foreach (QQuickAnimatorJob *job, m_activeLeafAnimations) {
        if (!job->target())
            continue;
        else if (m_deletedSinceLastFrame.contains(job->target()))
            job->targetWasDeleted();
        else if (job->isTransform()) {
            QQuickTransformAnimatorJob *xform = static_cast<QQuickTransformAnimatorJob *>(job);
            xform->transformHelper()->sync();
        }
    }
    foreach (QQuickItem *wiped, m_deletedSinceLastFrame) {
        QQuickTransformAnimatorJob::Helper *helper = m_transforms.take(wiped);
        // Helper will now already have been reset in all animators referencing it.
        delete helper;
    }

    m_deletedSinceLastFrame.clear();
}

void QQuickAnimatorController::afterNodeSync()
{
    foreach (QQuickAnimatorJob *job, m_activeLeafAnimations) {
        if (job->isUniform() && job->target())
            static_cast<QQuickUniformAnimatorJob *>(job)->afterNodeSync();
    }
}

void QQuickAnimatorController::proxyWasDestroyed(QQuickAnimatorProxyJob *proxy)
{
    lock();
    m_proxiesToStop.remove(proxy);
    unlock();
}

void QQuickAnimatorController::stopProxyJobs()
{
    // Need to make a copy under lock and then stop while unlocked.
    // Stopping triggers writeBack which in turn may lock, so it needs
    // to be outside the lock. It is also safe because deletion of
    // proxies happens on the GUI thread, where this code is also executing.
    lock();
    QSet<QQuickAnimatorProxyJob *> jobs = m_proxiesToStop;
    m_proxiesToStop.clear();
    unlock();
    foreach (QQuickAnimatorProxyJob *p, jobs)
        p->stop();
}

void QQuickAnimatorController::animationFinished(QAbstractAnimationJob *job)
{
    /* We are currently on the render thread and m_deleting is primarily
     * being written on the GUI Thread and read during sync. However, we don't
     * need to lock here as this is a direct result of animationDriver->advance()
     * which is already locked. For non-threaded render loops no locking is
     * needed in any case.
     */
    if (!m_deleting.contains(job)) {
        QQuickAnimatorProxyJob *proxy = m_animatorRoots[job];
        if (proxy) {
            m_window->update();
            m_proxiesToStop << proxy;
        }
        // else already gone...
    }
}

void QQuickAnimatorController::animationStateChanged(QAbstractAnimationJob *job,
                                                     QAbstractAnimationJob::State newState,
                                                     QAbstractAnimationJob::State oldState)
{
    Q_ASSERT(job->isRenderThreadJob());
    QQuickAnimatorJob *animator = static_cast<QQuickAnimatorJob *>(job);
    if (newState == QAbstractAnimationJob::Running) {
        m_activeLeafAnimations << animator;
        animator->setHasBeenRunning(true);
    } else if (oldState == QAbstractAnimationJob::Running) {
        m_activeLeafAnimations.remove(animator);
    }
}



void QQuickAnimatorController::requestSync()
{
    // Force a "sync" pass as the newly started animation needs to sync properties from GUI.
    m_window->maybeUpdate();
}

// These functions are called on the GUI thread.
void QQuickAnimatorController::startJob(QQuickAnimatorProxyJob *proxy, QAbstractAnimationJob *job)
{
    proxy->markJobManagedByController();
    m_starting[job] = proxy;
    m_stopping.remove(job);
    requestSync();
}

void QQuickAnimatorController::stopJob(QQuickAnimatorProxyJob *proxy, QAbstractAnimationJob *job)
{
    m_stopping[job] = proxy;
    m_starting.remove(job);
    requestSync();
}

void QQuickAnimatorController::deleteJob(QAbstractAnimationJob *job)
{
    lock();
    m_deleting << job;
    requestSync();
    unlock();
}

QT_END_NAMESPACE
