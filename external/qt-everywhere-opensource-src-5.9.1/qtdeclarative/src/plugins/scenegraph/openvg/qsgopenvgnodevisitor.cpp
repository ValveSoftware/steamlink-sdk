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

#include "qsgopenvgnodevisitor.h"
#include "qsgopenvginternalrectanglenode.h"
#include "qsgopenvginternalimagenode.h"
#include "qsgopenvgpublicnodes.h"
#include "qsgopenvgglyphnode_p.h"
#include "qsgopenvgpainternode.h"
#if QT_CONFIG(quick_sprite)
#include "qsgopenvgspritenode.h"
#endif
#include "qsgopenvgrenderable.h"

#include "qopenvgcontext_p.h"

#include <QtQuick/qsgsimplerectnode.h>
#include <QtQuick/qsgsimpletexturenode.h>
#include <QtQuick/qsgrendernode.h>

QT_BEGIN_NAMESPACE

QSGOpenVGNodeVisitor::QSGOpenVGNodeVisitor()
{
    //Store the current matrix state
    QVector<VGfloat> matrix(9);
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
    vgGetMatrix(matrix.data());

    m_transformStack.push(QOpenVGMatrix(matrix.constData()));

    // Opacity
    m_opacityState.push(1.0f);
}

bool QSGOpenVGNodeVisitor::visit(QSGTransformNode *node)
{
    const QVector<float> matrixData = { node->matrix().constData()[0], node->matrix().constData()[1], node->matrix().constData()[3],
                                        node->matrix().constData()[4], node->matrix().constData()[5], node->matrix().constData()[7],
                                        node->matrix().constData()[12], node->matrix().constData()[13], node->matrix().constData()[15] };
    const QOpenVGMatrix matrix2d(matrixData.constData());

    m_transformStack.push(m_transformStack.top() * matrix2d);
    return true;
}

void QSGOpenVGNodeVisitor::endVisit(QSGTransformNode *)
{
    m_transformStack.pop();
}

bool QSGOpenVGNodeVisitor::visit(QSGClipNode *node)
{
    VGMaskOperation maskOperation = VG_INTERSECT_MASK;
    if (m_clipStack.count() == 0) {
        vgSeti(VG_MASKING, VG_TRUE);
        vgMask(0,VG_FILL_MASK, 0, 0, VG_MAXINT, VG_MAXINT);
    }

    // Render clip node geometry to mask
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
    vgLoadIdentity();
    VGPath clipPath = generateClipPath(node->clipRect());
    vgRenderToMask(clipPath, VG_FILL_PATH, maskOperation);

    m_clipStack.push(clipPath);

    return true;
}

void QSGOpenVGNodeVisitor::endVisit(QSGClipNode *)
{
    // Remove clip node geometry from mask
    auto clipState = m_clipStack.pop();
    vgDestroyPath(clipState);

    if (m_clipStack.count() == 0) {
        vgSeti(VG_MASKING, VG_FALSE);
    } else {
        // Recreate the mask
        vgMask(0,VG_FILL_MASK, 0, 0, VG_MAXINT, VG_MAXINT);
        vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
        vgLoadIdentity();
        for (auto path : qAsConst(m_clipStack)) {
            vgRenderToMask(path, VG_FILL_PATH, VG_INTERSECT_MASK);
        }
    }
}

bool QSGOpenVGNodeVisitor::visit(QSGGeometryNode *node)
{
    if (QSGSimpleRectNode *rectNode = dynamic_cast<QSGSimpleRectNode *>(node)) {
        // TODO: Try and render the QSGSimpleRectNode
        Q_UNUSED(rectNode)
        return false;
    } else if (QSGSimpleTextureNode *tn = dynamic_cast<QSGSimpleTextureNode *>(node)) {
        // TODO: Try and render the QSGSimpleTextureNode
        Q_UNUSED(tn)
        return false;
    } else if (QSGOpenVGNinePatchNode *nn = dynamic_cast<QSGOpenVGNinePatchNode *>(node)) {
        renderRenderableNode(nn);
    } else if (QSGOpenVGRectangleNode *rn = dynamic_cast<QSGOpenVGRectangleNode *>(node)) {
        renderRenderableNode(rn);
    } else if (QSGOpenVGImageNode *n = dynamic_cast<QSGOpenVGImageNode *>(node)) {
        renderRenderableNode(n);
    } else {
        // We dont know, so skip
        return false;
    }

    return true;
}

