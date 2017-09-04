/****************************************************************************
**
** Copyright (C) 2016 Canonical Ltd
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

#include <qhumiditysensor.h>
#include "qhumiditysensor_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QHumidityReading)

/*!
    \class QHumidityReading
    \ingroup sensors_reading
    \inmodule QtSensors
    \since 5.9

    \brief The QHumidityReading class holds readings from the humidity sensor.

    \section2 QHumidityReading Units

    The humidity sensor returns the relative humidity as a percentage, and absolute humidity in
    grams per cubic meter (g/m3).
    Note that some sensors may not support absolute humidity, 0 will be returned in this case.
*/

/*!
    \property QHumidityReading::relativeHumidity
    \brief Relative humidity
    Returned as a percentage.

    \sa {QHumidityReading Units}
*/

qreal QHumidityReading::relativeHumidity() const
{
    return d->relativeHumidity;
}

/*!
    Sets relativeHumidity to \a humidity.
*/
void QHumidityReading::setRelativeHumidity(qreal humidity)
{
    d->relativeHumidity = humidity;
}

/*!
    \property QHumidityReading::absoluteHumidity
    \brief Absolute humidity
    Measured in grams per cubic meter.
    Note that some sensors may not support absolute humidity.

    \sa {QHumidityReading Units}
*/

qreal QHumidityReading::absoluteHumidity() const
{
    return d->absoluteHumidity;
}

/*!
    Sets absoluteHumidity to \a value.
*/
void QHumidityReading::setAbsoluteHumidity(qreal value)
{
    d->absoluteHumidity = value;
}

// =====================================================================

/*!
    \class QHumidityFilter
    \ingroup sensors_filter
    \inmodule QtSensors
    \since 5.9

    \brief The QHumidityFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QHumidityReading
    instead of QSensorReading.
*/

/*!
    \fn QHumidityFilter::filter(QHumidityReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QHumidityFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QHumidityReading*>(reading));
}

char const * const QHumiditySensor::type("QHumiditySensor");


/*!
    \class QHumiditySensor
    \ingroup sensors_type
    \inmodule QtSensors
    \since 5.9

    \brief The QHumiditySensor class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    This class also features a reading() function that returns a QHumidityReading instead of a QSensorReading.

    For details about how the sensor works, see \l QHumidityReading.

    \sa QHumidityReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QHumiditySensor::QHumiditySensor(QObject *parent)
    : QSensor(QHumiditySensor::type, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QHumiditySensor::~QHumiditySensor()
{
}

/*!
    \fn QHumiditySensor::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QHumidityReading *QHumiditySensor::reading() const
{
    return static_cast<QHumidityReading*>(QSensor::reading());
}

#include "moc_qhumiditysensor.cpp"
QT_END_NAMESPACE
