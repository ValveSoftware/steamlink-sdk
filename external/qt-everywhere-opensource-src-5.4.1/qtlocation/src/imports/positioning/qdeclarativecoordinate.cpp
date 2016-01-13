/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#include "qdeclarativecoordinate_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmlbasictype coordinate
    \inqmlmodule QtPositioning
    \ingroup qml-QtPositioning5-basictypes
    \since 5.2

    \brief The coordinate type represents and stores a geographic position.

    The \c coordinate type represents a geographic position in the form of \l {latitude},
    \l longitude and \l altitude attributes.  The \l latitude attribute specifies the number of
    decimal degrees above and below the equator.  A positive latitude indicates the Northern
    Hemisphere and a negative latitude indicates the Southern Hemisphere.  The \l longitude
    attribute specifies the number of decimal degrees east and west.  A positive longitude
    indicates the Eastern Hemisphere and a negative longitude indicates the Western Hemisphere.
    The \l altitude attribute specifies the number of meters above sea level.  Together, these
    attributes specify a 3-dimensional position anywhere on or near the Earth's surface.

    The \l isValid attribute can be used to test if a coordinate is valid.  A coordinate is
    considered valid if it has a valid latitude and longitude.  A valid altitude is not required.
    The latitude must be between -90 and 90 inclusive and the longitude must be between -180 and
    180 inclusive.

    The coordinate type is used by many other types in the Qt Location module, for specifying
    the position of an object on a Map, the current position of a device and many other tasks.
    They also feature a number of important utility methods that make otherwise complex
    calculations simple to use, such as \l atDistanceAndAzimuth().

    \section1 Accuracy

    The latitude, longitude and altitude attributes stored in the coordinate type are represented
    as doubles, giving them approximately 16 decimal digits of precision -- enough to specify
    micrometers.  The calculations performed in coordinate's methods such as \l azimuthTo() and
    \l distanceTo() also use doubles for all intermediate values, but the inherent inaccuracies in
    their spherical Earth model dominate the amount of error in their output.

    \section1 Example Usage

    Use properties of type \l variant to store a \c {coordinate}.  To create a \c coordinate use
    one of the methods described below.  In all cases, specifying the \l altitude attribute is
    optional.

    To create a \c coordinate value, use the \l{QtPositioning::coordinate}{QtPositioning.coordinate()}
    function:

    \qml
    import QtPositioning 5.2

    Location { coordinate: QtPositioning.coordinate(-27.5, 153.1) }
    \endqml

    or as separate \l latitude, \l longitude and \l altitude components:

    \qml
    Location {
        coordinate {
            latitude: -27.5
            longitude: 153.1
        }
    }
    \endqml

    When integrating with C++, note that any QGeoCoordinate value passed into QML from C++ is
    automatically converted into a \c coordinate value, and vice-versa.

    \section1 Properties

    \section2 latitude

    \code
    real latitude
    \endcode

    This property holds latitude value of the geographical position
    (decimal degrees). A positive latitude indicates the Northern Hemisphere,
    and a negative latitude indicates the Southern Hemisphere.
    If the property has not been set, its default value is NaN.

    \section2 longitude

    \code
    real longitude
    \endcode

    This property holds the longitude value of the geographical position
    (decimal degrees). A positive longitude indicates the Eastern Hemisphere,
    and a negative longitude indicates the Western Hemisphere
    If the property has not been set, its default value is NaN.

    \section2 altitude

    \code
    real altitude
    \endcode

    This property holds the value of altitude (meters above sea level).
    If the property has not been set, its default value is NaN.

    \section2 isValid

    \code
    bool isValid
    \endcode

    This property holds the current validity of the coordinate. Coordinates
    are considered valid if they have been set with a valid latitude and
    longitude (altitude is not required).

    The latitude must be between -90 to 90 inclusive to be considered valid,
    and the longitude must be between -180 to 180 inclusive to be considered
    valid.

    This is a read-only property.

    \section1 Methods

    \section2 distanceTo()

    \code
    real distanceTo(coordinate other)
    \endcode

    Returns the distance (in meters) from this coordinate to the coordinate specified by \a other.
    Altitude is not used in the calculation.

    This calculation returns the great-circle distance between the two coordinates, with an
    assumption that the Earth is spherical for the purpose of this calculation.

    \section2 azimuthTo()

    \code
    real azimuth(coordinate other)
    \endcode

    Returns the azimuth (or bearing) in degrees from this coordinate to the coordinate specified by
    \a other.  Altitude is not used in the calculation.

    There is an assumption that the Earth is spherical for the purpose of this calculation.

    \section2 atDistanceAndAzimuth()

    \code
    coordinate atDistanceAndAzimuth(real distance, real azimuth)
    \endcode

    Returns the coordinate that is reached by traveling \a distance metres from this coordinate at
    \a azimuth degrees along a great-circle.

    There is an assumption that the Earth is spherical for the purpose of this calculation.
