/****************************************************************************
**
** Copyright (C) 2016 Canonical, Ltd
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
#include <qlidsensor.h>
#include "qlidsensor_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QLidReading)

/*!
    \class QLidReading
    \ingroup sensors_reading
    \inmodule QtSensors
    \since 5.9

    \brief The QLidReading class holds readings from the Lid sensor.

    A normal laptop has what we call a front lid.

    If the laptop can be converted to a tablet by rotating or closing the lid
    where the display is out, this is known as a back lid.

    \section2 QLidReading Units
    The Lid sensor can detect if a device's lid is closed or not. A lid can be a laptop,
    a laptop that converts to a tablet, or even a cover for a tablet or phone.
*/

/*!
    \property QLidReading::backLidClosed
    \brief A value indicating whether the back lid is closed.
    A back lid can be when a convertable laptop is closed
    into to tablet mode without keyboard.

    \sa {QLidReading Units}
*/

bool QLidReading::backLidClosed() const
{
    return d->backLidClosed;
}

/*!
    Sets the backLidClosed value to \a closed.
*/
void QLidReading::setBackLidClosed(bool closed)
{
    d->backLidClosed = closed;
}

/*!
    \property QLidReading::frontLidClosed
    \brief A value indicating whether the front lid is closed.
    A front lid would be a normal laptop lid.
    \sa {QLidReading Units}
*/

bool QLidReading::frontLidClosed() const
{
    return d->frontLidClosed;
}

/*!
    Sets the frontLidClosed value to \a closed.
*/
void QLidReading::setFrontLidClosed(bool closed)
{
    d->frontLidClosed = closed;
}

// =====================================================================

/*!
    \class QLidFilter
    \ingroup sensors_filter
    \inmodule QtSensors
    \since 5.9

    \brief The QLidFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QLidReading
    instead of QSensorReading.
*/

/*!
    \fn QLidFilter::filter(QLidReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QLidFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QLidReading*>(reading));
}

char const * const QLidSensor::type("QLidSensor");

/*!
    \class QLidSensor
    \ingroup sensors_type
    \inmodule QtSensors
    \since 5.9

    \brief The QLidSensor class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    This class also features a reading() function that returns a QLidReading instead
    of a QSensorReading.

    For details about how the sensor works, see \l QLidReading.

    \sa QLidReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QLidSensor::QLidSensor(QObject *parent)
    : QSensor(QLidSensor::type, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QLidSensor::~QLidSensor()
{
}

/*!
    \fn QLidSensor::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QLidReading *QLidSensor::reading() const
{
    return static_cast<QLidReading*>(QSensor::reading());
}

#include "moc_qlidsensor.cpp"
QT_END_NAMESPACE
