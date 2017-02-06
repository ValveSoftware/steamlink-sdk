/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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

#include "rendersettings_p.h"

#include <Qt3DRender/QFrameGraphNode>
#include <Qt3DRender/private/abstractrenderer_p.h>
#include <Qt3DRender/private/qrendersettings_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

RenderSettings::RenderSettings()
    : BackendNode()
    , m_renderPolicy(QRenderSettings::OnDemand)
    , m_pickMethod(QPickingSettings::BoundingVolumePicking)
    , m_pickResultMode(QPickingSettings::NearestPick)
    , m_faceOrientationPickingMode(QPickingSettings::FrontFace)
    , m_activeFrameGraph()
{
}

void RenderSettings::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QRenderSettingsData>>(change);
    const auto &data = typedChange->data;
    m_activeFrameGraph = data.activeFrameGraphId;
    m_renderPolicy = data.renderPolicy;
    m_pickMethod = data.pickMethod;
    m_pickResultMode = data.pickResultMode;
    m_faceOrientationPickingMode = data.faceOrientationPickingMode;
}

void RenderSettings::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    if (e->type() == PropertyUpdated) {
        QPropertyUpdatedChangePtr propertyChange = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("pickMethod"))
            m_pickMethod = propertyChange->value().value<QPickingSettings::PickMethod>();
        else if (propertyChange->propertyName() == QByteArrayLiteral("pickResult"))
            m_pickResultMode = propertyChange->value().value<QPickingSettings::PickResultMode>();
        else if (propertyChange->propertyName() == QByteArrayLiteral("faceOrientationPickingMode"))
            m_faceOrientationPickingMode = propertyChange->value().value<QPickingSettings::FaceOrientationPickingMode>();
        else if (propertyChange->propertyName() == QByteArrayLiteral("activeFrameGraph"))
            m_activeFrameGraph = propertyChange->value().value<QNodeId>();
        else if (propertyChange->propertyName() == QByteArrayLiteral("renderPolicy"))
            m_renderPolicy = propertyChange->value().value<QRenderSettings::RenderPolicy>();
        markDirty(AbstractRenderer::AllDirty);
    }

    BackendNode::sceneChangeEvent(e);
}

RenderSettingsFunctor::RenderSettingsFunctor(AbstractRenderer *renderer)
    : m_renderer(renderer)
{
}

Qt3DCore::QBackendNode *RenderSettingsFunctor::create(const Qt3DCore::QNodeCreatedChangeBasePtr &change) const
{
    Q_UNUSED(change);
    if (m_renderer->settings() != nullptr) {
        qWarning() << "Renderer settings already exists";
        return nullptr;
    }

    RenderSettings *settings = new RenderSettings;
    settings->setRenderer(m_renderer);
    m_renderer->setSettings(settings);
    return settings;
}

Qt3DCore::QBackendNode *RenderSettingsFunctor::get(Qt3DCore::QNodeId id) const
{
    Q_UNUSED(id);
    return m_renderer->settings();
}

void RenderSettingsFunctor::destroy(Qt3DCore::QNodeId id) const
{
    Q_UNUSED(id);
    // Deletes the old settings object
    auto settings = m_renderer->settings();
    delete settings;
    m_renderer->setSettings(nullptr);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
