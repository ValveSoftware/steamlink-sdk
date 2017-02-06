/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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

#include "serverbufferitem.h"

#include "serverbuffertextureprovider.h"

#include <QtQuick/QSGSimpleTextureNode>
#include <QtQuick/QQuickWindow>

#include <QtWaylandCompositor/private/qwlserverbufferintegration_p.h>

QT_BEGIN_NAMESPACE

ServerBufferItem::ServerBufferItem(QtWayland::ServerBuffer *serverBuffer, QQuickItem *parent)
    : QQuickItem(parent)
    , m_server_buffer(serverBuffer)
    , m_provider(new ServerBufferTextureProvider)
    , m_useTextureAlpha(false)
{
    setFlag(QQuickItem::ItemHasContents);
    setWidth(serverBuffer->size().width());
    setHeight(serverBuffer->size().height());
    update();
}

ServerBufferItem::~ServerBufferItem()
{
}

bool ServerBufferItem::isYInverted() const
{
    return m_server_buffer->isYInverted();
}

int ServerBufferItem::depth() const
{
    return m_server_buffer->format() == QtWayland::ServerBuffer::RGBA32 ? 32 : 8;
}

QSGTextureProvider *ServerBufferItem::textureProvider() const
{
    return m_provider;
}

void ServerBufferItem::updateTexture()
{
    if (m_provider->texture())
        return;

    QQuickWindow::CreateTextureOptions opt = QQuickWindow::TextureHasAlphaChannel;
    GLuint texture;
    glGenTextures(1,&texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    m_server_buffer->bindTextureToBuffer();
    m_provider->setTexture(window()->createTextureFromId(texture, m_server_buffer->size(), opt));
}

QSGNode *ServerBufferItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    updateTexture();
    QSGSimpleTextureNode *node = static_cast<QSGSimpleTextureNode *>(oldNode);

    if (!node) {
        node = new QSGSimpleTextureNode();
    }

    node->setTexture(m_provider->texture());

    if (isYInverted()) {
        node->setRect(0, height(), width(), -height());
    } else {
        node->setRect(0, 0, width(), height());
    }

    return node;
}

void ServerBufferItem::setUseTextureAlpha(bool useTextureAlpha)
{
    m_useTextureAlpha = useTextureAlpha;

    if ((flags() & ItemHasContents) != 0) {
        update();
    }
}

QT_END_NAMESPACE

