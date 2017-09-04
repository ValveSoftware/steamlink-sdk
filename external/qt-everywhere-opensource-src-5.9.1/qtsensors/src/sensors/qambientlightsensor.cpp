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

#include "qambientlightsensor.h"
#include "qambientlightsensor_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QAmbientLightReading)

/*!
    \class QAmbientLightReading
    \ingroup sensors_reading
    \inmodule QtSensors
    \since 5.1

    \brief The QAmbientLightReading class represents one reading from the
           ambient light sensor.

    \section2 QAmbientLightReading Units
    The ambient light sensor returns the intensity of the ambient light
    using the pre-defined values found in the QAmbientLightReading::LightLevel
    enum.
*/

/*!
    \enum QAmbientLightReading::LightLevel

    This enum represents the ambient light level.

    \value Undefined  The light level is unknown.
    \value Dark       It is dark.
    \value Twilight   It is moderately dark.
    \value Light      It is light (eg. internal lights).
    \value Bright     It is bright (eg. shade).
    \value Sunny      It is very bright (eg. direct sunlight).
*/

/*!
    \property QAmbientLightReading::lightLevel
    \brief the ambient light level.

    The value represents the ambient light and comes from QAmbientLightReading::LightLevel.
    \sa {QAmbientLightReading Units}
*/

QAmbientLightReading::LightLevel QAmbientLightReading::lightLevel() const
{
    return static_cast<LightLevel>(d->lightLevel);
}

/*!
    Sets the ambient light level to \a lightLevel.
*/
void QAmbientLightReading::setLightLevel(QAmbientLightReading::LightLevel lightLevel)
{
    switch (lightLevel) {
    case Dark:
    case Twilight:
    case Light:
    case Bright:
    case Sunny:
        d->lightLevel = lightLevel;
        break;
    default:
        d->lightLevel = Undefined;
        break;
    }
}

// =====================================================================

/*!
    \class QAmbientLightFilter
    \ingroup sensors_filter
    \inmodule QtSensors
    \since 5.1

    \brief The QAmbientLightFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QAmbientLightReading
    instead of QSensorReading.
*/

/*!
    \fn QAmbientLightFilter::filter(QAmbientLightReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QAmbientLightFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QAmbientLightReading*>(reading));
}

char const * const QAmbientLightSensor::type("QAmbientLightSensor");

/*!
    \class QAmbientLightSensor
    \ingroup sensors_type
    \inmodule QtSensors
    \since 5.1

    \brief The QAmbientLightSensor class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    This class also features a reading() function that returns a QAmbientLightReading instead of a QSensorReading.

    For details about how the sensor works, see \l QAmbientLightReading.

    \sa QAmbientLightReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QAmbientLightSensor::QAmbientLightSensor(QObject *parent)
    : QSensor(QAmbientLightSensor::type, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QAmbientLightSensor::~QAmbientLightSensor()
{
}

/*!
    \fn QAmbientLightSensor::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QAmbientLightReading *QAmbientLightSensor::reading() const
{
    return static_cast<QAmbientLightReading*>(QSensor::reading());
}

#include "moc_qambientlightsensor.cpp"
QT_END_NAMESPACE

