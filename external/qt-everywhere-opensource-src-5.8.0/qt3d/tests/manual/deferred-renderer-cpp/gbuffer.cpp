/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "gbuffer.h"

GBuffer::GBuffer(Qt3DCore::QNode *parent)
    : Qt3DRender::QRenderTarget(parent)
{
    const Qt3DRender::QAbstractTexture::TextureFormat formats[AttachmentsCount] = {
        Qt3DRender::QAbstractTexture::RGBA32F,
        Qt3DRender::QAbstractTexture::RGB32F,
        Qt3DRender::QAbstractTexture::RGB16F,
        Qt3DRender::QAbstractTexture::D32F
    };

    const Qt3DRender::QRenderTargetOutput::AttachmentPoint attachmentPoints[AttachmentsCount] = {
        Qt3DRender::QRenderTargetOutput::Color0,
        Qt3DRender::QRenderTargetOutput::Color1,
        Qt3DRender::QRenderTargetOutput::Color2,
        Qt3DRender::QRenderTargetOutput::Depth
    };

    for (int i = 0; i < AttachmentsCount; i++) {
        Qt3DRender::QRenderTargetOutput *output = new Qt3DRender::QRenderTargetOutput(this);

        m_textures[i] = new Qt3DRender::QTexture2D();
        m_textures[i]->setFormat(formats[i]);
        m_textures[i]->setWidth(1024);
        m_textures[i]->setHeight(1024);
        m_textures[i]->setGenerateMipMaps(false);
        m_textures[i]->setWrapMode(Qt3DRender::QTextureWrapMode(Qt3DRender::QTextureWrapMode::ClampToEdge));
        m_textures[i]->setMinificationFilter(Qt3DRender::QAbstractTexture::Linear);
        m_textures[i]->setMagnificationFilter(Qt3DRender::QAbstractTexture::Linear);

        output->setTexture(m_textures[i]);
        output->setAttachmentPoint(attachmentPoints[i]);
        addOutput(output);
    }
}

Qt3DRender::QAbstractTexture *GBuffer::colorTexture() const
{
    return m_textures[Color];
}

Qt3DRender::QAbstractTexture *GBuffer::positionTexture() const
{
    return m_textures[Position];
}

Qt3DRender::QAbstractTexture *GBuffer::normalTexture() const
{
    return m_textures[Normal];
}

Qt3DRender::QAbstractTexture *GBuffer::depthTexture() const
{
    return m_textures[Depth];
}
