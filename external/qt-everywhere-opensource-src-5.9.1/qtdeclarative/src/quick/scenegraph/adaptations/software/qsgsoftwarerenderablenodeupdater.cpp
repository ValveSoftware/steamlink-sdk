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

#include "qsgsoftwarerenderablenodeupdater_p.h"

#include "qsgabstractsoftwarerenderer_p.h"
#include "qsgsoftwareinternalimagenode_p.h"
#include "qsgsoftwareinternalrectanglenode_p.h"
#include "qsgsoftwareglyphnode_p.h"
#include "qsgsoftwarepublicnodes_p.h"
#include "qsgsoftwarepainternode_p.h"
#include "qsgsoftwarepixmaptexture_p.h"

#include <QtQuick/qsgsimplerectnode.h>
#include <QtQuick/qsgsimpletexturenode.h>
#include <QtQuick/qsgrendernode.h>

QT_BEGIN_NAMESPACE

QSGSoftwareRenderableNodeUpdater::QSGSoftwareRenderableNodeUpdater(QSGAbstractSoftwareRenderer *renderer)
    : m_renderer(renderer)
{
    m_opacityState.push(1.0f);
    // Invalid RectF by default for no clip
    m_clipState.push(QRegion());
    m_hasClip = false;
    m_transformState.push(QTransform());
}

QSGSoftwareRenderableNodeUpdater::~QSGSoftwareRenderableNodeUpdater()
{

}

bool QSGSoftwareRenderableNodeUpdater::visit(QSGTransformNode *node)
{
    m_transformState.push(node->matrix().toTransform() * m_transformState.top());
    m_stateMap[node] = currentState(node);
    return true;
}

void QSGSoftwareRenderableNodeUpdater::endVisit(QSGTransformNode *)
{
    m_transformState.pop();
}

bool QSGSoftwareRenderableNodeUpdater::visit(QSGClipNode *node)
{
    // Make sure to translate the clip rect into world coordinates
    if (m_clipState.count() == 1) {
        m_clipState.push(m_transformState.top().map(QRegion(node->clipRect().toRect())));
        m_hasClip = true;
    } else {
        const QRegion transformedClipRect = m_transformState.top().map(QRegion(node->clipRect().toRect()));
        m_clipState.push(transformedClipRect.intersected(m_clipState.top()));
    }
    m_stateMap[node] = currentState(node);
    return true;
}

void QSGSoftwareRenderableNodeUpdater::endVisit(QSGClipNode *)
{
    m_clipState.pop();
    if (m_clipState.count() == 1)
        m_hasClip = false;
}

bool QSGSoftwareRenderableNodeUpdater::visit(QSGGeometryNode *node)
{
    if (QSGSimpleRectNode *rectNode = dynamic_cast<QSGSimpleRectNode *>(node)) {
        return updateRenderableNode(QSGSoftwareRenderableNode::SimpleRect, rectNode);
    } else if (QSGSimpleTextureNode *tn = dynamic_cast<QSGSimpleTextureNode *>(node)) {
        return updateRenderableNode(QSGSoftwareRenderableNode::SimpleTexture, tn);
    } else if (QSGNinePatchNode *nn = dynamic_cast<QSGNinePatchNode *>(node)) {
        return updateRenderableNode(QSGSoftwareRenderableNode::NinePatch, nn);
    } else if (QSGRectangleNode *rn = dynamic_cast<QSGRectangleNode *>(node)) {
        return updateRenderableNode(QSGSoftwareRenderableNode::SimpleRectangle, rn);
    } else if (QSGImageNode *n = dynamic_cast<QSGImageNode *>(node)) {
        return updateRenderableNode(QSGSoftwareRenderableNode::SimpleImage, n);
    } else {
        // We dont know, so skip
        return false;
    }
}

void QSGSoftwareRenderableNodeUpdater::endVisit(QSGGeometryNode *)
{
}

bool QSGSoftwareRenderableNodeUpdater::visit(QSGOpacityNode *node)
{
    m_opacityState.push(m_opacityState.top() * node->opacity());
    m_stateMap[node] = currentState(node);
    return true;
}

void QSGSoftwareRenderableNodeUpdater::endVisit(QSGOpacityNode *)
{
    m_opacityState.pop();
}

bool QSGSoftwareRenderableNodeUpdater::visit(QSGInternalImageNode *node)
{
    return updateRenderableNode(QSGSoftwareRenderableNode::Image, node);
}

void QSGSoftwareRenderableNodeUpdater::endVisit(QSGInternalImageNode *)
{
}

bool QSGSoftwareRenderableNodeUpdater::visit(QSGPainterNode *node)
{
    return updateRenderableNode(QSGSoftwareRenderableNode::Painter, node);
}

void QSGSoftwareRenderableNodeUpdater::endVisit(QSGPainterNode *)
{
}

