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

#include "qgeocircle.h"
#include "qgeocircle_p.h"

#include "qgeocoordinate.h"
#include "qnumeric.h"
#include "qlocationutils_p.h"

#include "qdoublevector2d_p.h"
#include "qdoublevector3d_p.h"
#include <cmath>
QT_BEGIN_NAMESPACE

/*!
    \class QGeoCircle
    \inmodule QtPositioning
    \ingroup QtPositioning-positioning
    \since 5.2

    \brief The QGeoCircle class defines a circular geographic area.

    The circle is defined in terms of a QGeoCoordinate which specifies the
    center of the circle and a qreal which specifies the radius of the circle
    in meters.

    The circle is considered invalid if the center coordinate is invalid
    or if the radius is less than zero.

    This class is a \l Q_GADGET since Qt 5.5.  It can be
    \l{Cpp_value_integration_positioning}{directly used from C++ and QML}.
*/

/*!
    \property QGeoCircle::center
    \brief This property holds the center coordinate for the geo circle.

    The circle is considered invalid if this property contains an invalid
    coordinate.

    A default constructed QGeoCircle uses an invalid \l QGeoCoordinate
    as center.

    While this property is introduced in Qt 5.5, the related accessor functions
    exist since the first version of this class.

    \since 5.5
*/

/*!
    \property QGeoCircle::radius
    \brief This property holds the circle radius in meters.

    The circle is considered invalid if this property is negative.

    By default, the radius is initialized with \c -1.

    While this property is introduced in Qt 5.5, the related accessor functions
    exist since the first version of this class.

    \since 5.5
*/

inline QGeoCirclePrivate *QGeoCircle::d_func()
{
    return static_cast<QGeoCirclePrivate *>(d_ptr.data());
}

inline const QGeoCirclePrivate *QGeoCircle::d_func() const
{
    return static_cast<const QGeoCirclePrivate *>(d_ptr.constData());
}

struct CircleVariantConversions
{
    CircleVariantConversions()
    {
        QMetaType::registerConverter<QGeoShape, QGeoCircle>();
        QMetaType::registerConverter<QGeoCircle, QGeoShape>();
    }
};

Q_GLOBAL_STATIC(CircleVariantConversions, initCircleConversions)

/*!
    Constructs a new, invalid geo circle.
*/
QGeoCircle::QGeoCircle()
:   QGeoShape(new QGeoCirclePrivate)
{
    initCircleConversions();
}

/*!
    Constructs a new geo circle centered at \a center and with a radius of \a radius meters.
*/
QGeoCircle::QGeoCircle(const QGeoCoordinate &center, qreal radius)
{
    initCircleConversions();
    d_ptr = new QGeoCirclePrivate(center, radius);
}

/*!
    Constructs a new geo circle from the contents of \a other.
*/
QGeoCircle::QGeoCircle(const QGeoCircle &other)
:   QGeoShape(other)
{
    initCircleConversions();
}

/*!
    Constructs a new geo circle from the contents of \a other.
*/
QGeoCircle::QGeoCircle(const QGeoShape &other)
:   QGeoShape(other)
{
    initCircleConversions();
    if (type() != QGeoShape::CircleType)
        d_ptr = new QGeoCirclePrivate;
}

/*!
    Destroys this geo circle.
*/
QGeoCircle::~QGeoCircle() {}

/*!
    Assigns \a other to this geo circle and returns a reference to this geo circle.
*/
QGeoCircle &QGeoCircle::operator=(const QGeoCircle &other)
{
    QGeoShape::operator=(other);
    return *this;
}

/*!
    Returns whether this geo circle is equal to \a other.
*/
bool QGeoCircle::operator==(const QGeoCircle &other) const
{
    Q_D(const QGeoCircle);

    return *d == *other.d_func();
}

/*!
    Returns whether this geo circle is not equal to \a other.
*/
bool QGeoCircle::operator!=(const QGeoCircle &other) const
{
    Q_D(const QGeoCircle);

    return !(*d == *other.d_func());
}

bool QGeoCirclePrivate::isValid() const
{
    return m_center.isValid() && !qIsNaN(m_radius) && m_radius >= -1e-7;
}

bool QGeoCirclePrivate::isEmpty() const
{
    return !isValid() || m_radius <= 1e-7;
}

/*!
    Sets the center coordinate of this geo circle to \a center.
*/
void QGeoCircle::setCenter(const QGeoCoordinate &center)
{
    Q_D(QGeoCircle);

    d->setCenter(center);
}

