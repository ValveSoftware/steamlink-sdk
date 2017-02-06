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

#include "statesetnode_p.h"

#include <Qt3DRender/qrenderstateset.h>
#include <Qt3DRender/private/qrenderstateset_p.h>
#include <Qt3DRender/private/genericstate_p.h>
#include <Qt3DRender/private/renderstateset_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>

QT_BEGIN_NAMESPACE

using namespace Qt3DCore;

namespace Qt3DRender {
namespace Render {

StateSetNode::StateSetNode()
    : FrameGraphNode(FrameGraphNode::StateSet)
{
}

StateSetNode::~StateSetNode()
{
}

QVector<QNodeId> StateSetNode::renderStates() const
{
    return m_renderStates;
}

void StateSetNode::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    FrameGraphNode::initializeFromPeer(change);
    const auto typedChange = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<QRenderStateSetData>>(change);
    const auto &data = typedChange->data;
    for (const auto &stateId : qAsConst(data.renderStateIds))
        addRenderState(stateId);
}

void StateSetNode::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {
    case PropertyValueAdded: {
        const auto change = qSharedPointerCast<QPropertyNodeAddedChange>(e);
        if (change->propertyName() == QByteArrayLiteral("renderState")) {
            addRenderState(change->addedNodeId());
            markDirty(AbstractRenderer::AllDirty);
        }
        break;
    }

    case PropertyValueRemoved: {
        const auto propertyChange = qSharedPointerCast<QPropertyNodeRemovedChange>(e);
        if (propertyChange->propertyName() == QByteArrayLiteral("renderState")) {
            removeRenderState(propertyChange->removedNodeId());
            markDirty(AbstractRenderer::AllDirty);
        }
        break;
    }

    default:
        break;
    }
    FrameGraphNode::sceneChangeEvent(e);
}

void StateSetNode::addRenderState(QNodeId renderStateId)
{
    if (!m_renderStates.contains(renderStateId))
        m_renderStates.push_back(renderStateId);
}

void StateSetNode::removeRenderState(QNodeId renderStateId)
{
    m_renderStates.removeOne(renderStateId);
}

} // namespace Render

} // namespace Qt3DRender

QT_END_NAMESPACE
