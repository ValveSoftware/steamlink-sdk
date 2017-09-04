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
#include <qambienttemperaturesensor.h>
#include "qambienttemperaturesensor_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QAmbientTemperatureReading)

/*!
    \class QAmbientTemperatureReading
    \ingroup sensors_reading
    \inmodule QtSensors
    \since 5.1

    \brief The QAmbientTemperatureReading class holds readings of the ambient temperature.

    The ambient (room) temperature is the temperature in degree Celsius.
*/

/*!
    \property QAmbientTemperatureReading::temperature
    \brief The ambient temperature

    Measured in degree Celsius.
*/

qreal QAmbientTemperatureReading::temperature() const
{
    return d->temperature;
}

/*!
    Sets ambient temperature to \a temperature.
*/
void QAmbientTemperatureReading::setTemperature(qreal temperature)
{
    d->temperature = temperature;
}

// =====================================================================

/*!
    \class QAmbientTemperatureFilter
    \ingroup sensors_filter
    \inmodule QtSensors
    \since 5.1

    \brief The QAmbientTemperatureFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QAmbientTemperatureReading
    instead of QSensorReading.
*/

/*!
    \fn QAmbientTemperatureFilter::filter(QAmbientTemperatureReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QAmbientTemperatureFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QAmbientTemperatureReading*>(reading));
}

char const * const QAmbientTemperatureSensor::type("QAmbientTemperatureSensor");

/*!
    \class QAmbientTemperatureSensor
    \ingroup sensors_type
    \inmodule QtSensors
    \since 5.1

    \brief The QAmbientTemperatureSensor class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    This class also features a reading() function that returns a QAmbientTemperatureReading instead of a QSensorReading.

    For details about how the sensor works, see \l QAmbientTemperatureReading.

    \sa QAmbientTemperatureReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QAmbientTemperatureSensor::QAmbientTemperatureSensor(QObject *parent)
    : QSensor(QAmbientTemperatureSensor::type, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QAmbientTemperatureSensor::~QAmbientTemperatureSensor()
{
}

/*!
    \fn QAmbientTemperatureSensor::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QAmbientTemperatureReading *QAmbientTemperatureSensor::reading() const
{
    return static_cast<QAmbientTemperatureReading*>(QSensor::reading());
}

#include "moc_qambienttemperaturesensor.cpp"
QT_END_NAMESPACE
