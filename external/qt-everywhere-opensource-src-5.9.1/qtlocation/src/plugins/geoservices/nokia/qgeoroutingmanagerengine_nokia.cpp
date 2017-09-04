/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qgeoroutingmanagerengine_nokia.h"
#include "qgeoroutereply_nokia.h"
#include "qgeonetworkaccessmanager.h"
#include "qgeouriprovider.h"
#include "uri_constants.h"

#include <QStringList>
#include <QUrl>
#include <QLocale>
#include <QtPositioning/QGeoRectangle>

QT_BEGIN_NAMESPACE

QGeoRoutingManagerEngineNokia::QGeoRoutingManagerEngineNokia(
        QGeoNetworkAccessManager *networkManager,
        const QVariantMap &parameters,
        QGeoServiceProvider::Error *error,
        QString *errorString)
        : QGeoRoutingManagerEngine(parameters)
        , m_networkManager(networkManager)
        , m_uriProvider(new QGeoUriProvider(this, parameters, QStringLiteral("here.routing.host"), ROUTING_HOST))

{
    Q_ASSERT(networkManager);
    m_networkManager->setParent(this);

    m_appId = parameters.value(QStringLiteral("here.app_id")).toString();
    m_token = parameters.value(QStringLiteral("here.token")).toString();

    QGeoRouteRequest::FeatureTypes featureTypes;
    featureTypes |= QGeoRouteRequest::TollFeature;
    featureTypes |= QGeoRouteRequest::HighwayFeature;
    featureTypes |= QGeoRouteRequest::FerryFeature;
    featureTypes |= QGeoRouteRequest::TunnelFeature;
    featureTypes |= QGeoRouteRequest::DirtRoadFeature;
    featureTypes |= QGeoRouteRequest::ParksFeature;
    setSupportedFeatureTypes(featureTypes);

    QGeoRouteRequest::FeatureWeights featureWeights;
    featureWeights |= QGeoRouteRequest::DisallowFeatureWeight;
    featureWeights |= QGeoRouteRequest::AvoidFeatureWeight;
    featureWeights |= QGeoRouteRequest::PreferFeatureWeight;
    setSupportedFeatureWeights(featureWeights);

    QGeoRouteRequest::ManeuverDetails maneuverDetails;
    maneuverDetails |= QGeoRouteRequest::BasicManeuvers;
    setSupportedManeuverDetails(maneuverDetails);

    QGeoRouteRequest::RouteOptimizations optimizations;
    optimizations |= QGeoRouteRequest::ShortestRoute;
    optimizations |= QGeoRouteRequest::FastestRoute;
    setSupportedRouteOptimizations(optimizations);

    QGeoRouteRequest::TravelModes travelModes;
    travelModes |= QGeoRouteRequest::CarTravel;
    travelModes |= QGeoRouteRequest::PedestrianTravel;
    travelModes |= QGeoRouteRequest::PublicTransitTravel;
    travelModes |= QGeoRouteRequest::BicycleTravel;
    setSupportedTravelModes(travelModes);

    QGeoRouteRequest::SegmentDetails segmentDetails;
    segmentDetails |= QGeoRouteRequest::BasicSegmentData;
    setSupportedSegmentDetails(segmentDetails);

    if (error)
        *error = QGeoServiceProvider::NoError;

    if (errorString)
        *errorString = QString();
}

QGeoRoutingManagerEngineNokia::~QGeoRoutingManagerEngineNokia() {}

QGeoRouteReply *QGeoRoutingManagerEngineNokia::calculateRoute(const QGeoRouteRequest &request)
{
    const QStringList reqStrings = calculateRouteRequestString(request);

    if (reqStrings.isEmpty()) {
        QGeoRouteReply *reply = new QGeoRouteReply(QGeoRouteReply::UnsupportedOptionError, "The given route request options are not supported by this service provider.", this);
        emit error(reply, reply->error(), reply->errorString());
        return reply;
    }

    QList<QNetworkReply*> replies;
    foreach (const QString &reqString, reqStrings)
        replies.append(m_networkManager->get(QNetworkRequest(QUrl(reqString))));

    QGeoRouteReplyNokia *reply = new QGeoRouteReplyNokia(request, replies, this);

    connect(reply,
            SIGNAL(finished()),
            this,
            SLOT(routeFinished()));

    connect(reply,
            SIGNAL(error(QGeoRouteReply::Error,QString)),
            this,
            SLOT(routeError(QGeoRouteReply::Error,QString)));

    return reply;
}