void QSGOpenVGNodeVisitor::endVisit(QSGGeometryNode *)
{
}

bool QSGOpenVGNodeVisitor::visit(QSGOpacityNode *node)
{
    m_opacityState.push(m_opacityState.top() * node->opacity());
    return true;
}

void QSGOpenVGNodeVisitor::endVisit(QSGOpacityNode *)
{
    m_opacityState.pop();
}

bool QSGOpenVGNodeVisitor::visit(QSGInternalImageNode *node)
{
    renderRenderableNode(static_cast<QSGOpenVGInternalImageNode*>(node));
    return true;
}

void QSGOpenVGNodeVisitor::endVisit(QSGInternalImageNode *)
{
}

bool QSGOpenVGNodeVisitor::visit(QSGPainterNode *node)
{
    renderRenderableNode(static_cast<QSGOpenVGPainterNode*>(node));
    return true;
}

void QSGOpenVGNodeVisitor::endVisit(QSGPainterNode *)
{
}

bool QSGOpenVGNodeVisitor::visit(QSGInternalRectangleNode *node)
{
    renderRenderableNode(static_cast<QSGOpenVGInternalRectangleNode*>(node));
    return true;
}

void QSGOpenVGNodeVisitor::endVisit(QSGInternalRectangleNode *)
{
}

bool QSGOpenVGNodeVisitor::visit(QSGGlyphNode *node)
{
    renderRenderableNode(static_cast<QSGOpenVGGlyphNode*>(node));
    return true;
}

void QSGOpenVGNodeVisitor::endVisit(QSGGlyphNode *)
{
}

bool QSGOpenVGNodeVisitor::visit(QSGRootNode *)
{
    return true;
}

void QSGOpenVGNodeVisitor::endVisit(QSGRootNode *)
{
}

#if QT_CONFIG(quick_sprite)
bool QSGOpenVGNodeVisitor::visit(QSGSpriteNode *node)
{
    renderRenderableNode(static_cast<QSGOpenVGSpriteNode*>(node));
    return true;
}

void QSGOpenVGNodeVisitor::endVisit(QSGSpriteNode *)
{
}
#endif

bool QSGOpenVGNodeVisitor::visit(QSGRenderNode *)
{
    return true;
}

void QSGOpenVGNodeVisitor::endVisit(QSGRenderNode *)
{
}

VGPath QSGOpenVGNodeVisitor::generateClipPath(const QRectF &rect) const
{
    VGPath clipPath = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1, 0, 0, 0,
                                   VG_PATH_CAPABILITY_APPEND_TO);

    // Create command list
    static const VGubyte rectCommands[] = {
        VG_MOVE_TO_ABS,
        VG_LINE_TO_ABS,
        VG_LINE_TO_ABS,
        VG_LINE_TO_ABS,
        VG_CLOSE_PATH
    };

    const QOpenVGMatrix &transform = m_transformStack.top();

    // Create command data
    QVector<VGfloat> coordinates(8);
    const QPointF topLeft = transform.map(rect.topLeft());
    const QPointF topRight = transform.map(rect.topRight());
    const QPointF bottomLeft = transform.map(rect.bottomLeft());
    const QPointF bottomRight = transform.map(rect.bottomRight());
    coordinates[0] = bottomLeft.x();
    coordinates[1] = bottomLeft.y();
    coordinates[2] = bottomRight.x();
    coordinates[3] = bottomRight.y();
    coordinates[4] = topRight.x();
    coordinates[5] = topRight.y();
    coordinates[6] = topLeft.x();
    coordinates[7] = topLeft.y();

    vgAppendPathData(clipPath, 5, rectCommands, coordinates.constData());
    return clipPath;
}

void QSGOpenVGNodeVisitor::renderRenderableNode(QSGOpenVGRenderable *node)
{
    if (!node)
        return;
    node->setTransform(m_transformStack.top());
    node->setOpacity(m_opacityState.top());
    node->render();
}

QT_END_NAMESPACE
