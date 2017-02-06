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

#include "qsgd3d12texture_p.h"
#include "qsgd3d12engine_p.h"
#include <private/qsgcontext_p.h>

QT_BEGIN_NAMESPACE

#define RETAIN_IMAGE

void QSGD3D12Texture::create(const QImage &image, uint flags)
{
    // ### atlas?

    const bool alphaRequest = flags & QSGRenderContext::CreateTexture_Alpha;
    m_alphaWanted = alphaRequest && image.hasAlphaChannel();

    // The engine maps 8-bit formats to R8. This is fine for glyphs and such
    // but may not be what apps expect for ordinary image data. The OpenGL
    // implementation maps these to ARGB32_Pre so let's follow suit.
    if (image.depth() != 8)
        m_image = image;
    else
        m_image = image.convertToFormat(m_alphaWanted ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32);

    m_id = m_engine->genTexture();
    Q_ASSERT(m_id);

    // We could kick off the texture creation and the async upload right here.
    // Unfortunately we cannot tell at this stage if mipmaps will be enabled
    // via an Image element's mipmap property...so defer to bind().
    m_createPending = true;
}

QSGD3D12Texture::~QSGD3D12Texture()
{
    if (m_id)
        m_engine->releaseTexture(m_id);
}

int QSGD3D12Texture::textureId() const
{
    return m_id;
}

QSize QSGD3D12Texture::textureSize() const
{
    return m_image.size();
}

bool QSGD3D12Texture::hasAlphaChannel() const
{
    return m_alphaWanted;
}

bool QSGD3D12Texture::hasMipmaps() const
{
    return mipmapFiltering() != QSGTexture::None;
}

QRectF QSGD3D12Texture::normalizedTextureSubRect() const
{
    return QRectF(0, 0, 1, 1);
}

void QSGD3D12Texture::bind()
{
    // Called when the texture material updates the pipeline state.

    if (!m_createPending && hasMipmaps() != m_createdWithMipMaps) {
#ifdef RETAIN_IMAGE
        m_engine->releaseTexture(m_id);
        m_id = m_engine->genTexture();
        Q_ASSERT(m_id);
        m_createPending = true;
#else
        // ### this can be made working some day (something similar to
        // queueTextureResize) but skip for now
        qWarning("D3D12: mipmap property cannot be changed once the texture is created");
#endif
    }

    if (m_createPending) {
        m_createPending = false;

        QSGD3D12Engine::TextureCreateFlags createFlags = 0;
        if (m_alphaWanted)
            createFlags |= QSGD3D12Engine::TextureWithAlpha;

        m_createdWithMipMaps = hasMipmaps();
        if (m_createdWithMipMaps)
            createFlags |= QSGD3D12Engine::TextureWithMipMaps;

        m_engine->createTexture(m_id, m_image.size(), m_image.format(), createFlags);
        m_engine->queueTextureUpload(m_id, m_image);

#ifndef RETAIN_IMAGE
        m_image = QImage();
#endif
    }

    // Here we know that the texture is going to be used in the current frame
    // by the next draw call. Notify the engine so that it can wait for
    // possible pending uploads and set up the pipeline accordingly.
    m_engine->useTexture(m_id);
}

QT_END_NAMESPACE