QGeoRouteReply *QGeoRoutingManagerEngineNokia::updateRoute(const QGeoRoute &route, const QGeoCoordinate &position)
{
    const QStringList reqStrings = updateRouteRequestString(route, position);

    if (reqStrings.isEmpty()) {
        QGeoRouteReply *reply = new QGeoRouteReply(QGeoRouteReply::UnsupportedOptionError, "The given route request options are not supported by this service provider.", this);
        emit error(reply, reply->error(), reply->errorString());
        return reply;
    }

    QList<QNetworkReply*> replies;
    foreach (const QString &reqString, reqStrings)
        replies.append(m_networkManager->get(QNetworkRequest(QUrl(reqString))));

    QGeoRouteRequest updateRequest(route.request());
    updateRequest.setTravelModes(route.travelMode());
    QGeoRouteReplyNokia *reply = new QGeoRouteReplyNokia(updateRequest, replies, this);

    connect(reply,
            SIGNAL(finished()),
            this,
            SLOT(routeFinished()));

    connect(reply,
            SIGNAL(error(QGeoRouteReply::Error,QString)),
            this,
            SLOT(routeError(QGeoRouteReply::Error,QString)));

    return reply;
}

bool QGeoRoutingManagerEngineNokia::checkEngineSupport(const QGeoRouteRequest &request,
        QGeoRouteRequest::TravelModes travelModes) const
{
    QList<QGeoRouteRequest::FeatureType> featureTypeList = request.featureTypes();
    QGeoRouteRequest::FeatureTypes featureTypeFlag = QGeoRouteRequest::NoFeature;
    QGeoRouteRequest::FeatureWeights featureWeightFlag = QGeoRouteRequest::NeutralFeatureWeight;

    for (int i = 0; i < featureTypeList.size(); ++i) {
        featureTypeFlag |= featureTypeList.at(i);
        featureWeightFlag |= request.featureWeight(featureTypeList.at(i));
    }

    if ((featureTypeFlag & supportedFeatureTypes()) != featureTypeFlag)
        return false;

    if ((featureWeightFlag & supportedFeatureWeights()) != featureWeightFlag)
        return false;


    if ((request.maneuverDetail() & supportedManeuverDetails()) != request.maneuverDetail())
        return false;

    if ((request.segmentDetail() & supportedSegmentDetails()) != request.segmentDetail())
        return false;

    if ((request.routeOptimization() & supportedRouteOptimizations()) != request.routeOptimization())
        return false;

    if ((travelModes & supportedTravelModes()) != travelModes)
        return false;

    // Count the number of set bits (= number of travel modes) (popcount)
    int count = 0;

    for (unsigned bits = travelModes; bits; bits >>= 1)
        count += (bits & 1);

    // We only allow one travel mode at a time
    if (count != 1)
        return false;

    return true;
}

QStringList QGeoRoutingManagerEngineNokia::calculateRouteRequestString(const QGeoRouteRequest &request)
{
    bool supported = checkEngineSupport(request, request.travelModes());

    if (!supported)
        return QStringList();
    QStringList requests;

    QString baseRequest = QStringLiteral("http://");
    baseRequest += m_uriProvider->getCurrentHost();
    baseRequest += QStringLiteral("/routing/7.2/calculateroute.xml");

    baseRequest += QStringLiteral("?alternatives=");
    baseRequest += QString::number(request.numberAlternativeRoutes());

    if (!m_appId.isEmpty() && !m_token.isEmpty()) {
        baseRequest += QStringLiteral("&app_id=");
        baseRequest += m_appId;
        baseRequest += QStringLiteral("&token=");
        baseRequest += m_token;
    }

    int numWaypoints = request.waypoints().size();
    if (numWaypoints < 2)
        return QStringList();

    for (int i = 0;i < numWaypoints;++i) {
        baseRequest += QStringLiteral("&waypoint");
        baseRequest += QString::number(i);
        baseRequest += QStringLiteral("=geo!");
        baseRequest += trimDouble(request.waypoints().at(i).latitude());
        baseRequest += ',';
        baseRequest += trimDouble(request.waypoints().at(i).longitude());
    }

    QGeoRouteRequest::RouteOptimizations optimization = request.routeOptimization();

    QStringList types;
    if (optimization.testFlag(QGeoRouteRequest::ShortestRoute))
        types.append("shortest");
    if (optimization.testFlag(QGeoRouteRequest::FastestRoute))
        types.append("fastest");

    foreach (const QString &optimization, types) {
        QString requestString = baseRequest;
        requestString += modesRequestString(request, request.travelModes(), optimization);
        requestString += routeRequestString(request);
        requests << requestString;
    }

    return requests;
}

