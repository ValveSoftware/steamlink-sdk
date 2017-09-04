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

#include "geocodingmanagerengine_esri.h"
#include "geocodereply_esri.h"

#include <QVariantMap>
#include <QUrl>
#include <QUrlQuery>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QGeoCoordinate>
#include <QGeoAddress>
#include <QGeoShape>
#include <QGeoRectangle>

QT_BEGIN_NAMESPACE

// https://developers.arcgis.com/rest/geocode/api-reference/geocoding-find-address-candidates.htm
// https://developers.arcgis.com/rest/geocode/api-reference/geocoding-reverse-geocode.htm

static const QString kPrefixEsri(QStringLiteral("esri."));
static const QString kParamUserAgent(kPrefixEsri + QStringLiteral("useragent"));

static const QString kUrlGeocode(QStringLiteral("http://geocode.arcgis.com/arcgis/rest/services/World/GeocodeServer/findAddressCandidates"));
static const QString kUrlReverseGeocode(QStringLiteral("http://geocode.arcgis.com/arcgis/rest/services/World/GeocodeServer/reverseGeocode"));

static QString addressToQuery(const QGeoAddress &address)
{
    return address.street() + QStringLiteral(", ")
           + address.district() + QStringLiteral(", ")
           + address.city() + QStringLiteral(", ")
           + address.state() + QStringLiteral(", ")
           + address.country();
}

static QString boundingBoxToLtrb(const QGeoRectangle &rect)
{
    return QString::number(rect.topLeft().longitude()) + QLatin1Char(',')
           + QString::number(rect.topLeft().latitude()) + QLatin1Char(',')
           + QString::number(rect.bottomRight().longitude()) + QLatin1Char(',')
           + QString::number(rect.bottomRight().latitude());
}

GeoCodingManagerEngineEsri::GeoCodingManagerEngineEsri(const QVariantMap &parameters,
                                                       QGeoServiceProvider::Error *error,
                                                       QString *errorString)
:   QGeoCodingManagerEngine(parameters), m_networkManager(new QNetworkAccessManager(this))
{
    if (parameters.contains(kParamUserAgent))
        m_userAgent = parameters.value(kParamUserAgent).toString().toLatin1();
    else
        m_userAgent = QByteArrayLiteral("Qt Location based application");

    *error = QGeoServiceProvider::NoError;
    errorString->clear();
}

GeoCodingManagerEngineEsri::~GeoCodingManagerEngineEsri()
{
}

QGeoCodeReply *GeoCodingManagerEngineEsri::geocode(const QGeoAddress &address,
                                                   const QGeoShape &bounds)
{
    return geocode(addressToQuery(address), 1, -1, bounds);
}

QGeoCodeReply *GeoCodingManagerEngineEsri::geocode(const QString &address, int limit, int offset,
                                                   const QGeoShape &bounds)
{
    Q_UNUSED(offset)

    QNetworkRequest request;
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);

    QUrl url(kUrlGeocode);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("singleLine"), address);
    query.addQueryItem(QStringLiteral("f"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("outFields"), "*");

    if (bounds.type() == QGeoShape::RectangleType)
        query.addQueryItem(QStringLiteral("searchExtent"), boundingBoxToLtrb(bounds));

    if (limit != -1)
        query.addQueryItem(QStringLiteral("maxLocations"), QString::number(limit));

    url.setQuery(query);
    request.setUrl(url);

    QNetworkReply *reply = m_networkManager->get(request);
    GeoCodeReplyEsri *geocodeReply = new GeoCodeReplyEsri(reply, GeoCodeReplyEsri::Geocode, this);

    connect(geocodeReply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(geocodeReply, SIGNAL(error(QGeoCodeReply::Error,QString)),
            this, SLOT(replyError(QGeoCodeReply::Error,QString)));

    return geocodeReply;
}

QGeoCodeReply *GeoCodingManagerEngineEsri::reverseGeocode(const QGeoCoordinate &coordinate,
                                                          const QGeoShape &bounds)
{
    Q_UNUSED(bounds)

    QNetworkRequest request;
    request.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);

    QUrl url(kUrlReverseGeocode);

    QUrlQuery query;

    query.addQueryItem(QStringLiteral("f"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("langCode"), locale().name().left(2));
    query.addQueryItem(QStringLiteral("location"), QString::number(coordinate.longitude()) + QLatin1Char(',')
                       + QString::number(coordinate.latitude()));

    url.setQuery(query);
    request.setUrl(url);

    QNetworkReply *reply = m_networkManager->get(request);
    GeoCodeReplyEsri *geocodeReply = new GeoCodeReplyEsri(reply, GeoCodeReplyEsri::ReverseGeocode,
                                                          this);

    connect(geocodeReply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(geocodeReply, SIGNAL(error(QGeoCodeReply::Error,QString)),
            this, SLOT(replyError(QGeoCodeReply::Error,QString)));

    return geocodeReply;
}

void GeoCodingManagerEngineEsri::replyFinished()
{
    QGeoCodeReply *reply = qobject_cast<QGeoCodeReply *>(sender());
    if (reply)
        emit finished(reply);
}

void GeoCodingManagerEngineEsri::replyError(QGeoCodeReply::Error errorCode,
                                            const QString &errorString)
{
    QGeoCodeReply *reply = qobject_cast<QGeoCodeReply *>(sender());
    if (reply)
        emit error(reply, errorCode, errorString);
}

QT_END_NAMESPACE
