/****************************************************************************
**
** Copyright (C) 2013 Aaron McCarthy <mccarthy.aaron@gmail.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "qgeomapreplyosm.h"

#include <QtLocation/private/qgeotilespec_p.h>

QGeoMapReplyOsm::QGeoMapReplyOsm(QNetworkReply *reply, const QGeoTileSpec &spec, QObject *parent)
:   QGeoTiledMapReply(spec, parent), m_reply(reply)
{
    connect(m_reply, SIGNAL(finished()), this, SLOT(networkReplyFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(networkReplyError(QNetworkReply::NetworkError)));
    connect(m_reply, SIGNAL(destroyed()), this, SLOT(replyDestroyed()));
}

QGeoMapReplyOsm::~QGeoMapReplyOsm()
{
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = 0;
    }
}

void QGeoMapReplyOsm::abort()
{
    if (!m_reply)
        return;

    m_reply->abort();
}

QNetworkReply *QGeoMapReplyOsm::networkReply() const
{
    return m_reply;
}

void QGeoMapReplyOsm::replyDestroyed()
{
    m_reply = 0;
}

void QGeoMapReplyOsm::networkReplyFinished()
{
    if (!m_reply)
        return;

    if (m_reply->error() != QNetworkReply::NoError)
        return;

    QByteArray a = m_reply->readAll();

    setMapImageData(a);
    switch (tileSpec().mapId()) {
    case 1:
        setMapImageFormat("png");
        break;
    case 2:
        setMapImageFormat("png");
        break;
    default:
        qWarning("Unknown map id %d", tileSpec().mapId());
    }

    setFinished(true);

    m_reply->deleteLater();
    m_reply = 0;
}

void QGeoMapReplyOsm::networkReplyError(QNetworkReply::NetworkError error)
{
    if (!m_reply)
        return;

    if (error != QNetworkReply::OperationCanceledError)
        setError(QGeoTiledMapReply::CommunicationError, m_reply->errorString());

    setFinished(true);
    m_reply->deleteLater();
    m_reply = 0;
}
