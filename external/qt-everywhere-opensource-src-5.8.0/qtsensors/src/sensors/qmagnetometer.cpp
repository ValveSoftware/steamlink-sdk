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

#include "qmagnetometer.h"
#include "qmagnetometer_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QMagnetometerReading)

/*!
    \class QMagnetometerReading
    \ingroup sensors_reading
    \inmodule QtSensors
    \since 5.1

    \brief The QMagnetometerReading class represents one reading from the
           magnetometer.

    \section2 QMagnetometerReading Units
    The magnetometer returns magnetic flux density values along 3 axes.
    The scale of the values is teslas. The axes are arranged as follows.

    \image sensors-coordinates2.jpg

    The magnetometer can report on either raw magnetic flux values or geomagnetic flux values.
    By default it returns raw magnetic flux values. The QMagnetometer::returnGeoValues property
    must be set to return geomagnetic flux values.

    The primary difference between raw and geomagnetic values is that extra processing
    is done to eliminate local magnetic interference from the geomagnetic values so they
    represent only the effect of the Earth's magnetic field. This process is not perfect
    and the accuracy of each reading may change.

    The image below shows the difference between geomagnetic (on the left) and raw (on the right)
    readings for a phone that is being subjected to magnetic interference.

    \image sensors-geo-vs-raw-magnetism.jpg

    The accuracy of each reading is measured as a number from 0 to 1.
    A value of 1 is the highest level that the device can support and 0 is
    the worst.

    \section2 Calibration
    If the device is reporting low accuracy, then calibration might be needed before acceptable measurements
    can be provided.
    Basic calibration can usually be done by either rotating your device in a figure of eight, or by
    rotating the device along each of its three axes. For more information, check your device's
    documentation on how to calibrate the magnetic sensor.
*/

/*!
    \property QMagnetometerReading::x
    \brief the raw magnetic flux density on the X axis.

    Measured as teslas.
    \sa {QMagnetometerReading Units}
*/

qreal QMagnetometerReading::x() const
{
    return d->x;
}

/*!
    Sets the raw magnetic flux density on the X axis to \a x.
*/
void QMagnetometerReading::setX(qreal x)
{
    d->x = x;
}

/*!
    \property QMagnetometerReading::y
    \brief the raw magnetic flux density on the Y axis.

    Measured as teslas.
    \sa {QMagnetometerReading Units}
*/

qreal QMagnetometerReading::y() const
{
    return d->y;
}

/*!
    Sets the raw magnetic flux density on the Y axis to \a y.
*/
void QMagnetometerReading::setY(qreal y)
{
    d->y = y;
}

/*!
    \property QMagnetometerReading::z
    \brief the raw magnetic flux density on the Z axis.

    Measured as teslas.
    \sa {QMagnetometerReading Units}
*/

qreal QMagnetometerReading::z() const
{
    return d->z;
}

/*!
    Sets the raw magnetic flux density on the Z axis to \a z.
*/
void QMagnetometerReading::setZ(qreal z)
{
    d->z = z;
}

/*!
    \property QMagnetometerReading::calibrationLevel
    \brief the accuracy of the reading.

    Measured as a value from 0 to 1 with higher values being better.

    Note that this only changes when measuring geomagnetic flux density.
    Raw magnetic flux readings will always have a value of 1.
    \sa {QMagnetometerReading Units}, {Calibration}
*/

qreal QMagnetometerReading::calibrationLevel() const
{
    return d->calibrationLevel;
}

/*!
    Sets the accuracy of the reading to \a calibrationLevel.
*/
void QMagnetometerReading::setCalibrationLevel(qreal calibrationLevel)
{
    d->calibrationLevel = calibrationLevel;
}

// =====================================================================

/*!
    \class QMagnetometerFilter
    \ingroup sensors_filter
    \inmodule QtSensors
    \since 5.1

    \brief The QMagnetometerFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QMagnetometerReading
    instead of QSensorReading.
*/

/*!
    \fn QMagnetometerFilter::filter(QMagnetometerReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QMagnetometerFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QMagnetometerReading*>(reading));
}

char const * const QMagnetometer::type("QMagnetometer");

/*!
    \class QMagnetometer
    \ingroup sensors_type
    \inmodule QtSensors
    \since 5.1

    \brief The QMagnetometer class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    This class also features a reading() function that returns a QMagnetometerReading instead of a QSensorReading.

    For details about how the sensor works, see \l QMagnetometerReading.

    \sa QMagnetometerReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QMagnetometer::QMagnetometer(QObject *parent)
    : QSensor(QMagnetometer::type, *new QMagnetometerPrivate, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QMagnetometer::~QMagnetometer()
{
}

/*!
    \fn QMagnetometer::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QMagnetometerReading *QMagnetometer::reading() const
{
    return static_cast<QMagnetometerReading*>(QSensor::reading());
}

/*!
    \property QMagnetometer::returnGeoValues
    \brief a value indicating if geomagnetic values should be returned.

    Set to true to return geomagnetic flux density.
    Set to false (the default) to return raw magnetic flux density.

    The property must be set before calling start().
*/

bool QMagnetometer::returnGeoValues() const
{
    Q_D(const QMagnetometer);
    return d->returnGeoValues;
}

void QMagnetometer::setReturnGeoValues(bool returnGeoValues)
{
    Q_D(QMagnetometer);
    if (d->returnGeoValues != returnGeoValues) {
        d->returnGeoValues = returnGeoValues;
        emit returnGeoValuesChanged(returnGeoValues);
    }
}

#include "moc_qmagnetometer.cpp"
QT_END_NAMESPACE

