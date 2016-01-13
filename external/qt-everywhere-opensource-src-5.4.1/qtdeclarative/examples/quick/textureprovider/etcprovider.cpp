/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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

}

EtcTexture::~EtcTexture()
{
    if (m_texture_id)
        glDeleteTextures(1, &m_texture_id);
}

int EtcTexture::textureId() const
{
    if (m_texture_id == 0)
        glGenTextures(1, &const_cast<EtcTexture *>(this)->m_texture_id);
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
