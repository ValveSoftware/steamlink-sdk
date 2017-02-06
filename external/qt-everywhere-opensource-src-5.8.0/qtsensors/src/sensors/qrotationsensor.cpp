/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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

#include "qrotationsensor.h"
#include "qrotationsensor_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QRotationReading)

/*!
    \class QRotationReading
    \ingroup sensors_reading
    \inmodule QtSensors
    \since 5.1

    \brief The QRotationReading class represents one reading from the
           rotation sensor.

    \section2 QRotationReading Units

    The rotation reading contains 3 angles, measured in degrees that define
    the orientation of the device in three-dimensional space. These angles
    are similar to yaw, pitch and roll but are defined using only right hand
    rotation with axes as defined by the right hand cartesian coordinate system.

    \image sensors-rotation.jpg

    The three angles are applied to the device in the following order.

    \list
    \li Right-handed rotation z (-180, 180]. Starting from the y-axis and
     incrementing in the counter-clockwise direction.
    \li Right-handed rotation x [-90, 90]. Starting from the new
     (once-rotated) y-axis and incrementing towards the z-axis.
    \li Right-handed rotation y (-180, 180]. Starting from the new
     (twice-rotated) z-axis and incrementing towards the x-axis.
    \endlist

    Here is a visualization showing the order in which angles are applied.

    \image sensors-rotation-anim.gif

    The 0 point for the z angle is defined as a fixed, external entity and
    is device-specific. While magnetic North is typically used as this
    reference point it may not be. Do not attempt to compare values
    for the z angle between devices or even on the same device if it has
    moved a significant distance.

    If the device cannot detect a fixed, external entity the z angle will
    always be 0 and the QRotationSensor::hasZ property will be set to false.

    The 0 point for the x and y angles are defined as when the x and y axes
    of the device are oriented towards the horizon. Here is an example of
    how the x value will change with device movement.

    \image sensors-rotation2.jpg

    Here is an example of how the y value will change with device movement.

    \image sensors-rotation3.jpg

    Note that when x is 90 or -90, values for z and y achieve rotation around
    the same axis (due to the order of operations). In this case the y
    rotation will be 0.
*/

/*!
    \property QRotationReading::x
    \brief the rotation around the x axis.

    Measured as degrees.
    \sa {QRotationReading Units}
*/

qreal QRotationReading::x() const
{
    return d->x;
}

/*!
    \property QRotationReading::y
    \brief the rotation around the y axis.

    Measured as degrees.
    \sa {QRotationReading Units}
*/

qreal QRotationReading::y() const
{
    return d->y;
}

/*!
    \property QRotationReading::z
    \brief the rotation around the z axis.

    Measured as degrees.
    \sa {QRotationReading Units}
*/

qreal QRotationReading::z() const
{
    return d->z;
}

/*!
   \brief Sets the rotation from three euler angles.

   This is to be called from the backend.

   The angles are measured in degrees. The order of the rotations matters, as first the \a z rotation
   is applied, then the \a x rotation and finally the \a y rotation.

   \since 5.0
 */
void QRotationReading::setFromEuler(qreal x, qreal y, qreal z)
{
    d->x = x;
    d->y = y;
    d->z = z;
}

// =====================================================================

/*!
    \class QRotationFilter
    \ingroup sensors_filter
    \inmodule QtSensors
    \since 5.1

    \brief The QRotationFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QRotationReading
    instead of QSensorReading.
*/

/*!
    \fn QRotationFilter::filter(QRotationReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QRotationFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QRotationReading*>(reading));
}

char const * const QRotationSensor::type("QRotationSensor");

/*!
    \class QRotationSensor
    \ingroup sensors_type
    \inmodule QtSensors
    \since 5.1

    \brief The QRotationSensor class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    This class also features a reading() function that returns a QRotationReading instead of a QSensorReading.

    For details about how the sensor works, see \l QRotationReading.

    \sa QRotationReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QRotationSensor::QRotationSensor(QObject *parent)
    : QSensor(QRotationSensor::type, *new QRotationSensorPrivate, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QRotationSensor::~QRotationSensor()
{
}

/*!
    \fn QRotationSensor::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QRotationReading *QRotationSensor::reading() const
{
    return static_cast<QRotationReading*>(QSensor::reading());
}

/*!
    \property QRotationSensor::hasZ
    \brief a value indicating if the z angle is available.

    Returns true if z is available.
    Returns false if z is not available.
*/

bool QRotationSensor::hasZ() const
{
    Q_D(const QRotationSensor);
    return (d->hasZ);
}

/*!
    \since 5.1

    Sets whether the z angle is available to \a hasZ. This is to be called from the
    backend. By default the hasZ property is true, so a backend only has to
    call this if its rotation sensor can not report z angles.
*/
void QRotationSensor::setHasZ(bool hasZ)
{
    Q_D(QRotationSensor);
    if (d->hasZ != hasZ) {
        d->hasZ = hasZ;
        emit hasZChanged(hasZ);
    }
}

#include "moc_qrotationsensor.cpp"
QT_END_NAMESPACE

