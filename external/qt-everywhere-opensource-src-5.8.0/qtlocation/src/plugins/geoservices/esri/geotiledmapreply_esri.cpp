/****************************************************************************
**
** Copyright (C) 2013-2016 Esri <contracts@esri.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "geotiledmapreply_esri.h"

#include <QtLocation/private/qgeotilespec_p.h>

QT_BEGIN_NAMESPACE

static const unsigned char pngSignature[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00};
static const unsigned char jpegSignature[] = {0xFF, 0xD8, 0xFF, 0x00};
static const unsigned char gifSignature[] = {0x47, 0x49, 0x46, 0x38, 0x00};

GeoTiledMapReplyEsri::GeoTiledMapReplyEsri(QNetworkReply *reply, const QGeoTileSpec &spec,
                                           QObject *parent) :
    QGeoTiledMapReply(spec, parent), m_reply(reply)
{
    connect(m_reply, SIGNAL(finished()), this, SLOT(networkReplyFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(networkReplyError(QNetworkReply::NetworkError)));
    connect(m_reply, SIGNAL(destroyed()), this, SLOT(replyDestroyed()));
}

GeoTiledMapReplyEsri::~GeoTiledMapReplyEsri()
{
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = Q_NULLPTR;
    }
}

void GeoTiledMapReplyEsri::abort()
{
    if (!m_reply)
        return;

    m_reply->abort();
    QGeoTiledMapReply::abort();
}

void GeoTiledMapReplyEsri::replyDestroyed()
{
    m_reply = Q_NULLPTR;
}

void GeoTiledMapReplyEsri::networkReplyFinished()
{
    if (!m_reply)
        return;

    if (m_reply->error() != QNetworkReply::NoError)
    {
        setError(QGeoTiledMapReply::CommunicationError, m_reply->errorString());
        m_reply->deleteLater();
        m_reply = Q_NULLPTR;
        return;
    }

    QByteArray const& imageData = m_reply->readAll();

    bool validFormat = true;
    if (imageData.startsWith(reinterpret_cast<const char*>(pngSignature)))
        setMapImageFormat(QStringLiteral("png"));
    else if (imageData.startsWith(reinterpret_cast<const char*>(jpegSignature)))
        setMapImageFormat(QStringLiteral("jpg"));
    else if (imageData.startsWith(reinterpret_cast<const char*>(gifSignature)))
        setMapImageFormat(QStringLiteral("gif"));
    else
        validFormat = false;

    if (validFormat)
        setMapImageData(imageData);

    setFinished(true);

    m_reply->deleteLater();
    m_reply = Q_NULLPTR;
}

void GeoTiledMapReplyEsri::networkReplyError(QNetworkReply::NetworkError error)
{
    if (!m_reply)
        return;

    if (error != QNetworkReply::OperationCanceledError)
        setError(QGeoTiledMapReply::CommunicationError, m_reply->errorString());

    setFinished(true);
    m_reply->deleteLater();
    m_reply = Q_NULLPTR;
}

QT_END_NAMESPACE
