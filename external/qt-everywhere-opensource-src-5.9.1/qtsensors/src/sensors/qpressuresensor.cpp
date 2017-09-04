/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#include <qpressuresensor.h>
#include "qpressuresensor_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QPressureReading)

/*!
    \class QPressureReading
    \ingroup sensors_reading
    \inmodule QtSensors
    \since 5.1

    \brief The QPressureReading class holds readings from the pressure sensor.

    \section2 QPressureReading Units

    The pressure sensor returns atmospheric pressure values in Pascals.
*/

/*!
    \property QPressureReading::pressure
    \brief The measured atmospheric pressure.

    Returned as Pascals.
    \sa {QPressureReading Units}
*/

qreal QPressureReading::pressure() const
{
    return d->pressure;
}

/*!
    Sets the pressure to \a pressure.
*/
void QPressureReading::setPressure(qreal pressure)
{
    d->pressure = pressure;
}

/*!
    \property QPressureReading::temperature
    \brief The pressure sensor's temperature.
    \since 5.2

    The temperature is returned in degree Celsius.
    This property, if supported, provides the pressure sensor die temperature.
    Note that this temperature may be (and usually is) different than the temperature
    reported from QAmbientTemperatureSensor.
    Use QSensor::isFeatureSupported() with the QSensor::PressureSensorTemperature
    flag to determine its availability.
*/

qreal QPressureReading::temperature() const
{
    return d->temperature;
}

/*!
    Sets the pressure sensor's temperature to \a temperature.
    \since 5.2
*/
void QPressureReading::setTemperature(qreal temperature)
{
    d->temperature =  temperature;
}

// =====================================================================

/*!
    \class QPressureFilter
    \ingroup sensors_filter
    \inmodule QtSensors
    \since 5.1

    \brief The QPressureFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QPressureReading
    instead of QSensorReading.
*/

/*!
    \fn QPressureFilter::filter(QPressureReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QPressureFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QPressureReading*>(reading));
}

char const * const QPressureSensor::type("QPressureSensor");

/*!
    \class QPressureSensor
    \ingroup sensors_type
    \inmodule QtSensors
    \since 5.1

    \brief The QPressureSensor class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    This class also features a reading() function that returns a QPressureReading instead of a QSensorReading.

    For details about how the sensor works, see \l QPressureReading.

    \sa QPressureReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QPressureSensor::QPressureSensor(QObject *parent)
    : QSensor(QPressureSensor::type, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QPressureSensor::~QPressureSensor()
{
}

/*!
    \fn QPressureSensor::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QPressureReading *QPressureSensor::reading() const
{
    return static_cast<QPressureReading*>(QSensor::reading());
}

#include "moc_qpressuresensor.cpp"
QT_END_NAMESPACE
