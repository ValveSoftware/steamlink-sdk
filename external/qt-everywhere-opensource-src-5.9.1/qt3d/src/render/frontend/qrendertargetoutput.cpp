/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include "qrendertargetoutput.h"
#include "qrendertargetoutput_p.h"
#include "qtexture.h"
#include <Qt3DCore/qpropertyupdatedchange.h>

QT_BEGIN_NAMESPACE

namespace Qt3DRender {

/*!
    \class Qt3DRender::QRenderTargetOutput
    \brief The QRenderTargetOutput class allows the specification of an attachment
    of a render target (whether it is a color texture, a depth texture, etc... ).
    \since 5.7
    \inmodule Qt3DRender

    A QRenderTargetOutput specifies the attachment point and parameters for texture
    that is attached to render target. In addition to the attachment point, texture
    miplevel, layer and cubemap face can be specified. The texture attached to the
    QRenderTargetOutput must be compatible with the given parameters.
 */

/*!
    \qmltype RenderTargetOutput
    \brief The RenderTargetOutput type allows the specification of an attachment
    of a render target (whether it is a color texture, a depth texture, etc... ).
    \since 5.7
    \inmodule Qt3D.Render
    \inherits Node
    \instantiates Qt3DRender::QRenderTargetOutput

    A RenderTargetOutput specifies the attachment point and parameters for texture
    that is attached to render target. In addition to the attachment point, texture
    miplevel, layer and cubemap face can be specified. The texture attached to the
    RenderTargetOutput must be compatible with the given parameters.
 */

/*!
    \enum QRenderTargetOutput::AttachmentPoint

    This enumeration specifies the values for the attachment point.

    \value Color0 Color attachment point at index 0
    \value Color1 Color attachment point at index 1
    \value Color2 Color attachment point at index 2
    \value Color3 Color attachment point at index 3
    \value Color4 Color attachment point at index 4
    \value Color5 Color attachment point at index 5
    \value Color6 Color attachment point at index 6
    \value Color7 Color attachment point at index 7
    \value Color8 Color attachment point at index 8
    \value Color9 Color attachment point at index 9
    \value Color10 Color attachment point at index 10
    \value Color11 Color attachment point at index 11
    \value Color12 Color attachment point at index 12
    \value Color13 Color attachment point at index 13
    \value Color14 Color attachment point at index 14
    \value Color15 Color attachment point at index 15
    \value Depth Depth attachment point
    \value Stencil Stencil attachment point
    \value DepthStencil DepthStencil attachment point
*/

/*!
    \qmlproperty enumeration RenderTargetOutput::attachmentPoint
    Holds the attachment point of the RenderTargetOutput.
    \list
    \li RenderTargetOutput.Color0
    \li RenderTargetOutput.Color1
    \li RenderTargetOutput.Color2
    \li RenderTargetOutput.Color3
    \li RenderTargetOutput.Color4
    \li RenderTargetOutput.Color5
    \li RenderTargetOutput.Color6
    \li RenderTargetOutput.Color7
    \li RenderTargetOutput.Color8
    \li RenderTargetOutput.Color9
    \li RenderTargetOutput.Color10
    \li RenderTargetOutput.Color11
    \li RenderTargetOutput.Color12
    \li RenderTargetOutput.Color13
    \li RenderTargetOutput.Color14
    \li RenderTargetOutput.Color15
    \li RenderTargetOutput.Depth
    \li RenderTargetOutput.Stencil
    \li RenderTargetOutput.DepthStencil
    \endlist

    \sa Qt3DRender::QRenderTargetOutput::AttachmentPoint
*/

/*!
    \qmlproperty Texture RenderTargetOutput::texture
    Holds the texture attached to the attachment point.
*/

/*!
    \qmlproperty int RenderTargetOutput::mipLevel
    Holds the miplevel of the attached texture the rendering is directed to.
*/

/*!
    \qmlproperty int RenderTargetOutput::layer
    Holds the layer of the attached texture the rendering is directed to.
*/

/*!
    \qmlproperty enumeration RenderTargetOutput::face
    Holds the face of the attached cubemap texture the rendering is directed to.
    \list
    \li Texture.CubeMapPositiveX
    \li Texture.CubeMapNegativeX
    \li Texture.CubeMapPositiveY
    \li Texture.CubeMapNegativeY
    \li Texture.CubeMapPositiveZ
    \li Texture.CubeMapNegativeZ
    \endlist
    \sa Qt3DRender::QAbstractTexture::CubeMapFace
*/

/*!
    \property QRenderTargetOutput::attachmentPoint
    Holds the attachment point of the QRenderTargetOutput.
*/

/*!
    \property QRenderTargetOutput::texture
    Holds the texture attached to the attachment point.
*/

/*!
    \property QRenderTargetOutput::mipLevel
    Holds the miplevel of the attached texture the rendering is directed to.
*/

/*!
    \property QRenderTargetOutput::layer
    Holds the layer of the attached texture the rendering is directed to.
*/

/*!
    \property QRenderTargetOutput::face
    Holds the face of the attached cubemap texture the rendering is directed to.
*/

/*! \internal */
QRenderTargetOutputPrivate::QRenderTargetOutputPrivate()
    : QNodePrivate()
    , m_texture(nullptr)
    , m_attachmentPoint(QRenderTargetOutput::Color0)
    , m_mipLevel(0)
    , m_layer(0)
    , m_face(QAbstractTexture::CubeMapNegativeX)
{
}

/*!
    The constructor creates a new QRenderTargetOutput::QRenderTargetOutput instance
    with the specified \a parent.
 */
QRenderTargetOutput::QRenderTargetOutput(QNode *parent)
    : QNode(*new QRenderTargetOutputPrivate, parent)
{
}

/*! \internal */
QRenderTargetOutput::~QRenderTargetOutput()
{
}

/*! \internal */
QRenderTargetOutput::QRenderTargetOutput(QRenderTargetOutputPrivate &dd, QNode *parent)
    : QNode(dd, parent)
{
}

void QRenderTargetOutput::setAttachmentPoint(QRenderTargetOutput::AttachmentPoint attachmentPoint)
{
    Q_D(QRenderTargetOutput);
    if (attachmentPoint != d->m_attachmentPoint) {
        d->m_attachmentPoint = attachmentPoint;
        emit attachmentPointChanged(attachmentPoint);
    }
}

QRenderTargetOutput::AttachmentPoint QRenderTargetOutput::attachmentPoint() const
{
    Q_D(const QRenderTargetOutput);
    return d->m_attachmentPoint;
}

void QRenderTargetOutput::setTexture(QAbstractTexture *texture)
{
    Q_D(QRenderTargetOutput);
    if (texture != d->m_texture) {

        if (d->m_texture)
            d->unregisterDestructionHelper(d->m_texture);

        // Handle inline declaration
        if (texture && !texture->parent())
            texture->setParent(this);

        d->m_texture = texture;

        // Ensures proper bookkeeping
        if (d->m_texture)
            d->registerDestructionHelper(d->m_texture, &QRenderTargetOutput::setTexture, d->m_texture);

        emit textureChanged(texture);
    }
}

QAbstractTexture *QRenderTargetOutput::texture() const
{
    Q_D(const QRenderTargetOutput);
    return d->m_texture;
}

void QRenderTargetOutput::setMipLevel(int level)
{
    Q_D(QRenderTargetOutput);
    if (d->m_mipLevel != level) {
        d->m_mipLevel = level;
        emit mipLevelChanged(level);
    }
}

int QRenderTargetOutput::mipLevel() const
{
    Q_D(const QRenderTargetOutput);
    return d->m_mipLevel;
}

void QRenderTargetOutput::setLayer(int layer)
{
    Q_D(QRenderTargetOutput);
    if (d->m_layer != layer) {
        d->m_layer = layer;
        emit layerChanged(layer);
    }
}

int QRenderTargetOutput::layer() const
{
    Q_D(const QRenderTargetOutput);
    return d->m_layer;
}

void QRenderTargetOutput::setFace(QAbstractTexture::CubeMapFace face)
{
    Q_D(QRenderTargetOutput);
    if (d->m_face != face) {
        d->m_face = face;
        emit faceChanged(face);
    }
}

QAbstractTexture::CubeMapFace QRenderTargetOutput::face() const
{
    Q_D(const QRenderTargetOutput);
    return d->m_face;
}

Qt3DCore::QNodeCreatedChangeBasePtr QRenderTargetOutput::createNodeCreationChange() const
{
    auto creationChange = Qt3DCore::QNodeCreatedChangePtr<QRenderTargetOutputData>::create(this);
    auto &data = creationChange->data;
    Q_D(const QRenderTargetOutput);
    data.textureId = qIdForNode(texture());
    data.attachmentPoint = d->m_attachmentPoint;
    data.mipLevel = d->m_mipLevel;
    data.layer = d->m_layer;
    data.face = d->m_face;
    return creationChange;
}

} // namespace Qt3DRender

QT_END_NAMESPACE
