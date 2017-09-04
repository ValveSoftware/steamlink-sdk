/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Network Auth module of the Qt Toolkit.
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

#include "twittertimelinemodel.h"

#include <QtGui>
#include <QtCore>
#include <QtWidgets>

TwitterTimelineModel::TwitterTimelineModel(QObject *parent) : QAbstractTableModel(parent)
{
    connect(&twitter, &Twitter::authenticated, this, &TwitterTimelineModel::authenticated);
    connect(&twitter, &Twitter::authenticated, this, &TwitterTimelineModel::updateTimeline);
}

int TwitterTimelineModel::rowCount(const QModelIndex &parent) const
{
#if defined(QT_DEBUG)
    Q_ASSERT(!parent.isValid());
#else
    Q_UNUSED(parent)
#endif
    return tweets.size();
}

QVariant TwitterTimelineModel::data(const QModelIndex &index, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    auto it = tweets.begin();
    std::advance(it, index.row());
    switch (index.column())
    {
    case 0:
        return QString::number(it->id);
    case 1:
        return it->createdAt.toString(Qt::SystemLocaleShortDate);
    case 2:
        return it->user;
    case 3:
        return it->text;
    }
    return QVariant();
}

int TwitterTimelineModel::columnCount(const QModelIndex &) const
{
    return 4;
}

QVariant TwitterTimelineModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case 0:
            return QStringLiteral("Id");
        case 1:
            return QStringLiteral("Created at");
        case 2:
            return QStringLiteral("User");
        case 3:
            return QStringLiteral("Text");
        }
    }
    return section;
}

void TwitterTimelineModel::authenticate(const QPair<QString, QString> &clientCredentials)
{
    twitter.setClientCredentials(clientCredentials);
    twitter.grant();
}

QAbstractOAuth::Status TwitterTimelineModel::status() const
{
    return twitter.status();
}

void TwitterTimelineModel::updateTimeline()
{
    if (twitter.status() != Twitter::Status::Granted)
        QMessageBox::warning(nullptr, qApp->applicationName(), "Not authenticated");

    QUrl url("https://api.twitter.com/1.1/statuses/home_timeline.json");
    QUrlQuery query;
    if (tweets.size()) {
        // Tweets are time-ordered, newest first.  Pass the most recent
        // ID we have to request everything more recent than it:
        query.addQueryItem("since_id", QString::number(tweets.first().id));
        // From https://dev.twitter.com/rest/reference/get/search/tweets:
        // Returns results with an ID greater than (that is, more recent than)
        // the specified ID. There are limits to the number of Tweets which can
        // be accessed through the API. If the limit of Tweets has occurred
        // since the since_id, the since_id will be forced to the oldest ID
        // available.
    }
    url.setQuery(query);
    QNetworkReply *reply = twitter.get(url);
    connect(reply, &QNetworkReply::finished, this, &TwitterTimelineModel::parseJson);
}

void TwitterTimelineModel::parseJson()
{
    QJsonParseError parseError;
    auto reply = qobject_cast<QNetworkReply*>(sender());
    Q_ASSERT(reply);
    const auto data = reply->readAll();
    const auto document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error) {
        qCritical() << "TwitterTimelineModel::parseJson. Error at:" << parseError.offset
                    << parseError.errorString();
        return;
    } else if (document.isObject()) {
        // Error received :(
        const auto object = document.object();
        const auto errorArray = object.value("errors").toArray();
        Q_ASSERT_X(errorArray.size(), "parse", data);
        QStringList errors;
        for (const auto error : errorArray) {
            Q_ASSERT(error.isObject());
            Q_ASSERT(error.toObject().contains("message"));
            errors.append(error.toObject().value("message").toString());
        }
        QMessageBox::warning(nullptr, qApp->applicationName(), errors.join("<br />"));
        return;
    }

    Q_ASSERT_X(document.isArray(), "parse", data);
    const auto array = document.array();
    if (array.size()) {
        beginInsertRows(QModelIndex(), 0, array.size() - 1);
        auto before = tweets.begin();
        for (const auto &value : array) {
            Q_ASSERT(value.isObject());
            const auto object = value.toObject();
            const auto createdAt = QDateTime::fromString(object.value("created_at").toString(),
                                                         "ddd MMM dd HH:mm:ss +0000 yyyy");
            before = tweets.insert(before, Tweet{
                                       object.value("id").toVariant().toULongLong(),
                                       createdAt,
                                       object.value("user").toObject().value("name").toString(),
                                       object.value("text").toString()
                                   });
            std::advance(before, 1);
        }
        endInsertRows();
    }
}
