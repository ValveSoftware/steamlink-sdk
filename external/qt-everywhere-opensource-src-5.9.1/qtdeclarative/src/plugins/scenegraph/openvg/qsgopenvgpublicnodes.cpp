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

#include "qsgopenvgpublicnodes.h"
#include "qsgopenvghelpers.h"
#include <VG/openvg.h>
#include <QtGui/QPixmap>

QT_BEGIN_NAMESPACE

QSGOpenVGRectangleNode::QSGOpenVGRectangleNode()
{
    // Set Dummy material and geometry to avoid asserts
    setMaterial((QSGMaterial*)1);
    setGeometry((QSGGeometry*)1);

    m_rectPath = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1, 0, 0, 0,
                              VG_PATH_CAPABILITY_APPEND_TO);
    m_rectPaint = vgCreatePaint();
}

QSGOpenVGRectangleNode::~QSGOpenVGRectangleNode()
{
    vgDestroyPaint(m_rectPaint);
    vgDestroyPath(m_rectPath);
}

void QSGOpenVGRectangleNode::setRect(const QRectF &rect)
{
    m_rect = rect;
    m_pathDirty = true;
    markDirty(DirtyMaterial);
}

QRectF QSGOpenVGRectangleNode::rect() const
{
    return m_rect;
}

void QSGOpenVGRectangleNode::setColor(const QColor &color)
{
    m_color = color;
    m_paintDirty = true;
    markDirty(DirtyMaterial);
}

QColor QSGOpenVGRectangleNode::color() const
{
    return m_color;
}

void QSGOpenVGRectangleNode::setTransform(const QOpenVGMatrix &transform)
{
    // if there transform matrix is not affine, regenerate the path
    if (transform.isAffine())
        m_pathDirty = true;
    markDirty(DirtyGeometry);

    QSGOpenVGRenderable::setTransform(transform);
}

void QSGOpenVGRectangleNode::render()
{
    // Set Transform
    if (transform().isAffine()) {
        // Use current transform matrix
        vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
        vgLoadMatrix(transform().constData());
    } else {
        // map the path's to handle the perspective matrix
        vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
        vgLoadIdentity();
    }

    if (m_pathDirty) {
        vgClearPath(m_rectPath, VG_PATH_CAPABILITY_APPEND_TO);

        if (transform().isAffine()) {
        // Create command list
        static const VGubyte rectCommands[] = {
            VG_MOVE_TO_ABS,
            VG_HLINE_TO_REL,
            VG_VLINE_TO_REL,
            VG_HLINE_TO_REL,
            VG_CLOSE_PATH
        };

        // Create command data
        QVector<VGfloat> coordinates(5);
        coordinates[0] = m_rect.x();
        coordinates[1] = m_rect.y();
        coordinates[2] = m_rect.width();
        coordinates[3] = m_rect.height();
        coordinates[4] = -m_rect.width();

        vgAppendPathData(m_rectPath, 5, rectCommands, coordinates.constData());

        } else {
            // Pre-transform path
            static const VGubyte rectCommands[] = {
                VG_MOVE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_LINE_TO_ABS,
                VG_CLOSE_PATH
            };

            QVector<VGfloat> coordinates(8);
            const QPointF topLeft = transform().map(m_rect.topLeft());
            const QPointF topRight = transform().map(m_rect.topRight());
            const QPointF bottomLeft = transform().map(m_rect.bottomLeft());
            const QPointF bottomRight = transform().map(m_rect.bottomRight());
            coordinates[0] = bottomLeft.x();
            coordinates[1] = bottomLeft.y();
            coordinates[2] = bottomRight.x();
            coordinates[3] = bottomRight.y();
            coordinates[4] = topRight.x();
            coordinates[5] = topRight.y();
            coordinates[6] = topLeft.x();
            coordinates[7] = topLeft.y();

            vgAppendPathData(m_rectPath, 5, rectCommands, coordinates.constData());
        }

        m_pathDirty = false;
    }

    if (m_paintDirty) {
        vgSetPaint(m_rectPaint, VG_FILL_PATH);
        vgSetParameteri(m_rectPaint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
        vgSetParameterfv(m_rectPaint, VG_PAINT_COLOR, 4, QSGOpenVGHelpers::qColorToVGColor(m_color).constData());

        m_paintDirty = false;
    }

    vgSetPaint(m_rectPaint, VG_FILL_PATH);
    vgDrawPath(m_rectPath, VG_FILL_PATH);

}

QSGOpenVGImageNode::QSGOpenVGImageNode()
{
    // Set Dummy material and geometry to avoid asserts
    setMaterial((QSGMaterial*)1);
    setGeometry((QSGGeometry*)1);

}

QSGOpenVGImageNode::~QSGOpenVGImageNode()
{
    if (m_owns) {
        m_texture->deleteLater();
    }
}

void QSGOpenVGImageNode::setRect(const QRectF &rect)
{
    m_rect = rect;
    markDirty(DirtyMaterial);
}

QRectF QSGOpenVGImageNode::rect() const
{
    return m_rect;
}

void QSGOpenVGImageNode::setSourceRect(const QRectF &r)
{
    m_sourceRect = r;
}

QRectF QSGOpenVGImageNode::sourceRect() const
{
    return m_sourceRect;
}

void QSGOpenVGImageNode::setTexture(QSGTexture *texture)
{
    m_texture = texture;
    markDirty(DirtyMaterial);
}

QSGTexture *QSGOpenVGImageNode::texture() const
{
    return m_texture;
}

void QSGOpenVGImageNode::setFiltering(QSGTexture::Filtering filtering)
{
    m_filtering = filtering;
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering QSGOpenVGImageNode::filtering() const
{
    return m_filtering;
}

void QSGOpenVGImageNode::setMipmapFiltering(QSGTexture::Filtering)
{
}

QSGTexture::Filtering QSGOpenVGImageNode::mipmapFiltering() const
{
    return QSGTexture::None;
}

void QSGOpenVGImageNode::setTextureCoordinatesTransform(QSGImageNode::TextureCoordinatesTransformMode transformNode)
{
    if (m_transformMode == transformNode)
        return;
    m_transformMode = transformNode;
    markDirty(DirtyGeometry);
}

QSGImageNode::TextureCoordinatesTransformMode QSGOpenVGImageNode::textureCoordinatesTransform() const
{
    return m_transformMode;
}

void QSGOpenVGImageNode::setOwnsTexture(bool owns)
{
    m_owns = owns;
}

bool QSGOpenVGImageNode::ownsTexture() const
{
    return m_owns;
}

void QSGOpenVGImageNode::render()
{
    if (!m_texture) {
        return;
    }

    // Set Draw Mode
    if (opacity() < 1.0) {
        //Transparent
        vgSetPaint(opacityPaint(), VG_FILL_PATH);
        vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_MULTIPLY);
    } else {
        vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_NORMAL);
    }

    // Set Transform
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
    vgLoadMatrix(transform().constData());

    VGImage image = static_cast<VGImage>(m_texture->textureId());

    //Apply the TextureCoordinateTransform Flag
    if (m_transformMode != QSGImageNode::NoTransform) {
        float translateX = 0.0f;
        float translateY = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;

        if (m_transformMode & QSGImageNode::MirrorHorizontally) {
            translateX = m_rect.width();
            scaleX = -1.0;
        }

        if (m_transformMode & QSGImageNode::MirrorVertically) {
            translateY = m_rect.height();
            scaleY = -1.0;
        }

        vgTranslate(translateX, translateY);
        vgScale(scaleX, scaleY);
    }

    // If the the source rect is the same as the target rect
    if (m_sourceRect == m_rect) {
        vgDrawImage(image);
    } else {
        // Scale
        float scaleX = m_rect.width() / m_sourceRect.width();
        float scaleY = m_rect.height() / m_sourceRect.height();
        vgScale(scaleX, scaleY);
        VGImage subImage = vgChildImage(image, m_sourceRect.x(), m_sourceRect.y(), m_sourceRect.width(), m_sourceRect.height());
        vgDrawImage(subImage);
        vgDestroyImage(subImage);
    }

}