QStringList QGeoRoutingManagerEngineNokia::updateRouteRequestString(const QGeoRoute &route, const QGeoCoordinate &position)
{
    if (!checkEngineSupport(route.request(), route.travelMode()))
        return QStringList();
    QStringList requests;

    QString baseRequest = "http://";
    baseRequest += m_uriProvider->getCurrentHost();
    baseRequest += "/routing/7.2/getroute.xml";

    baseRequest += "?routeid=";
    baseRequest += route.routeId();

    baseRequest += "&pos=";
    baseRequest += QString::number(position.latitude());
    baseRequest += ',';
    baseRequest += QString::number(position.longitude());

    QGeoRouteRequest::RouteOptimizations optimization = route.request().routeOptimization();

    QStringList types;
    if (optimization.testFlag(QGeoRouteRequest::ShortestRoute))
        types.append("shortest");
    if (optimization.testFlag(QGeoRouteRequest::FastestRoute))
        types.append("fastest");

    foreach (const QString &optimization, types) {
        QString requestString = baseRequest;
        requestString += modesRequestString(route.request(), route.travelMode(), optimization);
        requestString += routeRequestString(route.request());
        requests << requestString;
    }

    return requests;
}

QString QGeoRoutingManagerEngineNokia::modesRequestString(const QGeoRouteRequest &request,
        QGeoRouteRequest::TravelModes travelModes, const QString &optimization) const
{
    QString requestString;

    QStringList modes;
    if (travelModes.testFlag(QGeoRouteRequest::CarTravel))
        modes.append("car");
    if (travelModes.testFlag(QGeoRouteRequest::PedestrianTravel))
        modes.append("pedestrian");
    if (travelModes.testFlag(QGeoRouteRequest::PublicTransitTravel))
        modes.append("publicTransport");

    QStringList featureStrings;
    QList<QGeoRouteRequest::FeatureType> featureTypes = request.featureTypes();
    for (int i = 0; i < featureTypes.size(); ++i) {
        QGeoRouteRequest::FeatureWeight weight = request.featureWeight(featureTypes.at(i));

        if (weight == QGeoRouteRequest::NeutralFeatureWeight)
            continue;

        QString weightString = "";
        switch (weight) {
            case QGeoRouteRequest::PreferFeatureWeight:
                weightString = '1';
                break;
            case QGeoRouteRequest::AvoidFeatureWeight:
                weightString = "-1";
                break;
            case QGeoRouteRequest::DisallowFeatureWeight:
                weightString = "-3";
                break;
            case QGeoRouteRequest::NeutralFeatureWeight:
            case QGeoRouteRequest::RequireFeatureWeight:
                break;
        }

        if (weightString.isEmpty())
            continue;

        switch (featureTypes.at(i)) {
            case QGeoRouteRequest::TollFeature:
                featureStrings.append("tollroad:" + weightString);
                break;
            case QGeoRouteRequest::HighwayFeature:
                featureStrings.append("motorway:" + weightString);
                break;
            case QGeoRouteRequest::FerryFeature:
                featureStrings.append("boatFerry:" + weightString);
                featureStrings.append("railFerry:" + weightString);
                break;
            case QGeoRouteRequest::TunnelFeature:
                featureStrings.append("tunnel:" + weightString);
                break;
            case QGeoRouteRequest::DirtRoadFeature:
                featureStrings.append("dirtRoad:" + weightString);
                break;
            case QGeoRouteRequest::PublicTransitFeature:
            case QGeoRouteRequest::ParksFeature:
            case QGeoRouteRequest::MotorPoolLaneFeature:
            case QGeoRouteRequest::NoFeature:
                break;
        }
    }

    requestString += "&mode=";
    requestString += optimization + ';' + modes.join(',');
    if (featureStrings.count())
        requestString += ';' + featureStrings.join(',');
    return requestString;
}

