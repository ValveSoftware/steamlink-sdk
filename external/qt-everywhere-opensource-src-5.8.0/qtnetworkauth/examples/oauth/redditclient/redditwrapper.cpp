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

#include "redditwrapper.h"

#include <QtGui>
#include <QtCore>
#include <QtNetworkAuth>

const QUrl newUrl("https://oauth.reddit.com/new");
const QUrl hotUrl("https://oauth.reddit.com/hot");
const QUrl liveThreadsUrl("https://oauth.reddit.com/live/XXXX/about.json");

RedditWrapper::RedditWrapper(QObject *parent) : QObject(parent)
{
    auto replyHandler = new QOAuthHttpServerReplyHandler(1337, this);
    oauth2.setReplyHandler(replyHandler);
    oauth2.setAuthorizationUrl(QUrl("https://www.reddit.com/api/v1/authorize"));
    oauth2.setAccessTokenUrl(QUrl("https://www.reddit.com/api/v1/access_token"));
    oauth2.setScope("identity read");

    connect(&oauth2, &QOAuth2AuthorizationCodeFlow::statusChanged, [=](
            QAbstractOAuth::Status status) {
        if (status == QAbstractOAuth::Status::Granted)
            emit authenticated();
    });
    oauth2.setModifyParametersFunction([&](QAbstractOAuth::Stage stage, QVariantMap *parameters) {
        if (stage == QAbstractOAuth::Stage::RequestingAuthorization && isPermanent())
            parameters->insert("duration", "permanent");
    });
    connect(&oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser,
            &QDesktopServices::openUrl);
}

RedditWrapper::RedditWrapper(const QString &clientIdentifier, QObject *parent) :
    RedditWrapper(parent)
{
    oauth2.setClientIdentifier(clientIdentifier);
}

QNetworkReply *RedditWrapper::requestHotThreads()
{
    qDebug() << "Getting hot threads...";
    return oauth2.get(hotUrl);
}

bool RedditWrapper::isPermanent() const
{
    return permanent;
}

void RedditWrapper::setPermanent(bool value)
{
    permanent = value;
}

void RedditWrapper::grant()
{
    oauth2.grant();
}

void RedditWrapper::subscribeToLiveUpdates()
{
    qDebug() << "Susbscribing...";
    QNetworkReply *reply = oauth2.get(liveThreadsUrl);
    connect(reply, &QNetworkReply::finished, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            qCritical() << "Reddit error:" << reply->errorString();
            return;
        }

        const auto json = reply->readAll();

        const auto document = QJsonDocument::fromJson(json);
        Q_ASSERT(document.isObject());
        const auto rootObject = document.object();
        const auto dataValue = rootObject.value("data");
        Q_ASSERT(dataValue.isObject());
        const auto dataObject = dataValue.toObject();
        const auto websocketUrlValue = dataObject.value("websocket_url");
        Q_ASSERT(websocketUrlValue.isString() && websocketUrlValue.toString().size());
        const QUrl websocketUrl(websocketUrlValue.toString());
        emit subscribed(websocketUrl);
    });
}