QSGOpenVGNinePatchNode::QSGOpenVGNinePatchNode()
{
    // Set Dummy material and geometry to avoid asserts
    setMaterial((QSGMaterial*)1);
    setGeometry((QSGGeometry*)1);

}

void QSGOpenVGNinePatchNode::setTexture(QSGTexture *texture)
{
    m_texture = texture;
    markDirty(DirtyMaterial);
}

void QSGOpenVGNinePatchNode::setBounds(const QRectF &bounds)
{
    if (m_bounds == bounds)
        return;
    m_bounds = bounds;
    markDirty(DirtyGeometry);
}

void QSGOpenVGNinePatchNode::setDevicePixelRatio(qreal ratio)
{
    if (m_pixelRatio == ratio)
        return;
    m_pixelRatio = ratio;
    markDirty(DirtyGeometry);
}

void QSGOpenVGNinePatchNode::setPadding(qreal left, qreal top, qreal right, qreal bottom)
{
    QMarginsF margins(left, top, right, bottom);
    if (m_margins == margins)
        return;
    m_margins = margins;
    markDirty(DirtyGeometry);
}

void QSGOpenVGNinePatchNode::update()
{

}

void QSGOpenVGNinePatchNode::render()
{
    if (!m_texture)
        return;

    // Set Draw Mode
    if (opacity() < 1.0) {
        //Transparent
        vgSetPaint(opacityPaint(), VG_FILL_PATH);
        vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_MULTIPLY);
    } else {
        vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_NORMAL);
    }

    // Set Transform
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
    vgLoadMatrix(transform().constData());

    VGImage image = static_cast<VGImage>(m_texture->textureId());

    //Draw borderImage
    QSGOpenVGHelpers::qDrawBorderImage(image, m_texture->textureSize(), m_bounds, m_bounds.marginsRemoved(m_margins), QRectF(0, 0, 1, 1));
}

QRectF QSGOpenVGNinePatchNode::bounds() const
{
    return m_bounds;
}


QT_END_NAMESPACE