/*!
    Returns the center coordinate of this geo circle. Equivalent to QGeoShape::center().
*/
QGeoCoordinate QGeoCircle::center() const
{
    Q_D(const QGeoCircle);

    return d->center();
}

/*!
    Sets the radius in meters of this geo circle to \a radius.
*/
void QGeoCircle::setRadius(qreal radius)
{
    Q_D(QGeoCircle);

    d->setRadius(radius);
}

/*!
    Returns the radius in meters of this geo circle.
*/
qreal QGeoCircle::radius() const
{
    Q_D(const QGeoCircle);

    return d->m_radius;
}

bool QGeoCirclePrivate::contains(const QGeoCoordinate &coordinate) const
{
    if (!isValid() || !coordinate.isValid())
        return false;

    // see QTBUG-41447 for details
    qreal distance = m_center.distanceTo(coordinate);
    if (qFuzzyCompare(distance, m_radius) || distance <= m_radius)
        return true;

    return false;
}

QGeoCoordinate QGeoCirclePrivate::center() const
{
    return m_center;
}

QGeoRectangle QGeoCirclePrivate::boundingGeoRectangle() const
{
    return m_bbox;
}

void QGeoCirclePrivate::updateBoundingBox()
{
    if (isEmpty()) {
        if (m_center.isValid()) {
            m_bbox.setTopLeft(m_center);
            m_bbox.setBottomRight(m_center);
        }
        return;
    }

    bool crossNorth = crossNorthPole();
    bool crossSouth = crossSouthPole();

    if (crossNorth && crossSouth) {
        // Circle crossing both poles fills the whole map
        m_bbox = QGeoRectangle(QGeoCoordinate(90.0, -180.0), QGeoCoordinate(-90.0, 180.0));
    } else if (crossNorth) {
        // Circle crossing one pole fills the map in the longitudinal direction
        m_bbox = QGeoRectangle(QGeoCoordinate(90.0, -180.0), QGeoCoordinate(m_center.atDistanceAndAzimuth(m_radius, 180.0).latitude(), 180.0));
    } else if (crossSouth) {
        m_bbox = QGeoRectangle(QGeoCoordinate(m_center.atDistanceAndAzimuth(m_radius, 0.0).latitude(), -180.0), QGeoCoordinate(-90, 180.0));
    } else {
        // Regular circle not crossing anything

        // Calculate geo bounding box of the circle
        //
        // A circle tangential point with a meridian, together with pole and
        // the circle center create a spherical triangle.
        // Finding the tangential point with the spherical law of sines:
        //
        // * lon_delta_in_rad : delta between the circle center and a tangential
        //   point (absolute value).
        // * r_in_rad : angular radius of the circle
        // * lat_in_rad : latitude of the circle center
        // * alpha_in_rad : angle between meridian and radius of the circle.
        //   At the tangential point, sin(alpha_in_rad) == 1.
        // * lat_delta_in_rad - absolute delta of latitudes between the circle center and
        //   any of the two points where the great circle going through the circle
        //   center and the pole crosses the circle. In other words, the points
        //   on the circle with azimuth 0 or 180.
        //
        //  Using:
        //  sin(lon_delta_in_rad)/sin(r_in_rad) = sin(alpha_in_rad)/sin(pi/2 - lat_in_rad)

        double r_in_rad = m_radius / QLocationUtils::earthMeanRadius(); // angular r
        double lat_delta_in_deg = QLocationUtils::degrees(r_in_rad);
        double lon_delta_in_deg = QLocationUtils::degrees(std::asin(
               std::sin(r_in_rad) /
               std::cos(QLocationUtils::radians(m_center.latitude()))
            ));

        QGeoCoordinate topLeft;
        topLeft.setLatitude(QLocationUtils::clipLat(m_center.latitude() + lat_delta_in_deg));
        topLeft.setLongitude(QLocationUtils::wrapLong(m_center.longitude() - lon_delta_in_deg));
        QGeoCoordinate bottomRight;
        bottomRight.setLatitude(QLocationUtils::clipLat(m_center.latitude() - lat_delta_in_deg));
        bottomRight.setLongitude(QLocationUtils::wrapLong(m_center.longitude() + lon_delta_in_deg));

        m_bbox = QGeoRectangle(topLeft, bottomRight);
    }
}

void QGeoCirclePrivate::setCenter(const QGeoCoordinate &c)
{
    m_center = c;
    updateBoundingBox();
}

