/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
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

#include "renderpass_p.h"
#include <Qt3DRender/private/filterkey_p.h>
#include <Qt3DRender/qrenderstate.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qparameter.h>

#include <Qt3DRender/private/qrenderpass_p.h>
#include <Qt3DRender/private/renderstates_p.h>
#include <Qt3DRender/private/renderstateset_p.h>

#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

RenderPass::RenderPass()
    : BackendNode()
{
}

RenderPass::~RenderPass()
{
}

void RenderPass::cleanup()
{
    QBackendNode::setEnabled(false);
    m_renderStates.clear();
    m_filterKeyList.clear();
    m_parameterPack.clear();
    m_shaderUuid = Qt3DCore::QNodeId();
}

void RenderPass::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QRenderPassData>>(change);
    const auto &data = typedChange->data;
    m_filterKeyList = data.filterKeyIds;
    m_parameterPack.setParameters(data.parameterIds);
    for (const auto &renderStateId : qAsConst(data.renderStateIds))
        addRenderState(renderStateId);
    m_shaderUuid = data.shaderId;
}

void RenderPass::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case PropertyValueAdded: {
        const auto change = qSharedPointerCast<QPropertyNodeAddedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("filterKeys"))
            appendFilterKey(change->addedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("shaderProgram"))
            m_shaderUuid = change->addedNodeId();
        else if (change->propertyName() == QByteArrayLiteral("renderState"))
            addRenderState(change->addedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("parameter"))
            m_parameterPack.appendParameter(change->addedNodeId());
        break;
    }

    case PropertyUpdated: {
        const auto change = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("shaderProgram"))
            m_shaderUuid = change->value().value<Qt3DCore::QNodeId>();
        break;
    }

    case PropertyValueRemoved: {
        const auto change = qSharedPointerCast<QPropertyNodeRemovedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("filterKeys"))
            removeFilterKey(change->removedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("shaderProgram"))
            m_shaderUuid = QNodeId();
        else if (change->propertyName() == QByteArrayLiteral("renderState"))
            removeRenderState(change->removedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("parameter"))
            m_parameterPack.removeParameter(change->removedNodeId());
        break;
    }

    default:
        break;
    }

    BackendNode::sceneChangeEvent(e);
    markDirty(AbstractRenderer::AllDirty);
}

Qt3DCore::QNodeId RenderPass::shaderProgram() const
{
    return m_shaderUuid;
}

QVector<Qt3DCore::QNodeId> RenderPass::filterKeys() const
{
    return m_filterKeyList;
}

QVector<Qt3DCore::QNodeId> RenderPass::parameters() const
{
    return m_parameterPack.parameters();
}

QVector<QNodeId> RenderPass::renderStates() const
{
    return m_renderStates;
}

void RenderPass::appendFilterKey(Qt3DCore::QNodeId filterKeyId)
{
    if (!m_filterKeyList.contains(filterKeyId))
        m_filterKeyList.append(filterKeyId);
}

void RenderPass::removeFilterKey(Qt3DCore::QNodeId filterKeyId)
{
    m_filterKeyList.removeOne(filterKeyId);
}

void RenderPass::addRenderState(QNodeId renderStateId)
{
    if (!m_renderStates.contains(renderStateId))
        m_renderStates.push_back(renderStateId);
}

void RenderPass::removeRenderState(QNodeId renderStateId)
{
    m_renderStates.removeOne(renderStateId);
}


} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
