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

    On BlackBerry, the altimeter uses a combination of pressure and location to determine
    the altitude, as using pressure alone would yield to inaccurate results due to changes
    in air pressure caused by the weather. The location information is used to compensate for
    the weather. This requires that the user has enabled location services in the global
    settings.
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
