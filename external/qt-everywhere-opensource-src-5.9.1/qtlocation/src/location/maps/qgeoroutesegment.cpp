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

#include "qgeoroutesegment.h"
#include "qgeoroutesegment_p.h"

#include "qgeocoordinate.h"
#include <QDateTime>

QT_BEGIN_NAMESPACE

/*!
    \class QGeoRouteSegment
    \inmodule QtLocation
    \ingroup QtLocation-routing
    \since 5.6

    \brief The QGeoRouteSegment class represents a segment of a route.

    A QGeoRouteSegment instance has information about the physical layout
    of the route segment, the length of the route and estimated time required
    to traverse the route segment and an optional QGeoManeuver associated with
    the end of the route segment.

    QGeoRouteSegment instances can be thought of as edges on a routing
    graph, with QGeoManeuver instances as optional labels attached to the
    vertices of the graph.
*/

/*!
    Constructs an invalid route segment object.

    The route segment will remain invalid until one of setNextRouteSegment(),
    setTravelTime(), setDistance(), setPath() or setManeuver() is called.
*/
QGeoRouteSegment::QGeoRouteSegment()
    : d_ptr(new QGeoRouteSegmentPrivate()) {}

/*!
    Constructs a route segment object from the contents of \a other.
*/
QGeoRouteSegment::QGeoRouteSegment(const QGeoRouteSegment &other)
    : d_ptr(other.d_ptr) {}

/*!
    \internal
*/
QGeoRouteSegment::QGeoRouteSegment(QExplicitlySharedDataPointer<QGeoRouteSegmentPrivate> &d_ptr)
    : d_ptr(d_ptr) {}

/*!
    Destroys this route segment object.
*/
QGeoRouteSegment::~QGeoRouteSegment() {}


/*!
    Assigns \a other to this route segment object and then returns a
    reference to this route segment object.
*/
QGeoRouteSegment &QGeoRouteSegment::operator= (const QGeoRouteSegment & other)
{
    if (this == &other)
        return *this;

    d_ptr = other.d_ptr;
    return *this;
}

/*!
    Returns whether this route segment and \a other are equal.

    The value of nextRouteSegment() is not considered in the comparison.
*/
bool QGeoRouteSegment::operator ==(const QGeoRouteSegment &other) const
{
    return ( (d_ptr.constData() == other.d_ptr.constData())
            || (*d_ptr) == (*other.d_ptr));
}

/*!
    Returns whether this route segment and \a other are not equal.

    The value of nextRouteSegment() is not considered in the comparison.
*/
bool QGeoRouteSegment::operator !=(const QGeoRouteSegment &other) const
{
    return !(operator==(other));
}

/*!
    Returns whether this route segment is valid or not.

    If nextRouteSegment() is called on the last route segment of a route, the
    returned value will be an invalid route segment.
*/
bool QGeoRouteSegment::isValid() const
{
    return d_ptr->valid;
}

/*!
    Sets the next route segment in the route to \a routeSegment.
*/
void QGeoRouteSegment::setNextRouteSegment(const QGeoRouteSegment &routeSegment)
{
    d_ptr->valid = true;
    d_ptr->nextSegment = routeSegment.d_ptr;
}

/*!
    Returns the next route segment in the route.

    Will return an invalid route segment if this is the last route
    segment in the route.
*/
QGeoRouteSegment QGeoRouteSegment::nextRouteSegment() const
{
    if (d_ptr->valid && d_ptr->nextSegment)
        return QGeoRouteSegment(d_ptr->nextSegment);

    QGeoRouteSegment segment;
    segment.d_ptr->valid = false;
    return segment;
}

/*!
    Sets the estimated amount of time it will take to traverse this segment of
    the route, in seconds, to \a secs.
*/
void QGeoRouteSegment::setTravelTime(int secs)
{
    d_ptr->valid = true;
    d_ptr->travelTime = secs;
}

/*!
    Returns the estimated amount of time it will take to traverse this segment
    of the route, in seconds.
*/
int QGeoRouteSegment::travelTime() const
{
    return d_ptr->travelTime;
}

/*!
    Sets the distance covered by this segment of the route, in meters, to \a distance.
*/
void QGeoRouteSegment::setDistance(qreal distance)
{
    d_ptr->valid = true;
    d_ptr->distance = distance;
}

/*!
    Returns the distance covered by this segment of the route, in meters.
*/
qreal QGeoRouteSegment::distance() const
{
    return d_ptr->distance;
}

/*!
    Sets the geometric shape of this segment of the route to \a path.

    The coordinates in \a path should be listed in the order in which they
    would be traversed by someone traveling along this segment of the route.
*/
void QGeoRouteSegment::setPath(const QList<QGeoCoordinate> &path)
{
    d_ptr->valid = true;
    d_ptr->path = path;
}

/*!
    Returns the geometric shape of this route segment of the route.

    The coordinates should be listed in the order in which they
    would be traversed by someone traveling along this segment of the route.
*/

QList<QGeoCoordinate> QGeoRouteSegment::path() const
{
    return d_ptr->path;
}

/*!
    Sets the maneuver for this route segment to \a maneuver.
*/
void QGeoRouteSegment::setManeuver(const QGeoManeuver &maneuver)
{
    d_ptr->valid = true;
    d_ptr->maneuver = maneuver;
}

/*!
    Returns the maneuver for this route segment.

    Will return an invalid QGeoManeuver if no information has been attached
    to the endpoint of this route segment.
*/
QGeoManeuver QGeoRouteSegment::maneuver() const
{
    return d_ptr->maneuver;
}

/*******************************************************************************
*******************************************************************************/

QGeoRouteSegmentPrivate::QGeoRouteSegmentPrivate()
    : valid(false),
      travelTime(0),
      distance(0.0) {}

QGeoRouteSegmentPrivate::QGeoRouteSegmentPrivate(const QGeoRouteSegmentPrivate &other)
    : QSharedData(other),
      valid(other.valid),
      travelTime(other.travelTime),
      distance(other.distance),
      path(other.path),
      maneuver(other.maneuver),
      nextSegment(other.nextSegment) {}

QGeoRouteSegmentPrivate::~QGeoRouteSegmentPrivate()
{
    nextSegment.reset();
}

bool QGeoRouteSegmentPrivate::operator ==(const QGeoRouteSegmentPrivate &other) const
{
    return ((valid == other.valid)
            && (travelTime == other.travelTime)
            && (distance == other.distance)
            && (path == other.path)
            && (maneuver == other.maneuver));
}

/*******************************************************************************
*******************************************************************************/

QT_END_NAMESPACE

