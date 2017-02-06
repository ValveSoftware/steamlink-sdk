/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "qgeorouteparserosrmv5_p.h"
#include "qgeorouteparser_p_p.h"
#include "qgeoroutesegment.h"
#include "qgeomaneuver.h"

#include <QtCore/private/qobject_p.h>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrlQuery>
#include <QtPositioning/private/qlocationutils_p.h>

QT_BEGIN_NAMESPACE

static QList<QGeoCoordinate> decodePolyline(const QString &polylineString)
{
    QList<QGeoCoordinate> path;
    if (polylineString.isEmpty())
        return path;

    QByteArray data = polylineString.toLatin1();

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
            coord.setLatitude(coord.latitude() + (double)diff/1e5);
        } else {
            coord.setLongitude(coord.longitude() + (double)diff/1e5);
            path.append(coord);
        }

        parsingLatitude = !parsingLatitude;

        value = 0;
        shift = 0;
    }

    return path;
}

static QString cardinalDirection4(QLocationUtils::CardinalDirection direction)
{
    switch (direction) {
        case QLocationUtils::CardinalN:
            //: Always used in "Head %1 [onto <street name>]"
            return QGeoRouteParserOsrmV5::tr("North");
        case QLocationUtils::CardinalE:
            return QGeoRouteParserOsrmV5::tr("East");
        case QLocationUtils::CardinalS:
            return QGeoRouteParserOsrmV5::tr("South");
        case QLocationUtils::CardinalW:
            return QGeoRouteParserOsrmV5::tr("West");
        default:
            return QString();
    }
}

