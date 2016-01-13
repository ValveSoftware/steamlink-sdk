/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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