*/


CoordinateValueType::CoordinateValueType(QObject *parent)
:   QQmlValueTypeBase<QGeoCoordinate>(qMetaTypeId<QGeoCoordinate>(), parent)
{
}

CoordinateValueType::~CoordinateValueType()
{
}

/*
    This property holds the value of altitude (meters above sea level).
    If the property has not been set, its default value is NaN.

*/
void CoordinateValueType::setAltitude(double altitude)
{
    v.setAltitude(altitude);
}

double CoordinateValueType::altitude() const
{
    return v.altitude();
}

/*
    This property holds the longitude value of the geographical position
    (decimal degrees). A positive longitude indicates the Eastern Hemisphere,
    and a negative longitude indicates the Western Hemisphere
    If the property has not been set, its default value is NaN.
*/
void CoordinateValueType::setLongitude(double longitude)
{
    v.setLongitude(longitude);
}

double CoordinateValueType::longitude() const
{
    return v.longitude();
}

/*
    This property holds latitude value of the geographical position
    (decimal degrees). A positive latitude indicates the Northern Hemisphere,
    and a negative latitude indicates the Southern Hemisphere.
    If the property has not been set, its default value is NaN.
*/
void CoordinateValueType::setLatitude(double latitude)
{
    v.setLatitude(latitude);
}

double CoordinateValueType::latitude() const
{
    return v.latitude();
}

/*
    This property holds the current validity of the coordinate. Coordinates
    are considered valid if they have been set with a valid latitude and
    longitude (altitude is not required).

    The latitude must be between -90 to 90 inclusive to be considered valid,
    and the longitude must be between -180 to 180 inclusive to be considered
    valid.
*/
bool CoordinateValueType::isValid() const
{
    return v.isValid();
}

QString CoordinateValueType::toString() const
{
    return QStringLiteral("QGeoCoordinate(%1,%2,%3)")
        .arg(v.latitude()).arg(v.longitude()).arg(v.altitude());
}

bool CoordinateValueType::isEqual(const QVariant &other) const
{
    if (other.userType() != qMetaTypeId<QGeoCoordinate>())
        return false;

    return v == other.value<QGeoCoordinate>();
}

/*
    Returns the distance (in meters) from this coordinate to the
    coordinate specified by other. Altitude is not used in the calculation.

    This calculation returns the great-circle distance between the two
    coordinates, with an assumption that the Earth is spherical for the
    purpose of this calculation.
*/
qreal CoordinateValueType::distanceTo(const QGeoCoordinate &coordinate) const
{
    return v.distanceTo(coordinate);
}

/*
    Returns the azimuth (or bearing) in degrees from this coordinate to the
    coordinate specified by other. Altitude is not used in the calculation.

    There is an assumption that the Earth is spherical for the purpose of
    this calculation.
*/
qreal CoordinateValueType::azimuthTo(const QGeoCoordinate &coordinate) const
{
    return v.azimuthTo(coordinate);
}

/*
    Returns the coordinate that is reached by traveling distance metres
    from the current coordinate at azimuth degrees along a great-circle.

    There is an assumption that the Earth is spherical for the purpose
    of this calculation.
*/
QGeoCoordinate CoordinateValueType::atDistanceAndAzimuth(qreal distance, qreal azimuth) const
{
    return v.atDistanceAndAzimuth(distance, azimuth);
}

#include "moc_qdeclarativecoordinate_p.cpp"

QT_END_NAMESPACE
