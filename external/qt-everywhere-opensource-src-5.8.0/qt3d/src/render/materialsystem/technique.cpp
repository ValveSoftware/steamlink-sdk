/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
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

#include "technique_p.h"

#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qgraphicsapifilter.h>
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/filterkey_p.h>
#include <Qt3DRender/private/qtechnique_p.h>
#include <Qt3DRender/private/shader_p.h>
#include <Qt3DCore/private/qchangearbiter_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>

#include <QDebug>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

Technique::Technique()
    : BackendNode()
    , m_isCompatibleWithRenderer(false)
    , m_nodeManager(nullptr)
{
}

Technique::~Technique()
{
    cleanup();
}

void Technique::cleanup()
{
    QBackendNode::setEnabled(false);
    m_parameterPack.clear();
    m_renderPasses.clear();
    m_filterKeyList.clear();
    m_isCompatibleWithRenderer = false;
}

void Technique::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QTechniqueData>>(change);
    const QTechniqueData &data = typedChange->data;

    m_graphicsApiFilterData = data.graphicsApiFilterData;
    m_filterKeyList = data.filterKeyIds;
    m_parameterPack.setParameters(data.parameterIds);
    m_renderPasses = data.renderPassIds;
}

void Technique::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case PropertyUpdated: {
        const auto change = qSharedPointerCast<QPropertyUpdatedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("graphicsApiFilterData")) {
            GraphicsApiFilterData filterData = change->value().value<GraphicsApiFilterData>();
            m_graphicsApiFilterData = filterData;
            // Notify the manager that our graphicsApiFilterData has changed
            // and that we therefore need to be check for compatibility again
            m_isCompatibleWithRenderer = false;
        }
        break;
    }

    case PropertyValueAdded: {
        const auto change = qSharedPointerCast<QPropertyNodeAddedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("pass"))
            appendRenderPass(change->addedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("parameter"))
            m_parameterPack.appendParameter(change->addedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("filterKeys"))
            appendFilterKey(change->addedNodeId());
        break;
    }

    case PropertyValueRemoved: {
        const auto change = qSharedPointerCast<QPropertyNodeRemovedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("pass"))
            removeRenderPass(change->removedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("parameter"))
            m_parameterPack.removeParameter(change->removedNodeId());
        else if (change->propertyName() == QByteArrayLiteral("filterKeys"))
            removeFilterKey(change->removedNodeId());
        break;
    }

    default:
        break;
    }
    markDirty(AbstractRenderer::AllDirty);
    BackendNode::sceneChangeEvent(e);
}

QVector<Qt3DCore::QNodeId> Technique::parameters() const
{
    return m_parameterPack.parameters();
}

void Technique::appendRenderPass(Qt3DCore::QNodeId renderPassId)
{
    if (!m_renderPasses.contains(renderPassId))
        m_renderPasses.append(renderPassId);
}

void Technique::removeRenderPass(Qt3DCore::QNodeId renderPassId)
{
    m_renderPasses.removeOne(renderPassId);
}

QVector<Qt3DCore::QNodeId> Technique::filterKeys() const
{
    return m_filterKeyList;
}

QVector<Qt3DCore::QNodeId> Technique::renderPasses() const
{
    return m_renderPasses;
}

const GraphicsApiFilterData *Technique::graphicsApiFilter() const
{
    return &m_graphicsApiFilterData;
}

bool Technique::isCompatibleWithRenderer() const
{
    return m_isCompatibleWithRenderer;
}

void Technique::setCompatibleWithRenderer(bool compatible)
{
    m_isCompatibleWithRenderer = compatible;
}

bool Technique::isCompatibleWithFilters(const QNodeIdVector &filterKeyIds)
{
    // There is a technique filter so we need to check for a technique with suitable criteria.
    // Check for early bail out if the technique doesn't have sufficient number of criteria and
    // can therefore never satisfy the filter
    if (m_filterKeyList.size() < filterKeyIds.size())
        return false;

    // Iterate through the filter criteria and for each one search for a criteria on the
    // technique that satisfies it
    for (const QNodeId filterKeyId : filterKeyIds) {
        FilterKey *filterKey = m_nodeManager->filterKeyManager()->lookupResource(filterKeyId);

        bool foundMatch = false;

        for (const QNodeId techniqueFilterKeyId : qAsConst(m_filterKeyList)) {
            FilterKey *techniqueFilterKey = m_nodeManager->filterKeyManager()->lookupResource(techniqueFilterKeyId);
            if ((foundMatch = (*techniqueFilterKey == *filterKey)))
                break;
        }

        // No match for TechniqueFilter criterion in any of the technique's criteria.
        // So no way this can match. Don't bother checking the rest of the criteria.
        if (!foundMatch)
            return false;
    }
    return true;
}

void Technique::setNodeManager(NodeManagers *nodeManager)
{
    m_nodeManager = nodeManager;
}

NodeManagers *Technique::nodeManager() const
{
    return m_nodeManager;
}

void Technique::appendFilterKey(Qt3DCore::QNodeId criterionId)
{
    if (!m_filterKeyList.contains(criterionId))
        m_filterKeyList.append(criterionId);
}

void Technique::removeFilterKey(Qt3DCore::QNodeId criterionId)
{
    m_filterKeyList.removeOne(criterionId);
}

TechniqueFunctor::TechniqueFunctor(AbstractRenderer *renderer, NodeManagers *manager)
    : m_manager(manager)
    , m_renderer(renderer)
{
}

QBackendNode *TechniqueFunctor::create(const QNodeCreatedChangeBasePtr &change) const
{
    Technique *technique = m_manager->techniqueManager()->getOrCreateResource(change->subjectId());
    technique->setNodeManager(m_manager);
    technique->setRenderer(m_renderer);
    return technique;
}

QBackendNode *TechniqueFunctor::get(QNodeId id) const
{
    return m_manager->techniqueManager()->lookupResource(id);
}

void TechniqueFunctor::destroy(QNodeId id) const
{
    m_manager->techniqueManager()->releaseResource(id);

}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