QString QGeoRoutingManagerEngineNokia::routeRequestString(const QGeoRouteRequest &request) const
{
    QString requestString;

    foreach (const QGeoRectangle &area, request.excludeAreas()) {
        requestString += QLatin1String("&avoidareas=");
        requestString += trimDouble(area.topLeft().latitude());
        requestString += QLatin1String(",");
        requestString += trimDouble(area.topLeft().longitude());
        requestString += QLatin1String(";");
        requestString += trimDouble(area.bottomRight().latitude());
        requestString += QLatin1String(",");
        requestString += trimDouble(area.bottomRight().longitude());
    }

//    TODO: work out what was going on here
//    - segment and instruction/maneuever functions are mixed and matched
//    - tried to implement sensible equivalents below
//    QStringList legAttributes;
//    if (request.instructionDetail() & QGeoRouteRequest::BasicSegmentData) {
//        requestString += "&linkattributes=sh,le"; //shape,length
//        legAttributes.append("links");
//    }
//
//    if (request.instructionDetail() & QGeoRouteRequest::BasicInstructions) {
//        legAttributes.append("maneuvers");
//        requestString += "&maneuverattributes=po,tt,le,di"; //position,traveltime,length,direction
//        if (!(request.instructionDetail() & QGeoRouteRequest::NoSegmentData))
//            requestString += ",li"; //link
//    }

    QStringList legAttributes;
    if (request.segmentDetail() & QGeoRouteRequest::BasicSegmentData) {
        requestString += "&linkattributes=sh,le"; //shape,length
        legAttributes.append("links");
    }

    if (request.maneuverDetail() & QGeoRouteRequest::BasicManeuvers) {
        legAttributes.append("maneuvers");
        requestString += "&maneuverattributes=po,tt,le,di"; //position,traveltime,length,direction
        if (!(request.segmentDetail() & QGeoRouteRequest::NoSegmentData))
            requestString += ",li"; //link
    }

    requestString += "&routeattributes=sm,sh,bb,lg"; //summary,shape,boundingBox,legs
    if (legAttributes.count() > 0) {
        requestString += "&legattributes=";
        requestString += legAttributes.join(",");
    }

    requestString += "&departure=";
    requestString += QDateTime::currentDateTime().toUTC().toString("yyyy-MM-ddThh:mm:ssZ");

    requestString += "&instructionformat=text";

    requestString += "&metricSystem=";
    if (QLocale::MetricSystem == measurementSystem())
        requestString  += "metric";
    else
        requestString += "imperial";

    const QLocale loc(locale());

    if (QLocale::C != loc.language() && QLocale::AnyLanguage != loc.language()) {
        requestString += "&language=";
        requestString += loc.name();
        //If the first language isn't supported, english will be selected automatically
        if (QLocale::English != loc.language())
            requestString += ",en_US";
    }

    return requestString;
}

QString QGeoRoutingManagerEngineNokia::trimDouble(double degree, int decimalDigits)
{
    QString sDegree = QString::number(degree, 'g', decimalDigits);

    int index = sDegree.indexOf('.');

    if (index == -1)
        return sDegree;
    else
        return QString::number(degree, 'g', decimalDigits + index);
}

void QGeoRoutingManagerEngineNokia::routeFinished()
{
    QGeoRouteReply *reply = qobject_cast<QGeoRouteReply *>(sender());

    if (!reply)
        return;

    if (receivers(SIGNAL(finished(QGeoRouteReply*))) == 0) {
        reply->deleteLater();
        return;
    }

    emit finished(reply);
}

void QGeoRoutingManagerEngineNokia::routeError(QGeoRouteReply::Error error, const QString &errorString)
{
    QGeoRouteReply *reply = qobject_cast<QGeoRouteReply *>(sender());

    if (!reply)
        return;

    if (receivers(SIGNAL(error(QGeoRouteReply*,QGeoRouteReply::Error,QString))) == 0) {
        reply->deleteLater();
        return;
    }

    emit this->error(reply, error, errorString);
}

QT_END_NAMESPACE
