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

#include "qdeclarativegeomaneuver_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype RouteManeuver
    \instantiates QDeclarativeGeoManeuver
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-routing
    \since Qt Location 5.5

    \brief The RouteManeuver type represents the information relevant to the
    point at which two RouteSegments meet.

    RouteSegment instances can be thought of as edges on a routing
    graph, with RouteManeuver instances as optional labels attached to the
    vertices of the graph.

    The most interesting information held in a RouteManeuver instance is
    normally the textual navigation to provide and the position at which to
    provide it, accessible by \l instructionText and \l position respectively.

    \section1 Example

    The following QML snippet demonstrates how to print information about a
    route maneuver:

    \snippet declarative/routing.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/routing.qml RouteManeuver
*/

QDeclarativeGeoManeuver::QDeclarativeGeoManeuver(QObject *parent)
    : QObject(parent)
{
}

QDeclarativeGeoManeuver::QDeclarativeGeoManeuver(const QGeoManeuver &maneuver, QObject *parent)
    : QObject(parent),
      maneuver_(maneuver)
{
}

QDeclarativeGeoManeuver::~QDeclarativeGeoManeuver() {}

/*!
    \qmlproperty bool RouteManeuver::valid

    This read-only property holds whether this maneuver is valid or not.

    Invalid maneuvers are used when there is no information
    that needs to be attached to the endpoint of a QGeoRouteSegment instance.
*/

bool QDeclarativeGeoManeuver::valid() const
{
    return maneuver_.isValid();
}

/*!
    \qmlproperty coordinate RouteManeuver::position

    This read-only property holds where the \l instructionText should be displayed.

*/

QGeoCoordinate QDeclarativeGeoManeuver::position() const
{
    return maneuver_.position();
}

/*!
    \qmlproperty string RouteManeuver::instructionText

    This read-only property holds textual navigation instruction.
*/

QString QDeclarativeGeoManeuver::instructionText() const
{
    return maneuver_.instructionText();
}

/*!
    \qmlproperty enumeration RouteManeuver::direction

    Describes the change in direction associated with the instruction text
    that is associated with a RouteManeuver.

    \list
    \li RouteManeuver.NoDirection - There is no direction associated with the instruction text
    \li RouteManeuver.DirectionForward - The instruction indicates that the direction of travel does not need to change
    \li RouteManeuver.DirectionBearRight - The instruction indicates that the direction of travel should bear to the right
    \li RouteManeuver.DirectionLightRight - The instruction indicates that a light turn to the right is required
    \li RouteManeuver.DirectionRight - The instruction indicates that a turn to the right is required
    \li RouteManeuver.DirectionHardRight - The instruction indicates that a hard turn to the right is required
    \li RouteManeuver.DirectionUTurnRight - The instruction indicates that a u-turn to the right is required
    \li RouteManeuver.DirectionUTurnLeft - The instruction indicates that a u-turn to the left is required
    \li RouteManeuver.DirectionHardLeft - The instruction indicates that a hard turn to the left is required
    \li RouteManeuver.DirectionLeft - The instruction indicates that a turn to the left is required
    \li RouteManeuver.DirectionLightLeft - The instruction indicates that a light turn to the left is required
    \li RouteManeuver.DirectionBearLeft - The instruction indicates that the direction of travel should bear to the left
    \endlist
*/

QDeclarativeGeoManeuver::Direction QDeclarativeGeoManeuver::direction() const
{
    return QDeclarativeGeoManeuver::Direction(maneuver_.direction());
}

/*!
    \qmlproperty int RouteManeuver::timeToNextInstruction

    This read-only property holds the estimated time it will take to travel
    from the point at which the associated instruction was issued and the
    point that the next instruction should be issued, in seconds.
*/

int QDeclarativeGeoManeuver::timeToNextInstruction() const
{
    return maneuver_.timeToNextInstruction();
}

/*!
    \qmlproperty real RouteManeuver::distanceToNextInstruction

    This read-only property holds the distance, in meters, between the point at which
    the associated instruction was issued and the point that the next instruction should
    be issued.
*/

qreal QDeclarativeGeoManeuver::distanceToNextInstruction() const
{
    return maneuver_.distanceToNextInstruction();
}

/*!
    \qmlproperty coordinate RouteManeuver::waypoint

    This property holds the waypoint associated with this maneuver.
    All maneuvers do not have a waypoint associated with them, this
    can be checked with \l waypointValid.

*/

QGeoCoordinate QDeclarativeGeoManeuver::waypoint() const
{
    return maneuver_.waypoint();
}

/*!
    \qmlproperty bool RouteManeuver::waypointValid

    This read-only property holds whether this \l waypoint, associated with this
    maneuver, is valid or not.
*/

bool QDeclarativeGeoManeuver::waypointValid() const
{
    return maneuver_.waypoint().isValid();
}

#include "moc_qdeclarativegeomaneuver_p.cpp"

QT_END_NAMESPACE
