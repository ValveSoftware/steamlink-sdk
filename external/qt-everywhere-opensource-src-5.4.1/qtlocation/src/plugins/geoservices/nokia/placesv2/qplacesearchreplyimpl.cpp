/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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
** This file is part of the Nokia services plugin for the Maps and
** Navigation API.  The use of these services, whether by use of the
** plugin or by other means, is governed by the terms and conditions
** described by the file NOKIA_TERMS_AND_CONDITIONS.txt in
** this package, located in the directory containing the Nokia services
** plugin source code.
**
****************************************************************************/

#include "qplacesearchreplyimpl.h"
#include "jsonparserhelpers.h"
#include "../qplacemanagerengine_nokiav2.h"
#include "../qgeoerror_messages.h"

#include <QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtLocation/QPlaceIcon>
#include <QtLocation/QPlaceResult>
#include <QtLocation/QPlaceProposedSearchResult>

#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QPlaceSearchReplyImpl::QPlaceSearchReplyImpl(const QPlaceSearchRequest &request,
                                             QNetworkReply *reply,
                                             QPlaceManagerEngineNokiaV2 *parent)
    :   QPlaceSearchReply(parent), m_reply(reply), m_engine(parent)
{
    Q_ASSERT(parent);

    setRequest(request);

    if (!m_reply)
        return;

    m_reply->setParent(this);
    connect(m_reply, SIGNAL(finished()), this, SLOT(replyFinished()));
}

QPlaceSearchReplyImpl::~QPlaceSearchReplyImpl()
{
}

void QPlaceSearchReplyImpl::abort()
{
    if (m_reply)
        m_reply->abort();
}

void QPlaceSearchReplyImpl::setError(QPlaceReply::Error error_, const QString &errorString)
{
    QPlaceReply::setError(error_, errorString);
    emit error(error_, errorString);
    setFinished(true);
    emit finished();
}

void QPlaceSearchReplyImpl::replyFinished()
{
    if (m_reply->error() != QNetworkReply::NoError) {
        switch (m_reply->error()) {
        case QNetworkReply::OperationCanceledError:
            setError(CancelError, QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, CANCEL_ERROR));
            break;
        case QNetworkReply::ContentNotFoundError:
            setError(PlaceDoesNotExistError,
                     QString::fromLatin1("The id, %1, does not reference an existing place")
                     .arg(request().recommendationId()));
            break;
        default:
            setError(CommunicationError, QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, NETWORK_ERROR));
        }
        return;
    }

    QJsonDocument document = QJsonDocument::fromJson(m_reply->readAll());
    if (!document.isObject()) {
        setError(ParseError, QCoreApplication::translate(NOKIA_PLUGIN_CONTEXT_NAME, PARSE_ERROR));
        return;
    }

    QJsonObject resultsObject = document.object();

    if (resultsObject.contains(QStringLiteral("results")))
        resultsObject = resultsObject.value(QStringLiteral("results")).toObject();

    QJsonArray items = resultsObject.value(QStringLiteral("items")).toArray();

    QList<QPlaceSearchResult> results;
    for (int i = 0; i < items.count(); ++i) {
        QJsonObject item = items.at(i).toObject();

        const QString type = item.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("urn:nlp-types:place"))
            results.append(parsePlaceResult(item));
        else if (type == QStringLiteral("urn:nlp-types:search"))
            results.append(parseSearchResult(item));
    }

    if (resultsObject.contains(QStringLiteral("next"))) {
        QPlaceSearchRequest request;
        request.setSearchContext(QUrl(resultsObject.value(QStringLiteral("next")).toString()));
        setNextPageRequest(request);
    }

    if (resultsObject.contains(QStringLiteral("previous"))) {
        QPlaceSearchRequest request;
        request.setSearchContext(QUrl(resultsObject.value(QStringLiteral("previous")).toString()));
        setPreviousPageRequest(request);
    }

    setResults(results);

    m_reply->deleteLater();
    m_reply = 0;

    setFinished(true);
    emit finished();
}

QPlaceResult QPlaceSearchReplyImpl::parsePlaceResult(const QJsonObject &item) const
{
    QPlaceResult result;

    if (item.contains(QStringLiteral("distance")))
        result.setDistance(item.value(QStringLiteral("distance")).toDouble());

    QPlace place;

    QGeoLocation location;

    location.setCoordinate(parseCoordinate(item.value(QStringLiteral("position")).toArray()));

    const QString vicinity = item.value(QStringLiteral("vicinity")).toString();
    QGeoAddress address;
    address.setText(vicinity);
    location.setAddress(address);

    if (item.contains(QStringLiteral("bbox"))) {
        QJsonArray bbox = item.value(QStringLiteral("bbox")).toArray();
        QGeoRectangle box(QGeoCoordinate(bbox.at(3).toDouble(), bbox.at(0).toDouble()),
                            QGeoCoordinate(bbox.at(1).toDouble(), bbox.at(2).toDouble()));
        location.setBoundingBox(box);
    }

    place.setLocation(location);

    QPlaceRatings ratings;
    ratings.setAverage(item.value(QStringLiteral("averageRating")).toDouble());
    ratings.setMaximum(5.0);
    place.setRatings(ratings);

    const QString title = item.value(QStringLiteral("title")).toString();
    place.setName(title);
    result.setTitle(title);

    QPlaceIcon icon = m_engine->icon(item.value(QStringLiteral("icon")).toString());
    place.setIcon(icon);
    result.setIcon(icon);

    place.setCategory(parseCategory(item.value(QStringLiteral("category")).toObject(),
                                    m_engine));

    //QJsonArray having = item.value(QStringLiteral("having")).toArray();

    result.setSponsored(item.value(QStringLiteral("sponsored")).toBool());

    QUrl href = item.value(QStringLiteral("href")).toString();
    //QUrl type = item.value(QStringLiteral("type")).toString();

    place.setPlaceId(href.path().mid(18, 41));

    QPlaceAttribute provider;
    provider.setText(QStringLiteral("nokia"));
    place.setExtendedAttribute(QPlaceAttribute::Provider, provider);
    place.setVisibility(QLocation::PublicVisibility);

    result.setPlace(place);

    return result;
}

QPlaceProposedSearchResult QPlaceSearchReplyImpl::parseSearchResult(const QJsonObject &item) const
{
    QPlaceProposedSearchResult result;

    result.setTitle(item.value(QStringLiteral("title")).toString());

    QPlaceIcon icon = m_engine->icon(item.value(QStringLiteral("icon")).toString());
    result.setIcon(icon);

    QPlaceSearchRequest request;
    request.setSearchContext(QUrl(item.value("href").toString()));

    result.setSearchRequest(request);

    return result;
}

QT_END_NAMESPACE
