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

#include "qorientationsensor.h"
#include "qorientationsensor_p.h"

QT_BEGIN_NAMESPACE

IMPLEMENT_READING(QOrientationReading)

/*!
    \class QOrientationReading
    \ingroup sensors_reading
    \inmodule QtSensors
    \since 5.1

    \brief The QOrientationReading class represents one reading from the
           orientation sensor.

    The orientation sensor reports the orientation of the device. As it operates below
    the UI level it does not report on or even know how the UI is rotated. Most importantly
    this means that this sensor cannot be used to detect if a device is in portrait or
    landscape mode.

    This sensor is useful to detect that a particular side of the device is pointing up.

    \section2 QOrientationReading Units
    The orientation sensor returns the orientation of the device using
    the pre-defined values found in the QOrientationReading::Orientation
    enum.
*/

/*!
    \enum QOrientationReading::Orientation

    This enum represents the orientation of the device.

    To explain the meaning of each value it is helpful to refer to the following diagram.

    \image sensors-sides.jpg

    The orientations are shown here in order: TopUp, TopDown, LeftUp, RightUp, FaceUp, FaceDown.

    \image sensors-orientation.jpg

    \value Undefined        The orientation is unknown.
    \value TopUp            The Top edge of the device is pointing up.
    \value TopDown          The Top edge of the device is pointing down.
    \value LeftUp           The Left edge of the device is pointing up.
    \value RightUp          The Right edge of the device is pointing up.
    \value FaceUp           The Face of the device is pointing up.
    \value FaceDown         The Face of the device is pointing down.

    It should be noted that the orientation sensor reports the orientation of the device
    and not the UI. The orientation of the device will not change just because the UI is
    rotated. This means this sensor cannot be used to detect if a device is in portrait
    or landscape mode.
*/

/*!
    \property QOrientationReading::orientation
    \brief the orientation of the device.

    The unit is an enumeration describing the orientation of the device.

    \sa {QOrientationReading Units}
*/

QOrientationReading::Orientation QOrientationReading::orientation() const
{
    return static_cast<QOrientationReading::Orientation>(d->orientation);
}

/*!
    Sets the \a orientation for the reading.
*/
void QOrientationReading::setOrientation(QOrientationReading::Orientation orientation)
{
    switch (orientation) {
    case TopUp:
    case TopDown:
    case LeftUp:
    case RightUp:
    case FaceUp:
    case FaceDown:
        d->orientation = orientation;
        break;
    default:
        d->orientation = Undefined;
        break;
    }
}

// =====================================================================

/*!
    \class QOrientationFilter
    \ingroup sensors_filter
    \inmodule QtSensors
    \since 5.1

    \brief The QOrientationFilter class is a convenience wrapper around QSensorFilter.

    The only difference is that the filter() method features a pointer to QOrientationReading
    instead of QSensorReading.
*/

/*!
    \fn QOrientationFilter::filter(QOrientationReading *reading)

    Called when \a reading changes. Returns false to prevent the reading from propagating.

    \sa QSensorFilter::filter()
*/

bool QOrientationFilter::filter(QSensorReading *reading)
{
    return filter(static_cast<QOrientationReading*>(reading));
}

char const * const QOrientationSensor::type("QOrientationSensor");

/*!
    \class QOrientationSensor
    \ingroup sensors_type
    \inmodule QtSensors
    \since 5.1

    \brief The QOrientationSensor class is a convenience wrapper around QSensor.

    The only behavioural difference is that this class sets the type properly.

    This class also features a reading() function that returns a QOrientationReading instead of a QSensorReading.

    For details about how the sensor works, see \l QOrientationReading.

    \sa QOrientationReading
*/

/*!
    Construct the sensor as a child of \a parent.
*/
QOrientationSensor::QOrientationSensor(QObject *parent)
    : QSensor(QOrientationSensor::type, parent)
{
}

/*!
    Destroy the sensor. Stops the sensor if it has not already been stopped.
*/
QOrientationSensor::~QOrientationSensor()
{
}

/*!
    \fn QOrientationSensor::reading() const

    Returns the reading class for this sensor.

    \sa QSensor::reading()
*/

QOrientationReading *QOrientationSensor::reading() const
{
    return static_cast<QOrientationReading*>(QSensor::reading());
}

#include "moc_qorientationsensor.cpp"
QT_END_NAMESPACE

