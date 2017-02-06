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
#include "qgeocoordinate.h"
#include "qgeocoordinate_p.h"
#include "qlocationutils_p.h"

#include <QDateTime>
#include <QHash>
#include <QDataStream>
#include <QDebug>
#include <qnumeric.h>

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

QT_BEGIN_NAMESPACE

static const double qgeocoordinate_EARTH_MEAN_RADIUS = 6371.0072;

inline static double qgeocoordinate_degToRad(double deg)
{
    return deg * M_PI / 180;
}
inline static double qgeocoordinate_radToDeg(double rad)
{
    return rad * 180 / M_PI;
}


QGeoCoordinatePrivate::QGeoCoordinatePrivate():
    lat(qQNaN()),
    lng(qQNaN()),
    alt(qQNaN())
{}

QGeoCoordinatePrivate::QGeoCoordinatePrivate(const QGeoCoordinatePrivate &other)
    : QSharedData(other),
      lat(other.lat),
      lng(other.lng),
      alt(other.alt)
{}

QGeoCoordinatePrivate::~QGeoCoordinatePrivate()
{}


QGeoMercatorCoordinatePrivate::QGeoMercatorCoordinatePrivate():
    QGeoCoordinatePrivate(),
    m_mercatorX(qQNaN()),
    m_mercatorY(qQNaN())
{}

QGeoMercatorCoordinatePrivate::QGeoMercatorCoordinatePrivate(const QGeoMercatorCoordinatePrivate &other)
    : QGeoCoordinatePrivate(other),
      m_mercatorX(other.m_mercatorX),
      m_mercatorY(other.m_mercatorY)
{}

QGeoMercatorCoordinatePrivate::~QGeoMercatorCoordinatePrivate()
{}

/*!
    \class QGeoCoordinate
    \inmodule QtPositioning
    \ingroup QtPositioning-positioning
    \since 5.2

    \brief The QGeoCoordinate class defines a geographical position on the surface of the Earth.

    A QGeoCoordinate is defined by latitude, longitude, and optionally, altitude.

    Use type() to determine whether a coordinate is a 2D coordinate (has
    latitude and longitude only) or 3D coordinate (has latitude, longitude
    and altitude). Use distanceTo() and azimuthTo() to calculate the distance
    and bearing between coordinates.

    The coordinate values should be specified using the WGS84 datum. For more information
    on geographical terms see this article on \l {http://en.wikipedia.org/wiki/Geographic_coordinate_system}{coordinates} and
    another on \l {http://en.wikipedia.org/wiki/Geodetic_system}{geodetic systems}
    including WGS84.

    Azimuth in this context is equivalent to a compass bearing based on true north.

    This class is a \l Q_GADGET since Qt 5.5. It can be
    \l{Cpp_value_integration_positioning}{directly used from C++ and QML}.
*/

/*!
    \enum QGeoCoordinate::CoordinateType
    Defines the types of a coordinate.

    \value InvalidCoordinate An invalid coordinate. A coordinate is invalid if its latitude or longitude values are invalid.
    \value Coordinate2D A coordinate with valid latitude and longitude values.
    \value Coordinate3D A coordinate with valid latitude and longitude values, and also an altitude value.
*/

/*!
    \enum QGeoCoordinate::CoordinateFormat
    Defines the possible formatting options for toString().

    \value Degrees Returns a string representation of the coordinates in decimal degrees format.
    \value DegreesWithHemisphere Returns a string representation of the coordinates in decimal degrees format, using 'N', 'S', 'E' or 'W' to indicate the hemispheres of the coordinates.
    \value DegreesMinutes Returns a string representation of the coordinates in degrees-minutes format.
    \value DegreesMinutesWithHemisphere Returns a string representation of the coordinates in degrees-minutes format, using 'N', 'S', 'E' or 'W' to indicate the hemispheres of the coordinates.
    \value DegreesMinutesSeconds Returns a string representation of the coordinates in degrees-minutes-seconds format.
    \value DegreesMinutesSecondsWithHemisphere Returns a string representation of the coordinates in degrees-minutes-seconds format, using 'N', 'S', 'E' or 'W' to indicate the hemispheres of the coordinates.

    \sa toString()
*/

