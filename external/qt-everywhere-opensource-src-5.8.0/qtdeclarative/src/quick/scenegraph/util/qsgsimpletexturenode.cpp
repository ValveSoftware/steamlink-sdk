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


#include "qsgsimpletexturenode.h"
#include <private/qsgnode_p.h>

QT_BEGIN_NAMESPACE

class QSGSimpleTextureNodePrivate : public QSGGeometryNodePrivate
{
public:
    QSGSimpleTextureNodePrivate()
        : QSGGeometryNodePrivate()
        , texCoordMode(QSGSimpleTextureNode::NoTransform)
        , isAtlasTexture(false)
        , ownsTexture(false)
    {}

    QRectF sourceRect;
    QSGSimpleTextureNode::TextureCoordinatesTransformMode texCoordMode;
    uint isAtlasTexture : 1;
    uint ownsTexture : 1;
};

static void qsgsimpletexturenode_update(QSGGeometry *g,
                                        QSGTexture *texture,
                                        const QRectF &rect,
                                        QRectF sourceRect,
                                        QSGSimpleTextureNode::TextureCoordinatesTransformMode texCoordMode)
{
    if (!texture)
        return;

    if (!sourceRect.width() || !sourceRect.height()) {
        QSize ts = texture->textureSize();
        sourceRect = QRectF(0, 0, ts.width(), ts.height());
    }

    // Maybe transform the texture coordinates
    if (texCoordMode.testFlag(QSGSimpleTextureNode::MirrorHorizontally)) {
        float tmp = sourceRect.left();
        sourceRect.setLeft(sourceRect.right());
        sourceRect.setRight(tmp);
    }
    if (texCoordMode.testFlag(QSGSimpleTextureNode::MirrorVertically)) {
        float tmp = sourceRect.top();
        sourceRect.setTop(sourceRect.bottom());
        sourceRect.setBottom(tmp);
    }

    QSGGeometry::updateTexturedRectGeometry(g, rect, texture->convertToNormalizedSourceRect(sourceRect));
}

/*!
  \class QSGSimpleTextureNode
  \brief The QSGSimpleTextureNode class is provided for convenience to easily draw
  textured content using the QML scene graph.

  \inmodule QtQuick

  \warning The simple texture node class must have a texture before being
  added to the scene graph to be rendered.

  \warning This utility class is only functional when running with the OpenGL
  or software backends of the Qt Quick scenegraph. For a proper cross-platform
  alternative prefer using QSGImageNode via
  QQuickWindow::createImageNode() or QSGEngine::createImageNode().

  \deprecated
*/

/*!
    Constructs a new simple texture node
 */
QSGSimpleTextureNode::QSGSimpleTextureNode()
    : QSGGeometryNode(*new QSGSimpleTextureNodePrivate)
    , m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    setGeometry(&m_geometry);
    setMaterial(&m_material);
    setOpaqueMaterial(&m_opaque_material);
    m_material.setMipmapFiltering(QSGTexture::None);
    m_opaque_material.setMipmapFiltering(QSGTexture::None);
#ifdef QSG_RUNTIME_DESCRIPTION
    qsgnode_set_description(this, QLatin1String("simpletexture"));
#endif
}

/*!
    Destroys the texture node
 */
QSGSimpleTextureNode::~QSGSimpleTextureNode()
{
    Q_D(QSGSimpleTextureNode);
    if (d->ownsTexture)
        delete m_material.texture();
}

/*!
    Sets the filtering to be used for this texture node to \a filtering.

    For smooth scaling, use QSGTexture::Linear; for normal scaling, use
    QSGTexture::Nearest.
 */
void QSGSimpleTextureNode::setFiltering(QSGTexture::Filtering filtering)
{
    if (m_material.filtering() == filtering)
        return;

    m_material.setFiltering(filtering);
    m_opaque_material.setFiltering(filtering);
    markDirty(DirtyMaterial);
}


/*!
    Returns the filtering currently set on this texture node
 */
QSGTexture::Filtering QSGSimpleTextureNode::filtering() const
{
    return m_material.filtering();
}


/*!
    Sets the target rect of this texture node to \a r.
 */
void QSGSimpleTextureNode::setRect(const QRectF &r)
{
    if (m_rect == r)
        return;
    m_rect = r;
    Q_D(QSGSimpleTextureNode);
    qsgsimpletexturenode_update(&m_geometry, texture(), m_rect, d->sourceRect, d->texCoordMode);
    markDirty(DirtyGeometry);
}

/*!
    \fn void QSGSimpleTextureNode::setRect(qreal x, qreal y, qreal w, qreal h)
    \overload

    Sets the rectangle of this texture node to begin at (\a x, \a y) and have
    width \a w and height \a h.
 */

