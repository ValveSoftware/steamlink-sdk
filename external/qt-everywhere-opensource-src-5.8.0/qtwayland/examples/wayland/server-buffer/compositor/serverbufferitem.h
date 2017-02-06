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

#ifndef SERVERBUFFERITEM_H
#define SERVERBUFFERITEM_H

#include <QtQuick/QQuickItem>
#include <QtQuick/qsgtexture.h>

#include <QtQuick/qsgtextureprovider.h>

QT_BEGIN_NAMESPACE

class ServerBufferTextureProvider;

namespace QtWayland {
class ServerBuffer;
}

class ServerBufferItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QtWayland::ServerBuffer *serverBuffer READ serverBuffer CONSTANT)
    Q_PROPERTY(bool useTextureAlpha READ useTextureAlpha WRITE setUseTextureAlpha NOTIFY useTextureAlphaChanged)
    Q_PROPERTY(bool isYInverted READ isYInverted NOTIFY yInvertedChanged)
    Q_PROPERTY(int depth READ depth CONSTANT)

public:
    ServerBufferItem(QtWayland::ServerBuffer *serverBuffer, QQuickItem *parent = 0);
    ~ServerBufferItem();

    QtWayland::ServerBuffer *serverBuffer() const { return m_server_buffer; }

    bool isYInverted() const;
    int depth() const;

    bool isTextureProvider() const { return true; }
    QSGTextureProvider *textureProvider() const;

    bool useTextureAlpha() const  { return m_useTextureAlpha; }
    void setUseTextureAlpha(bool useTextureAlpha);

    void setDamagedFlag(bool on);

signals:
    void useTextureAlphaChanged();
    void yInvertedChanged();
    void serverBufferChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *);

private:
    void updateTexture();
    QtWayland::ServerBuffer *m_server_buffer;
    ServerBufferTextureProvider *m_provider;
    bool m_useTextureAlpha;
};

QT_END_NAMESPACE

#endif
