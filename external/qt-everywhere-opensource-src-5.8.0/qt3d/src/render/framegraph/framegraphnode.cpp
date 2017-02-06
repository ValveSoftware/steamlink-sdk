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

#include "framegraphnode_p.h"
#include <Qt3DRender/private/renderer_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {
namespace Render {

FrameGraphNode::FrameGraphNode()
    : BackendNode()
    , m_nodeType(InvalidNodeType)
    , m_manager(nullptr)
{
}

FrameGraphNode::FrameGraphNode(FrameGraphNodeType nodeType, QBackendNode::Mode mode)
    : BackendNode(mode)
    , m_nodeType(nodeType)
    , m_manager(nullptr)
{
}

FrameGraphNode::~FrameGraphNode()
{
}

void FrameGraphNode::initializeFromPeer(const Qt3DCore::QNodeCreatedChangeBasePtr &change)
{
    // Set up the parent child relationship and enabled state
    setParentId(change->parentId());
}

void FrameGraphNode::setFrameGraphManager(FrameGraphManager *manager)
{
    if (m_manager != manager)
        m_manager = manager;
}

FrameGraphManager *FrameGraphNode::manager() const
{
    return m_manager;
}

void FrameGraphNode::setParentId(Qt3DCore::QNodeId parentId)
{
    if (m_parentId != parentId) {
        // We already had a parent, tell it to abandon us
        if (!m_parentId.isNull()) {
            FrameGraphNode *parent = m_manager->lookupNode(m_parentId);
            if (parent != nullptr)
                parent->m_childrenIds.removeAll(peerId());
        }
        m_parentId = parentId;
        FrameGraphNode *parent = m_manager->lookupNode(m_parentId);
        if (parent != nullptr && !parent->m_childrenIds.contains(peerId()))
            parent->m_childrenIds.append(peerId());
    }
}

void FrameGraphNode::appendChildId(Qt3DCore::QNodeId childId)
{
    if (!m_childrenIds.contains(childId)) {
        FrameGraphNode *child = m_manager->lookupNode(childId);
        if (child != nullptr) {
            m_childrenIds.append(childId);
            child->m_parentId = peerId();
        }
    }
}

void FrameGraphNode::removeChildId(Qt3DCore::QNodeId childId)
{
    if (m_childrenIds.contains(childId)) {
        FrameGraphNode *child = m_manager->lookupNode(childId);
        if (child != nullptr) {
            child->m_parentId = Qt3DCore::QNodeId();
        }
        m_childrenIds.removeAll(childId);
    }
}

Qt3DCore::QNodeId FrameGraphNode::parentId() const
{
    return m_parentId;
}

QVector<Qt3DCore::QNodeId> FrameGraphNode::childrenIds() const
{
    return m_childrenIds;
}

FrameGraphNode *FrameGraphNode::parent() const
{
    return m_manager->lookupNode(m_parentId);
}

QVector<FrameGraphNode *> FrameGraphNode::children() const
{
    QVector<FrameGraphNode *> children;
    children.reserve(m_childrenIds.size());

    for (Qt3DCore::QNodeId id : m_childrenIds) {
        FrameGraphNode *child = m_manager->lookupNode(id);
        if (child != nullptr)
            children << child;
    }
    return children;
}

void FrameGraphNode::sceneChangeEvent(const Qt3DCore::QSceneChangePtr &e)
{
    switch (e->type()) {

    case Qt3DCore::PropertyValueAdded: {
       Qt3DCore::QPropertyNodeAddedChangePtr change = qSharedPointerCast<Qt3DCore::QPropertyNodeAddedChange>(e);
        if (change->metaObject()->inherits(&QFrameGraphNode::staticMetaObject))
            appendChildId(change->addedNodeId());
        break;
    }

    case Qt3DCore::PropertyValueRemoved: {
        Qt3DCore::QPropertyNodeRemovedChangePtr change = qSharedPointerCast<Qt3DCore::QPropertyNodeRemovedChange>(e);
        if (change->metaObject()->inherits(&QFrameGraphNode::staticMetaObject))
            removeChildId(change->removedNodeId());
        break;
    }
    default:
        break;
    }

    markDirty(AbstractRenderer::AllDirty);
    BackendNode::sceneChangeEvent(e);
}

} // namespace Render
} // namespace Qt3DRender

QT_END_NAMESPACE
