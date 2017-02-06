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

#include "renderpassfilternode_p.h"
#include "qfilterkey.h"
#include "qrenderpassfilter.h"
#include <Qt3DRender/private/qrenderpassfilter_p.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

RenderPassFilter::RenderPassFilter()
    : FrameGraphNode(FrameGraphNode::RenderPassFilter)
{
}

void RenderPassFilter::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    FrameGraphNode::initializeFromPeer(change);
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QRenderPassFilterData>>(change);
    const auto &data = typedChange->data;
    m_filters = data.matchIds;
    m_parameterPack.clear();
    m_parameterPack.setParameters(data.parameterIds);
}

QVector<Qt3DCore::QNodeId> RenderPassFilter::filters() const
{
    return m_filters;
}

void RenderPassFilter::appendFilter(Qt3DCore::QNodeId criterionId)
{
    if (!m_filters.contains(criterionId))
        m_filters.append(criterionId);
}

void RenderPassFilter::removeFilter(Qt3DCore::QNodeId criterionId)
{
    m_filters.removeOne(criterionId);
}

QVector<Qt3DCore::QNodeId> RenderPassFilter::parameters() const
{
    return m_parameterPack.parameters();
}

void RenderPassFilter::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{

    switch (e->type()) {
    case PropertyValueAdded: {
        const auto change = qSharedPointerCast<QPropertyNodeAddedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("match")) {
            appendFilter(change->addedNodeId());
            markDirty(AbstractRenderer::AllDirty);
        } else if (change->propertyName() == QByteArrayLiteral("parameter")) {
            m_parameterPack.appendParameter(change->addedNodeId());
            markDirty(AbstractRenderer::AllDirty);
        }
        break;
    }

    case PropertyValueRemoved: {
        const auto change = qSharedPointerCast<QPropertyNodeRemovedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("match")) {
            removeFilter(change->removedNodeId());
            markDirty(AbstractRenderer::AllDirty);
        } else if (change->propertyName() == QByteArrayLiteral("parameter")) {
            m_parameterPack.removeParameter(change->removedNodeId());
            markDirty(AbstractRenderer::AllDirty);
        }
        break;
    }

    default:
        break;
    }
    FrameGraphNode::sceneChangeEvent(e);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
