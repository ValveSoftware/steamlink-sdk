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

#include "twitter.h"

#include <QtGui>
#include <QtCore>
#include <QtNetwork>

Twitter::Twitter(QObject *parent) :
    Twitter(QString(), qMakePair(QString(), QString()), parent)
{}

Twitter::Twitter(const QPair<QString, QString> &clientCredentials, QObject *parent) :
    Twitter(QString(), clientCredentials, parent)
{}

Twitter::Twitter(const QString &screenName,
                 const QPair<QString, QString> &clientCredentials,
                 QObject *parent) :
    QOAuth1(clientCredentials.first, clientCredentials.second, nullptr, parent)
{
    replyHandler = new QOAuthHttpServerReplyHandler(this);
    setReplyHandler(replyHandler);
    setTemporaryCredentialsUrl(QUrl("https://api.twitter.com/oauth/request_token"));
    setAuthorizationUrl(QUrl("https://api.twitter.com/oauth/authenticate"));
    setTokenCredentialsUrl(QUrl("https://api.twitter.com/oauth/access_token"));

    connect(this, &QAbstractOAuth::authorizeWithBrowser, [=](QUrl url) {
        QUrlQuery query(url);

        // Forces the user to enter their credentials to authorize the correct
        // user account
        query.addQueryItem("force_login", "true");

        if (!screenName.isEmpty())
            query.addQueryItem("screen_name", screenName);
        url.setQuery(query);
        QDesktopServices::openUrl(url);
    });

    connect(this, &QOAuth1::granted, this, &Twitter::authenticated);

    if (!clientCredentials.first.isEmpty() && !clientCredentials.second.isEmpty())
        grant();
}
