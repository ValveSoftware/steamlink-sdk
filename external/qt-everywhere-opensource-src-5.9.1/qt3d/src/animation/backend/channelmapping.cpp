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

#include "channelmapping_p.h"
#include <Qt3DAnimation/qchannelmapping.h>
#include <Qt3DAnimation/private/qchannelmapping_p.h>
#include <Qt3DAnimation/private/animationlogging_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DAnimation {
namespace Animation {

ChannelMapping::ChannelMapping()
    : BackendNode(ReadOnly)
    , m_channelName()
    , m_targetId()
    , m_property()
    , m_type(static_cast<int>(QVariant::Invalid))
    , m_propertyName(nullptr)
{
}

void ChannelMapping::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QChannelMappingData>>(change);
    const auto &data = typedChange->data;
    m_channelName = data.channelName;
    m_targetId = data.targetId;
    m_property = data.property;
    m_type = data.type;
    m_propertyName = data.propertyName;
}

void ChannelMapping::cleanup()
{
    setEnabled(false);
    m_channelName.clear();
    m_targetId = Qt3DCore::QNodeId();
    m_property.clear();
    m_type = static_cast<int>(QVariant::Invalid);
    m_propertyName = nullptr;
}

void ChannelMapping::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case Qt3DCore::PropertyUpdated: {
        const auto change = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("channelName"))
            m_channelName = change->value().toString();
        else if (change->propertyName() == QByteArrayLiteral("target"))
            m_targetId = change->value().value<Qt3DCore::QNodeId>();
        else if (change->propertyName() == QByteArrayLiteral("property"))
            m_property = change->value().toString();
        else if (change->propertyName() == QByteArrayLiteral("type"))
            m_type = change->value().toInt();
        else if (change->propertyName() == QByteArrayLiteral("propertyName"))
            m_propertyName = static_cast<const char *>(const_cast<const void *>(change->value().value<void *>()));
        break;
    }

    default:
        break;
    }
    QBackendNode::sceneChangeEvent(e);
}

} // namespace Animation
} // namespace Qt3DAnimation

QT_END_NAMESPACE
