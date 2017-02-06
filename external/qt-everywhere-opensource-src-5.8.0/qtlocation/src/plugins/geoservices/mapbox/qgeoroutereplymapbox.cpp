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

#include "qgeoroutereplymapbox.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtLocation/QGeoRouteSegment>
#include <QtLocation/QGeoManeuver>

QT_BEGIN_NAMESPACE

static QList<QGeoCoordinate> parsePolyline(const QString &line)
{
    QList<QGeoCoordinate> path;
    QByteArray data(line.toLocal8Bit());

    int mode = 0, shift = 0, value = 0, coord[2] = {0, 0};
    for (int i = 0; i < data.length(); ++i) {
        int c = data.at(i) - 63;
        value |= (c & 0x1f) << shift;
        shift += 5;
        if (c & 0x20) continue;
        coord[mode] += (value & 1) ? ~(value >> 1) : (value >> 1);
        if (mode) path.append(QGeoCoordinate((double)coord[0]/1e5, (double)coord[1]/1e5));
        mode = 1 - mode;
        value = shift = 0;
    }
    return path;
}

static QList<QGeoCoordinate> parseGeometry(const QJsonValue &geometry)
{
    QList<QGeoCoordinate> path;
    if (geometry.isString()) path = parsePolyline(geometry.toString());
    if (geometry.isObject()) {
        QJsonArray coords = geometry.toObject().value(QStringLiteral("coordinates")).toArray();
        for (int i = 0; i < coords.count(); i++) {
            QJsonArray coord = coords.at(i).toArray();
            if (coord.count() != 2) continue;
            path.append(QGeoCoordinate(coord.at(1).toDouble(), coord.at(0).toDouble()));
        }
    }
    return path;
}

QGeoRouteReplyMapbox::QGeoRouteReplyMapbox(QNetworkReply *reply, const QGeoRouteRequest &request,
                                     QObject *parent)
:   QGeoRouteReply(request, parent), m_reply(reply)
{
    connect(m_reply, SIGNAL(finished()), this, SLOT(networkReplyFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(networkReplyError(QNetworkReply::NetworkError)));
}

QGeoRouteReplyMapbox::~QGeoRouteReplyMapbox()
{
    if (m_reply)
        m_reply->deleteLater();
}

void QGeoRouteReplyMapbox::abort()
{
    if (!m_reply)
        return;

    m_reply->abort();

    m_reply->deleteLater();
    m_reply = 0;
}

static QGeoRoute constructRoute(const QJsonObject &obj)
{
    QGeoRoute route;
    route.setDistance(obj.value(QStringLiteral("distance")).toDouble());
    route.setTravelTime(obj.value(QStringLiteral("duration")).toDouble());

    QList<QGeoCoordinate> path = parseGeometry(obj.value(QStringLiteral("geometry")));
    route.setPath(path);

    QGeoRouteSegment firstSegment, lastSegment;
    QJsonArray legs = obj.value(QStringLiteral("legs")).toArray();

    for (int i = 0; i < legs.count(); i++) {
        QJsonObject leg = legs.at(i).toObject();
        QJsonArray steps = leg.value("steps").toArray();

        for (int j = 0; j < steps.count(); j++) {
            QJsonObject step = steps.at(j).toObject();
            QJsonObject stepManeuver = step.value("maneuver").toObject();

            QGeoRouteSegment segment;
            segment.setDistance(step.value("distance").toDouble());
            segment.setTravelTime(step.value(QStringLiteral("duration")).toDouble());

            QGeoManeuver maneuver;
            maneuver.setDistanceToNextInstruction(step.value("distance").toDouble());
            maneuver.setInstructionText(stepManeuver.value("instruction").toString());
            maneuver.setTimeToNextInstruction(step.value(QStringLiteral("duration")).toDouble());
            QJsonArray location = stepManeuver.value(QStringLiteral("location")).toArray();
            if (location.count() > 1)
                maneuver.setPosition(QGeoCoordinate(location.at(0).toDouble(), location.at(1).toDouble()));

            QString modifier = stepManeuver.value("modifier").toString();
            int bearing1 = stepManeuver.value("bearing_before").toInt();
            int bearing2 = stepManeuver.value("bearing_after").toInt();

            if (modifier == "straight")
                maneuver.setDirection(QGeoManeuver::DirectionForward);
            else if (modifier == "slight right")
                maneuver.setDirection(QGeoManeuver::DirectionLightRight);
            else if (modifier == "right")
                maneuver.setDirection(QGeoManeuver::DirectionRight);
            else if (modifier == "sharp right")
                maneuver.setDirection(QGeoManeuver::DirectionHardRight);
            else if (modifier == "uturn")
                maneuver.setDirection(bearing2 - bearing1 > 180 ? QGeoManeuver::DirectionUTurnLeft : QGeoManeuver::DirectionUTurnRight);
            else if (modifier == "sharp left")
                maneuver.setDirection(QGeoManeuver::DirectionHardLeft);
            else if (modifier == "left")
                maneuver.setDirection(QGeoManeuver::DirectionLeft);
            else if (modifier == "slight left")
                maneuver.setDirection(QGeoManeuver::DirectionLightLeft);
            else
                maneuver.setDirection(QGeoManeuver::NoDirection);

            segment.setManeuver(maneuver);
            segment.setPath(parseGeometry(step.value(QStringLiteral("geometry"))));

            if (!firstSegment.isValid()) firstSegment = segment;
            if (lastSegment.isValid()) lastSegment.setNextRouteSegment(segment);
            lastSegment = segment;
        }
    }
    route.setFirstRouteSegment(firstSegment);
    return route;
}

void QGeoRouteReplyMapbox::networkReplyFinished()
{
    if (!m_reply)
        return;

    if (m_reply->error() != QNetworkReply::NoError) {
        setError(QGeoRouteReply::CommunicationError, m_reply->errorString());
        m_reply->deleteLater();
        m_reply = 0;
        return;
    }

    QJsonDocument document = QJsonDocument::fromJson(m_reply->readAll());
    if (document.isObject()) {
        QJsonObject object = document.object();

        QString status = object.value(QStringLiteral("code")).toString();
        if (status != QStringLiteral("Ok")) {
            setError(QGeoRouteReply::UnknownError, object.value(QStringLiteral("message")).toString());
            m_reply->deleteLater();
            m_reply = 0;
            return;
        }

        QList<QGeoRoute> list;
        QJsonArray routes = object.value(QStringLiteral("routes")).toArray();
        for (int i = 0; i < routes.count(); i++) {
            QGeoRoute route = constructRoute(routes.at(i).toObject());
            list.append(route);
        }
        setRoutes(list);
        setFinished(true);
    } else {
        setError(QGeoRouteReply::ParseError, QStringLiteral("Couldn't parse json."));
    }

    m_reply->deleteLater();
    m_reply = 0;
}

void QGeoRouteReplyMapbox::networkReplyError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    if (!m_reply)
        return;

    setError(QGeoRouteReply::CommunicationError, m_reply->errorString());

    m_reply->deleteLater();
    m_reply = 0;
}

QT_END_NAMESPACE
