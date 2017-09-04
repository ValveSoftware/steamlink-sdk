/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Gunnar Sletta <gunnar@sletta.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickanimatorcontroller_p.h"

#include <private/qquickwindow_p.h>
#include <private/qsgrenderloop_p.h>

#include <private/qanimationgroupjob_p.h>

#include <QtGui/qscreen.h>

#include <QtCore/qcoreapplication.h>

QT_BEGIN_NAMESPACE

QQuickAnimatorController::~QQuickAnimatorController()
{
}

QQuickAnimatorController::QQuickAnimatorController(QQuickWindow *window)
    : m_window(window)
{
}

static void qquickanimator_invalidate_jobs(QAbstractAnimationJob *job)
{
    if (job->isRenderThreadJob()) {
        static_cast<QQuickAnimatorJob *>(job)->invalidate();
    } else if (job->isGroup()) {
        QAnimationGroupJob *g = static_cast<QAnimationGroupJob *>(job);
        for (QAbstractAnimationJob *a = g->firstChild(); a; a = a->nextSibling())
            qquickanimator_invalidate_jobs(a);
    }
}

void QQuickAnimatorController::windowNodesDestroyed()
{
    for (const QSharedPointer<QAbstractAnimationJob> &toStop : qAsConst(m_rootsPendingStop)) {
        qquickanimator_invalidate_jobs(toStop.data());
        toStop->stop();
    }
    m_rootsPendingStop.clear();

    // Clear animation roots and iterate over a temporary to avoid that job->stop()
    // modifies the m_animationRoots and messes with our iteration
    const auto roots = m_animationRoots;
    m_animationRoots.clear();
    for (const QSharedPointer<QAbstractAnimationJob> &job : roots) {
        qquickanimator_invalidate_jobs(job.data());

        // Stop it and add it to the list of pending start so it might get
        // started later on.
        job->stop();
        m_rootsPendingStart.insert(job);
    }
}

void QQuickAnimatorController::advance()
{
    bool running = false;
    for (const QSharedPointer<QAbstractAnimationJob> &job : qAsConst(m_animationRoots)) {
        if (job->isRunning()) {
            running = true;
            break;
        }
    }

    for (QQuickAnimatorJob *job : qAsConst(m_runningAnimators))
        job->commit();

    if (running)
        m_window->update();
}

static void qquickanimator_sync_before_start(QAbstractAnimationJob *job)
{
    if (job->isRenderThreadJob()) {
        static_cast<QQuickAnimatorJob *>(job)->preSync();
    } else if (job->isGroup()) {
        QAnimationGroupJob *g = static_cast<QAnimationGroupJob *>(job);
        for (QAbstractAnimationJob *a = g->firstChild(); a; a = a->nextSibling())
            qquickanimator_sync_before_start(a);
    }
}

void QQuickAnimatorController::beforeNodeSync()
{
    for (const QSharedPointer<QAbstractAnimationJob> &toStop : qAsConst(m_rootsPendingStop))
        toStop->stop();
    m_rootsPendingStop.clear();


    for (QQuickAnimatorJob *job : qAsConst(m_runningAnimators))
        job->preSync();

    // Start pending jobs
    for (const QSharedPointer<QAbstractAnimationJob> &job : qAsConst(m_rootsPendingStart)) {
        Q_ASSERT(!job->isRunning());

        // We want to make sure that presync is called before
        // updateAnimationTime is called the very first time, so before
        // starting a tree of jobs, we go through it and call preSync on all
        // its animators.
        qquickanimator_sync_before_start(job.data());

        // The start the job..
        job->start();
        m_animationRoots.insert(job.data(), job);
    }
    m_rootsPendingStart.clear();

    // Issue an update directly on the window to force another render pass.
    if (m_animationRoots.size())
        m_window->update();
}

void QQuickAnimatorController::afterNodeSync()
{
    for (QQuickAnimatorJob *job : qAsConst(m_runningAnimators))
        job->postSync();
}

void QQuickAnimatorController::animationFinished(QAbstractAnimationJob *job)
{
     m_animationRoots.remove(job);
}

void QQuickAnimatorController::animationStateChanged(QAbstractAnimationJob *job,
                                                     QAbstractAnimationJob::State newState,
                                                     QAbstractAnimationJob::State oldState)
{
    Q_ASSERT(job->isRenderThreadJob());
    QQuickAnimatorJob *animator = static_cast<QQuickAnimatorJob *>(job);
    if (newState == QAbstractAnimationJob::Running) {
        m_runningAnimators.insert(animator);
    } else if (oldState == QAbstractAnimationJob::Running) {
        animator->commit();
        m_runningAnimators.remove(animator);
    }
}


void QQuickAnimatorController::requestSync()
{
    // Force a "sync" pass as the newly started animation needs to sync properties from GUI.
    m_window->maybeUpdate();
}

// All this is being executed on the GUI thread while the animator controller
// is locked.
void QQuickAnimatorController::start_helper(QAbstractAnimationJob *job)
{
    if (job->isRenderThreadJob()) {
        QQuickAnimatorJob *j = static_cast<QQuickAnimatorJob *>(job);
        j->addAnimationChangeListener(this, QAbstractAnimationJob::StateChange);
        j->initialize(this);
    } else if (job->isGroup()) {
        QAnimationGroupJob *g = static_cast<QAnimationGroupJob *>(job);
        for (QAbstractAnimationJob *a = g->firstChild(); a; a = a->nextSibling())
            start_helper(a);
    }
}

// Called by the proxy when it is time to kick off an animation job
void QQuickAnimatorController::start(const QSharedPointer<QAbstractAnimationJob> &job)
{
    m_rootsPendingStart.insert(job);
    m_rootsPendingStop.remove(job);
    job->addAnimationChangeListener(this, QAbstractAnimationJob::Completion);
    start_helper(job.data());
    requestSync();
}


// Called by the proxy when it is time to stop an animation job.
void QQuickAnimatorController::cancel(const QSharedPointer<QAbstractAnimationJob> &job)
{
    m_rootsPendingStart.remove(job);
    m_rootsPendingStop.insert(job);
}


QT_END_NAMESPACE

#include "moc_qquickanimatorcontroller_p.cpp"
