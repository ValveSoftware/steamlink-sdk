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

#include "qsgd3d12painternode_p.h"
#include "qsgd3d12rendercontext_p.h"
#include "qsgd3d12engine_p.h"
#include <private/qquickitem_p.h>
#include <qmath.h>

QT_BEGIN_NAMESPACE

QSGD3D12PainterTexture::QSGD3D12PainterTexture(QSGD3D12Engine *engine)
    : QSGD3D12Texture(engine)
{
}

void QSGD3D12PainterTexture::bind()
{
    if (m_image.isNull()) {
        if (!m_id) {
            m_id = m_engine->genTexture();
            m_engine->createTexture(m_id, QSize(16, 16), QImage::Format_RGB32, 0);
        }
    } else if (m_image.size() != lastSize) {
        lastSize = m_image.size();
        if (m_id)
            m_engine->releaseTexture(m_id);
        m_id = m_engine->genTexture();
        m_engine->createTexture(m_id, m_image.size(), m_image.format(), QSGD3D12Engine::TextureWithAlpha);
        m_engine->queueTextureUpload(m_id, m_image);
    } else if (!dirty.isEmpty()) {
        const int bpl = m_image.bytesPerLine();
        const uchar *p = m_image.constBits() + dirty.y() * bpl + dirty.x() * 4;
        QImage subImg(p, dirty.width(), dirty.height(), bpl, QImage::Format_ARGB32_Premultiplied);
        m_engine->queueTextureUpload(m_id, subImg, dirty.topLeft());
    }

    dirty = QRect();

    m_engine->useTexture(m_id);
}

QSGD3D12PainterNode::QSGD3D12PainterNode(QQuickPaintedItem *item)
    : m_item(item),
      m_engine(static_cast<QSGD3D12RenderContext *>(QQuickItemPrivate::get(item)->sceneGraphRenderContext())->engine()),
      m_texture(new QSGD3D12PainterTexture(m_engine)),
      m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4),
      m_dirtyGeometry(false),
      m_dirtyContents(false)
{
    setGeometry(&m_geometry);
    m_material.setTexture(m_texture);
    setMaterial(&m_material);
}

QSGD3D12PainterNode::~QSGD3D12PainterNode()
{
    delete m_texture;
}

void QSGD3D12PainterNode::setPreferredRenderTarget(QQuickPaintedItem::RenderTarget)
{
    // always QImage-based
}

void QSGD3D12PainterNode::setSize(const QSize &size)
{
    if (m_size == size)
        return;

    m_size = size;
    m_dirtyGeometry = true;
}

void QSGD3D12PainterNode::setDirty(const QRect &dirtyRect)
{
    m_dirtyRect = dirtyRect;
    m_dirtyContents = true;
    markDirty(DirtyMaterial);
}

void QSGD3D12PainterNode::setOpaquePainting(bool)
{
    // ignored
}

void QSGD3D12PainterNode::setLinearFiltering(bool linearFiltering)
{
    m_material.setFiltering(linearFiltering ? QSGTexture::Linear : QSGTexture::Nearest);
    markDirty(DirtyMaterial);
}

void QSGD3D12PainterNode::setMipmapping(bool)
{
    // ### not yet
}

void QSGD3D12PainterNode::setSmoothPainting(bool s)
{
    if (m_smoothPainting == s)
        return;

    m_smoothPainting = s;
    m_dirtyContents = true;
    markDirty(DirtyMaterial);
}

void QSGD3D12PainterNode::setFillColor(const QColor &c)
{
    if (m_fillColor == c)
        return;

    m_fillColor = c;
    m_dirtyContents = true;
    markDirty(DirtyMaterial);
}

void QSGD3D12PainterNode::setContentsScale(qreal s)
{
    if (m_contentsScale == s)
        return;

    m_contentsScale = s;
    m_dirtyContents = true;
    markDirty(DirtyMaterial);
}

void QSGD3D12PainterNode::setFastFBOResizing(bool)
{
    // nope
}

void QSGD3D12PainterNode::setTextureSize(const QSize &size)
{
    if (m_textureSize == size)
        return;

    m_textureSize = size;
    m_dirtyGeometry = true;
}

QImage QSGD3D12PainterNode::toImage() const
{
    return *m_texture->image();
}

void QSGD3D12PainterNode::update()
{
    if (m_dirtyGeometry) {
        m_dirtyGeometry = false;
        QRectF src(0, 0, 1, 1);
        QRectF dst(QPointF(0, 0), m_size);
        QSGGeometry::updateTexturedRectGeometry(&m_geometry, dst, src);
        markDirty(DirtyGeometry);
    }

    QImage *img = m_texture->image();
    if (img->size() != m_textureSize) {
        *img = QImage(m_textureSize, QImage::Format_ARGB32_Premultiplied);
        img->fill(Qt::transparent);
        m_dirtyContents = true;
    }

    if (m_dirtyContents) {
        m_dirtyContents = false;
        if (!img->isNull()) {
            QRect dirtyRect = m_dirtyRect.isNull() ? QRect(QPoint(0, 0), m_size) : m_dirtyRect;
            QPainter painter;
            painter.begin(img);
            if (m_smoothPainting)
                painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);

            QRect clipRect;
            QRect dirtyTextureRect;

            if (m_contentsScale == 1) {
                float scaleX = m_textureSize.width() / (float) m_size.width();
                float scaleY = m_textureSize.height() / (float) m_size.height();
                painter.scale(scaleX, scaleY);
                clipRect = dirtyRect;
                dirtyTextureRect = QRectF(dirtyRect.x() * scaleX,
                                          dirtyRect.y() * scaleY,
                                          dirtyRect.width() * scaleX,
                                          dirtyRect.height() * scaleY).toAlignedRect();
            } else {
                painter.scale(m_contentsScale, m_contentsScale);
                QRect sclip(qFloor(dirtyRect.x() / m_contentsScale),
                            qFloor(dirtyRect.y() / m_contentsScale),
                            qCeil(dirtyRect.width() / m_contentsScale + dirtyRect.x() / m_contentsScale
                                  - qFloor(dirtyRect.x() / m_contentsScale)),
                            qCeil(dirtyRect.height() / m_contentsScale + dirtyRect.y() / m_contentsScale
                                  - qFloor(dirtyRect.y() / m_contentsScale)));
                clipRect = sclip;
                dirtyTextureRect = dirtyRect;
            }

            // only clip if we were originally updating only a subrect
            if (!m_dirtyRect.isNull())
                painter.setClipRect(clipRect);

            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(clipRect, m_fillColor);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

            m_item->paint(&painter);
            painter.end();

            m_texture->dirty = dirtyTextureRect;
        }
        m_dirtyRect = QRect();
    }
}

QSGTexture *QSGD3D12PainterNode::texture() const
{
    return m_texture;
}

QT_END_NAMESPACE
