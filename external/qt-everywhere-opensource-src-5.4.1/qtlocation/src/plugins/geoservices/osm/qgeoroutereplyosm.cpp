/****************************************************************************
**
** Copyright (C) 2013 Aaron McCarthy <mccarthy.aaron@gmail.com>
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
****************************************************************************/

#include "qgeoroutereplyosm.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtLocation/QGeoRouteSegment>
#include <QtLocation/QGeoManeuver>

QT_BEGIN_NAMESPACE

static QList<QGeoCoordinate> parsePolyline(const QByteArray &data)
{
    QList<QGeoCoordinate> path;

    bool parsingLatitude = true;

    int shift = 0;
    int value = 0;

    QGeoCoordinate coord(0, 0);

    for (int i = 0; i < data.length(); ++i) {
        unsigned char c = data.at(i) - 63;

        value |= (c & 0x1f) << shift;
        shift += 5;

        // another chunk
        if (c & 0x20)
            continue;

        int diff = (value & 1) ? ~(value >> 1) : (value >> 1);

        if (parsingLatitude) {
            coord.setLatitude(coord.latitude() + (double)diff/1e6);
        } else {
            coord.setLongitude(coord.longitude() + (double)diff/1e6);
            path.append(coord);
        }

        parsingLatitude = !parsingLatitude;

        value = 0;
        shift = 0;
    }

    return path;
}

static QGeoManeuver::InstructionDirection osrmInstructionDirection(const QString &instructionCode)
{
    if (instructionCode == QLatin1String("0"))
        return QGeoManeuver::NoDirection;
    else if (instructionCode == QLatin1String("1"))
        return QGeoManeuver::DirectionForward;
    else if (instructionCode == QLatin1String("2"))
        return QGeoManeuver::DirectionBearRight;
    else if (instructionCode == QLatin1String("3"))
        return QGeoManeuver::DirectionRight;
    else if (instructionCode == QLatin1String("4"))
        return QGeoManeuver::DirectionHardRight;
    else if (instructionCode == QLatin1String("5"))
        return QGeoManeuver::DirectionUTurnLeft;
    else if (instructionCode == QLatin1String("6"))
        return QGeoManeuver::DirectionHardLeft;
    else if (instructionCode == QLatin1String("7"))
        return QGeoManeuver::DirectionLeft;
    else if (instructionCode == QLatin1String("8"))
        return QGeoManeuver::DirectionBearLeft;
    else if (instructionCode == QLatin1String("9"))
        return QGeoManeuver::NoDirection;
    else if (instructionCode == QLatin1String("10"))
        return QGeoManeuver::DirectionForward;
    else if (instructionCode == QLatin1String("11"))
        return QGeoManeuver::NoDirection;
    else if (instructionCode == QLatin1String("12"))
        return QGeoManeuver::NoDirection;
    else if (instructionCode == QLatin1String("13"))
        return QGeoManeuver::NoDirection;
    else if (instructionCode == QLatin1String("14"))
        return QGeoManeuver::NoDirection;
    else if (instructionCode == QLatin1String("15"))
        return QGeoManeuver::NoDirection;
    else
        return QGeoManeuver::NoDirection;
}