/*!
    \property QGeoCoordinate::latitude
    \brief This property holds the latitude in decimal degrees.

    The property is undefined (\l {qQNaN()}) if the latitude has not been set.
    A positive latitude indicates the Northern Hemisphere, and a negative
    latitude indicates the Southern Hemisphere. When setting the latitude the
    new value should be in the
    \l {http://en.wikipedia.org/wiki/World_Geodetic_System}{WGS84} datum format.

    To be valid, the latitude must be between -90 to 90 inclusive.

    While this property is introduced in Qt 5.5, the related accessor functions
    exist since the first version of this class.

    \since 5.5
*/

/*!
    \property QGeoCoordinate::longitude
    \brief This property holds the longitude in decimal degrees.

    The property is undefined (\l {qQNaN()}) if the longitude has not been set.
    A positive longitude indicates the Eastern Hemisphere, and a negative
    longitude indicates the Western Hemisphere. When setting the longitude the
    new value should be in the
    \l {http://en.wikipedia.org/wiki/World_Geodetic_System}{WGS84} datum format.

    To be valid, the longitude must be between -180 to 180 inclusive.

    While this property is introduced in Qt 5.5, the related accessor functions
    exist since the first version of this class.

    \since 5.5
*/

/*!
    \property QGeoCoordinate::altitude
    \brief This property holds the altitude in meters above sea level.

    The property is undefined (\l {qQNaN()}) if the altitude has not been set.

    While this property is introduced in Qt 5.5, the related accessor functions
    exist since the first version of this class.

    \since 5.5
*/

/*!
    \property QGeoCoordinate::isValid
    \brief This property holds the validity of this geo coordinate.

    The geo coordinate is valid if the \l [CPP]{longitude} and \l [CPP]{latitude}
    properties have been set to valid values.

    While this property is introduced in Qt 5.5, the related accessor functions
    exist since the first version of this class.

    \since 5.5
*/

/*!
    Constructs a coordinate. The coordinate will be invalid until
    setLatitude() and setLongitude() have been called.
*/
QGeoCoordinate::QGeoCoordinate()
        : d(new QGeoCoordinatePrivate)
{
}

/*!
    Constructs a coordinate with the given \a latitude and \a longitude.

    If the latitude is not between -90 to 90 inclusive, or the longitude
    is not between -180 to 180 inclusive, none of the values are set and
    the type() will be QGeoCoordinate::InvalidCoordinate.

    \sa isValid()
*/
QGeoCoordinate::QGeoCoordinate(double latitude, double longitude)
        : d(new QGeoCoordinatePrivate)
{
    if (QLocationUtils::isValidLat(latitude) && QLocationUtils::isValidLong(longitude)) {
        d->lat = latitude;
        d->lng = longitude;
    }
}

/*!
    Constructs a coordinate with the given \a latitude, \a longitude
    and \a altitude.

    If the latitude is not between -90 to 90 inclusive, or the longitude
    is not between -180 to 180 inclusive, none of the values are set and
    the type() will be QGeoCoordinate::InvalidCoordinate.

    Note that \a altitude specifies the meters above sea level.

    \sa isValid()
*/
QGeoCoordinate::QGeoCoordinate(double latitude, double longitude, double altitude)
        : d(new QGeoCoordinatePrivate)
{
    if (QLocationUtils::isValidLat(latitude) && QLocationUtils::isValidLong(longitude)) {
        d->lat = latitude;
        d->lng = longitude;
        d->alt = altitude;
    }
}

/*!
    Constructs a coordinate from the contents of \a other.
*/
QGeoCoordinate::QGeoCoordinate(const QGeoCoordinate &other)
        : d(other.d)
{}

/*!
    Assigns \a other to this coordinate and returns a reference to this coordinate.
*/
QGeoCoordinate &QGeoCoordinate::operator=(const QGeoCoordinate &other)
{
    if (this == &other)
        return *this;

    d = other.d;
    return (*this);
}

/*!
    Destroys the coordinate object.
*/
QGeoCoordinate::~QGeoCoordinate()
{
}

