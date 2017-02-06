/****************************************************************************
**
** Copyright (C) 2016 Vlad Seryakov <vseryakov@gmail.com>
** Copyright (C) 2016 Aaron McCarthy <mccarthy.aaron@gmail.com>
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

#include "qgeoroutingmanagerenginemapbox.h"
#include "qgeoroutereplymapbox.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QDebug>

QT_BEGIN_NAMESPACE

QGeoRoutingManagerEngineMapbox::QGeoRoutingManagerEngineMapbox(const QVariantMap &parameters,
                                                         QGeoServiceProvider::Error *error,
                                                         QString *errorString)
    : QGeoRoutingManagerEngine(parameters),
      m_networkManager(new QNetworkAccessManager(this)),
      m_userAgent("Qt Location based application")
{
    if (parameters.contains(QStringLiteral("mapbox.useragent"))) {
        m_userAgent = parameters.value(QStringLiteral("mapbox.useragent")).toString().toLatin1();
    }

    if (parameters.contains(QStringLiteral("mapbox.access_token"))) {
        m_accessToken = parameters.value(QStringLiteral("mapbox.access_token")).toString();
    }

    *error = QGeoServiceProvider::NoError;
    errorString->clear();
}

QGeoRoutingManagerEngineMapbox::~QGeoRoutingManagerEngineMapbox()
{
}

QGeoRouteReply* QGeoRoutingManagerEngineMapbox::calculateRoute(const QGeoRouteRequest &request)
{
    QNetworkRequest networkRequest;
    networkRequest.setRawHeader("User-Agent", m_userAgent);

    QString url("https://api.mapbox.com/directions/v5/mapbox/");

    QGeoRouteRequest::TravelModes travelModes = request.travelModes();
    if (travelModes.testFlag(QGeoRouteRequest::PedestrianTravel))
        url += "walking/";
    else
    if (travelModes.testFlag(QGeoRouteRequest::BicycleTravel))
        url += "cycling/";
    else
    if (travelModes.testFlag(QGeoRouteRequest::CarTravel))
        url += "driving/";

    foreach (const QGeoCoordinate &c, request.waypoints()) {
        url += QString("%1,%2;").arg(c.longitude()).arg(c.latitude());
    }
    if (url.right(1) == ";") url.chop(1);
    url += QString("?steps=true&overview=full&geometries=geojson&access_token=%1").arg(m_accessToken);

    networkRequest.setUrl(QUrl(url));

    QNetworkReply *reply = m_networkManager->get(networkRequest);
    QGeoRouteReplyMapbox *routeReply = new QGeoRouteReplyMapbox(reply, request, this);

    connect(routeReply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(routeReply, SIGNAL(error(QGeoRouteReply::Error,QString)),
            this, SLOT(replyError(QGeoRouteReply::Error,QString)));

    return routeReply;
}

void QGeoRoutingManagerEngineMapbox::replyFinished()
{
    QGeoRouteReply *reply = qobject_cast<QGeoRouteReply *>(sender());
    if (reply)
        emit finished(reply);
}

void QGeoRoutingManagerEngineMapbox::replyError(QGeoRouteReply::Error errorCode,
                                             const QString &errorString)
{
    QGeoRouteReply *reply = qobject_cast<QGeoRouteReply *>(sender());
    if (reply)
        emit error(reply, errorCode, errorString);
}

QT_END_NAMESPACE
