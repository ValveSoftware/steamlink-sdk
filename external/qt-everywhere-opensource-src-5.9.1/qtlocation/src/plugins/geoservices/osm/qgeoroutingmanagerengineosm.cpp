/****************************************************************************
**
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

#include "qgeoroutingmanagerengineosm.h"
#include "qgeoroutereplyosm.h"
#include "QtLocation/private/qgeorouteparserosrmv4_p.h"
#include "QtLocation/private/qgeorouteparserosrmv5_p.h"

#include <QtCore/QUrlQuery>

#include <QtCore/QDebug>

QGeoRoutingManagerEngineOsm::QGeoRoutingManagerEngineOsm(const QVariantMap &parameters,
                                                         QGeoServiceProvider::Error *error,
                                                         QString *errorString)
:   QGeoRoutingManagerEngine(parameters), m_networkManager(new QNetworkAccessManager(this))
{
    if (parameters.contains(QStringLiteral("osm.useragent")))
        m_userAgent = parameters.value(QStringLiteral("osm.useragent")).toString().toLatin1();
    else
        m_userAgent = "Qt Location based application";

    if (parameters.contains(QStringLiteral("osm.routing.host")))
        m_urlPrefix = parameters.value(QStringLiteral("osm.routing.host")).toString().toLatin1();
    else
        m_urlPrefix = QStringLiteral("http://router.project-osrm.org/route/v1/driving/");
        // for v4 it was "http://router.project-osrm.org/viaroute"

    if (parameters.contains(QStringLiteral("osm.routing.apiversion"))
            && (parameters.value(QStringLiteral("osm.routing.apiversion")).toString().toLatin1() == QByteArray("v4")))
        m_routeParser = new QGeoRouteParserOsrmV4(this);
    else
        m_routeParser = new QGeoRouteParserOsrmV5(this);

    *error = QGeoServiceProvider::NoError;
    errorString->clear();
}

QGeoRoutingManagerEngineOsm::~QGeoRoutingManagerEngineOsm()
{
}

QGeoRouteReply* QGeoRoutingManagerEngineOsm::calculateRoute(const QGeoRouteRequest &request)
{
    QNetworkRequest networkRequest;
    networkRequest.setHeader(QNetworkRequest::UserAgentHeader, m_userAgent);

    networkRequest.setUrl(routeParser()->requestUrl(request, m_urlPrefix));

    QNetworkReply *reply = m_networkManager->get(networkRequest);

    QGeoRouteReplyOsm *routeReply = new QGeoRouteReplyOsm(reply, request, this);

    connect(routeReply, SIGNAL(finished()), this, SLOT(replyFinished()));
    connect(routeReply, SIGNAL(error(QGeoRouteReply::Error,QString)),
            this, SLOT(replyError(QGeoRouteReply::Error,QString)));

    return routeReply;
}

const QGeoRouteParser *QGeoRoutingManagerEngineOsm::routeParser() const
{
    return m_routeParser;
}

void QGeoRoutingManagerEngineOsm::replyFinished()
{
    QGeoRouteReply *reply = qobject_cast<QGeoRouteReply *>(sender());
    if (reply)
        emit finished(reply);
}

void QGeoRoutingManagerEngineOsm::replyError(QGeoRouteReply::Error errorCode,
                                             const QString &errorString)
{
    QGeoRouteReply *reply = qobject_cast<QGeoRouteReply *>(sender());
    if (reply)
        emit error(reply, errorCode, errorString);
}
