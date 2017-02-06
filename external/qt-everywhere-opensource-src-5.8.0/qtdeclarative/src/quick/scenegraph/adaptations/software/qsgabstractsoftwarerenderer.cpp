/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qsgabstractsoftwarerenderer_p.h"

#include "qsgsoftwarerenderablenodeupdater_p.h"
#include "qsgsoftwarerenderlistbuilder_p.h"
#include "qsgsoftwarecontext_p.h"
#include "qsgsoftwarerenderablenode_p.h"

#include <QtCore/QLoggingCategory>
#include <QtGui/QWindow>
#include <QtQuick/QSGSimpleRectNode>

Q_LOGGING_CATEGORY(lc2DRender, "qt.scenegraph.softwarecontext.abstractrenderer")

QT_BEGIN_NAMESPACE

QSGAbstractSoftwareRenderer::QSGAbstractSoftwareRenderer(QSGRenderContext *context)
    : QSGRenderer(context)
    , m_background(new QSGSimpleRectNode)
    , m_nodeUpdater(new QSGSoftwareRenderableNodeUpdater(this))
{
    // Setup special background node
    auto backgroundRenderable = new QSGSoftwareRenderableNode(QSGSoftwareRenderableNode::SimpleRect, m_background);
    addNodeMapping(m_background, backgroundRenderable);
}

QSGAbstractSoftwareRenderer::~QSGAbstractSoftwareRenderer()
{
    // Cleanup RenderableNodes
    delete m_background;

    qDeleteAll(m_nodes);

    delete m_nodeUpdater;
}

QSGSoftwareRenderableNode *QSGAbstractSoftwareRenderer::renderableNode(QSGNode *node) const
{
    return m_nodes.value(node, nullptr);
}

void QSGAbstractSoftwareRenderer::addNodeMapping(QSGNode *node, QSGSoftwareRenderableNode *renderableNode)
{
    m_nodes.insert(node, renderableNode);
}

void QSGAbstractSoftwareRenderer::appendRenderableNode(QSGSoftwareRenderableNode *node)
{
    m_renderableNodes.append(node);
}

void QSGAbstractSoftwareRenderer::nodeChanged(QSGNode *node, QSGNode::DirtyState state)
{
        if (state & QSGNode::DirtyGeometry) {
            nodeGeometryUpdated(node);
        }
        if (state & QSGNode::DirtyMaterial) {
            nodeMaterialUpdated(node);
        }
        if (state & QSGNode::DirtyMatrix) {
            nodeMatrixUpdated(node);
        }
        if (state & QSGNode::DirtyNodeAdded) {
            nodeAdded(node);
        }
        if (state & QSGNode::DirtyNodeRemoved) {
            nodeRemoved(node);
        }
        if (state & QSGNode::DirtyOpacity) {
            nodeOpacityUpdated(node);
        }
        if (state & QSGNode::DirtySubtreeBlocked) {
            m_nodeUpdater->updateNodes(node);
        }
        if (state & QSGNode::DirtyForceUpdate) {
            m_nodeUpdater->updateNodes(node);
        }
        QSGRenderer::nodeChanged(node, state);
}

QRegion QSGAbstractSoftwareRenderer::renderNodes(QPainter *painter)
{
    QRegion dirtyRegion;
    // If there are no nodes, do nothing
    if (m_renderableNodes.isEmpty())
        return dirtyRegion;

    auto iterator = m_renderableNodes.begin();
    // First node is the background and needs to painted without blending
    auto backgroundNode = *iterator;
    dirtyRegion += backgroundNode->renderNode(painter, /*force opaque painting*/ true);
    iterator++;

    for (; iterator != m_renderableNodes.end(); ++iterator) {
        auto node = *iterator;
        dirtyRegion += node->renderNode(painter);
    }

    return dirtyRegion;
}

void QSGAbstractSoftwareRenderer::buildRenderList()
{
    // Clear the previous renderlist
    m_renderableNodes.clear();
    // Add the background renderable (always first)
    m_renderableNodes.append(renderableNode(m_background));
    // Build the renderlist
    QSGSoftwareRenderListBuilder(this).visitChildren(rootNode());
}

