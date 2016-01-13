 /****************************************************************************
 **
 ** Copyright (C) 2014 BlackBerry Limited. All rights reserved.
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

#include <qdistancesensor.h>
#include "qdistancesensor_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QDistanceReading)

/*!
    \class QDistanceReading
    \ingroup sensors_reading
    \inmodule QtSensors
    \since 5.4

    \brief The QDistanceReading class holds distance reading in cm from the proximity sensor.

    The QDistanceReading class holds distance reading in cm from the proximity sensor.
    Note: Some proximity sensor only support two values for distance, a near and far value.
    In this case, the sensor should report its maximum range value to represent the far state,
    and a lesser value to represent the near state.

    \section2 QDistanceReading Units
    The distance is measured in cm

    The distance sensor is typically located in the front face of a device, and thus will
    measure the distance of an object from the device's front face.
*/

/*!
    \property QDistanceReading::distance
    \brief distance of object from front face of device

    \sa {QDistanceReading Units}
*/

qreal QDistanceReading::distance() const
{
    return d->distance;
}

/*!
    Sets distance to \a distance.
*/
void QDistanceReading::setDistance(qreal distance)
{
    d->distance = distance;
}

// =====================================================================

/*!
    \class QDistanceFilter
    \ingroup sensors_filter
    \inmodule QtSensors
    \since 5.4

    \brief The QDistanceFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QDistanceReading
    instead of QSensorReading.
*/

/*!
    \fn QDistanceFilter::filter(QDistanceReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QDistanceFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QDistanceReading*>(reading));
}

char const * const QDistanceSensor::type("QDistanceSensor");

/*!
    \class QDistanceSensor
    \ingroup sensors_type
    \inmodule QtSensors
    \since 5.4

    \brief The QDistanceSensor class is a convenience wrapper around QSensor.

    The only behavioral difference is that this class sets the type properly.

    This class also features a reading() function that returns a QDistanceReading instead of a QSensorReading.

    For details about how the sensor works, see \l QDistanceReading.

    \sa QDistanceReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QDistanceSensor::QDistanceSensor(QObject *parent)
    : QSensor(QDistanceSensor::type, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QDistanceSensor::~QDistanceSensor()
{
}

/*!
    \fn QDistanceSensor::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QDistanceReading *QDistanceSensor::reading() const
{
    return static_cast<QDistanceReading*>(QSensor::reading());
}

#include "moc_qdistancesensor.cpp"
QT_END_NAMESPACE