/*!
    Returns true if the latitude, longitude and altitude of this
    coordinate are the same as those of \a other.

    The longitude will be ignored if the latitude is +/- 90 degrees.
*/
bool QGeoCoordinate::operator==(const QGeoCoordinate &other) const
{
    bool latEqual = (qIsNaN(d->lat) && qIsNaN(other.d->lat))
                        || qFuzzyCompare(d->lat, other.d->lat);
    bool lngEqual = (qIsNaN(d->lng) && qIsNaN(other.d->lng))
                        || qFuzzyCompare(d->lng, other.d->lng);
    bool altEqual = (qIsNaN(d->alt) && qIsNaN(other.d->alt))
                        || qFuzzyCompare(d->alt, other.d->alt);

    if (!qIsNaN(d->lat) && ((d->lat == 90.0) || (d->lat == -90.0)))
        lngEqual = true;

    return (latEqual && lngEqual && altEqual);
}

/*!
    \fn bool QGeoCoordinate::operator!=(const QGeoCoordinate &other) const;

    Returns true if the latitude, longitude or altitude of this
    coordinate are not the same as those of \a other.
*/

/*!
    Returns true if the \l longitude and \l latitude are valid.
*/
bool QGeoCoordinate::isValid() const
{
    CoordinateType t = type();
    return t == Coordinate2D || t == Coordinate3D;
}

/*!
    Returns the type of this coordinate.
*/
QGeoCoordinate::CoordinateType QGeoCoordinate::type() const
{
    if (QLocationUtils::isValidLat(d->lat)
            && QLocationUtils::isValidLong(d->lng)) {
        if (qIsNaN(d->alt))
            return Coordinate2D;
        return Coordinate3D;
    }
    return InvalidCoordinate;
}


/*!
    Returns the latitude, in decimal degrees. The return value is undefined
    if the latitude has not been set.

    A positive latitude indicates the Northern Hemisphere, and a negative
    latitude indicates the Southern Hemisphere.

    \sa setLatitude(), type()
*/
double QGeoCoordinate::latitude() const
{
    return d->lat;
}

/*!
    Sets the latitude (in decimal degrees) to \a latitude. The value should
    be in the WGS84 datum.

    To be valid, the latitude must be between -90 to 90 inclusive.

    \sa latitude()
*/
void QGeoCoordinate::setLatitude(double latitude)
{
    d->lat = latitude;
}

/*!
    Returns the longitude, in decimal degrees. The return value is undefined
    if the longitude has not been set.

    A positive longitude indicates the Eastern Hemisphere, and a negative
    longitude indicates the Western Hemisphere.

    \sa setLongitude(), type()
*/
double QGeoCoordinate::longitude() const
{
    return d->lng;
}

/*!
    Sets the longitude (in decimal degrees) to \a longitude. The value should
    be in the WGS84 datum.

    To be valid, the longitude must be between -180 to 180 inclusive.

    \sa longitude()
*/
void QGeoCoordinate::setLongitude(double longitude)
{
    d->lng = longitude;
}

/*!
    Returns the altitude (meters above sea level).

    The return value is undefined if the altitude has not been set.

    \sa setAltitude(), type()
*/
double QGeoCoordinate::altitude() const
{
    return d->alt;
}

/*!
    Sets the altitude (meters above sea level) to \a altitude.

    \sa altitude()
*/
void QGeoCoordinate::setAltitude(double altitude)
{
    d->alt = altitude;
}

/*!
    Returns the distance (in meters) from this coordinate to the coordinate
    specified by \a other. Altitude is not used in the calculation.

    This calculation returns the great-circle distance between the two
    coordinates, with an assumption that the Earth is spherical for the
    purpose of this calculation.

    Returns 0 if the type of this coordinate or the type of \a other is
    QGeoCoordinate::InvalidCoordinate.
*/
qreal QGeoCoordinate::distanceTo(const QGeoCoordinate &other) const
{
    if (type() == QGeoCoordinate::InvalidCoordinate
            || other.type() == QGeoCoordinate::InvalidCoordinate) {
        return 0;
    }

    // Haversine formula
    double dlat = qgeocoordinate_degToRad(other.d->lat - d->lat);
    double dlon = qgeocoordinate_degToRad(other.d->lng - d->lng);
    double haversine_dlat = sin(dlat / 2.0);
    haversine_dlat *= haversine_dlat;
    double haversine_dlon = sin(dlon / 2.0);
    haversine_dlon *= haversine_dlon;
    double y = haversine_dlat
             + cos(qgeocoordinate_degToRad(d->lat))
             * cos(qgeocoordinate_degToRad(other.d->lat))
             * haversine_dlon;
    double x = 2 * asin(sqrt(y));
    return qreal(x * qgeocoordinate_EARTH_MEAN_RADIUS * 1000);
}

