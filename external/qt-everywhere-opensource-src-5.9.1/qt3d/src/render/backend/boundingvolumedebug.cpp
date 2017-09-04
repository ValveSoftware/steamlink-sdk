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
#if 0
#include "boundingvolumedebug_p.h"
#include <Qt3DRender/private/qboundingvolumedebug_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

namespace Render {

BoundingVolumeDebug::BoundingVolumeDebug()
    : BackendNode(QBackendNode::ReadWrite)
    , m_recursive(false)
    , m_radius(0.0f)
{
}

BoundingVolumeDebug::~BoundingVolumeDebug()
{
}

void BoundingVolumeDebug::cleanup()
{
    m_recursive = false;
    m_radius = 0.0f;
    m_center = QVector3D();
}

void BoundingVolumeDebug::updateFromPeer(Qt3DCore::QNode *peer)
{
    QBoundingVolumeDebug *bvD = static_cast<QBoundingVolumeDebug *>(peer);
    if (bvD) {
        m_recursive = bvD->recursive();
    }
}

void BoundingVolumeDebug::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    const Qt3DCore::QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<Qt3DCore::QPropertyUpdatedChange>(e);
    const QByteArray propertyName = propertyChange->propertyName();

    if (propertyChange->type() == Qt3DCore::PropertyUpdated) {
        if (propertyName == QByteArrayLiteral("recursive")) {
            m_recursive = propertyChange->value().toBool();
        }
        markDirty(AbstractRenderer::AllDirty);
    }
}

void BoundingVolumeDebug::setRadius(float radius)
{
    if (m_radius != radius) {
        m_radius = radius;
        auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerdId());
        e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
        e->setPropertyName("radius");
        e->setTargetNode(peerId());
        e->setValue(QVariant(radius));
        notifyObservers(e);
    }
}

void BoundingVolumeDebug::setCenter(const QVector3D &center)
{
    if (m_center != center) {
        m_center = center;
        auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerdId());
        e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
        e->setPropertyName("center");
        e->setTargetNode(peerId());
        e->setValue(QVariant::fromValue(center));
        notifyObservers(e);
    }
}

} // Render

} // Qt3DRender

QT_END_NAMESPACE

#endif
