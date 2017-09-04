/****************************************************************************
**
** Copyright (C) 2017 Klaralvdalens Datakonsult AB (KDAB).
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

#include "levelofdetail_p.h"
#include <Qt3DRender/QLevelOfDetail>
#include <Qt3DRender/private/qlevelofdetail_p.h>
#include <Qt3DRender/private/stringtoint_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <QVariant>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

LevelOfDetail::LevelOfDetail()
    : BackendNode(BackendNode::ReadWrite)
    , m_currentIndex(0)
    , m_thresholdType(QLevelOfDetail::DistanceToCameraThreshold)
    , m_volumeOverride()
{
}

LevelOfDetail::~LevelOfDetail()
{
    cleanup();
}

void LevelOfDetail::initializeFromPeer(const QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QLevelOfDetailData>>(change);
    const auto &data = typedChange->data;
    m_camera = data.camera;
    m_currentIndex = data.currentIndex;
    m_thresholdType = data.thresholdType;
    m_thresholds = data.thresholds;
    m_volumeOverride = data.volumeOverride;
}

void LevelOfDetail::cleanup()
{
    QBackendNode::setEnabled(false);
}

void LevelOfDetail::sceneChangeEvent(const QSceneChangePtr &e)
{
    if (e->type() == PropertyUpdated) {
        const QPropertyUpdatedChangePtr &propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("currentIndex"))
            m_currentIndex = propertyChange->value().value<int>();
        else if (propertyChange->propertyName() == QByteArrayLiteral("camera"))
            m_camera = propertyChange->value().value<Qt3DCore::QNodeId>();
        else if (propertyChange->propertyName() == QByteArrayLiteral("thresholdType"))
            m_thresholdType = propertyChange->value().value<QLevelOfDetail::ThresholdType>();
        else if (propertyChange->propertyName() == QByteArrayLiteral("thresholds"))
            m_thresholds = propertyChange->value().value<QVector<qreal>>();
        else if (propertyChange->propertyName() == QByteArrayLiteral("volumeOverride"))
            m_volumeOverride = propertyChange->value().value<Qt3DRender::QLevelOfDetailBoundingSphere>();
    }

    markDirty(AbstractRenderer::GeometryDirty);

    BackendNode::sceneChangeEvent(e);
}

void LevelOfDetail::setCurrentIndex(int currentIndex)
{
    if (m_currentIndex != currentIndex) {
        m_currentIndex = currentIndex;
        auto e = Qt3DCore::QPropertyUpdatedChangePtr::create(peerId());
        e->setDeliveryFlags(Qt3DCore::QSceneChange::DeliverToAll);
        e->setPropertyName("currentIndex");
        e->setValue(m_currentIndex);
        notifyObservers(e);
    }
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