/*!
    Returns the azimuth (or bearing) in degrees from this coordinate to the
    coordinate specified by \a other. Altitude is not used in the calculation.

    The bearing returned is the bearing from the origin to \a other along the
    great-circle between the two coordinates. There is an assumption that the
    Earth is spherical for the purpose of this calculation.

    Returns 0 if the type of this coordinate or the type of \a other is
    QGeoCoordinate::InvalidCoordinate.
*/
qreal QGeoCoordinate::azimuthTo(const QGeoCoordinate &other) const
{
    if (type() == QGeoCoordinate::InvalidCoordinate
            || other.type() == QGeoCoordinate::InvalidCoordinate) {
        return 0;
    }

    double dlon = qgeocoordinate_degToRad(other.d->lng - d->lng);
    double lat1Rad = qgeocoordinate_degToRad(d->lat);
    double lat2Rad = qgeocoordinate_degToRad(other.d->lat);

    double y = sin(dlon) * cos(lat2Rad);
    double x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(dlon);

    double azimuth = qgeocoordinate_radToDeg(atan2(y, x)) + 360.0;
    double whole;
    double fraction = modf(azimuth, &whole);
    return qreal((int(whole + 360) % 360) + fraction);
}

void QGeoCoordinatePrivate::atDistanceAndAzimuth(const QGeoCoordinate &coord,
                                                 qreal distance, qreal azimuth,
                                                 double *lon, double *lat)
{
    double latRad = qgeocoordinate_degToRad(coord.d->lat);
    double lonRad = qgeocoordinate_degToRad(coord.d->lng);
    double cosLatRad = cos(latRad);
    double sinLatRad = sin(latRad);

    double azimuthRad = qgeocoordinate_degToRad(azimuth);

    double ratio = (distance / (qgeocoordinate_EARTH_MEAN_RADIUS * 1000.0));
    double cosRatio = cos(ratio);
    double sinRatio = sin(ratio);

    double resultLatRad = asin(sinLatRad * cosRatio
                               + cosLatRad * sinRatio * cos(azimuthRad));
    double resultLonRad = lonRad + atan2(sin(azimuthRad) * sinRatio * cosLatRad,
                                   cosRatio - sinLatRad * sin(resultLatRad));

    *lat = qgeocoordinate_radToDeg(resultLatRad);
    *lon = qgeocoordinate_radToDeg(resultLonRad);
}

/*!
    Returns the coordinate that is reached by traveling \a distance meters
    from the current coordinate at \a azimuth (or bearing) along a great-circle.
    There is an assumption that the Earth is spherical for the purpose of this
    calculation.

    The altitude will have \a distanceUp added to it.

    Returns an invalid coordinate if this coordinate is invalid.
*/
QGeoCoordinate QGeoCoordinate::atDistanceAndAzimuth(qreal distance, qreal azimuth, qreal distanceUp) const
{
    if (!isValid())
        return QGeoCoordinate();

    double resultLon, resultLat;
    QGeoCoordinatePrivate::atDistanceAndAzimuth(*this, distance, azimuth,
                                                &resultLon, &resultLat);

    if (resultLon > 180.0)
        resultLon -= 360.0;
    else if (resultLon < -180.0)
        resultLon += 360.0;

    double resultAlt = d->alt + distanceUp;
    return QGeoCoordinate(resultLat, resultLon, resultAlt);
}

