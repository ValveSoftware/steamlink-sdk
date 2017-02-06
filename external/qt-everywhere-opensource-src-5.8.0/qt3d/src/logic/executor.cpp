/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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

#include "executor_p.h"
#include <Qt3DLogic/qframeaction.h>
#include <Qt3DCore/qnode.h>
#include <Qt3DCore/private/qscene_p.h>
#include <QtCore/qsemaphore.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DLogic {
namespace Logic {

Executor::Executor(QObject *parent)
    : QObject(parent)
    , m_scene(nullptr)
{
}

void Executor::clearQueueAndProceed()
{
    // Clear the queue of nodes to process frame updates for (throw the work away).
    // If the semaphore is acquired, release it to allow the logic job and hence the
    // manager and frame to complete and shutdown to continue.
    m_nodeIds.clear();
    if (m_semaphore->available() == 0)
        m_semaphore->release();
}

void Executor::enqueueLogicFrameUpdates(const QVector<Qt3DCore::QNodeId> &nodeIds)
{
    m_nodeIds = nodeIds;
}

bool Executor::event(QEvent *e)
{
    if (e->type() == QEvent::User) {
        FrameUpdateEvent *ev = static_cast<FrameUpdateEvent *>(e);
        processLogicFrameUpdates(ev->deltaTime());
        e->setAccepted(true);
        return true;
    }
    return false;
}

/*!
   \internal

   Called from context of main thread
*/
void Executor::processLogicFrameUpdates(float dt)
{
    Q_ASSERT(m_scene);
    Q_ASSERT(m_semaphore);
    const QVector<QNode *> nodes = m_scene->lookupNodes(m_nodeIds);
    for (QNode *node : nodes) {
        QFrameAction *frameAction = qobject_cast<QFrameAction *>(node);
        if (frameAction)
            frameAction->onTriggered(dt);
    }

    // Release the semaphore so the calling Manager can continue
    m_semaphore->release();
}

} // namespace Logic
} // namespace Qt3DLogic

QT_END_NAMESPACE
