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

#include "georoutingmanagerengine_esri.h"
#include "georoutereply_esri.h"

#include <QUrlQuery>

QT_BEGIN_NAMESPACE

static const QString kPrefixEsri(QStringLiteral("esri."));
static const QString kParamUserAgent(kPrefixEsri + QStringLiteral("useragent"));
static const QString kParamToken(kPrefixEsri + QStringLiteral("token"));

static const QString kUrlRouting(QStringLiteral("http://route.arcgis.com/arcgis/rest/services/World/Route/NAServer/Route_World/solve"));

GeoRoutingManagerEngineEsri::GeoRoutingManagerEngineEsri(const QVariantMap &parameters,
                                                         QGeoServiceProvider::Error *error,
                                                         QString *errorString) :
    QGeoRoutingManagerEngine(parameters), m_networkManager(new QNetworkAccessManager(this))
{
    if (parameters.contains(kParamUserAgent))
        m_userAgent = parameters.value(kParamUserAgent).toString().toLatin1();
    else
        m_userAgent = QByteArrayLiteral("Qt Location based application");

    m_token = parameters.value(kParamToken).toString();

    *error = QGeoServiceProvider::NoError;
    errorString->clear();
}

GeoRoutingManagerEngineEsri::~GeoRoutingManagerEngineEsri()
{
}

// REST reference: http://resources.arcgis.com/en/help/arcgis-rest-api/index.html#//02r300000036000000

QGeoRouteReply *GeoRoutingManagerEngineEsri::calculateRoute(const QGeoRouteRequest &request)
{
    QNetworkRequest networkRequest;
    networkRequest.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);

    QUrl url(kUrlRouting);
    QUrlQuery query;
    QString stops;

    foreach (const QGeoCoordinate &coordinate, request.waypoints())
    {
        if (!stops.isEmpty())
            stops += "; ";

        stops += QString::number(coordinate.longitude()) + QLatin1Char(',') +
                 QString::number(coordinate.latitude());
    }

    query.addQueryItem(QStringLiteral("stops"), stops);
    query.addQueryItem(QStringLiteral("f"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("directionsLanguage"), preferedDirectionLangage());
    query.addQueryItem(QStringLiteral("directionsLengthUnits"), preferedDirectionsLengthUnits());
    query.addQueryItem(QStringLiteral("token"), m_token);

    url.setQuery(query);
    networkRequest.setUrl(url);

    QNetworkReply *reply = m_networkManager->get(networkRequest);
    GeoRouteReplyEsri *routeReply = new GeoRouteReplyEsri(reply, request, this);

    connect(routeReply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(routeReply, SIGNAL(error(QGeoRouteReply::Error,QString)), this, SLOT(replyError(QGeoRouteReply::Error,QString)));

    return routeReply;
}

void GeoRoutingManagerEngineEsri::replyFinished()
{
    QGeoRouteReply *reply = qobject_cast<QGeoRouteReply *>(sender());
    if (reply)
        emit finished(reply);
}

void GeoRoutingManagerEngineEsri::replyError(QGeoRouteReply::Error errorCode, const QString &errorString)
{
    QGeoRouteReply *reply = qobject_cast<QGeoRouteReply *>(sender());
    if (reply)
        emit error(reply, errorCode, errorString);
}

QString GeoRoutingManagerEngineEsri::preferedDirectionLangage()
{
    // list of supported langages is defined in:
    // http://resources.arcgis.com/en/help/arcgis-rest-api/index.html#//02r300000036000000
    const QStringList supportedLanguages = {
        "ar", // Generate directions in Arabic
        "cs", // Generate directions in Czech
        "de", // Generate directions in German
        "el", // Generate directions in Greek
        "en", // Generate directions in English (default)
        "es", // Generate directions in Spanish
        "et", // Generate directions in Estonian
        "fr", // Generate directions in French
        "he", // Generate directions in Hebrew
        "it", // Generate directions in Italian
        "ja", // Generate directions in Japanese
        "ko", // Generate directions in Korean
        "lt", // Generate directions in Lithuanian
        "lv", // Generate directions in Latvian
        "nl", // Generate directions in Dutch
        "pl", // Generate directions in Polish
        "pt-BR", // Generate directions in Brazilian Portuguese
        "pt-PT", // Generate directions in Portuguese (Portugal)
        "ru", // Generate directions in Russian
        "sv", // Generate directions in Swedish
        "tr", // Generate directions in Turkish
        "zh-CN" // Simplified Chinese
    };

    for (const QString &language: locale().uiLanguages())
    {
        if (language.startsWith("pt_BR")) // Portuguese (Brazilian)
            return QStringLiteral("pt-BR");
        if (language.startsWith("pt"))    // Portuguese (Portugal)
            return QStringLiteral("pt-PT");
        if (language.startsWith("zh"))    // Portuguese (Portugal)
            return QStringLiteral("zh-CN");

        const QString country = language.left(2);
        if (supportedLanguages.contains(country))
            return country;
    }
    return QStringLiteral("en");  // default value
}

QString GeoRoutingManagerEngineEsri::preferedDirectionsLengthUnits()
{
    switch (measurementSystem())
    {
    case QLocale::MetricSystem:
        return QStringLiteral("esriNAUMeters");
        break;
    case QLocale::ImperialUSSystem:
        return QStringLiteral( "esriNAUFeet");
        break;
    case QLocale::ImperialUKSystem:
        return QStringLiteral("esriNAUFeet");
        break;
    default:
        return QStringLiteral("esriNAUMeters");
        break;
    }
    return QStringLiteral("esriNAUMeters");
}

QT_END_NAMESPACE