/*!
    Returns this coordinate as a string in the specified \a format.

    For example, if this coordinate has a latitude of -27.46758, a longitude
    of 153.027892 and an altitude of 28.1, these are the strings
    returned depending on \a format:

    \table
    \header
        \li \a format value
        \li Returned string
    \row
        \li \l Degrees
        \li -27.46758\unicode{0xB0}, 153.02789\unicode{0xB0}, 28.1m
    \row
        \li \l DegreesWithHemisphere
        \li 27.46758\unicode{0xB0} S, 153.02789\unicode{0xB0} E, 28.1m
    \row
        \li \l DegreesMinutes
        \li -27\unicode{0xB0} 28.054', 153\unicode{0xB0} 1.673', 28.1m
    \row
        \li \l DegreesMinutesWithHemisphere
        \li 27\unicode{0xB0} 28.054 S', 153\unicode{0xB0} 1.673' E, 28.1m
    \row
        \li \l DegreesMinutesSeconds
        \li -27\unicode{0xB0} 28' 3.2", 153\unicode{0xB0} 1' 40.4", 28.1m
    \row
        \li \l DegreesMinutesSecondsWithHemisphere
        \li 27\unicode{0xB0} 28' 3.2" S, 153\unicode{0xB0} 1' 40.4" E, 28.1m
    \endtable

    The altitude field is omitted if no altitude is set.

    If the coordinate is invalid, an empty string is returned.
*/
QString QGeoCoordinate::toString(CoordinateFormat format) const
{
    if (type() == QGeoCoordinate::InvalidCoordinate)
        return QString();

    QString latStr;
    QString longStr;

    double absLat = qAbs(d->lat);
    double absLng = qAbs(d->lng);
    QChar symbol(0x00B0);   // degrees symbol

    switch (format) {
        case Degrees:
        case DegreesWithHemisphere: {
            latStr = QString::number(absLat, 'f', 5) + symbol;
            longStr = QString::number(absLng, 'f', 5) + symbol;
            break;
        }
        case DegreesMinutes:
        case DegreesMinutesWithHemisphere: {
            double latMin = (absLat - int(absLat)) * 60;
            double lngMin = (absLng - int(absLng)) * 60;

            if (qRound(latMin) >= 60) {
                absLat++;
                latMin = qAbs(latMin - 60.0f);
                //avoid invalid latitude due to latMin rounding below
                if (qRound(absLat) >= 90)
                    latMin = 0.0f;
            }
            if (qRound(lngMin) >= 60) {
                absLng++;
                lngMin = qAbs(lngMin - 60.0f);
                // avoid invalid longitude due to lngMin rounding below
                if (qRound(absLng) >= 180)
                    lngMin = 0.0f;
            }

            latStr = QString::fromLatin1("%1%2 %3'")
                     .arg(QString::number(int(absLat)))
                     .arg(symbol)
                     .arg(QString::number(latMin, 'f', 3));
            longStr = QString::fromLatin1("%1%2 %3'")
                      .arg(QString::number(int(absLng)))
                      .arg(symbol)
                      .arg(QString::number(lngMin, 'f', 3));
            break;
        }
        case DegreesMinutesSeconds:
        case DegreesMinutesSecondsWithHemisphere: {
            double latMin = (absLat - int(absLat)) * 60;
            double lngMin = (absLng - int(absLng)) * 60;
            double latSec = (latMin - int(latMin)) * 60;
            double lngSec = (lngMin - int(lngMin)) * 60;

            // overflow to full minutes
            if (qRound(latSec) >= 60) {
                latMin++;
                latSec = qAbs(latSec - 60.0f);
                // overflow to full degrees
                if (qRound(latMin) >= 60) {
                    absLat++;
                    latMin = qAbs(latMin - 60.0f);
                    // avoid invalid latitude due to latSec rounding below
                    if (qRound(absLat) >= 90)
                        latSec = 0.0f;
                }
            }
            if (qRound(lngSec) >= 60) {
                lngMin++;
                lngSec = qAbs(lngSec - 60.0f);
                if (qRound(lngMin) >= 60) {
                    absLng++;
                    lngMin = qAbs(lngMin - 60.0f);
                    // avoid invalid longitude due to lngSec rounding below
                    if (qRound(absLng) >= 180)
                        lngSec = 0.0f;
                }
            }

            latStr = QString::fromLatin1("%1%2 %3' %4\"")
                     .arg(QString::number(int(absLat)))
                     .arg(symbol)
                     .arg(QString::number(int(latMin)))
                     .arg(QString::number(latSec, 'f', 1));
            longStr = QString::fromLatin1("%1%2 %3' %4\"")
                      .arg(QString::number(int(absLng)))
                      .arg(symbol)
                      .arg(QString::number(int(lngMin)))
                      .arg(QString::number(lngSec, 'f', 1));
            break;
        }
    }

    // now add the "-" to the start, or append the hemisphere char
    switch (format) {
        case Degrees:
        case DegreesMinutes:
        case DegreesMinutesSeconds: {
            if (d->lat < 0)
                latStr.insert(0, QStringLiteral("-"));
            if (d->lng < 0)
                longStr.insert(0, QStringLiteral("-"));
            break;
        }
        case DegreesWithHemisphere:
        case DegreesMinutesWithHemisphere:
        case DegreesMinutesSecondsWithHemisphere: {
            if (d->lat < 0)
                latStr.append(QString::fromLatin1(" S"));
            else if (d->lat > 0)
                latStr.append(QString::fromLatin1(" N"));
            if (d->lng < 0)
                longStr.append(QString::fromLatin1(" W"));
            else if (d->lng > 0)
                longStr.append(QString::fromLatin1(" E"));
            break;
        }
    }

    if (qIsNaN(d->alt))
        return QString::fromLatin1("%1, %2").arg(latStr, longStr);
    return QString::fromLatin1("%1, %2, %3m").arg(latStr, longStr, QString::number(d->alt));
}