static QString exitOrdinal(int exit)
{
    static QList<QString> ordinals;

    if (!ordinals.size()) {
        ordinals.append(QStringLiteral(""));
        //: always used in " and take the %1 exit [onto <street name>]"
        ordinals.append(QGeoRouteParserOsrmV5::tr("first", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("second", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("third", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("fourth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("fifth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("sixth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("seventh", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("eighth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("ninth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("tenth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("eleventh", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("twelfth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("thirteenth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("fourteenth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("fifteenth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("sixteenth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("seventeenth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("eighteenth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("nineteenth", "roundabout exit"));
        ordinals.append(QGeoRouteParserOsrmV5::tr("twentieth", "roundabout exit"));
    };

    if (exit < 1 || exit > ordinals.size())
        return QString();
    return ordinals[exit];
}

static QString exitDirection(int exit, const QString &wayName)
{
    /*: Always appended to one of the following strings:
        - "Enter the roundabout"
        - "Enter the rotary"
        - "Enter the rotary <rotaryname>"
    */
    static QString directionExit = QGeoRouteParserOsrmV5::tr(" and take the %1 exit");
    static QString directionExitOnto = QGeoRouteParserOsrmV5::tr(" and take the %1 exit onto %2");

    if (exit < 1 || exit > 20)
        return QString();
    if (wayName.isEmpty())
        return directionExit.arg(exitOrdinal(exit));
    else
        return directionExitOnto.arg(exitOrdinal(exit), wayName);
}

static QString instructionArrive(QGeoManeuver::InstructionDirection direction)
{
    switch (direction) {
        case QGeoManeuver::DirectionForward:
            return QGeoRouteParserOsrmV5::tr("You have arrived at your destination, straight ahead");
        case QGeoManeuver::DirectionUTurnLeft:
        case QGeoManeuver::DirectionHardLeft:
        case QGeoManeuver::DirectionLeft:
        case QGeoManeuver::DirectionLightLeft:
        case QGeoManeuver::DirectionBearLeft:
            return QGeoRouteParserOsrmV5::tr("You have arrived at your destination, on the left");
        case QGeoManeuver::DirectionUTurnRight:
        case QGeoManeuver::DirectionHardRight:
        case QGeoManeuver::DirectionRight:
        case QGeoManeuver::DirectionLightRight:
        case QGeoManeuver::DirectionBearRight:
            return QGeoRouteParserOsrmV5::tr("You have arrived at your destination, on the right");
        default:
            return QGeoRouteParserOsrmV5::tr("You have arrived at your destination");
    }
}

static QString instructionContinue(const QString &wayName, QGeoManeuver::InstructionDirection direction)
{
    switch (direction) {
        case QGeoManeuver::DirectionForward:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Continue straight");
            else
                return QGeoRouteParserOsrmV5::tr("Continue straight on %1").arg(wayName);
        case QGeoManeuver::DirectionHardLeft:
        case QGeoManeuver::DirectionLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Continue left");
            else
                return QGeoRouteParserOsrmV5::tr("Continue left onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightLeft:
        case QGeoManeuver::DirectionBearLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Continue slightly left");
            else
                return QGeoRouteParserOsrmV5::tr("Continue slightly left on %1").arg(wayName);
        case QGeoManeuver::DirectionHardRight:
        case QGeoManeuver::DirectionRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Continue right");
            else
                return QGeoRouteParserOsrmV5::tr("Continue right onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightRight:
        case QGeoManeuver::DirectionBearRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Continue slightly right");
            else
                return QGeoRouteParserOsrmV5::tr("Continue slightly right on %1").arg(wayName);
        case QGeoManeuver::DirectionUTurnLeft:
        case QGeoManeuver::DirectionUTurnRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Make a U-turn");
            else
                return QGeoRouteParserOsrmV5::tr("Make a U-turn onto %1").arg(wayName);
        default:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Continue");
            else
                return QGeoRouteParserOsrmV5::tr("Continue on %1").arg(wayName);
    }
}

static QString instructionDepart(const QJsonObject &maneuver, const QString &wayName)
{
    double bearing = maneuver.value(QLatin1String("bearing_after")).toDouble(-1.0);
    if (bearing >= 0.0) {
        if (wayName.isEmpty())
            //: %1 is "North", "South", "East" or "West"
            return QGeoRouteParserOsrmV5::tr("Head %1").arg(cardinalDirection4(QLocationUtils::azimuthToCardinalDirection4(bearing)));
        else
            return QGeoRouteParserOsrmV5::tr("Head %1 onto %2").arg(cardinalDirection4(QLocationUtils::azimuthToCardinalDirection4(bearing)), wayName);
    } else {
        if (wayName.isEmpty())
            return QGeoRouteParserOsrmV5::tr("Depart");
        else
            return QGeoRouteParserOsrmV5::tr("Depart onto %1").arg(wayName);
    }
}

static QString instructionEndOfRoad(const QString &wayName, QGeoManeuver::InstructionDirection direction)
{
    switch (direction) {
        case QGeoManeuver::DirectionHardLeft:
        case QGeoManeuver::DirectionLeft:
        case QGeoManeuver::DirectionLightLeft:
        case QGeoManeuver::DirectionBearLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the end of the road, turn left");
            else
                return QGeoRouteParserOsrmV5::tr("At the end of the road, turn left onto %1").arg(wayName);
        case QGeoManeuver::DirectionHardRight:
        case QGeoManeuver::DirectionRight:
        case QGeoManeuver::DirectionLightRight:
        case QGeoManeuver::DirectionBearRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the end of the road, turn right");
            else
                return QGeoRouteParserOsrmV5::tr("At the end of the road, turn right onto %1").arg(wayName);
        case QGeoManeuver::DirectionUTurnLeft:
        case QGeoManeuver::DirectionUTurnRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the end of the road, make a U-turn");
            else
                return QGeoRouteParserOsrmV5::tr("At the end of the road, make a U-turn onto %1").arg(wayName);
        case QGeoManeuver::DirectionForward:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the end of the road, continue straight");
            else
                return QGeoRouteParserOsrmV5::tr("At the end of the road, continue straight onto %1").arg(wayName);
        default:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the end of the road, continue");
            else
                return QGeoRouteParserOsrmV5::tr("At the end of the road, continue onto %1").arg(wayName);
    }
}

static QString instructionFerry(const QString &wayName)
{
    QString instruction = QGeoRouteParserOsrmV5::tr("Take the ferry");
    if (!wayName.isEmpty())
        instruction += QLatin1String(" [") + wayName + QLatin1Char(']');

    return instruction;
}

static QString instructionFork(const QString &wayName, QGeoManeuver::InstructionDirection direction)
{
    switch (direction) {
        case QGeoManeuver::DirectionHardLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the fork, take a sharp left");
            else
                return QGeoRouteParserOsrmV5::tr("At the fork, take a sharp left onto %1").arg(wayName);
        case QGeoManeuver::DirectionLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the fork, turn left");
            else
                return QGeoRouteParserOsrmV5::tr("At the fork, turn left onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightLeft:
        case QGeoManeuver::DirectionBearLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the fork, keep left");
            else
                return QGeoRouteParserOsrmV5::tr("At the fork, keep left onto %1").arg(wayName);
        case QGeoManeuver::DirectionHardRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the fork, take a sharp right");
            else
                return QGeoRouteParserOsrmV5::tr("At the fork, take a sharp right onto %1").arg(wayName);
        case QGeoManeuver::DirectionRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the fork, turn right");
            else
                return QGeoRouteParserOsrmV5::tr("At the fork, turn right onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightRight:
        case QGeoManeuver::DirectionBearRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the fork, keep right");
            else
                return QGeoRouteParserOsrmV5::tr("At the fork, keep right onto %1").arg(wayName);
        case QGeoManeuver::DirectionUTurnLeft:
        case QGeoManeuver::DirectionUTurnRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Make a U-turn");
            else
                return QGeoRouteParserOsrmV5::tr("Make a U-turn onto %1").arg(wayName);
        case QGeoManeuver::DirectionForward:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the fork, continue straight ahead");
            else
                return QGeoRouteParserOsrmV5::tr("At the fork, continue straight ahead onto %1").arg(wayName);
        default:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the fork, continue");
            else
                return QGeoRouteParserOsrmV5::tr("At the fork, continue onto %1").arg(wayName);
    }
}

static QString instructionMerge(const QString &wayName, QGeoManeuver::InstructionDirection direction)
{
    switch (direction) {
        case QGeoManeuver::DirectionUTurnLeft:
        case QGeoManeuver::DirectionHardLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Merge sharply left");
            else
                return QGeoRouteParserOsrmV5::tr("Merge sharply left onto %1").arg(wayName);
        case QGeoManeuver::DirectionLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Merge left");
            else
                return QGeoRouteParserOsrmV5::tr("Merge left onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightLeft:
        case QGeoManeuver::DirectionBearLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Merge slightly left");
            else
                return QGeoRouteParserOsrmV5::tr("Merge slightly left on %1").arg(wayName);
        case QGeoManeuver::DirectionUTurnRight:
        case QGeoManeuver::DirectionHardRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Merge sharply right");
            else
                return QGeoRouteParserOsrmV5::tr("Merge sharply right onto %1").arg(wayName);
        case QGeoManeuver::DirectionRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Merge right");
            else
                return QGeoRouteParserOsrmV5::tr("Merge right onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightRight:
        case QGeoManeuver::DirectionBearRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Merge slightly right");
            else
                return QGeoRouteParserOsrmV5::tr("Merge slightly right on %1").arg(wayName);
        case QGeoManeuver::DirectionForward:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Merge straight");
            else
                return QGeoRouteParserOsrmV5::tr("Merge straight on %1").arg(wayName);
        default:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Merge");
            else
                return QGeoRouteParserOsrmV5::tr("Merge onto %1").arg(wayName);
    }
}

static QString instructionNewName(const QString &wayName, QGeoManeuver::InstructionDirection direction)
{
    switch (direction) {
        case QGeoManeuver::DirectionHardLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Take a sharp left");
            else
                return QGeoRouteParserOsrmV5::tr("Take a sharp left onto %1").arg(wayName);
        case QGeoManeuver::DirectionLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Turn left");
            else
                return QGeoRouteParserOsrmV5::tr("Turn left onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightLeft:
        case QGeoManeuver::DirectionBearLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Continue slightly left");
            else
                return QGeoRouteParserOsrmV5::tr("Continue slightly left onto %1").arg(wayName);
        case QGeoManeuver::DirectionHardRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Take a sharp right");
            else
                return QGeoRouteParserOsrmV5::tr("Take a sharp right onto %1").arg(wayName);
        case QGeoManeuver::DirectionRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Turn right");
            else
                return QGeoRouteParserOsrmV5::tr("Turn right onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightRight:
        case QGeoManeuver::DirectionBearRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Contine slightly right");
            else
                return QGeoRouteParserOsrmV5::tr("Contine slightly right onto %1").arg(wayName);
        case QGeoManeuver::DirectionUTurnLeft:
        case QGeoManeuver::DirectionUTurnRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Make a U-turn");
            else
                return QGeoRouteParserOsrmV5::tr("Make a U-turn onto %1").arg(wayName);
        case QGeoManeuver::DirectionForward:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Continue straight");
            else
                return QGeoRouteParserOsrmV5::tr("Continue straight onto %1").arg(wayName);
        default:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Continue");
            else
                return QGeoRouteParserOsrmV5::tr("Continue onto %1").arg(wayName);
    }
}

static QString instructionNotification(const QString &wayName, QGeoManeuver::InstructionDirection direction)
{
    switch (direction) {
        case QGeoManeuver::DirectionUTurnLeft:
        case QGeoManeuver::DirectionHardLeft:
        case QGeoManeuver::DirectionLeft:
        case QGeoManeuver::DirectionLightLeft:
        case QGeoManeuver::DirectionBearLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Continue on the left");
            else
                return QGeoRouteParserOsrmV5::tr("Continue on the left on %1").arg(wayName);
        case QGeoManeuver::DirectionUTurnRight:
        case QGeoManeuver::DirectionHardRight:
        case QGeoManeuver::DirectionRight:
        case QGeoManeuver::DirectionLightRight:
        case QGeoManeuver::DirectionBearRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Contine on the right");
            else
                return QGeoRouteParserOsrmV5::tr("Contine on the right on %1").arg(wayName);
        case QGeoManeuver::DirectionForward:
        default:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Continue");
            else
                return QGeoRouteParserOsrmV5::tr("Continue on %1").arg(wayName);
    }
}

static QString instructionOffRamp(const QString &wayName, QGeoManeuver::InstructionDirection direction)
{
    switch (direction) {
        case QGeoManeuver::DirectionUTurnLeft:
        case QGeoManeuver::DirectionHardLeft:
        case QGeoManeuver::DirectionLeft:
        case QGeoManeuver::DirectionLightLeft:
        case QGeoManeuver::DirectionBearLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Take the ramp on the left");
            else
                return QGeoRouteParserOsrmV5::tr("Take the ramp on the left onto %1").arg(wayName);
        case QGeoManeuver::DirectionUTurnRight:
        case QGeoManeuver::DirectionHardRight:
        case QGeoManeuver::DirectionRight:
        case QGeoManeuver::DirectionLightRight:
        case QGeoManeuver::DirectionBearRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Take the ramp on the right");
            else
                return QGeoRouteParserOsrmV5::tr("Take the ramp on the right onto %1").arg(wayName);
        case QGeoManeuver::DirectionForward:
        default:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Take the ramp");
            else
                return QGeoRouteParserOsrmV5::tr("Take the ramp onto %1").arg(wayName);
    }
}

static QString instructionOnRamp(const QString &wayName, QGeoManeuver::InstructionDirection direction)
{
    return instructionOffRamp(wayName, direction);
}

static QString instructionPushingBike(const QString &wayName)
{
    if (wayName.isEmpty())
        return QGeoRouteParserOsrmV5::tr("Get off the bike and push");
    else
        return QGeoRouteParserOsrmV5::tr("Get off the bike and push onto %1").arg(wayName);
}

static QString instructionRotary(const QJsonObject &step, const QJsonObject &maneuver, const QString &wayName)
{
    QString instruction;
    QString rotaryName = step.value(QLatin1String("rotary_name")).toString();
    //QString modifier = maneuver.value(QLatin1String("modifier")).toString(); // Apparently not used for rotaries
    int exit = maneuver.value(QLatin1String("exit")).toInt(0);

    //: This string will be prepended to " and take the <nth> exit [onto <streetname>]
    instruction += QGeoRouteParserOsrmV5::tr("Enter the rotary");
    if (!rotaryName.isEmpty())
        instruction +=  QLatin1Char(' ') + rotaryName;
    instruction += exitDirection(exit, wayName);
    return instruction;
}

static QString instructionRoundabout(const QJsonObject &maneuver, const QString &wayName)
{
    QString instruction;
    //QString modifier = maneuver.value(QLatin1String("modifier")).toString(); // Apparently not used for rotaries
    int exit = maneuver.value(QLatin1String("exit")).toInt(0);

    //: This string will be prepended to " and take the <nth> exit [onto <streetname>]
    instruction += QGeoRouteParserOsrmV5::tr("Enter the roundabout");
    instruction += exitDirection(exit, wayName);
    return instruction;
}

static QString instructionRoundaboutTurn(const QString &wayName, QGeoManeuver::InstructionDirection direction)
{
    switch (direction) {
        case QGeoManeuver::DirectionForward:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the roundabout, continue straight");
            else
                return QGeoRouteParserOsrmV5::tr("At the roundabout, continue straight on %1").arg(wayName);
        case QGeoManeuver::DirectionHardLeft:
        case QGeoManeuver::DirectionLeft:
        case QGeoManeuver::DirectionLightLeft:
        case QGeoManeuver::DirectionBearLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the roundabout, turn left");
            else
                return QGeoRouteParserOsrmV5::tr("At the roundabout, turn left onto %1").arg(wayName);
        case QGeoManeuver::DirectionHardRight:
        case QGeoManeuver::DirectionRight:
        case QGeoManeuver::DirectionLightRight:
        case QGeoManeuver::DirectionBearRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the roundabout, turn right");
            else
                return QGeoRouteParserOsrmV5::tr("At the roundabout, turn right onto %1").arg(wayName);
        case QGeoManeuver::DirectionUTurnLeft:
        case QGeoManeuver::DirectionUTurnRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the roundabout, turn around");
            else
                return QGeoRouteParserOsrmV5::tr("At the roundabout, turn around onto %1").arg(wayName);
        default:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("At the roundabout, continue");
            else
                return QGeoRouteParserOsrmV5::tr("At the roundabout, continue onto %1").arg(wayName);
    }
}

static QString instructionTrain(const QString &wayName)
{
    QString instruction = QGeoRouteParserOsrmV5::tr("Take the train");
    if (!wayName.isEmpty())
        instruction += QLatin1String(" [") + wayName + QLatin1Char(']');

    return instruction;
}

static QString instructionTurn(const QString &wayName, QGeoManeuver::InstructionDirection direction)
{
    switch (direction) {
        case QGeoManeuver::DirectionForward:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Go straight");
            else
                return QGeoRouteParserOsrmV5::tr("Go straight onto %1").arg(wayName);
        case QGeoManeuver::DirectionHardLeft:
        case QGeoManeuver::DirectionLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Turn left");
            else
                return QGeoRouteParserOsrmV5::tr("Turn left onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightLeft:
        case QGeoManeuver::DirectionBearLeft:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Turn slightly left");
            else
                return QGeoRouteParserOsrmV5::tr("Turn slightly left onto %1").arg(wayName);
        case QGeoManeuver::DirectionHardRight:
        case QGeoManeuver::DirectionRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Turn right");
            else
                return QGeoRouteParserOsrmV5::tr("Turn right onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightRight:
        case QGeoManeuver::DirectionBearRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Turn slightly right");
            else
                return QGeoRouteParserOsrmV5::tr("Turn slightly right onto %1").arg(wayName);
        case QGeoManeuver::DirectionUTurnLeft:
        case QGeoManeuver::DirectionUTurnRight:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Make a U-turn");
            else
                return QGeoRouteParserOsrmV5::tr("Make a U-turn onto %1").arg(wayName);
        default:
            if (wayName.isEmpty())
                return QGeoRouteParserOsrmV5::tr("Turn");
            else
                return QGeoRouteParserOsrmV5::tr("Turn onto %1").arg(wayName);
    }
}

static QString instructionUseLane(const QJsonObject &maneuver, const QString &wayName, QGeoManeuver::InstructionDirection direction)
{
    QString laneTypes = maneuver.value(QLatin1String("laneTypes")).toString();
    QString laneInstruction;
    if (laneTypes == QLatin1String("xo") || laneTypes == QLatin1String("xoo") || laneTypes == QLatin1String("xxo"))
        //: "and <instruction direction> [onto <street name>] will be appended to this string. E.g., "Keep right and make a sharp left"
        laneInstruction = QLatin1String("Keep right");
    else if (laneTypes == QLatin1String("ox") || laneTypes == QLatin1String("oox") || laneTypes == QLatin1String("oxx"))
        laneInstruction = QLatin1String("Keep left");
    else if (laneTypes == QLatin1String("xox"))
        laneInstruction = QLatin1String("Use the middle lane");
    else if (laneTypes == QLatin1String("oxo"))
        laneInstruction = QLatin1String("Use the left or the right lane");

    if (laneInstruction.isEmpty()) {
        if (wayName.isEmpty())
            return QGeoRouteParserOsrmV5::tr("Continue straight");
        else
            return QGeoRouteParserOsrmV5::tr("Continue straight onto %1").arg(wayName);
    }

    switch (direction) {
        case QGeoManeuver::DirectionForward:
            if (wayName.isEmpty())
                //: This string will be prepended with lane instructions. E.g., "Use the left or the right lane and continue straight"
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and continue straight");
            else
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and continue straight onto %1").arg(wayName);
        case QGeoManeuver::DirectionHardLeft:
            if (wayName.isEmpty())
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and make a sharp left");
            else
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and make a sharp left onto %1").arg(wayName);
        case QGeoManeuver::DirectionLeft:
            if (wayName.isEmpty())
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and turn left");
            else
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and turn left onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightLeft:
        case QGeoManeuver::DirectionBearLeft:
            if (wayName.isEmpty())
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and make a slight left");
            else
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and make a slight left onto %1").arg(wayName);
        case QGeoManeuver::DirectionHardRight:
            if (wayName.isEmpty())
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and make a sharp right");
            else
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and make a sharp right onto %1").arg(wayName);
        case QGeoManeuver::DirectionRight:
            if (wayName.isEmpty())
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and turn right");
            else
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and turn right onto %1").arg(wayName);
        case QGeoManeuver::DirectionLightRight:
        case QGeoManeuver::DirectionBearRight:
            if (wayName.isEmpty())
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and make a slight right");
            else
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and make a slight right onto %1").arg(wayName);
        case QGeoManeuver::DirectionUTurnLeft:
        case QGeoManeuver::DirectionUTurnRight:
            if (wayName.isEmpty())
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and make a U-turn");
            else
                return laneInstruction + QGeoRouteParserOsrmV5::tr(" and make a U-turn onto %1").arg(wayName);
        default:
            return laneInstruction;
    }
}

static QString instructionText(const QJsonObject &step, const QJsonObject &maneuver, QGeoManeuver::InstructionDirection direction) {
    QString modifier;
    if (maneuver.value(QLatin1String("modifier")).isString())
        modifier = maneuver.value(QLatin1String("modifier")).toString();
    QString maneuverType;
    if (maneuver.value(QLatin1String("type")).isString())
        maneuverType = maneuver.value(QLatin1String("type")).toString();
    QString wayName = QStringLiteral("unknown street");
    if (step.value(QLatin1String("name")).isString())
        wayName = step.value(QLatin1String("name")).toString();


    if (maneuverType == QLatin1String("arrive"))
        return instructionArrive(direction);
    else if (maneuverType == QLatin1String("continue"))
        return instructionContinue(wayName, direction);
    else if (maneuverType == QLatin1String("depart"))
        return instructionDepart(maneuver, wayName);
    else if (maneuverType == QLatin1String("end of road"))
        return instructionEndOfRoad(wayName, direction);
    else if (maneuverType == QLatin1String("ferry"))
        return instructionFerry(wayName);
    else if (maneuverType == QLatin1String("fork"))
        return instructionFork(wayName, direction);
    else if (maneuverType == QLatin1String("merge"))
        return instructionMerge(wayName, direction);
    else if (maneuverType == QLatin1String("new name"))
        return instructionNewName(wayName, direction);
    else if (maneuverType == QLatin1String("notification"))
        return instructionNotification(wayName, direction);
    else if (maneuverType == QLatin1String("off ramp"))
        return instructionOffRamp(wayName, direction);
    else if (maneuverType == QLatin1String("on ramp"))
        return instructionOnRamp(wayName, direction);
    else if (maneuverType == QLatin1String("pushing bike"))
        return instructionPushingBike(wayName);
    else if (maneuverType == QLatin1String("rotary"))
        return instructionRotary(step, maneuver, wayName);
    else if (maneuverType == QLatin1String("roundabout"))
        return instructionRoundabout(maneuver, wayName);
    else if (maneuverType == QLatin1String("roundabout turn"))
        return instructionRoundaboutTurn(wayName, direction);
    else if (maneuverType == QLatin1String("train"))
        return instructionTrain(wayName);
    else if (maneuverType == QLatin1String("turn"))
        return instructionTurn(wayName, direction);
    else if (maneuverType == QLatin1String("use lane"))
        return instructionUseLane(maneuver, wayName, direction);
    else
        return maneuverType + QLatin1String(" to/onto ") + wayName;
}

static QGeoManeuver::InstructionDirection instructionDirection(const QJsonObject &maneuver)
{
    QString modifier;
    if (maneuver.value(QLatin1String("modifier")).isString())
        modifier = maneuver.value(QLatin1String("modifier")).toString();

    if (modifier.isEmpty())
        return QGeoManeuver::NoDirection;
    else if (modifier == QLatin1String("straight"))
        return QGeoManeuver::DirectionForward;
    else if (modifier == QLatin1String("right"))
        return QGeoManeuver::DirectionRight;
    else if (modifier == QLatin1String("sharp right"))
        return QGeoManeuver::DirectionHardRight;
    else if (modifier == QLatin1String("slight right"))
        return QGeoManeuver::DirectionLightRight;
    else if (modifier == QLatin1String("uturn"))
        return QGeoManeuver::DirectionUTurnRight;
    else if (modifier == QLatin1String("left"))
        return QGeoManeuver::DirectionLeft;
    else if (modifier == QLatin1String("sharp left"))
        return QGeoManeuver::DirectionHardLeft;
    else if (modifier == QLatin1String("slight left"))
        return QGeoManeuver::DirectionLightLeft;
    else
        return QGeoManeuver::NoDirection;
}

static QGeoRouteSegment parseStep(const QJsonObject &step) {
    // OSRM Instructions documentation: https://github.com/Project-OSRM/osrm-text-instructions/blob/master/instructions.json
    QGeoRouteSegment segment;
    if (!step.value(QLatin1String("maneuver")).isObject())
        return segment;
    QJsonObject maneuver = step.value(QLatin1String("maneuver")).toObject();
    if (!step.value(QLatin1String("duration")).isDouble())
        return segment;
    if (!step.value(QLatin1String("distance")).isDouble())
        return segment;
    if (!step.value(QLatin1String("intersections")).isArray())
        return segment;
    if (!maneuver.value(QLatin1String("location")).isArray())
        return segment;

    double time = step.value(QLatin1String("duration")).toDouble();
    double distance = step.value(QLatin1String("distance")).toDouble();

    QJsonArray position = maneuver.value(QLatin1String("location")).toArray();
    if (position.isEmpty())
        return segment;
    double latitude = position[1].toDouble();
    double longitude = position[0].toDouble();
    QGeoCoordinate coord(latitude, longitude);

    QString geometry = step.value(QLatin1String("geometry")).toString();
    QList<QGeoCoordinate> path = decodePolyline(geometry);

    QGeoManeuver geoManeuver;
    geoManeuver.setDirection(instructionDirection(maneuver));
    geoManeuver.setDistanceToNextInstruction(distance);
    geoManeuver.setTimeToNextInstruction(time);
    geoManeuver.setInstructionText(instructionText(step, maneuver, geoManeuver.direction()));
    geoManeuver.setPosition(coord);
    geoManeuver.setWaypoint(coord);

    segment.setDistance(distance);
    segment.setPath(path);
    segment.setTravelTime(time);
    segment.setManeuver(geoManeuver);
    return segment;
}

class QGeoRouteParserOsrmV5Private :  public QGeoRouteParserPrivate
{
    Q_DECLARE_PUBLIC(QGeoRouteParserOsrmV5)
public:
    QGeoRouteParserOsrmV5Private();
    virtual ~QGeoRouteParserOsrmV5Private();

    QGeoRouteReply::Error parseReply(QList<QGeoRoute> &routes, QString &errorString, const QByteArray &reply) const Q_DECL_OVERRIDE;
    QUrl requestUrl(const QGeoRouteRequest &request, const QString &prefix) const Q_DECL_OVERRIDE;
};

QGeoRouteParserOsrmV5Private::QGeoRouteParserOsrmV5Private() : QGeoRouteParserPrivate()
{
}

QGeoRouteParserOsrmV5Private::~QGeoRouteParserOsrmV5Private()
{
}

QGeoRouteReply::Error QGeoRouteParserOsrmV5Private::parseReply(QList<QGeoRoute> &routes, QString &errorString, const QByteArray &reply) const
{
    // OSRM v5 specs: https://github.com/Project-OSRM/osrm-backend/blob/master/docs/http.md
    QJsonDocument document = QJsonDocument::fromJson(reply);
    if (document.isObject()) {
        QJsonObject object = document.object();

        QString status = object.value(QStringLiteral("code")).toString();
        if (status != QLatin1String("Ok")) {
            errorString = status;
            return QGeoRouteReply::UnknownError;
        }
        if (!object.value(QLatin1String("routes")).isArray()) {
            errorString = QLatin1String("No routes found");
            return QGeoRouteReply::ParseError;
        }

        QJsonArray osrmRoutes = object.value(QLatin1String("routes")).toArray();
        foreach (const QJsonValue &r, osrmRoutes) {
            if (!r.isObject())
                continue;
            QJsonObject route = r.toObject();
            if (!route.value(QLatin1String("legs")).isArray())
                continue;
            if (!route.value(QLatin1String("duration")).isDouble())
                continue;
            if (!route.value(QLatin1String("distance")).isDouble())
                continue;

            double distance = route.value(QLatin1String("distance")).toDouble();
            double travelTime = route.value(QLatin1String("duration")).toDouble();
            bool error = false;
            QList<QGeoRouteSegment> segments;

            QJsonArray legs = route.value(QLatin1String("legs")).toArray();
            foreach (const QJsonValue &l, legs) {
                if (!l.isObject()) { // invalid leg record
                    error = true;
                    break;
                }
                QJsonObject leg = l.toObject();
                if (!leg.value(QLatin1String("steps")).isArray()) { // Invalid steps field
                    error = true;
                    break;
                }
                QJsonArray steps = leg.value(QLatin1String("steps")).toArray();
                foreach (const QJsonValue &s, steps) {
                    if (!s.isObject()) {
                        error = true;
                        break;
                    }
                    QGeoRouteSegment segment = parseStep(s.toObject());
                    if (segment.isValid()) {
                        segments.append(segment);
                    } else {
                        error = true;
                        break;
                    }
                }
                if (error)
                    break;
            }

            if (!error) {
                QList<QGeoCoordinate> path;
                foreach (const QGeoRouteSegment &s, segments)
                    path.append(s.path());

                for (int i = segments.size() - 1; i > 0; --i)
                    segments[i-1].setNextRouteSegment(segments[i]);

                QGeoRoute r;
                r.setDistance(distance);
                r.setTravelTime(travelTime);
                if (!path.isEmpty()) {
                    r.setPath(path);
                    r.setFirstRouteSegment(segments.first());
                }
                //r.setTravelMode(QGeoRouteRequest::CarTravel); // The only one supported by OSRM demo service, but other OSRM servers might do cycle or pedestrian too
                routes.append(r);
            }
        }

        // setError(QGeoRouteReply::NoError, status);  // can't do this, or NoError is emitted and does damages
        return QGeoRouteReply::NoError;
    } else {
        errorString = QStringLiteral("Couldn't parse json.");
        return QGeoRouteReply::ParseError;
    }
}

QUrl QGeoRouteParserOsrmV5Private::requestUrl(const QGeoRouteRequest &request, const QString &prefix) const
{
    QString routingUrl = prefix;
    int notFirst = 0;
    foreach (const QGeoCoordinate &c, request.waypoints()) {
        if (notFirst)
            routingUrl.append(QLatin1Char(';'));
        routingUrl.append(QString::number(c.longitude())).append(QLatin1Char(',')).append(QString::number(c.latitude()));
        ++notFirst;
    }

    QUrl url(routingUrl);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("overview"), QStringLiteral("full"));
    query.addQueryItem(QStringLiteral("steps"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("geometries"), QStringLiteral("polyline"));
    query.addQueryItem(QStringLiteral("alternatives"), QStringLiteral("true"));
    url.setQuery(query);
    return url;
}

QGeoRouteParserOsrmV5::QGeoRouteParserOsrmV5(QObject *parent) : QGeoRouteParser(*new QGeoRouteParserOsrmV5Private(), parent)
{
}

QGeoRouteParserOsrmV5::~QGeoRouteParserOsrmV5()
{
}

QT_END_NAMESPACE
