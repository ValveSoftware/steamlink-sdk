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

#ifndef ETCPROVIDER_H
#define ETCPROVIDER_H

#include <qopengl.h>
#include <QQuickImageProvider>
#include <QtQuick/QSGTexture>
#include <QUrl>

class EtcProvider : public QQuickImageProvider
{
public:
    EtcProvider()
        : QQuickImageProvider(QQuickImageProvider::Texture)
    {}

    QQuickTextureFactory *requestTexture(const QString &id, QSize *size, const QSize &requestedSize);

    void setBaseUrl(const QUrl &base);

private:
    QUrl m_baseUrl;
};

class EtcTexture : public QSGTexture
{
    Q_OBJECT
public:
    EtcTexture();
    ~EtcTexture();

    void bind();

    QSize textureSize() const { return m_size; }
    int textureId() const;

    bool hasAlphaChannel() const { return false; }
    bool hasMipmaps() const { return false; }

    QByteArray m_data;
    QSize m_size;
    QSize m_paddedSize;
    GLuint m_texture_id;
    bool m_uploaded;
};

#endif // ETCPROVIDER_H
