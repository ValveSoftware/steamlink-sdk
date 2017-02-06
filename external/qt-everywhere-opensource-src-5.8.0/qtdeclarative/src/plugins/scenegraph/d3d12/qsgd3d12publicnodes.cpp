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

#include "qsgd3d12publicnodes_p.h"

QT_BEGIN_NAMESPACE

QSGD3D12RectangleNode::QSGD3D12RectangleNode()
    : m_geometry(QSGGeometry::defaultAttributes_Point2D(), 4)
{
    QSGGeometry::updateRectGeometry(&m_geometry, QRectF());
    setMaterial(&m_material);
    setGeometry(&m_geometry);
#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("rectangle"));
#endif
}

void QSGD3D12RectangleNode::setRect(const QRectF &rect)
{
    QSGGeometry::updateRectGeometry(&m_geometry, rect);
    markDirty(QSGNode::DirtyGeometry);
}

QRectF QSGD3D12RectangleNode::rect() const
{
    const QSGGeometry::Point2D *pts = m_geometry.vertexDataAsPoint2D();
    return QRectF(pts[0].x,
                  pts[0].y,
                  pts[3].x - pts[0].x,
                  pts[3].y - pts[0].y);
}

void QSGD3D12RectangleNode::setColor(const QColor &color)
{
    if (color != m_material.color()) {
        m_material.setColor(color);
        markDirty(QSGNode::DirtyMaterial);
    }
}

QColor QSGD3D12RectangleNode::color() const
{
    return m_material.color();
}

QSGD3D12ImageNode::QSGD3D12ImageNode()
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4),
      m_texCoordMode(QSGD3D12ImageNode::NoTransform),
      m_isAtlasTexture(false),
      m_ownsTexture(false)
{
    setGeometry(&m_geometry);
    setMaterial(&m_material);
    m_material.setMipmapFiltering(QSGTexture::None);
#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("image"));
#endif
}

QSGD3D12ImageNode::~QSGD3D12ImageNode()
{
    if (m_ownsTexture)
        delete m_material.texture();
}

void QSGD3D12ImageNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.filtering() == filtering)
        return;

    m_material.setFiltering(filtering);
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering QSGD3D12ImageNode::filtering() const
{
    return m_material.filtering();
}

void QSGD3D12ImageNode::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.mipmapFiltering() == filtering)
        return;

    m_material.setMipmapFiltering(filtering);
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering QSGD3D12ImageNode::mipmapFiltering() const
{
    return m_material.mipmapFiltering();
}

void QSGD3D12ImageNode::setRect(const QRectF &r)
{
    if (m_rect == r)
        return;

    m_rect = r;
    QSGImageNode::rebuildGeometry(&m_geometry, texture(), m_rect, m_sourceRect, m_texCoordMode);
    markDirty(DirtyGeometry);
}

QRectF QSGD3D12ImageNode::rect() const
{
    return m_rect;
}

void QSGD3D12ImageNode::setSourceRect(const QRectF &r)
{
    if (m_sourceRect == r)
        return;

    m_sourceRect = r;
    QSGImageNode::rebuildGeometry(&m_geometry, texture(), m_rect, m_sourceRect, m_texCoordMode);
    markDirty(DirtyGeometry);
}

QRectF QSGD3D12ImageNode::sourceRect() const
{
    return m_sourceRect;
}

void QSGD3D12ImageNode::setTexture(QSGTexture *texture)
{
    Q_ASSERT(texture);

    if (m_ownsTexture)
        delete m_material.texture();

    m_material.setTexture(texture);
    QSGImageNode::rebuildGeometry(&m_geometry, texture, m_rect, m_sourceRect, m_texCoordMode);

    DirtyState dirty = DirtyMaterial;
    const bool wasAtlas = m_isAtlasTexture;
    m_isAtlasTexture = texture->isAtlasTexture();
    if (wasAtlas || m_isAtlasTexture)
        dirty |= DirtyGeometry;

    markDirty(dirty);
}

QSGTexture *QSGD3D12ImageNode::texture() const
{
    return m_material.texture();
}

void QSGD3D12ImageNode::setTextureCoordinatesTransform(TextureCoordinatesTransformMode mode)
{
    if (m_texCoordMode == mode)
        return;

    m_texCoordMode = mode;
    QSGImageNode::rebuildGeometry(&m_geometry, texture(), m_rect, m_sourceRect, m_texCoordMode);
    markDirty(DirtyMaterial);
}

QSGD3D12ImageNode::TextureCoordinatesTransformMode QSGD3D12ImageNode::textureCoordinatesTransform() const
{
    return m_texCoordMode;
}

void QSGD3D12ImageNode::setOwnsTexture(bool owns)
{
    m_ownsTexture = owns;
}

bool QSGD3D12ImageNode::ownsTexture() const
{
    return m_ownsTexture;
}

QSGD3D12NinePatchNode::QSGD3D12NinePatchNode()
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    m_geometry.setDrawingMode(QSGGeometry::DrawTriangleStrip);
    setGeometry(&m_geometry);
    setMaterial(&m_material);
}

QSGD3D12NinePatchNode::~QSGD3D12NinePatchNode()
{
    delete m_material.texture();
}

void QSGD3D12NinePatchNode::setTexture(QSGTexture *texture)
{
    delete m_material.texture();
    m_material.setTexture(texture);
}

void QSGD3D12NinePatchNode::setBounds(const QRectF &bounds)
{
    m_bounds = bounds;
}

void QSGD3D12NinePatchNode::setDevicePixelRatio(qreal ratio)
{
    m_devicePixelRatio = ratio;
}

void QSGD3D12NinePatchNode::setPadding(qreal left, qreal top, qreal right, qreal bottom)
{
    m_padding = QVector4D(left, top, right, bottom);
}

void QSGD3D12NinePatchNode::update()
{
    QSGNinePatchNode::rebuildGeometry(m_material.texture(), &m_geometry, m_padding, m_bounds, m_devicePixelRatio);
    markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
}

QT_END_NAMESPACE