QRegion QSGAbstractSoftwareRenderer::optimizeRenderList()
{
    // Iterate through the renderlist from front to back
    // Objective is to update the dirty status and rects.
    for (auto i = m_renderableNodes.rbegin(); i != m_renderableNodes.rend(); ++i) {
        auto node = *i;
        if (!m_dirtyRegion.isEmpty()) {
            // See if the current dirty regions apply to the current node
            node->addDirtyRegion(m_dirtyRegion, true);
        }

        if (!m_obscuredRegion.isEmpty()) {
            // Don't try to paint things that are covered by opaque objects
            node->subtractDirtyRegion(m_obscuredRegion);
        }

        // Keep up with obscured regions
        if (node->isOpaque()) {
            m_obscuredRegion += QRegion(node->boundingRect());
        }

        if (node->isDirty()) {
            // Don't paint things outside of the rendering area
            if (!m_background->rect().toRect().contains(node->boundingRect(), /*proper*/ true)) {
                // Some part(s) of node is(are) outside of the rendering area
                QRegion renderArea(m_background->rect().toRect());
                QRegion outsideRegions = node->dirtyRegion().subtracted(renderArea);
                if (!outsideRegions.isEmpty())
                    node->subtractDirtyRegion(outsideRegions);
            }

            // Get the dirty region's to pass to the next nodes
            if (node->isOpaque()) {
                // if isOpaque, subtract node's dirty rect from m_dirtyRegion
                m_dirtyRegion -= node->dirtyRegion();
            } else {
                // if isAlpha, add node's dirty rect to m_dirtyRegion
                m_dirtyRegion += node->dirtyRegion();
            }
            // if previousDirtyRegion has content outside of boundingRect add to m_dirtyRegion
            QRegion prevDirty = node->previousDirtyRegion();
            if (!prevDirty.isNull())
                m_dirtyRegion += prevDirty;
        }
    }

    // Empty dirtyRegion (for second pass)
    m_dirtyRegion = QRegion();
    m_obscuredRegion = QRegion();

    // Iterate through the renderlist from back to front
    // Objective is to make sure all non-opaque items are painted when an item under them is dirty
    for (auto j = m_renderableNodes.begin(); j != m_renderableNodes.end(); ++j) {
        auto node = *j;

        if (!node->isOpaque() && !m_dirtyRegion.isEmpty()) {
            // Only blended nodes need to be updated
            node->addDirtyRegion(m_dirtyRegion, true);
        }

        m_dirtyRegion += node->dirtyRegion();
    }

    QRegion updateRegion = m_dirtyRegion;

    // Empty dirtyRegion
    m_dirtyRegion = QRegion();
    m_obscuredRegion = QRegion();

    return updateRegion;
}

void QSGAbstractSoftwareRenderer::setBackgroundColor(const QColor &color)
{
    if (m_background->color() == color)
        return;
    m_background->setColor(color);
    renderableNode(m_background)->markMaterialDirty();
}

void QSGAbstractSoftwareRenderer::setBackgroundSize(const QSize &size)
{
    if (m_background->rect().size().toSize() == size)
        return;
    m_background->setRect(0.0f, 0.0f, size.width(), size.height());
    renderableNode(m_background)->markGeometryDirty();
    // Invalidate the whole scene when the background is resized
    markDirty();
}

QColor QSGAbstractSoftwareRenderer::backgroundColor()
{
    return m_background->color();
}

QSize QSGAbstractSoftwareRenderer::backgroundSize()
{
    return m_background->rect().size().toSize();
}

void QSGAbstractSoftwareRenderer::nodeAdded(QSGNode *node)
{
    qCDebug(lc2DRender) << "nodeAdded" << (void*)node;

    m_nodeUpdater->updateNodes(node);
}

void QSGAbstractSoftwareRenderer::nodeRemoved(QSGNode *node)
{
    qCDebug(lc2DRender) << "nodeRemoved" << (void*)node;

    auto renderable = renderableNode(node);
    // remove mapping
    if (renderable != nullptr) {
        // Need to mark this region dirty in the other nodes
        QRegion dirtyRegion = renderable->previousDirtyRegion(true);
        if (dirtyRegion.isEmpty())
            dirtyRegion = renderable->boundingRect();
        m_dirtyRegion += dirtyRegion;
        m_nodes.remove(node);
        delete renderable;
    }

    // Remove all children nodes as well
    for (QSGNode *child = node->firstChild(); child; child = child->nextSibling()) {
        nodeRemoved(child);
    }

    m_nodeUpdater->updateNodes(node, true);
}

void QSGAbstractSoftwareRenderer::nodeGeometryUpdated(QSGNode *node)
{
    qCDebug(lc2DRender) << "nodeGeometryUpdated";

    // Mark node as dirty
    auto renderable = renderableNode(node);
    if (renderable != nullptr) {
        renderable->markGeometryDirty();
    } else {
        m_nodeUpdater->updateNodes(node);
    }
}

void QSGAbstractSoftwareRenderer::nodeMaterialUpdated(QSGNode *node)
{
    qCDebug(lc2DRender) << "nodeMaterialUpdated";

    // Mark node as dirty
    auto renderable = renderableNode(node);
    if (renderable != nullptr) {
        renderable->markMaterialDirty();
    } else {
        m_nodeUpdater->updateNodes(node);
    }
}

void QSGAbstractSoftwareRenderer::nodeMatrixUpdated(QSGNode *node)
{
    qCDebug(lc2DRender) << "nodeMaterialUpdated";

    // Update children nodes
    m_nodeUpdater->updateNodes(node);
}

void QSGAbstractSoftwareRenderer::nodeOpacityUpdated(QSGNode *node)
{
    qCDebug(lc2DRender) << "nodeOpacityUpdated";

    // Update children nodes
    m_nodeUpdater->updateNodes(node);
}

void QSGAbstractSoftwareRenderer::markDirty()
{
    m_dirtyRegion = QRegion(m_background->rect().toRect());
}

QT_END_NAMESPACE
