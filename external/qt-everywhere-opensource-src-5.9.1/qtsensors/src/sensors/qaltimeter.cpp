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
#include <qaltimeter.h>
#include "qaltimeter_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QAltimeterReading)

/*!
    \class QAltimeterReading
    \ingroup sensors_reading
    \inmodule QtSensors
    \since 5.1

    \brief The QAltimeterReading class holds readings from the altimeter sensor.

    The altitude is reported in meters relative to mean sea level.
*/

/*!
    \property QAltimeterReading::altitude
    \brief The altitude in meters relative to mean sea level.
*/

qreal QAltimeterReading::altitude() const
{
    return d->altitude;
}

/*!
    Sets the altitude to \a altitude.
*/
void QAltimeterReading::setAltitude(qreal altitude)
{
    d->altitude = altitude;
}

// =====================================================================

/*!
    \class QAltimeterFilter
    \ingroup sensors_filter
    \inmodule QtSensors
    \since 5.1

    \brief The QAltimeterFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QAltimeterReading
    instead of QSensorReading.
*/

/*!
    \fn QAltimeterFilter::filter(QAltimeterReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QAltimeterFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QAltimeterReading*>(reading));
}

char const * const QAltimeter::type("QAltimeter");

/*!
    \class QAltimeter
    \ingroup sensors_type
    \inmodule QtSensors
    \since 5.1

    \brief The QAltimeter class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    This class also features a reading() function that returns a QAltimeterReading instead of a QSensorReading.

    For details about how the sensor works, see \l QAltimeterReading.

    \sa QAltimeterReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QAltimeter::QAltimeter(QObject *parent)
    : QSensor(QAltimeter::type, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QAltimeter::~QAltimeter()
{
}

/*!
    \fn QAltimeter::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QAltimeterReading *QAltimeter::reading() const
{
    return static_cast<QAltimeterReading*>(QSensor::reading());
}

#include "moc_qaltimeter.cpp"
QT_END_NAMESPACE