bool QSGSoftwareRenderableNodeUpdater::visit(QSGInternalRectangleNode *node)
{
    return updateRenderableNode(QSGSoftwareRenderableNode::Rectangle, node);
}

void QSGSoftwareRenderableNodeUpdater::endVisit(QSGInternalRectangleNode *)
{
}

bool QSGSoftwareRenderableNodeUpdater::visit(QSGGlyphNode *node)
{
    return updateRenderableNode(QSGSoftwareRenderableNode::Glyph, node);
}

void QSGSoftwareRenderableNodeUpdater::endVisit(QSGGlyphNode *)
{
}

bool QSGSoftwareRenderableNodeUpdater::visit(QSGRootNode *node)
{
    m_stateMap[node] = currentState(node);
    return true;
}

void QSGSoftwareRenderableNodeUpdater::endVisit(QSGRootNode *)
{
}

#if QT_CONFIG(quick_sprite)
bool QSGSoftwareRenderableNodeUpdater::visit(QSGSpriteNode *node)
{
    return updateRenderableNode(QSGSoftwareRenderableNode::SpriteNode, node);
}

void QSGSoftwareRenderableNodeUpdater::endVisit(QSGSpriteNode *)
{

}
#endif

bool QSGSoftwareRenderableNodeUpdater::visit(QSGRenderNode *node)
{
    return updateRenderableNode(QSGSoftwareRenderableNode::RenderNode, node);
}

void QSGSoftwareRenderableNodeUpdater::endVisit(QSGRenderNode *)
{
}

void QSGSoftwareRenderableNodeUpdater::updateNodes(QSGNode *node, bool isNodeRemoved)
{
    m_opacityState.clear();
    m_clipState.clear();
    m_transformState.clear();

    auto parentNode = node->parent();
    // If the node was deleted, it will have no parent
    // check if the state map has the previous parent
    if ((!parentNode || isNodeRemoved ) && m_stateMap.contains(node))
        parentNode = m_stateMap[node].parent;

    // If we find a parent, use its state for updating the new children
    if (parentNode && m_stateMap.contains(parentNode)) {
        auto state = m_stateMap[parentNode];
        m_opacityState.push(state.opacity);
        m_transformState.push(state.transform);
        m_clipState.push(state.clip);
        m_hasClip = state.hasClip;
    } else {
        // There is no parent, and no previous parent, so likely a root node
        m_opacityState.push(1.0f);
        m_transformState.push(QTransform());
        m_clipState.push(QRegion());
        m_hasClip = false;
    }

    // If the node is being removed, then cleanup the state data
    // Then just visit the children without visiting the now removed node
    if (isNodeRemoved) {
        m_stateMap.remove(node);
        return;
    }

    // Visit the current node itself first
    switch (node->type()) {
    case QSGNode::ClipNodeType: {
        QSGClipNode *c = static_cast<QSGClipNode*>(node);
        if (visit(c))
            visitChildren(c);
        endVisit(c);
        break;
    }
    case QSGNode::TransformNodeType: {
        QSGTransformNode *c = static_cast<QSGTransformNode*>(node);
        if (visit(c))
            visitChildren(c);
        endVisit(c);
        break;
    }
    case QSGNode::OpacityNodeType: {
        QSGOpacityNode *c = static_cast<QSGOpacityNode*>(node);
        if (visit(c))
            visitChildren(c);
        endVisit(c);
        break;
    }
    case QSGNode::GeometryNodeType: {
        if (node->flags() & QSGNode::IsVisitableNode) {
            QSGVisitableNode *v = static_cast<QSGVisitableNode*>(node);
            v->accept(this);
        } else {
            QSGGeometryNode *c = static_cast<QSGGeometryNode*>(node);
            if (visit(c))
                visitChildren(c);
            endVisit(c);
        }
        break;
    }
    case QSGNode::RootNodeType: {
        QSGRootNode *root = static_cast<QSGRootNode*>(node);
        if (visit(root))
            visitChildren(root);
        endVisit(root);
        break;
    }
    case QSGNode::BasicNodeType: {
            visitChildren(node);
        break;
    }
    case QSGNode::RenderNodeType: {
        QSGRenderNode *r = static_cast<QSGRenderNode*>(node);
        if (visit(r))
            visitChildren(r);
        endVisit(r);
        break;
    }
    default:
        Q_UNREACHABLE();
        break;
    }
}

QSGSoftwareRenderableNodeUpdater::NodeState QSGSoftwareRenderableNodeUpdater::currentState(QSGNode *node) const
{
    NodeState state;
    state.opacity = m_opacityState.top();
    state.clip = m_clipState.top();
    state.hasClip = m_hasClip;
    state.transform = m_transformState.top();
    state.parent = node->parent();
    return state;
}

QT_END_NAMESPACE
