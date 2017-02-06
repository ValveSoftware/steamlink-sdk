/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QQUICKANIMATORCONTROLLER_P_H
#define QQUICKANIMATORCONTROLLER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qquickanimatorjob_p.h"
#include <QtQuick/qsgnode.h>
#include <QtQuick/qquickitem.h>

#include <QtCore/qmutex.h>
#include <QtCore/qthread.h>

QT_BEGIN_NAMESPACE

class QQuickAnimatorControllerGuiThreadEntity;

class QQuickAnimatorController : public QObject, public QAnimationJobChangeListener
{
    Q_OBJECT

public:
    QQuickAnimatorController(QQuickWindow *window);
    ~QQuickAnimatorController();

    void advance();
    void beforeNodeSync();
    void afterNodeSync();

    void animationFinished(QAbstractAnimationJob *job);
    void animationStateChanged(QAbstractAnimationJob *job, QAbstractAnimationJob::State newState, QAbstractAnimationJob::State oldState);

    void requestSync();

    // These are called from the GUI thread (the proxy)
    void startJob(QQuickAnimatorProxyJob *proxy, QAbstractAnimationJob *job);
    void stopJob(QQuickAnimatorProxyJob *proxy, QAbstractAnimationJob *job);
    void deleteJob(QAbstractAnimationJob *job);

    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }


    void proxyWasDestroyed(QQuickAnimatorProxyJob *proxy);
    void stopProxyJobs();
    void windowNodesDestroyed();

public Q_SLOTS:
    void itemDestroyed(QObject *);

public:
    // These are manipulated from the GUI thread and should only
    // be updated during the sync() phase.
    QHash<QAbstractAnimationJob *, QQuickAnimatorProxyJob *> m_starting;
    QHash<QAbstractAnimationJob *, QQuickAnimatorProxyJob *> m_stopping;
    QSet<QAbstractAnimationJob *> m_deleting;

    QHash<QAbstractAnimationJob *, QQuickAnimatorProxyJob *> m_animatorRoots;
    QSet<QQuickAnimatorJob *> m_activeLeafAnimations;
    QHash<QQuickItem *, QQuickTransformAnimatorJob::Helper *> m_transforms;
    QSet<QQuickItem *> m_deletedSinceLastFrame;
    QQuickWindow *m_window;
    QQuickAnimatorControllerGuiThreadEntity *m_guiEntity;
    QSet<QQuickAnimatorProxyJob *> m_proxiesToStop;
    QMutex m_mutex;

    bool m_nodesAreInvalid;
};

class QQuickAnimatorControllerGuiThreadEntity : public QObject
{
    Q_OBJECT
public:
    QPointer<QQuickAnimatorController> controller;

public Q_SLOTS:
    void frameSwapped();
};



QT_END_NAMESPACE

#endif // QQUICKANIMATORCONTROLLER_P_H
