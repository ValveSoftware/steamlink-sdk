/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "qmltiltsensor.h"
#include <QtSensors/qtiltsensor.h>

QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

/*!
    \qmltype TiltSensor
    \instantiates QmlTiltSensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits Sensor
    \brief The TiltSensor element reports tilt events
           along the X and Y axes.

    The TiltSensor element reports tilt events along the X and Y axes.

    This element wraps the QTiltSensor class. Please see the documentation for
    QTiltSensor for details.

    \sa TiltReading
*/

QmlTiltSensor::QmlTiltSensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QTiltSensor(this))
{
}

QmlTiltSensor::~QmlTiltSensor()
{
}

QmlSensorReading *QmlTiltSensor::createReading() const
{
    return new QmlTiltSensorReading(m_sensor);
}

QSensor *QmlTiltSensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmlmethod TiltSensor::calibrate()
    Calibrate the tilt sensor.

    Please see QTiltSensor::calibrate() for information about this property.
*/
void QmlTiltSensor::calibrate()
{
    m_sensor->calibrate();
}

/*!
    \qmltype TiltReading
    \instantiates QmlTiltSensorReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits SensorReading
    \brief The TiltReading element holds the most recent TiltSensor reading.

    The TiltReading element holds the most recent TiltSensor reading.

    This element wraps the QTiltReading class. Please see the documentation for
    QTiltReading for details.

    This element cannot be directly created.
*/

QmlTiltSensorReading::QmlTiltSensorReading(QTiltSensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
{
}

QmlTiltSensorReading::~QmlTiltSensorReading()
{
}

/*!
    \qmlproperty qreal TiltReading::yRotation
    This property holds the amount of tilt on the Y axis.

    Please see QTiltReading::yRotation for information about this property.
*/

qreal QmlTiltSensorReading::yRotation() const
{
    return m_yRotation;
}

/*!
    \qmlproperty qreal TiltReading::xRotation
    This property holds the amount of tilt on the X axis.

    Please see QTiltReading::xRotation for information about this property.
*/

qreal QmlTiltSensorReading::xRotation() const
{
    return m_xRotation;
}

QSensorReading *QmlTiltSensorReading::reading() const
{
    return m_sensor->reading();
}

void QmlTiltSensorReading::readingUpdate()
{
    qreal tiltY = m_sensor->reading()->yRotation();
    if (m_yRotation != tiltY) {
        m_yRotation = tiltY;
        Q_EMIT yRotationChanged();
    }
    qreal tiltX = m_sensor->reading()->xRotation();
    if (m_xRotation != tiltX) {
        m_xRotation = tiltX;
        Q_EMIT xRotationChanged();
    }
}
