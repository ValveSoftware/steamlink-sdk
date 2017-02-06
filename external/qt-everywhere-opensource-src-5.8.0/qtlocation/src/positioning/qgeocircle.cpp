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

#include "qdoublevector2d_p.h"
#include "qdoublevector3d_p.h"
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
    return m_center.isValid() && !qIsNaN(radius) && radius >= -1e-7;
}

bool QGeoCirclePrivate::isEmpty() const
{
    return !isValid() || radius <= 1e-7;
}

/*!
    Sets the center coordinate of this geo circle to \a center.
*/
void QGeoCircle::setCenter(const QGeoCoordinate &center)
{
    Q_D(QGeoCircle);

    d->m_center = center;
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

    d->radius = radius;
}

/*!
    Returns the radius in meters of this geo circle.
*/
qreal QGeoCircle::radius() const
{
    Q_D(const QGeoCircle);

    return d->radius;
}

bool QGeoCirclePrivate::contains(const QGeoCoordinate &coordinate) const
{
    if (!isValid() || !coordinate.isValid())
        return false;

    // see QTBUG-41447 for details
    qreal distance = m_center.distanceTo(coordinate);
    if (qFuzzyCompare(distance, radius) || distance <= radius)
        return true;

    return false;
}

QGeoCoordinate QGeoCirclePrivate::center() const
{
    return m_center;
}

/*!
  Extends the circle to include \a coordinate
*/
void QGeoCirclePrivate::extendShape(const QGeoCoordinate &coordinate)
{
    if (!isValid() || !coordinate.isValid() || contains(coordinate))
        return;

    radius = m_center.distanceTo(coordinate);
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

    if (lon < -180.0)
        lon += 360.0;
    if (lon > 180.0)
        lon -= 360.0;

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

    d->m_center = QGeoCoordinate(lat, lon);
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
:   QGeoShapePrivate(QGeoShape::CircleType), radius(-1.0)
{
}

QGeoCirclePrivate::QGeoCirclePrivate(const QGeoCoordinate &center, qreal radius)
:   QGeoShapePrivate(QGeoShape::CircleType), m_center(center), radius(radius)
{
}

QGeoCirclePrivate::QGeoCirclePrivate(const QGeoCirclePrivate &other)
:   QGeoShapePrivate(QGeoShape::CircleType), m_center(other.m_center),
    radius(other.radius)
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

    return radius == otherCircle.radius && m_center == otherCircle.m_center;
}

QT_END_NAMESPACE

