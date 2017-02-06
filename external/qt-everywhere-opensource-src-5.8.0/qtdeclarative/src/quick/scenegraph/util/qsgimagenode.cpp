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

#include "qsgimagenode.h"

QT_BEGIN_NAMESPACE

/*!
  \class QSGImageNode
  \brief The QSGImageNode class is provided for convenience to easily draw
  textured content using the QML scene graph.

  \inmodule QtQuick
  \since 5.8

  \warning The image node class must have a texture before being
  added to the scene graph to be rendered.
 */

/*!
    \fn void QSGImageNode::setRect(const QRectF &rect)

    Sets the target rect of this image node to \a rect.
 */

/*!
    \fn void QSGImageNode::setRect(qreal x, qreal y, qreal w, qreal h)
    \overload

    Sets the rectangle of this image node to begin at (\a x, \a y) and have
    width \a w and height \a h.
 */

/*!
    \fn QRectF QSGImageNode::rect() const

    Returns the target rect of this image node.
 */

/*!
    \fn void QSGImageNode::setSourceRect(const QRectF &rect)

    Sets the source rect of this image node to \a rect.
 */

/*!
    \fn void QSGImageNode::setSourceRect(qreal x, qreal y, qreal w, qreal h)
    \overload

    Sets the rectangle of this image node to show its texture from (\a x, \a y) and
    have width \a w and height \a h relatively to the QSGTexture::textureSize.
 */

/*!
    \fn QRectF QSGImageNode::sourceRect() const

    Returns the source rect of this image node.
 */

/*!
    \fn void QSGImageNode::setTexture(QSGTexture *texture)

    Sets the texture of this image node to \a texture.

    Use setOwnsTexture() to set whether the node should take
    ownership of the texture. By default, the node does not
    take ownership.

    \warning An image node must have a texture before being added to the
    scenegraph to be rendered.
 */

/*!
    \fn QSGTexture *QSGImageNode::texture() const

    Returns the texture for this image node.
 */

/*!
    \fn void QSGImageNode::setFiltering(QSGTexture::Filtering filtering)

    Sets the filtering to be used for this image node to \a filtering.

    For smooth scaling, use QSGTexture::Linear. For normal scaling, use
    QSGTexture::Nearest.
 */

/*!
    \fn QSGTexture::Filtering QSGImageNode::filtering() const

    Returns the filtering for this image node.
 */

/*!
    \fn void QSGImageNode::setMipmapFiltering(QSGTexture::Filtering filtering)

    Sets the mipmap filtering to be used for this image node to \a filtering.

    For smooth scaling between mip maps, use QSGTexture::Linear.
    For normal scaling, use QSGTexture::Nearest.
 */

/*!
    \fn QSGTexture::Filtering QSGImageNode::mipmapFiltering() const

    Returns the mipmap filtering for this image node.
 */

/*!
    \enum QSGImageNode::TextureCoordinatesTransformFlag

    The TextureCoordinatesTransformFlag enum is used to specify the mode used
    to generate texture coordinates for a textured quad.

    \value NoTransform          Texture coordinates are oriented with window coordinates
                                i.e. with origin at top-left.

    \value MirrorHorizontally   Texture coordinates are inverted in the horizontal axis with
                                respect to window coordinates

    \value MirrorVertically     Texture coordinates are inverted in the vertical axis with
                                respect to window coordinates
 */

/*!
    \fn void QSGImageNode::setTextureCoordinatesTransform(TextureCoordinatesTransformMode mode)

    Sets the method used to generate texture coordinates to \a mode. This can
    be used to obtain correct orientation of the texture. This is commonly
    needed when using a third-party OpenGL library to render to texture as
    OpenGL has an inverted y-axis relative to Qt Quick.
 */

/*!
    \fn QSGImageNode::TextureCoordinatesTransformMode textureCoordinatesTransform() const

    Returns the mode used to generate texture coordinates for this node.
 */

/*!
    \fn void QSGImageNode::setOwnsTexture(bool owns)

    Sets whether the node takes ownership of the texture to \a owns.

    By default, the node does not take ownership of the texture.
 */

/*!
    \fn bool QSGImageNode::ownsTexture() const

    \return \c true if the node takes ownership of the texture; otherwise \c false.
 */

void QSGImageNode::rebuildGeometry(QSGGeometry *g,
                                   QSGTexture *texture,
                                   const QRectF &rect,
                                   QRectF sourceRect,
                                   TextureCoordinatesTransformMode texCoordMode)
{
    if (!texture)
        return;

    if (!sourceRect.width() || !sourceRect.height()) {
        QSize ts = texture->textureSize();
        sourceRect = QRectF(0, 0, ts.width(), ts.height());
    }

    // Maybe transform the texture coordinates
    if (texCoordMode.testFlag(QSGImageNode::MirrorHorizontally)) {
        float tmp = sourceRect.left();
        sourceRect.setLeft(sourceRect.right());
        sourceRect.setRight(tmp);
    }
    if (texCoordMode.testFlag(QSGImageNode::MirrorVertically)) {
        float tmp = sourceRect.top();
        sourceRect.setTop(sourceRect.bottom());
        sourceRect.setBottom(tmp);
    }

    QSGGeometry::updateTexturedRectGeometry(g, rect, texture->convertToNormalizedSourceRect(sourceRect));
}

QT_END_NAMESPACE
