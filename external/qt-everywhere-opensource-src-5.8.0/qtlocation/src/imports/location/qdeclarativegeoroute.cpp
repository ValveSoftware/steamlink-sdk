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

#include "qdeclarativegeoroute_p.h"
#include "locationvaluetypehelper_p.h"

#include <QtQml/QQmlEngine>
#include <QtQml/qqmlinfo.h>
#include <QtQml/private/qqmlengine_p.h>
#include <QtPositioning/QGeoRectangle>

QT_BEGIN_NAMESPACE

/*!
    \qmltype Route
    \instantiates QDeclarativeGeoRoute
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-routing
    \since Qt Location 5.5

    \brief The Route type represents one geographical route.

    A Route type contains high level information about a route, such
    as the length the route, the estimated travel time for the route,
    and enough information to render a basic image of the route on a map.

    The QGeoRoute object also contains a list of \l RouteSegment objects which
    describe subsections of the route in greater detail.

    The primary means of acquiring Route objects is \l RouteModel.

    \section1 Example

    This example shows how to display a route's maneuvers in a ListView:

    \snippet declarative/routing.qml QtQuick import
    \snippet declarative/maps.qml QtLocation import
    \codeline
    \snippet declarative/routing.qml Route Maneuver List1
    \snippet declarative/routing.qml Route Maneuver List2
    \snippet declarative/routing.qml Route Maneuver List3

*/

QDeclarativeGeoRoute::QDeclarativeGeoRoute(QObject *parent)
    : QObject(parent)
{
    this->init();
}

QDeclarativeGeoRoute::QDeclarativeGeoRoute(const QGeoRoute &route, QObject *parent)
    : QObject(parent),
      route_(route)
{
    this->init();
}

QDeclarativeGeoRoute::~QDeclarativeGeoRoute() {}

void QDeclarativeGeoRoute::init()
{
    QGeoRouteSegment segment = route_.firstRouteSegment();
    while (segment.isValid()) {
        QDeclarativeGeoRouteSegment *routeSegment = new QDeclarativeGeoRouteSegment(segment, this);
        QQmlEngine::setContextForObject(routeSegment, QQmlEngine::contextForObject(this));
        segments_.append(routeSegment);
        segment = segment.nextRouteSegment();
    }
}

/*!
    \internal
*/
QList<QGeoCoordinate> QDeclarativeGeoRoute::routePath()
{
    return route_.path();
}

/*!
    \qmlproperty georectangle QtLocation::Route::bounds

    Read-only property which holds a bounding box which encompasses the entire route.

*/

QGeoRectangle QDeclarativeGeoRoute::bounds() const
{
    return route_.bounds();
}

/*!
    \qmlproperty int QtLocation::Route::travelTime

    Read-only property which holds the estimated amount of time it will take to
    traverse this route, in seconds.

*/

int QDeclarativeGeoRoute::travelTime() const
{
    return route_.travelTime();
}

/*!
    \qmlproperty real QtLocation::Route::distance

    Read-only property which holds distance covered by this route, in meters.
*/

qreal QDeclarativeGeoRoute::distance() const
{
    return route_.distance();
}

/*!
    \qmlproperty QJSValue QtLocation::Route::path

    Read-only property which holds the geographical coordinates of this route.
    Coordinates are listed in the order in which they would be traversed by someone
    traveling along this segment of the route.

    To access individual segments you can use standard list accessors: 'path.length'
    indicates the number of objects and 'path[index starting from zero]' gives
    the actual object.

    \sa QtPositioning::coordinate
*/

QJSValue QDeclarativeGeoRoute::path() const
{
    QQmlContext *context = QQmlEngine::contextForObject(parent());
    QQmlEngine *engine = context->engine();
    QV4::ExecutionEngine *v4 = QQmlEnginePrivate::getV4Engine(engine);

    QV4::Scope scope(v4);
    QV4::Scoped<QV4::ArrayObject> pathArray(scope, v4->newArrayObject(route_.path().length()));
    for (int i = 0; i < route_.path().length(); ++i) {
        const QGeoCoordinate &c = route_.path().at(i);

        QV4::ScopedValue cv(scope, v4->fromVariant(QVariant::fromValue(c)));
        pathArray->putIndexed(i, cv);
    }

    return QJSValue(v4, pathArray.asReturnedValue());
}

void QDeclarativeGeoRoute::setPath(const QJSValue &value)
{
    if (!value.isArray())
        return;

    QList<QGeoCoordinate> pathList;
    quint32 length = value.property(QStringLiteral("length")).toUInt();
    for (quint32 i = 0; i < length; ++i) {
        bool ok;
        QGeoCoordinate c = parseCoordinate(value.property(i), &ok);

        if (!ok || !c.isValid()) {
            qmlInfo(this) << "Unsupported path type";
            return;
        }

        pathList.append(c);
    }

    if (route_.path() == pathList)
        return;

    route_.setPath(pathList);

    emit pathChanged();
}

/*!
    \qmlproperty list<RouteSegment> QtLocation::Route::segments

    Read-only property which holds the list of \l RouteSegment objects of this route.

    To access individual segments you can use standard list accessors: 'segments.length'
    indicates the number of objects and 'segments[index starting from zero]' gives
    the actual objects.

    \sa RouteSegment
*/

QQmlListProperty<QDeclarativeGeoRouteSegment> QDeclarativeGeoRoute::segments()
{
    return QQmlListProperty<QDeclarativeGeoRouteSegment>(this, 0, segments_append, segments_count,
                                                         segments_at, segments_clear);
}

/*!
    \internal
*/
void QDeclarativeGeoRoute::segments_append(QQmlListProperty<QDeclarativeGeoRouteSegment> *prop,
                                           QDeclarativeGeoRouteSegment *segment)
{
    static_cast<QDeclarativeGeoRoute *>(prop->object)->appendSegment(segment);
}

/*!
    \internal
*/
int QDeclarativeGeoRoute::segments_count(QQmlListProperty<QDeclarativeGeoRouteSegment> *prop)
{
    return static_cast<QDeclarativeGeoRoute *>(prop->object)->segments_.count();
}

/*!
    \internal
*/
QDeclarativeGeoRouteSegment *QDeclarativeGeoRoute::segments_at(QQmlListProperty<QDeclarativeGeoRouteSegment> *prop, int index)
{
    return static_cast<QDeclarativeGeoRoute *>(prop->object)->segments_.at(index);
}

/*!
    \internal
*/
void QDeclarativeGeoRoute::segments_clear(QQmlListProperty<QDeclarativeGeoRouteSegment> *prop)
{
    static_cast<QDeclarativeGeoRoute *>(prop->object)->clearSegments();
}

/*!
    \internal
*/
void QDeclarativeGeoRoute::appendSegment(QDeclarativeGeoRouteSegment *segment)
{
    segments_.append(segment);
}

/*!
    \internal
*/
void QDeclarativeGeoRoute::clearSegments()
{
    segments_.clear();
}

#include "moc_qdeclarativegeoroute_p.cpp"

QT_END_NAMESPACE
