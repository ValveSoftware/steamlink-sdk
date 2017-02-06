/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "locationsingleton.h"

/*!
    \qmltype QtPositioning
    \instantiates LocationSingleton
    \inqmlmodule QtPositioning
    \since 5.2

    \brief The QtPositioning global object provides useful functions for working with location-based
           types in QML.

    \qml
    import QtPositioning 5.2

    Item {
        property variant coordinate: QtPositioning.coordinate(-27.5, 153.1)
    }
    \endqml
*/

LocationSingleton::LocationSingleton(QObject *parent)
:   QObject(parent)
{
}

/*!
    \qmlmethod coordinate ::QtPositioning::coordinate()

    Constructs an invalid coordinate.

*/
QGeoCoordinate LocationSingleton::coordinate() const
{
    return QGeoCoordinate();
}

/*!
    \qmlmethod coordinate QtPositioning::coordinate(real latitude, real longitue, real altitude) const

    Constructs a coordinate with the specified \a latitude, \a longitude and optional \a altitude.
    Both \a latitude and \a longitude must be valid, otherwise an invalid coordinate is returned.

    \sa {coordinate}
*/
QGeoCoordinate LocationSingleton::coordinate(double latitude, double longitude, double altitude) const
{
    return QGeoCoordinate(latitude, longitude, altitude);
}

/*!
    \qmlmethod geoshape QtPositioning::shape() const

    Constructs an invalid geoshape.

    \sa {geoshape}
*/
QGeoShape LocationSingleton::shape() const
{
    return QGeoShape();
}

/*!
    \qmlmethod georectangle QtPositioning::rectangle() const

    Constructs an invalid georectangle.

    \sa {georectangle}
*/
QGeoRectangle LocationSingleton::rectangle() const
{
    return QGeoRectangle();
}

/*!
    \qmlmethod georectangle QtPositioning::rectangle(coordinate center, real width, real height) const

    Constructs a georectangle centered at \a center with a width of \a width degrees and a hight of
    \a height degrees.

    \sa {georectangle}
*/
QGeoRectangle LocationSingleton::rectangle(const QGeoCoordinate &center,
                                           double width, double height) const
{
    return QGeoRectangle(center, width, height);
}

/*!
    \qmlmethod georectangle QtPositioning::rectangle(coordinate topLeft, coordinate bottomRight) const

    Constructs a georectangle with its top left corner positioned at \a topLeft and its bottom
    right corner positioned at \a {bottomLeft}.

    \sa {georectangle}
*/
QGeoRectangle LocationSingleton::rectangle(const QGeoCoordinate &topLeft,
                                           const QGeoCoordinate &bottomRight) const
{
    return QGeoRectangle(topLeft, bottomRight);
}

/*!
    \qmlmethod georectangle QtLocation5::QtLocation::rectangle(list<coordinate> coordinates) const

    Constructs a georectangle from the list of coordinates, the returned list is the smallest possible
    containing all the coordinates.

    \sa {georectangle}
*/
QGeoRectangle LocationSingleton::rectangle(const QVariantList &coordinates) const
{
    QList<QGeoCoordinate> internalCoordinates;
    for (int i = 0; i < coordinates.size(); i++) {
        if (coordinates.at(i).canConvert<QGeoCoordinate>())
            internalCoordinates << coordinates.at(i).value<QGeoCoordinate>();
    }
    return QGeoRectangle(internalCoordinates);
}

/*!
    \qmlmethod geocircle QtPositioning::circle() const

    Constructs an invalid geocircle.

    \sa {geocircle}
*/
QGeoCircle LocationSingleton::circle() const
{
    return QGeoCircle();
}

/*!
    \qmlmethod geocircle QtPositioning::circle(coordinate center, real radius) const

    Constructs a geocircle centered at \a center with a radius of \a radius meters.
*/
QGeoCircle LocationSingleton::circle(const QGeoCoordinate &center, qreal radius) const
{
    return QGeoCircle(center, radius);
}

/*!
    \qmlmethod geocircle QtPositioning::shapeToCircle(geoshape shape) const

    Converts \a shape to a geocircle.

    \sa {geocircle}
    \since 5.5
*/
QGeoCircle LocationSingleton::shapeToCircle(const QGeoShape &shape) const
{
    return QGeoCircle(shape);
}

/*!
    \qmlmethod georectangle QtPositioning::shapeToRectangle(geoshape shape) const

    Converts \a shape to a georectangle.

    \sa {georectangle}
    \since 5.5
*/
QGeoRectangle LocationSingleton::shapeToRectangle(const QGeoShape &shape) const
{
    return QGeoRectangle(shape);
}