QGeoCoordinate::QGeoCoordinate(QGeoCoordinatePrivate &dd):
    d(&dd)
{
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const QGeoCoordinate &coord)
{
    QDebugStateSaver saver(dbg);
    double lat = coord.latitude();
    double lng = coord.longitude();

    dbg.nospace() << "QGeoCoordinate(";
    if (qIsNaN(lat))
        dbg << '?';
    else
        dbg << lat;
    dbg << ", ";
    if (qIsNaN(lng))
        dbg << '?';
    else
        dbg << lng;
    if (coord.type() == QGeoCoordinate::Coordinate3D) {
        dbg << ", ";
        dbg << coord.altitude();
    }
    dbg << ')';
    return dbg;
}
#endif

#ifndef QT_NO_DATASTREAM
/*!
    \fn QDataStream &operator<<(QDataStream &stream, const QGeoCoordinate &coordinate)

    \relates QGeoCoordinate

    Writes the given \a coordinate to the specified \a stream.

    \sa {Serializing Qt Data Types}
*/

QDataStream &operator<<(QDataStream &stream, const QGeoCoordinate &coordinate)
{
    stream << coordinate.latitude();
    stream << coordinate.longitude();
    stream << coordinate.altitude();
    return stream;
}
#endif

#ifndef QT_NO_DATASTREAM
/*!
    \fn  QDataStream &operator>>(QDataStream &stream, QGeoCoordinate &coordinate)
    \relates QGeoCoordinate

    Reads a coordinate from the specified \a stream into the given
    \a coordinate.

    \sa {Serializing Qt Data Types}
*/

QDataStream &operator>>(QDataStream &stream, QGeoCoordinate &coordinate)
{
    double value;
    stream >> value;
    coordinate.setLatitude(value);
    stream >> value;
    coordinate.setLongitude(value);
    stream >> value;
    coordinate.setAltitude(value);
    return stream;
}
#endif

/*! \fn uint qHash(const QGeoCoordinate &coordinate, uint seed = 0)
    \relates QHash
    \since Qt 5.7

    Returns a hash value for \a coordinate, using \a seed to seed the calculation.
*/
uint qHash(const QGeoCoordinate &coordinate, uint seed)
{
    QtPrivate::QHashCombine hash;
    // north and south pole are geographically equivalent (no matter the longitude)
    if (coordinate.latitude() != 90.0 && coordinate.latitude() != -90.0)
        seed = hash(seed, coordinate.longitude());
    seed = hash(seed, coordinate.latitude());
    seed = hash(seed, coordinate.altitude());
    return seed;
}

QT_END_NAMESPACE
