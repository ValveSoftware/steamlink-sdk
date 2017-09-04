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

#include "qsgopenvgspritenode.h"
#include "qsgopenvgtexture.h"

QT_BEGIN_NAMESPACE

QSGOpenVGSpriteNode::QSGOpenVGSpriteNode()
    : m_time(0.0f)
{
    // Set Dummy material and geometry to avoid asserts
    setMaterial((QSGMaterial*)1);
    setGeometry((QSGGeometry*)1);
}

QSGOpenVGSpriteNode::~QSGOpenVGSpriteNode()
{

}

void QSGOpenVGSpriteNode::setTexture(QSGTexture *texture)
{
    m_texture = static_cast<QSGOpenVGTexture*>(texture);
    markDirty(DirtyMaterial);
}

void QSGOpenVGSpriteNode::setTime(float time)
{
    if (m_time != time) {
        m_time = time;
        markDirty(DirtyMaterial);
    }
}

void QSGOpenVGSpriteNode::setSourceA(const QPoint &source)
{
    if (m_sourceA != source) {
        m_sourceA = source;
        markDirty(DirtyMaterial);
    }
}

void QSGOpenVGSpriteNode::setSourceB(const QPoint &source)
{
    if (m_sourceB != source) {
        m_sourceB = source;
        markDirty(DirtyMaterial);
    }
}

void QSGOpenVGSpriteNode::setSpriteSize(const QSize &size)
{
    if (m_spriteSize != size) {
        m_spriteSize = size;
        markDirty(DirtyMaterial);
    }
}

void QSGOpenVGSpriteNode::setSheetSize(const QSize &size)
{
    if (m_sheetSize != size) {
        m_sheetSize = size;
        markDirty(DirtyMaterial);
    }
}

void QSGOpenVGSpriteNode::setSize(const QSizeF &size)
{
    if (m_size != size) {
        m_size = size;
        markDirty(DirtyGeometry);
    }
}

void QSGOpenVGSpriteNode::setFiltering(QSGTexture::Filtering)
{
}

void QSGOpenVGSpriteNode::update()
{
}

void QSGOpenVGSpriteNode::render()
{
    if (!m_texture)
        return;

    VGImage image = static_cast<VGImage>(m_texture->textureId());

    QRectF sourceRect(m_sourceA, m_spriteSize);
    QRectF targetRect(0, 0, m_size.width(), m_size.height());

    VGImage sourceImage = vgChildImage(image, sourceRect.x(), sourceRect.y(), sourceRect.width(), sourceRect.height());

    // Set Draw Mode
    if (opacity() < 1.0) {
        //Transparent
        vgSetPaint(opacityPaint(), VG_FILL_PATH);
        vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_MULTIPLY);
    } else {
        vgSeti(VG_IMAGE_MODE, VG_DRAW_IMAGE_NORMAL);
    }

    // Set Image Matrix
    vgSeti(VG_MATRIX_MODE, VG_MATRIX_IMAGE_USER_TO_SURFACE);
    vgLoadMatrix(transform().constData());

    if (sourceRect != targetRect) {
        // Scale
        float scaleX = targetRect.width() / sourceRect.width();
        float scaleY = targetRect.height() / sourceRect.height();
        vgScale(scaleX, scaleY);
    }

    vgDrawImage(sourceImage);

    vgDestroyImage(sourceImage);
}

QT_END_NAMESPACE
