/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "clipanimator_p.h"
#include <Qt3DAnimation/qclipanimator.h>
#include <Qt3DAnimation/private/qclipanimator_p.h>
#include <Qt3DAnimation/private/animationclip_p.h>
#include <Qt3DAnimation/private/managers_p.h>
#include <Qt3DAnimation/private/animationlogging_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qpropertyupdatedchangebase_p.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {
namespace Animation {

ClipAnimator::ClipAnimator()
    : BackendNode(ReadWrite)
    , m_clipId()
    , m_mapperId()
    , m_running(false)
    , m_loops(1)
    , m_startGlobalTime(0)
    , m_mappingData()
    , m_currentLoop(0)
{
}

void ClipAnimator::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QClipAnimatorData>>(change);
    const auto &data = typedChange->data;
    m_clipId = data.clipId;
    m_mapperId = data.mapperId;
    m_running = data.running;
    m_loops = data.loops;
    setDirty(Handler::ClipAnimatorDirty);
}

void ClipAnimator::setClipId(Qt3DCore::QNodeId clipId)
{
    m_clipId = clipId;
    setDirty(Handler::ClipAnimatorDirty);
}

void ClipAnimator::setMapperId(Qt3DCore::QNodeId mapperId)
{
    m_mapperId = mapperId;
    setDirty(Handler::ClipAnimatorDirty);
}

void ClipAnimator::setRunning(bool running)
{
    m_running = running;
    if (!running)
        m_currentLoop = 0;
    setDirty(Handler::ClipAnimatorDirty);
}

void ClipAnimator::cleanup()
{
    setEnabled(false);
    m_handler = nullptr;
    m_clipId = Qt3DCore::QNodeId();
    m_mapperId = Qt3DCore::QNodeId();
    m_running = false;
    m_loops = 1;
}

void ClipAnimator::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case Qt3DCore::PropertyUpdated: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("clip"))
            setClipId(change->value().value<Qt3DCore::QNodeId>());
        else if (change->propertyName() == QByteArrayLiteral("channelMapper"))
            setMapperId(change->value().value<Qt3DCore::QNodeId>());
        else if (change->propertyName() == QByteArrayLiteral("running"))
            setRunning(change->value().toBool());
        else if (change->propertyName() == QByteArrayLiteral("loops"))
            m_loops = change->value().toInt();
        break;
    }

    default:
        break;
    }
    QBackendNode::sceneChangeEvent(e);
}

void ClipAnimator::sendPropertyChanges(const QVector<Qt3DCore::QSceneChangePtr> &changes)
{
    for (const Qt3DCore::QSceneChangePtr &change : changes)
        notifyObservers(change);
}

} // namespace Animation
} // namespace Qt3DAnimation

QT_END_NAMESPACE