const QString osrmInstructionText(const QString &instructionCode, const QString &wayname)
{
    if (instructionCode == QLatin1String("0")) {
        return QString();
    } else if (instructionCode == QLatin1String("1")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("Go straight.");
        else
            return QGeoRouteReplyOsm::tr("Go straight onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("2")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("Turn slightly right.");
        else
            return QGeoRouteReplyOsm::tr("Turn slightly right onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("3")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("Turn right.");
        else
            return QGeoRouteReplyOsm::tr("Turn right onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("4")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("Make a sharp right.");
        else
            return QGeoRouteReplyOsm::tr("Make a sharp right onto %1.").arg(wayname);
    }
    else if (instructionCode == QLatin1String("5")) {
        return QGeoRouteReplyOsm::tr("When it is safe to do so, perform a U-turn.");
    } else if (instructionCode == QLatin1String("6")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("Make a sharp left.");
        else
            return QGeoRouteReplyOsm::tr("Make a sharp left onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("7")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("Turn left.");
        else
            return QGeoRouteReplyOsm::tr("Turn left onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("8")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("Turn slightly left.");
        else
            return QGeoRouteReplyOsm::tr("Turn slightly left onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("9")) {
        return QGeoRouteReplyOsm::tr("Reached waypoint.");
    } else if (instructionCode == QLatin1String("10")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("Head on.");
        else
            return QGeoRouteReplyOsm::tr("Head onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("11")) {
        return QGeoRouteReplyOsm::tr("Enter the roundabout.");
    } else if (instructionCode == QLatin1String("11-1")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("At the roundabout take the first exit.");
        else
            return QGeoRouteReplyOsm::tr("At the roundabout take the first exit onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("11-2")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("At the roundabout take the second exit.");
        else
            return QGeoRouteReplyOsm::tr("At the roundabout take the second exit onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("11-3")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("At the roundabout take the third exit.");
        else
            return QGeoRouteReplyOsm::tr("At the roundabout take the third exit onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("11-4")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("At the roundabout take the fourth exit.");
        else
            return QGeoRouteReplyOsm::tr("At the roundabout take the fourth exit onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("11-5")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("At the roundabout take the fifth exit.");
        else
            return QGeoRouteReplyOsm::tr("At the roundabout take the fifth exit onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("11-6")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("At the roundabout take the sixth exit.");
        else
            return QGeoRouteReplyOsm::tr("At the roundabout take the sixth exit onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("11-7")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("At the roundabout take the seventh exit.");
        else
            return QGeoRouteReplyOsm::tr("At the roundabout take the seventh exit onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("11-8")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("At the roundabout take the eighth exit.");
        else
            return QGeoRouteReplyOsm::tr("At the roundabout take the eighth exit onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("11-9")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("At the roundabout take the ninth exit.");
        else
            return QGeoRouteReplyOsm::tr("At the roundabout take the ninth exit onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("12")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("Leave the roundabout.");
        else
            return QGeoRouteReplyOsm::tr("Leave the roundabout onto %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("13")) {
        return QGeoRouteReplyOsm::tr("Stay on the roundabout.");
    } else if (instructionCode == QLatin1String("14")) {
        if (wayname.isEmpty())
            return QGeoRouteReplyOsm::tr("Start at the end of the street.");
        else
            return QGeoRouteReplyOsm::tr("Start at the end of %1.").arg(wayname);
    } else if (instructionCode == QLatin1String("15")) {
        return QGeoRouteReplyOsm::tr("You have reached your destination.");
    } else {
        return QGeoRouteReplyOsm::tr("Don't know what to say for '%1'").arg(instructionCode);
    }
}

QGeoRouteReplyOsm::QGeoRouteReplyOsm(QNetworkReply *reply, const QGeoRouteRequest &request,
                                     QObject *parent)
:   QGeoRouteReply(request, parent), m_reply(reply)
{
    connect(m_reply, SIGNAL(finished()), this, SLOT(networkReplyFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(networkReplyError(QNetworkReply::NetworkError)));
}

QGeoRouteReplyOsm::~QGeoRouteReplyOsm()
{
    if (m_reply)
        m_reply->deleteLater();
}

void QGeoRouteReplyOsm::abort()
{
    if (!m_reply)
        return;

    m_reply->abort();

    m_reply->deleteLater();
    m_reply = 0;
}

static QGeoRoute constructRoute(const QByteArray &geometry, const QJsonArray &instructions,
                                const QJsonObject &summary)
{
    QGeoRoute route;

    QList<QGeoCoordinate> path = parsePolyline(geometry);

    QGeoRouteSegment firstSegment;
    int firstPosition = -1;

    int segmentPathLengthCount = 0;

    for (int i = instructions.count() - 1; i >= 0; --i) {
        QJsonArray instruction = instructions.at(i).toArray();

        if (instruction.count() != 8) {
            qWarning("Instruction does not contain enough fields.");
            continue;
        }

        const QString instructionCode = instruction.at(0).toString();
        const QString wayname = instruction.at(1).toString();
        double segmentLength = instruction.at(2).toDouble();
        int position = instruction.at(3).toDouble();
        int time = instruction.at(4).toDouble();
        //const QString segmentLengthString = instruction.at(5).toString();
        //const QString direction = instruction.at(6).toString();
        //double azimuth = instruction.at(7).toDouble();

        QGeoRouteSegment segment;
        segment.setDistance(segmentLength);

        QGeoManeuver maneuver;
        maneuver.setDirection(osrmInstructionDirection(instructionCode));
        maneuver.setDistanceToNextInstruction(segmentLength);
        maneuver.setInstructionText(osrmInstructionText(instructionCode, wayname));
        maneuver.setPosition(path.at(position));
        maneuver.setTimeToNextInstruction(time);

        segment.setManeuver(maneuver);

        if (firstPosition == -1)
            segment.setPath(path.mid(position));
        else
            segment.setPath(path.mid(position, firstPosition - position));

        segmentPathLengthCount += segment.path().length();

        segment.setTravelTime(time);

        segment.setNextRouteSegment(firstSegment);

        firstSegment = segment;
        firstPosition = position;
    }

    route.setDistance(summary.value(QStringLiteral("total_distance")).toDouble());
    route.setTravelTime(summary.value(QStringLiteral("total_time")).toDouble());
    route.setFirstRouteSegment(firstSegment);
    route.setPath(path);

    return route;
}

void QGeoRouteReplyOsm::networkReplyFinished()
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

        //double version = object.value(QStringLiteral("version")).toDouble();
        int status = object.value(QStringLiteral("status")).toDouble();
        QString statusMessage = object.value(QStringLiteral("status_message")).toString();

        // status code is 0 in case of success
        // status code is 207 if no route was found
        // an error occurred when trying to find a route
        if (0 != status) {
            setError(QGeoRouteReply::UnknownError, statusMessage);
            m_reply->deleteLater();
            m_reply = 0;
            return;
        }

        QJsonObject routeSummary = object.value(QStringLiteral("route_summary")).toObject();

        QByteArray routeGeometry =
            object.value(QStringLiteral("route_geometry")).toString().toLatin1();

        QJsonArray routeInstructions = object.value(QStringLiteral("route_instructions")).toArray();

        QGeoRoute route = constructRoute(routeGeometry, routeInstructions, routeSummary);

        QList<QGeoRoute> routes;
        routes.append(route);

        QJsonArray alternativeSummaries =
            object.value(QStringLiteral("alternative_summaries")).toArray();
        QJsonArray alternativeGeometries =
            object.value(QStringLiteral("alternative_geometries")).toArray();
        QJsonArray alternativeInstructions =
            object.value(QStringLiteral("alternative_instructions")).toArray();

        if (alternativeSummaries.count() == alternativeGeometries.count() &&
            alternativeSummaries.count() == alternativeInstructions.count()) {
            for (int i = 0; i < alternativeSummaries.count(); ++i) {
                route = constructRoute(alternativeGeometries.at(i).toString().toLatin1(),
                                       alternativeInstructions.at(i).toArray(),
                                       alternativeSummaries.at(i).toObject());
                //routes.append(route);
            }
        }

        setRoutes(routes);

        setFinished(true);
    } else {
        setError(QGeoRouteReply::ParseError, QStringLiteral("Couldn't parse json."));
    }

    m_reply->deleteLater();
    m_reply = 0;
}

void QGeoRouteReplyOsm::networkReplyError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    if (!m_reply)
        return;

    setError(QGeoRouteReply::CommunicationError, m_reply->errorString());

    m_reply->deleteLater();
    m_reply = 0;
}

QT_END_NAMESPACE
