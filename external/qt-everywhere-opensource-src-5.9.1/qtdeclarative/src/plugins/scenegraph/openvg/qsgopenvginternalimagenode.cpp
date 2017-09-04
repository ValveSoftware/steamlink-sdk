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

#include "qsgopenvginternalimagenode.h"
#include "qsgopenvghelpers.h"

#include <VG/openvg.h>

QT_BEGIN_NAMESPACE

QSGOpenVGInternalImageNode::QSGOpenVGInternalImageNode()
{
    // Set Dummy material and geometry to avoid asserts
    setMaterial((QSGMaterial*)1);
    setGeometry((QSGGeometry*)1);
}

QSGOpenVGInternalImageNode::~QSGOpenVGInternalImageNode()
{
    if (m_subSourceRectImage != 0)
        vgDestroyImage(m_subSourceRectImage);
}

void QSGOpenVGInternalImageNode::render()
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
    QSize textureSize = m_texture->textureSize();

    if (image == VG_INVALID_HANDLE || !textureSize.isValid())
        return;


    // If Mirrored
    if (m_mirror) {
        vgTranslate(m_targetRect.width(), 0.0f);
        vgScale(-1.0, 1.0);
    }

    if (m_smooth)
        vgSeti(VG_IMAGE_QUALITY, VG_IMAGE_QUALITY_BETTER);
    else
        vgSeti(VG_IMAGE_QUALITY, VG_IMAGE_QUALITY_NONANTIALIASED);


    if (m_innerTargetRect != m_targetRect) {
        // border image
        QSGOpenVGHelpers::qDrawBorderImage(image, textureSize, m_targetRect, m_innerTargetRect, m_subSourceRect);
    } else if (m_tileHorizontal || m_tileVertical) {
        // Tilled Image

        float sx = m_targetRect.width() / (m_subSourceRect.width() * textureSize.width());
        float sy = m_targetRect.height() / (m_subSourceRect.height() * textureSize.height());
        QPointF offset(m_subSourceRect.left() * textureSize.width(), m_subSourceRect.top() * textureSize.height());

        QSGOpenVGHelpers::qDrawTiled(image, textureSize, m_targetRect, offset, sx, sy);

    } else {
        // Regular BLIT

        QRectF sr(m_subSourceRect.left() * textureSize.width(), m_subSourceRect.top() * textureSize.height(),
                  m_subSourceRect.width() * textureSize.width(), m_subSourceRect.height() * textureSize.height());

        if (m_subSourceRectImageDirty) {
            if (m_subSourceRectImage != 0)
                vgDestroyImage(m_subSourceRectImage);
            m_subSourceRectImage = vgChildImage(image, sr.x(), sr.y(), sr.width(), sr.height());
            m_subSourceRectImageDirty = false;
        }

        // If the the source rect is the same as the target rect
        if (sr == m_targetRect) {
            vgDrawImage(image);
        } else {
            // Scale
            float scaleX = m_targetRect.width() / sr.width();
            float scaleY = m_targetRect.height() / sr.height();
            vgTranslate(m_targetRect.x(), m_targetRect.y());
            vgScale(scaleX, scaleY);
            vgDrawImage(m_subSourceRectImage);
        }
    }
}

void QSGOpenVGInternalImageNode::setTargetRect(const QRectF &rect)
{
    if (rect == m_targetRect)
        return;
    m_targetRect = rect;
    markDirty(DirtyGeometry);
}

void QSGOpenVGInternalImageNode::setInnerTargetRect(const QRectF &rect)
{
    if (rect == m_innerTargetRect)
        return;
    m_innerTargetRect = rect;
    markDirty(DirtyGeometry);
}

void QSGOpenVGInternalImageNode::setInnerSourceRect(const QRectF &rect)
{
    if (rect == m_innerSourceRect)
        return;
    m_innerSourceRect = rect;
    markDirty(DirtyGeometry);
}

void QSGOpenVGInternalImageNode::setSubSourceRect(const QRectF &rect)
{
    if (rect == m_subSourceRect)
        return;
    m_subSourceRect = rect;
    m_subSourceRectImageDirty = true;
    markDirty(DirtyGeometry);
}

void QSGOpenVGInternalImageNode::setTexture(QSGTexture *texture)
{
    m_texture = texture;
    m_subSourceRectImageDirty = true;
    markDirty(DirtyMaterial);
}

void QSGOpenVGInternalImageNode::setMirror(bool mirror)
{
    if (m_mirror != mirror) {
        m_mirror = mirror;
        markDirty(DirtyMaterial);
    }
}

void QSGOpenVGInternalImageNode::setMipmapFiltering(QSGTexture::Filtering)
{
}

void QSGOpenVGInternalImageNode::setFiltering(QSGTexture::Filtering filtering)
{
    bool smooth = (filtering == QSGTexture::Linear);
    if (smooth == m_smooth)
        return;

    m_smooth = smooth;
    markDirty(DirtyMaterial);
}

void QSGOpenVGInternalImageNode::setHorizontalWrapMode(QSGTexture::WrapMode wrapMode)
{
    bool tileHorizontal = (wrapMode == QSGTexture::Repeat);
    if (tileHorizontal == m_tileHorizontal)
        return;

    m_tileHorizontal = tileHorizontal;
    markDirty(DirtyMaterial);
}

void QSGOpenVGInternalImageNode::setVerticalWrapMode(QSGTexture::WrapMode wrapMode)
{
    bool tileVertical = (wrapMode == QSGTexture::Repeat);
    if (tileVertical == m_tileVertical)
        return;

    m_tileVertical = (wrapMode == QSGTexture::Repeat);
    markDirty(DirtyMaterial);
}

void QSGOpenVGInternalImageNode::update()
{
}

void QSGOpenVGInternalImageNode::preprocess()
{
    bool doDirty = false;
    QSGLayer *t = qobject_cast<QSGLayer *>(m_texture);
    if (t) {
        doDirty = t->updateTexture();
        markDirty(DirtyGeometry);
    }
    if (doDirty)
        markDirty(DirtyMaterial);
}

QT_END_NAMESPACE




