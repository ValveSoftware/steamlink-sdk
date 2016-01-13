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
*/

inline QGeoCirclePrivate *QGeoCircle::d_func()
{
    return static_cast<QGeoCirclePrivate *>(d_ptr.data());
}

inline const QGeoCirclePrivate *QGeoCircle::d_func() const
{
    return static_cast<const QGeoCirclePrivate *>(d_ptr.constData());
}

/*!
    Constructs a new, invalid geo circle.
*/
QGeoCircle::QGeoCircle()
:   QGeoShape(new QGeoCirclePrivate)
{
}

/*!
    Constructs a new geo circle centered at \a center and with a radius of \a radius meters.
*/
QGeoCircle::QGeoCircle(const QGeoCoordinate &center, qreal radius)
{
    d_ptr = new QGeoCirclePrivate(center, radius);
}

/*!
    Constructs a new geo circle from the contents of \a other.
*/
QGeoCircle::QGeoCircle(const QGeoCircle &other)
:   QGeoShape(other)
{
}

/*!
    Constructs a new geo circle from the contents of \a other.
*/
QGeoCircle::QGeoCircle(const QGeoShape &other)
:   QGeoShape(other)
{
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
    return center.isValid() && !qIsNaN(radius) && radius >= -1e-7;
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

    d->center = center;
}

/*!
    Returns the center coordinate of this geo circle.
*/
QGeoCoordinate QGeoCircle::center() const
{
    Q_D(const QGeoCircle);

    return d->center;
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
    qreal distance = center.distanceTo(coordinate);
    if (qFuzzyCompare(distance, radius) || distance <= radius)
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

    radius = center.distanceTo(coordinate);
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

    double lat = d->center.latitude();
    double lon = d->center.longitude();

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

    d->center = QGeoCoordinate(lat, lon);
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

/*******************************************************************************
*******************************************************************************/

QGeoCirclePrivate::QGeoCirclePrivate()
:   QGeoShapePrivate(QGeoShape::CircleType), radius(-1.0)
{
}

QGeoCirclePrivate::QGeoCirclePrivate(const QGeoCoordinate &center, qreal radius)
:   QGeoShapePrivate(QGeoShape::CircleType), center(center), radius(radius)
{
}

QGeoCirclePrivate::QGeoCirclePrivate(const QGeoCirclePrivate &other)
:   QGeoShapePrivate(QGeoShape::CircleType), center(other.center),
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

    return radius == otherCircle.radius && center == otherCircle.center;
}

QT_END_NAMESPACE

