/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "qdeclarativegeoroutesegment_p.h"

#include <QtQml/QQmlEngine>
#include <QtQml/QQmlContext>
#include <QtQml/private/qqmlengine_p.h>
#include <private/qqmlvaluetypewrapper_p.h>
#include <private/qjsvalue_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype RouteSegment
    \instantiates QDeclarativeGeoRouteSegment
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-routing
    \since Qt Location 5.0

    \brief The RouteSegment type represents a segment of a Route.

    A RouteSegment instance has information about the physical layout
    of the route segment, the length of the route and estimated time required
    to traverse the route segment and optional RouteManeuvers associated with
    the end of the route segment.

    RouteSegment instances can be thought of as edges on a routing
    graph, with RouteManeuver instances as optional labels attached to the
    vertices of the graph.

    The primary means of acquiring Route objects is via Routes via \l RouteModel.

    \section1 Example

    The following QML snippet demonstrates how to print information about a
    route segment:

    \snippet declarative/routing.qml QtQuick import
    \snippet declarative/routing.qml QtLocation import
    \codeline
    \snippet declarative/routing.qml RouteSegment
*/

QDeclarativeGeoRouteSegment::QDeclarativeGeoRouteSegment(QObject *parent)
    : QObject(parent)
{
    maneuver_ = new QDeclarativeGeoManeuver(this);
}

QDeclarativeGeoRouteSegment::QDeclarativeGeoRouteSegment(const QGeoRouteSegment &segment,
                                                         QObject *parent)
    : QObject(parent),
      segment_(segment)
{
    maneuver_ = new QDeclarativeGeoManeuver(segment_.maneuver(), this);
}

QDeclarativeGeoRouteSegment::~QDeclarativeGeoRouteSegment() {}

/*!
    \qmlproperty int QtLocation::RouteSegment::travelTime

    Read-only property which holds the estimated amount of time it will take to
    traverse this segment, in seconds.

*/

int QDeclarativeGeoRouteSegment::travelTime() const
{
    return segment_.travelTime();
}

/*!
    \qmlproperty real QtLocation::RouteSegment::distance

    Read-only property which holds the distance covered by this segment of the route, in meters.

*/

qreal QDeclarativeGeoRouteSegment::distance() const
{
    return segment_.distance();
}

/*!
    \qmlproperty RouteManeuver QtLocation::RouteSegment::maneuver

    Read-only property which holds the maneuver for this route segment.

    Will return invalid maneuver if no information has been attached to the endpoint
    of this route segment.
*/

QDeclarativeGeoManeuver *QDeclarativeGeoRouteSegment::maneuver() const
{
    return maneuver_;
}

/*!
    \qmlproperty QJSValue QtLocation::RouteSegment::path

    Read-only property which holds the geographical coordinates of this segment.
    Coordinates are listed in the order in which they would be traversed by someone
    traveling along this segment of the route.

    To access individual segments you can use standard list accessors: 'path.length'
    indicates the number of objects and 'path[index starting from zero]' gives
    the actual object.

    \sa QtPositioning::coordinate
*/

QJSValue QDeclarativeGeoRouteSegment::path() const
{
    QQmlContext *context = QQmlEngine::contextForObject(parent());
    QQmlEngine *engine = context->engine();
    QV8Engine *v8Engine = QQmlEnginePrivate::getV8Engine(engine);
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(v8Engine);

    QV4::Scope scope(v4);
    QV4::Scoped<QV4::ArrayObject> pathArray(scope, v4->newArrayObject(segment_.path().length()));
    for (int i = 0; i < segment_.path().length(); ++i) {
        const QGeoCoordinate &c = segment_.path().at(i);

        QQmlValueType *vt = QQmlValueTypeFactory::valueType(qMetaTypeId<QGeoCoordinate>());
        QV4::ScopedValue cv(scope, QV4::QmlValueTypeWrapper::create(v8Engine, QVariant::fromValue(c), vt));

        pathArray->putIndexed(i, cv);
    }

    return new QJSValuePrivate(v4, QV4::ValueRef(pathArray));
}

#include "moc_qdeclarativegeoroutesegment_p.cpp"

QT_END_NAMESPACE
