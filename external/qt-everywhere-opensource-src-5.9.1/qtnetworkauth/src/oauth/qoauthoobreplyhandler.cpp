/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Network Auth module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QT_NO_HTTP

#include "qoauthoobreplyhandler.h"

#include <QtCore/qurlquery.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsondocument.h>

#include <QtNetwork/qnetworkreply.h>

QT_BEGIN_NAMESPACE

QOAuthOobReplyHandler::QOAuthOobReplyHandler(QObject *parent)
    : QAbstractOAuthReplyHandler(parent)
{}

QString QOAuthOobReplyHandler::callback() const
{
    return QStringLiteral("oob");
}

void QOAuthOobReplyHandler::networkReplyFinished(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        qWarning("QOAuthOobReplyHandler::networkReplyFinished: %s",
                 qPrintable(reply->errorString()));
        return;
    }
    if (reply->header(QNetworkRequest::ContentTypeHeader).isNull()) {
        qWarning("QOAuthOobReplyHandler::networkReplyFinished: Empty Content-type header");
        return;
    }
    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).isNull() ?
                QStringLiteral("text/html") :
                reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        qWarning("QOAuthOobReplyHandler::networkReplyFinished: No received data");
        return;
    }

    Q_EMIT replyDataReceived(data);

    QVariantMap ret;

    if (contentType.startsWith(QStringLiteral("text/html")) ||
            contentType.startsWith(QStringLiteral("application/x-www-form-urlencoded"))) {
        ret = parseResponse(data);
    } else if (contentType.startsWith(QStringLiteral("application/json"))) {
        const QJsonDocument document = QJsonDocument::fromJson(data);
        if (!document.isObject()) {
            qWarning("QOAuthOobReplyHandler::networkReplyFinished: Received data is not a JSON"
                     "object: %s", qPrintable(QString::fromUtf8(data)));
            return;
        }
        const QJsonObject object = document.object();
        if (object.isEmpty()) {
            qWarning("QOAuthOobReplyHandler::networkReplyFinished: Received empty JSON object: %s",
                     qPrintable(QString::fromUtf8(data)));
        }
        ret = object.toVariantMap();
    } else {
        qWarning("QOAuthOobReplyHandler::networkReplyFinished: Unknown Content-type: %s",
                 qPrintable(contentType));
        return;
    }

    Q_EMIT tokensReceived(ret);
}

QVariantMap QOAuthOobReplyHandler::parseResponse(const QByteArray &response)
{
    QVariantMap ret;
    QUrlQuery query(QString::fromUtf8(response));
    auto queryItems = query.queryItems(QUrl::FullyDecoded);
    for (auto it = queryItems.begin(), end = queryItems.end(); it != end; ++it)
        ret.insert(it->first, it->second);
    return ret;
}

QT_END_NAMESPACE

#endif // QT_NO_HTTP
