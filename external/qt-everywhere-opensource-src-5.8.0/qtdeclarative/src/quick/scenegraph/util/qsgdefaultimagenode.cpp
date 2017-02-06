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

#include "qsgdefaultimagenode_p.h"
#include <private/qsgnode_p.h>

QT_BEGIN_NAMESPACE

QSGDefaultImageNode::QSGDefaultImageNode()
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    , m_texCoordMode(QSGDefaultImageNode::NoTransform)
    , m_isAtlasTexture(false)
    , m_ownsTexture(false)
{
    setGeometry(&m_geometry);
    setMaterial(&m_material);
    setOpaqueMaterial(&m_opaque_material);
    m_material.setMipmapFiltering(QSGTexture::None);
    m_opaque_material.setMipmapFiltering(QSGTexture::None);
#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("image"));
#endif
}

QSGDefaultImageNode::~QSGDefaultImageNode()
{
    if (m_ownsTexture)
        delete m_material.texture();
}

void QSGDefaultImageNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.filtering() == filtering)
        return;

    m_material.setFiltering(filtering);
    m_opaque_material.setFiltering(filtering);
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering QSGDefaultImageNode::filtering() const
{
    return m_material.filtering();
}

void QSGDefaultImageNode::setMipmapFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.mipmapFiltering() == filtering)
        return;

    m_material.setMipmapFiltering(filtering);
    m_opaque_material.setMipmapFiltering(filtering);
    markDirty(DirtyMaterial);
}

QSGTexture::Filtering QSGDefaultImageNode::mipmapFiltering() const
{
    return m_material.mipmapFiltering();
}

void QSGDefaultImageNode::setRect(const QRectF &r)
{
    if (m_rect == r)
        return;

    m_rect = r;
    rebuildGeometry(&m_geometry, texture(), m_rect, m_sourceRect, m_texCoordMode);
    markDirty(DirtyGeometry);
}

QRectF QSGDefaultImageNode::rect() const
{
    return m_rect;
}

void QSGDefaultImageNode::setSourceRect(const QRectF &r)
{
    if (m_sourceRect == r)
        return;

    m_sourceRect = r;
    rebuildGeometry(&m_geometry, texture(), m_rect, m_sourceRect, m_texCoordMode);
    markDirty(DirtyGeometry);
}

QRectF QSGDefaultImageNode::sourceRect() const
{
    return m_sourceRect;
}

void QSGDefaultImageNode::setTexture(QSGTexture *texture)
{
    Q_ASSERT(texture);
    if (m_ownsTexture)
        delete m_material.texture();
    m_material.setTexture(texture);
    m_opaque_material.setTexture(texture);
    rebuildGeometry(&m_geometry, texture, m_rect, m_sourceRect, m_texCoordMode);

    DirtyState dirty = DirtyMaterial;
    // It would be tempting to skip the extra bit here and instead use
    // m_material.texture to get the old state, but that texture could
    // have been deleted in the mean time.
    bool wasAtlas = m_isAtlasTexture;
    m_isAtlasTexture = texture->isAtlasTexture();
    if (wasAtlas || m_isAtlasTexture)
        dirty |= DirtyGeometry;
    markDirty(dirty);
}

QSGTexture *QSGDefaultImageNode::texture() const
{
    return m_material.texture();
}

void QSGDefaultImageNode::setTextureCoordinatesTransform(TextureCoordinatesTransformMode mode)
{
    if (m_texCoordMode == mode)
        return;
    m_texCoordMode = mode;
    rebuildGeometry(&m_geometry, texture(), m_rect, m_sourceRect, m_texCoordMode);
    markDirty(DirtyMaterial);
}

QSGDefaultImageNode::TextureCoordinatesTransformMode QSGDefaultImageNode::textureCoordinatesTransform() const
{
    return m_texCoordMode;
}

void QSGDefaultImageNode::setOwnsTexture(bool owns)
{
    m_ownsTexture = owns;
}

bool QSGDefaultImageNode::ownsTexture() const
{
    return m_ownsTexture;
}

QT_END_NAMESPACE