/*!
    Returns the target rect of this texture node.
 */
QRectF QSGSimpleTextureNode::rect() const
{
    return m_rect;
}

/*!
    Sets the source rect of this texture node to \a r.

    \since 5.5
 */
void QSGSimpleTextureNode::setSourceRect(const QRectF &r)
{
    Q_D(QSGSimpleTextureNode);
    if (d->sourceRect == r)
        return;
    d->sourceRect = r;
    qsgsimpletexturenode_update(&m_geometry, texture(), m_rect, d->sourceRect, d->texCoordMode);
    markDirty(DirtyGeometry);
}

/*!
    \fn void QSGSimpleTextureNode::setSourceRect(qreal x, qreal y, qreal w, qreal h)
    \overload
    \since 5.5

    Sets the rectangle of this texture node to show its texture from (\a x, \a y) and
    have width \a w and height \a h relatively to the QSGTexture::textureSize.
 */

/*!
    Returns the source rect of this texture node.

    \since 5.5
 */
QRectF QSGSimpleTextureNode::sourceRect() const
{
    Q_D(const QSGSimpleTextureNode);
    return d->sourceRect;
}

/*!
    Sets the texture of this texture node to \a texture.

    Use setOwnsTexture() to set whether the node should take
    ownership of the texture. By default, the node does not
    take ownership.

    \warning A texture node must have a texture before being added
    to the scenegraph to be rendered.
 */
void QSGSimpleTextureNode::setTexture(QSGTexture *texture)
{
    Q_ASSERT(texture);
    Q_D(QSGSimpleTextureNode);
    if (d->ownsTexture)
        delete m_material.texture();
    m_material.setTexture(texture);
    m_opaque_material.setTexture(texture);
    qsgsimpletexturenode_update(&m_geometry, texture, m_rect, d->sourceRect, d->texCoordMode);

    DirtyState dirty = DirtyMaterial;
    // It would be tempting to skip the extra bit here and instead use
    // m_material.texture to get the old state, but that texture could
    // have been deleted in the mean time.
    bool wasAtlas = d->isAtlasTexture;
    d->isAtlasTexture = texture->isAtlasTexture();
    if (wasAtlas || d->isAtlasTexture)
        dirty |= DirtyGeometry;
    markDirty(dirty);
}



/*!
    Returns the texture for this texture node
 */
QSGTexture *QSGSimpleTextureNode::texture() const
{
    return m_material.texture();
}

/*!
    \enum QSGSimpleTextureNode::TextureCoordinatesTransformFlag

    The TextureCoordinatesTransformFlag enum is used to specify the
    mode used to generate texture coordinates for a textured quad.

    \value NoTransform          Texture coordinates are oriented with window coordinates
                                i.e. with origin at top-left.

    \value MirrorHorizontally   Texture coordinates are inverted in the horizontal axis with
                                respect to window coordinates

    \value MirrorVertically     Texture coordinates are inverted in the vertical axis with
                                respect to window coordinates
 */

/*!
    Sets the method used to generate texture coordinates to \a mode. This can be used to obtain
    correct orientation of the texture. This is commonly needed when using a third party OpenGL
    library to render to texture as OpenGL has an inverted y-axis relative to Qt Quick.

    \sa textureCoordinatesTransform()
 */
void QSGSimpleTextureNode::setTextureCoordinatesTransform(QSGSimpleTextureNode::TextureCoordinatesTransformMode mode)
{
    Q_D(QSGSimpleTextureNode);
    if (d->texCoordMode == mode)
        return;
    d->texCoordMode = mode;
    qsgsimpletexturenode_update(&m_geometry, texture(), m_rect, d->sourceRect, d->texCoordMode);
    markDirty(DirtyMaterial);
}

/*!
    Returns the mode used to generate texture coordinates for this node.

    \sa setTextureCoordinatesTransform()
 */
QSGSimpleTextureNode::TextureCoordinatesTransformMode QSGSimpleTextureNode::textureCoordinatesTransform() const
{
    Q_D(const QSGSimpleTextureNode);
    return d->texCoordMode;
}

/*!
    Sets whether the node takes ownership of the texture to \a owns.

    By default, the node does not take ownership of the texture.

    \sa setTexture()

    \since 5.4
 */
void QSGSimpleTextureNode::setOwnsTexture(bool owns)
{
    Q_D(QSGSimpleTextureNode);
    d->ownsTexture = owns;
}

/*!
   Returns \c true if the node takes ownership of the texture; otherwise returns \c false.

   \since 5.4
 */
bool QSGSimpleTextureNode::ownsTexture() const
{
    Q_D(const QSGSimpleTextureNode);
    return d->ownsTexture;
}

QT_END_NAMESPACE