void QGeoCirclePrivate::setRadius(const qreal r)
{
    m_radius = r;
    updateBoundingBox();
}

bool QGeoCirclePrivate::crossNorthPole() const
{
    const QGeoCoordinate northPole(90.0, m_center.longitude());
    qreal distanceToPole = m_center.distanceTo(northPole);
    if (distanceToPole < m_radius)
        return true;
    return false;
}

bool QGeoCirclePrivate::crossSouthPole() const
{
    const QGeoCoordinate southPole(-90.0, m_center.longitude());
    qreal distanceToPole = m_center.distanceTo(southPole);
    if (distanceToPole < m_radius)
        return true;
    return false;
}

/*!
  Extends the circle to include \a coordinate
*/
void QGeoCirclePrivate::extendShape(const QGeoCoordinate &coordinate)
{
    if (!isValid() || !coordinate.isValid() || contains(coordinate))
        return;

    setRadius(m_center.distanceTo(coordinate));
}

/*!
    Translates this geo circle by \a degreesLatitude northwards and \a degreesLongitude eastwards.

    Negative values of \a degreesLatitude and \a degreesLongitude correspond to
    southward and westward translation respectively.
*/
void QGeoCircle::translate(double degreesLatitude, double degreesLongitude)
{
    // TODO handle dlat, dlon larger than 360 degrees

    Q_D(QGeoCircle);

    double lat = d->m_center.latitude();
    double lon = d->m_center.longitude();

    lat += degreesLatitude;
    lon += degreesLongitude;
    lon = QLocationUtils::wrapLong(lon);

    // TODO: remove this and simply clip latitude.
    if (lat > 90.0) {
        lat = 180.0 - lat;
        if (lon < 0.0)
            lon = 180.0;
        else
            lon -= 180;
    }

    if (lat < -90.0) {
        lat = 180.0 + lat;
        if (lon < 0.0)
            lon = 180.0;
        else
            lon -= 180;
    }

    d->setCenter(QGeoCoordinate(lat, lon));
}

/*!
    Returns a copy of this geo circle translated by \a degreesLatitude northwards and
    \a degreesLongitude eastwards.

    Negative values of \a degreesLatitude and \a degreesLongitude correspond to
    southward and westward translation respectively.

    \sa translate()
*/
QGeoCircle QGeoCircle::translated(double degreesLatitude, double degreesLongitude) const
{
    QGeoCircle result(*this);
    result.translate(degreesLatitude, degreesLongitude);
    return result;
}

/*!
    Extends the geo circle to also cover the coordinate \a coordinate

    \since 5.9
*/
void QGeoCircle::extendCircle(const QGeoCoordinate &coordinate)
{
    Q_D(QGeoCircle);
    d->extendShape(coordinate);
}

/*!
    Returns the geo circle properties as a string.

    \since 5.5
*/

QString QGeoCircle::toString() const
{
    if (type() != QGeoShape::CircleType) {
        qWarning("Not a circle");
        return QStringLiteral("QGeoCircle(not a circle)");
    }

    return QStringLiteral("QGeoCircle({%1, %2}, %3)")
        .arg(center().latitude())
        .arg(center().longitude())
        .arg(radius());
}

/*******************************************************************************
*******************************************************************************/

QGeoCirclePrivate::QGeoCirclePrivate()
:   QGeoShapePrivate(QGeoShape::CircleType), m_radius(-1.0)
{
}

QGeoCirclePrivate::QGeoCirclePrivate(const QGeoCoordinate &center, qreal radius)
:   QGeoShapePrivate(QGeoShape::CircleType), m_center(center), m_radius(radius)
{
    updateBoundingBox();
}

QGeoCirclePrivate::QGeoCirclePrivate(const QGeoCirclePrivate &other)
:   QGeoShapePrivate(QGeoShape::CircleType), m_center(other.m_center),
    m_radius(other.m_radius), m_bbox(other.m_bbox)
{
}

QGeoCirclePrivate::~QGeoCirclePrivate() {}

QGeoShapePrivate *QGeoCirclePrivate::clone() const
{
    return new QGeoCirclePrivate(*this);
}

bool QGeoCirclePrivate::operator==(const QGeoShapePrivate &other) const
{
    if (!QGeoShapePrivate::operator==(other))
        return false;

    const QGeoCirclePrivate &otherCircle = static_cast<const QGeoCirclePrivate &>(other);

    return m_radius == otherCircle.m_radius && m_center == otherCircle.m_center;
}

QT_END_NAMESPACE
