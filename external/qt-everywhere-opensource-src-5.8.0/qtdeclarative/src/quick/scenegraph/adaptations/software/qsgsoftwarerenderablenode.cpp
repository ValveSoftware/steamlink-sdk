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

#include "qsgsoftwarerenderablenode_p.h"

#include "qsgsoftwareinternalimagenode_p.h"
#include "qsgsoftwareinternalrectanglenode_p.h"
#include "qsgsoftwareglyphnode_p.h"
#include "qsgsoftwarepublicnodes_p.h"
#include "qsgsoftwarepainternode_p.h"
#include "qsgsoftwarepixmaptexture_p.h"
#if QT_CONFIG(quick_sprite)
#include "qsgsoftwarespritenode_p.h"
#endif

#include <qsgsimplerectnode.h>
#include <qsgsimpletexturenode.h>
#include <private/qsgrendernode_p.h>
#include <private/qsgtexture_p.h>

Q_LOGGING_CATEGORY(lcRenderable, "qt.scenegraph.softwarecontext.renderable")

QT_BEGIN_NAMESPACE

QSGSoftwareRenderableNode::QSGSoftwareRenderableNode(NodeType type, QSGNode *node)
    : m_nodeType(type)
    , m_isOpaque(true)
    , m_isDirty(true)
    , m_hasClipRegion(false)
    , m_opacity(1.0f)
{
    switch (m_nodeType) {
    case QSGSoftwareRenderableNode::SimpleRect:
        m_handle.simpleRectNode = static_cast<QSGSimpleRectNode*>(node);
        break;
    case QSGSoftwareRenderableNode::SimpleTexture:
        m_handle.simpleTextureNode = static_cast<QSGSimpleTextureNode*>(node);
        break;
    case QSGSoftwareRenderableNode::Image:
        m_handle.imageNode = static_cast<QSGSoftwareInternalImageNode*>(node);
        break;
    case QSGSoftwareRenderableNode::Painter:
        m_handle.painterNode = static_cast<QSGSoftwarePainterNode*>(node);
        break;
    case QSGSoftwareRenderableNode::Rectangle:
        m_handle.rectangleNode = static_cast<QSGSoftwareInternalRectangleNode*>(node);
        break;
    case QSGSoftwareRenderableNode::Glyph:
        m_handle.glpyhNode = static_cast<QSGSoftwareGlyphNode*>(node);
        break;
    case QSGSoftwareRenderableNode::NinePatch:
        m_handle.ninePatchNode = static_cast<QSGSoftwareNinePatchNode*>(node);
        break;
    case QSGSoftwareRenderableNode::SimpleRectangle:
        m_handle.simpleRectangleNode = static_cast<QSGRectangleNode*>(node);
        break;
    case QSGSoftwareRenderableNode::SimpleImage:
        m_handle.simpleImageNode = static_cast<QSGImageNode*>(node);
        break;
#if QT_CONFIG(quick_sprite)
    case QSGSoftwareRenderableNode::SpriteNode:
        m_handle.spriteNode = static_cast<QSGSoftwareSpriteNode*>(node);
        break;
#endif
    case QSGSoftwareRenderableNode::RenderNode:
        m_handle.renderNode = static_cast<QSGRenderNode*>(node);
        break;
    case QSGSoftwareRenderableNode::Invalid:
        m_handle.simpleRectNode = nullptr;
        break;
    }
}

QSGSoftwareRenderableNode::~QSGSoftwareRenderableNode()
{

}

