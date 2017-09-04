/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

#include "etcprovider.h"

#include <QFile>
#include <QDebug>
#include <qopenglfunctions.h>
#include <qqmlfile.h>

//#define ETC_DEBUG

#ifndef GL_ETC1_RGB8_OES
    #define GL_ETC1_RGB8_OES 0x8d64
#endif

typedef struct {
    char aName[6];
    unsigned short iBlank;
    /* NB: Beware endianness issues here. */
    unsigned char iPaddedWidthMSB;
    unsigned char iPaddedWidthLSB;
    unsigned char iPaddedHeightMSB;
    unsigned char iPaddedHeightLSB;
    unsigned char iWidthMSB;
    unsigned char iWidthLSB;
    unsigned char iHeightMSB;
    unsigned char iHeightLSB;
} ETCHeader;

unsigned short getWidth(ETCHeader *pHeader)
{
    return (pHeader->iWidthMSB << 8) | pHeader->iWidthLSB;
}

unsigned short getHeight(ETCHeader *pHeader)
{
    return (pHeader->iHeightMSB << 8) | pHeader->iHeightLSB;
}

unsigned short getPaddedWidth(ETCHeader *pHeader)
{
    return (pHeader->iPaddedWidthMSB << 8) | pHeader->iPaddedWidthLSB;
}

unsigned short getPaddedHeight(ETCHeader *pHeader)
{
    return (pHeader->iPaddedHeightMSB << 8) | pHeader->iPaddedHeightLSB;
}

EtcTexture::EtcTexture()
    : m_texture_id(0), m_uploaded(false)
{
    initializeOpenGLFunctions();
}

EtcTexture::~EtcTexture()
{
    if (m_texture_id)
        glDeleteTextures(1, &m_texture_id);
}

int EtcTexture::textureId() const
{
    if (m_texture_id == 0) {
        EtcTexture *texture = const_cast<EtcTexture*>(this);
        texture->glGenTextures(1, &texture->m_texture_id);
    }
    return m_texture_id;
}

void EtcTexture::bind()
{
    if (m_uploaded && m_texture_id) {
        glBindTexture(GL_TEXTURE_2D, m_texture_id);
        return;
    }

    if (m_texture_id == 0)
        glGenTextures(1, &m_texture_id);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);

#ifdef ETC_DEBUG
    qDebug() << "glCompressedTexImage2D, width: " << m_size.width() << "height" << m_size.height() <<
                "paddedWidth: " << m_paddedSize.width() << "paddedHeight: " << m_paddedSize.height();
#endif

#ifndef QT_NO_DEBUG
    while (glGetError() != GL_NO_ERROR) { }
#endif

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    Q_ASSERT(ctx != 0);
    ctx->functions()->glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_ETC1_RGB8_OES,
                                             m_size.width(), m_size.height(), 0,
                                             (m_paddedSize.width() * m_paddedSize.height()) >> 1,
                                             m_data.data() + 16);

#ifndef QT_NO_DEBUG
    // Gracefully fail in case of an error...
    GLuint error = glGetError();
    if (error != GL_NO_ERROR) {
        qDebug () << "glCompressedTexImage2D for compressed texture failed, error: " << error;
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
        return;
    }
#endif

    m_uploaded = true;
    updateBindOptions(true);
}

class QEtcTextureFactory : public QQuickTextureFactory
{
public:
    QByteArray m_data;
    QSize m_size;
    QSize m_paddedSize;

    QSize textureSize() const { return m_size; }
    int textureByteCount() const { return m_data.size(); }

    QSGTexture *createTexture(QQuickWindow *) const {
        EtcTexture *texture = new EtcTexture;
        texture->m_data = m_data;
        texture->m_size = m_size;
        texture->m_paddedSize = m_paddedSize;
        return texture;
    }
};

QQuickTextureFactory *EtcProvider::requestTexture(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(requestedSize);
    QEtcTextureFactory *ret = 0;

    size->setHeight(0);
    size->setWidth(0);

    QUrl url = QUrl(id);
    if (url.isRelative() && !m_baseUrl.isEmpty())
        url = m_baseUrl.resolved(url);
    QString path = QQmlFile::urlToLocalFileOrQrc(url);

    QFile file(path);
#ifdef ETC_DEBUG
    qDebug() << "requestTexture opening file: " << path;
#endif
    if (file.open(QIODevice::ReadOnly)) {
        ret = new QEtcTextureFactory;
        ret->m_data = file.readAll();
        if (!ret->m_data.isEmpty()) {
            ETCHeader *pETCHeader = NULL;
            pETCHeader = (ETCHeader *)ret->m_data.data();
            size->setHeight(getHeight(pETCHeader));
            size->setWidth(getWidth(pETCHeader));
            ret->m_size = *size;
            ret->m_paddedSize.setHeight(getPaddedHeight(pETCHeader));
            ret->m_paddedSize.setWidth(getPaddedWidth(pETCHeader));
        }
        else {
            delete ret;
            ret = 0;
        }
    }

#ifdef ETC_DEBUG
    if (ret)
        qDebug() << "requestTexture returning: " << ret->m_data.length() << ", bytes; width: " << size->width() << ", height: " << size->height();
    else
        qDebug () << "File not found.";
#endif

    return ret;
}

void EtcProvider::setBaseUrl(const QUrl &base)
{
    m_baseUrl = base;
}