void QSGSoftwareRenderableNode::update()
{
    // Update the Node properties
    m_isDirty = true;

    QRect boundingRect;

    switch (m_nodeType) {
    case QSGSoftwareRenderableNode::SimpleRect:
        if (m_handle.simpleRectNode->color().alpha() == 255 && !m_transform.isRotating())
            m_isOpaque = true;
        else
            m_isOpaque = false;

        boundingRect = m_handle.simpleRectNode->rect().toRect();
        break;
    case QSGSoftwareRenderableNode::SimpleTexture:
        if (!m_handle.simpleTextureNode->texture()->hasAlphaChannel() && !m_transform.isRotating())
            m_isOpaque = true;
        else
            m_isOpaque = false;

        boundingRect = m_handle.simpleTextureNode->rect().toRect();
        break;
    case QSGSoftwareRenderableNode::Image:
        // There isn't a way to tell, so assume it's not
        m_isOpaque = false;

        boundingRect = m_handle.imageNode->rect().toRect();
        break;
    case QSGSoftwareRenderableNode::Painter:
        if (m_handle.painterNode->opaquePainting() && !m_transform.isRotating())
            m_isOpaque = true;
        else
            m_isOpaque = false;

        boundingRect = QRect(0, 0, m_handle.painterNode->size().width(), m_handle.painterNode->size().height());
        break;
    case QSGSoftwareRenderableNode::Rectangle:
        if (m_handle.rectangleNode->isOpaque() && !m_transform.isRotating())
            m_isOpaque = true;
        else
            m_isOpaque = false;

        boundingRect = m_handle.rectangleNode->rect().toRect();
        break;
    case QSGSoftwareRenderableNode::Glyph:
        // Always has alpha
        m_isOpaque = false;

        boundingRect = m_handle.glpyhNode->boundingRect().toAlignedRect();
        break;
    case QSGSoftwareRenderableNode::NinePatch:
        // Difficult to tell, assume non-opaque
        m_isOpaque = false;

        boundingRect = m_handle.ninePatchNode->bounds().toRect();
        break;
    case QSGSoftwareRenderableNode::SimpleRectangle:
        if (m_handle.simpleRectangleNode->color().alpha() == 255 && !m_transform.isRotating())
            m_isOpaque = true;
        else
            m_isOpaque = false;

        boundingRect = m_handle.simpleRectangleNode->rect().toRect();
        break;
    case QSGSoftwareRenderableNode::SimpleImage:
        if (!m_handle.simpleImageNode->texture()->hasAlphaChannel() && !m_transform.isRotating())
            m_isOpaque = true;
        else
            m_isOpaque = false;

        boundingRect = m_handle.simpleImageNode->rect().toRect();
        break;
#if QT_CONFIG(quick_sprite)
    case QSGSoftwareRenderableNode::SpriteNode:
        m_isOpaque = m_handle.spriteNode->isOpaque();
        boundingRect = m_handle.spriteNode->rect().toRect();
        break;
#endif
    case QSGSoftwareRenderableNode::RenderNode:
        if (m_handle.renderNode->flags().testFlag(QSGRenderNode::OpaqueRendering) && !m_transform.isRotating())
            m_isOpaque = true;
        else
            m_isOpaque = false;

        boundingRect = m_handle.renderNode->rect().toRect();
        break;
    default:
        break;
    }

    m_boundingRect = m_transform.mapRect(boundingRect);

    if (m_hasClipRegion && m_clipRegion.rectCount() <= 1) {
        // If there is a clipRegion, and it is empty, the item wont be rendered
        if (m_clipRegion.isEmpty())
            m_boundingRect = QRect();
        else
            m_boundingRect = m_boundingRect.intersected(m_clipRegion.rects().first());
    }

    // Overrides
    if (m_opacity < 1.0f)
        m_isOpaque = false;

    m_dirtyRegion = QRegion(m_boundingRect);
}

struct RenderNodeState : public QSGRenderNode::RenderState
{
    const QMatrix4x4 *projectionMatrix() const override { return &ident; }
    QRect scissorRect() const override { return QRect(); }
    bool scissorEnabled() const override { return false; }
    int stencilValue() const override { return 0; }
    bool stencilEnabled() const override { return false; }
    const QRegion *clipRegion() const override { return &cr; }
    QMatrix4x4 ident;
    QRegion cr;
};

QRegion QSGSoftwareRenderableNode::renderNode(QPainter *painter, bool forceOpaquePainting)
{
    Q_ASSERT(painter);

    // Check for don't paint conditions
    if (m_nodeType != RenderNode) {
        if (!m_isDirty || qFuzzyIsNull(m_opacity) || m_dirtyRegion.isEmpty()) {
            m_isDirty = false;
            m_dirtyRegion = QRegion();
            return QRegion();
        }
    } else {
        if (!m_isDirty || qFuzzyIsNull(m_opacity)) {
            m_isDirty = false;
            m_dirtyRegion = QRegion();
            return QRegion();
        } else {
            QSGRenderNodePrivate *rd = QSGRenderNodePrivate::get(m_handle.renderNode);
            QMatrix4x4 m = m_transform;
            rd->m_matrix = &m;
            rd->m_opacity = m_opacity;
            RenderNodeState rs;
            rs.cr = m_clipRegion;

            const QRect br = m_handle.renderNode->flags().testFlag(QSGRenderNode::BoundedRectRendering)
                ? m_boundingRect :
                QRect(0, 0, painter->device()->width(), painter->device()->height());

            painter->save();
            painter->setClipRegion(br, Qt::ReplaceClip);
            m_handle.renderNode->render(&rs);
            painter->restore();

            m_previousDirtyRegion = QRegion(br);
            m_isDirty = false;
            m_dirtyRegion = QRegion();
            return br;
        }
    }

    painter->save();
    painter->setOpacity(m_opacity);

    // Set clipRegion to m_dirtyRegion (in world coordinates)
    // as m_dirtyRegion already accounts for clipRegion
    painter->setClipRegion(m_dirtyRegion, Qt::ReplaceClip);
    if (m_clipRegion.rectCount() > 1)
        painter->setClipRegion(m_clipRegion, Qt::IntersectClip);

    painter->setTransform(m_transform, false); //precalculated worldTransform
    if (forceOpaquePainting || m_isOpaque)
        painter->setCompositionMode(QPainter::CompositionMode_Source);

    switch (m_nodeType) {
    case QSGSoftwareRenderableNode::SimpleRect:
        painter->fillRect(m_handle.simpleRectNode->rect(), m_handle.simpleRectNode->color());
        break;
    case QSGSoftwareRenderableNode::SimpleTexture:
    {
        QSGTexture *texture = m_handle.simpleTextureNode->texture();
        if (QSGSoftwarePixmapTexture *pt = dynamic_cast<QSGSoftwarePixmapTexture *>(texture)) {
            const QPixmap &pm = pt->pixmap();
            painter->drawPixmap(m_handle.simpleTextureNode->rect(), pm, m_handle.simpleTextureNode->sourceRect());
        } else if (QSGPlainTexture *pt = dynamic_cast<QSGPlainTexture *>(texture)) {
            const QImage &im = pt->image();
            painter->drawImage(m_handle.simpleTextureNode->rect(), im, m_handle.simpleTextureNode->sourceRect());
        }
    }
        break;
    case QSGSoftwareRenderableNode::Image:
        m_handle.imageNode->paint(painter);
        break;
    case QSGSoftwareRenderableNode::Painter:
        m_handle.painterNode->paint(painter);
        break;
    case QSGSoftwareRenderableNode::Rectangle:
        m_handle.rectangleNode->paint(painter);
        break;
    case QSGSoftwareRenderableNode::Glyph:
        m_handle.glpyhNode->paint(painter);
        break;
    case QSGSoftwareRenderableNode::NinePatch:
        m_handle.ninePatchNode->paint(painter);
        break;
    case QSGSoftwareRenderableNode::SimpleRectangle:
        static_cast<QSGSoftwareRectangleNode *>(m_handle.simpleRectangleNode)->paint(painter);
        break;
    case QSGSoftwareRenderableNode::SimpleImage:
        static_cast<QSGSoftwareImageNode *>(m_handle.simpleImageNode)->paint(painter);
        break;
#if QT_CONFIG(quick_sprite)
    case QSGSoftwareRenderableNode::SpriteNode:
        static_cast<QSGSoftwareSpriteNode *>(m_handle.spriteNode)->paint(painter);
        break;
#endif
    default:
        break;
    }

    painter->restore();

    QRegion areaToBeFlushed = m_dirtyRegion;
    m_previousDirtyRegion = QRegion(m_boundingRect);
    m_isDirty = false;
    m_dirtyRegion = QRegion();

    return areaToBeFlushed;
}

QRect QSGSoftwareRenderableNode::boundingRect() const
{
    // This returns the bounding area of a renderable node in world coordinates
    return m_boundingRect;
}

bool QSGSoftwareRenderableNode::isDirtyRegionEmpty() const
{
    return m_dirtyRegion.isEmpty();
}

void QSGSoftwareRenderableNode::setTransform(const QTransform &transform)
{
    if (m_transform == transform)
        return;
    m_transform = transform;
    update();
}

void QSGSoftwareRenderableNode::setClipRegion(const QRegion &clipRect, bool hasClipRegion)
{
    if (m_clipRegion == clipRect && m_hasClipRegion == hasClipRegion)
        return;

    m_clipRegion = clipRect;
    m_hasClipRegion = hasClipRegion;
    update();
}

void QSGSoftwareRenderableNode::setOpacity(float opacity)
{
    if (qFuzzyCompare(m_opacity, opacity))
        return;

    m_opacity = opacity;
    update();
}

void QSGSoftwareRenderableNode::markGeometryDirty()
{
    update();
}

void QSGSoftwareRenderableNode::markMaterialDirty()
{
    update();
}

void QSGSoftwareRenderableNode::addDirtyRegion(const QRegion &dirtyRegion, bool forceDirty)
{
    // Check if the dirty region applys to this node
    QRegion prev = m_dirtyRegion;
    if (dirtyRegion.intersects(boundingRect())) {
        if (forceDirty)
            m_isDirty = true;
        m_dirtyRegion += dirtyRegion.intersected(boundingRect());
    }
    qCDebug(lcRenderable) << "addDirtyRegion: " << dirtyRegion << "old dirtyRegion: " << prev << "new dirtyRegion: " << m_dirtyRegion;
}

void QSGSoftwareRenderableNode::subtractDirtyRegion(const QRegion &dirtyRegion)
{
    QRegion prev = m_dirtyRegion;
    if (m_isDirty) {
        // Check if this rect concerns us
        if (dirtyRegion.intersects(QRegion(boundingRect()))) {
            m_dirtyRegion -= dirtyRegion;
            if (m_dirtyRegion.isEmpty())
                m_isDirty = false;
        }
    }
    qCDebug(lcRenderable) << "subtractDirtyRegion: " << dirtyRegion << "old dirtyRegion" << prev << "new dirtyRegion: " << m_dirtyRegion;
}

QRegion QSGSoftwareRenderableNode::previousDirtyRegion(bool wasRemoved) const
{
    // When removing a node, the boundingRect shouldn't be subtracted
    // because a deleted node has no valid boundingRect
    if (wasRemoved)
        return m_previousDirtyRegion;

    return m_previousDirtyRegion.subtracted(QRegion(m_boundingRect));
}

QRegion QSGSoftwareRenderableNode::dirtyRegion() const
{
    return m_dirtyRegion;
}

QT_END_NAMESPACE
