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

#include "qmlaccelerometer.h"
#include <QtSensors/QAccelerometer>

/*!
    \qmltype Accelerometer
    \instantiates QmlAccelerometer
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits Sensor
    \brief The Accelerometer element reports on linear acceleration
           along the X, Y and Z axes.

    The Accelerometer element reports on linear acceleration
    along the X, Y and Z axes.

    This element wraps the QAccelerometer class. Please see the documentation for
    QAccelerometer for details.

    \sa AccelerometerReading
*/

QmlAccelerometer::QmlAccelerometer(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QAccelerometer(this))
{
    connect(m_sensor, SIGNAL(accelerationModeChanged(AccelerationMode)),
            this, SIGNAL(accelerationModeChanged(AccelerationMode)));
}

QmlAccelerometer::~QmlAccelerometer()
{
}

/*!
    \qmlproperty AccelerationMode Accelerometer::accelerationMode
    \since QtSensors 5.1

    This property holds the current acceleration mode.

    Please see QAccelerometer::accelerationMode for information about this property.
*/

QmlAccelerometer::AccelerationMode QmlAccelerometer::accelerationMode() const
{
    return static_cast<QmlAccelerometer::AccelerationMode>(m_sensor->accelerationMode());
}

void QmlAccelerometer::setAccelerationMode(QmlAccelerometer::AccelerationMode accelerationMode)
{
    m_sensor->setAccelerationMode(static_cast<QAccelerometer::AccelerationMode>(accelerationMode));
}

QmlSensorReading *QmlAccelerometer::createReading() const
{
    return new QmlAccelerometerReading(m_sensor);
}

QSensor *QmlAccelerometer::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype AccelerometerReading
    \instantiates QmlAccelerometerReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits SensorReading
    \brief The AccelerometerReading element holds the most recent Accelerometer reading.

    The AccelerometerReading element holds the most recent Accelerometer reading.

    This element wraps the QAccelerometerReading class. Please see the documentation for
    QAccelerometerReading for details.

    This element cannot be directly created.
*/

QmlAccelerometerReading::QmlAccelerometerReading(QAccelerometer *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
{
}

QmlAccelerometerReading::~QmlAccelerometerReading()
{
}

/*!
    \qmlproperty qreal AccelerometerReading::x
    This property holds the acceleration on the X axis.

    Please see QAccelerometerReading::x for information about this property.
*/

qreal QmlAccelerometerReading::x() const
{
    return m_x;
}

/*!
    \qmlproperty qreal AccelerometerReading::y
    This property holds the acceleration on the Y axis.

    Please see QAccelerometerReading::y for information about this property.
*/

qreal QmlAccelerometerReading::y() const
{
    return m_y;
}

/*!
    \qmlproperty qreal AccelerometerReading::z
    This property holds the acceleration on the Z axis.

    Please see QAccelerometerReading::z for information about this property.
*/

qreal QmlAccelerometerReading::z() const
{
    return m_z;
}

QSensorReading *QmlAccelerometerReading::reading() const
{
    return m_sensor->reading();
}

void QmlAccelerometerReading::readingUpdate()
{
    qreal aX = m_sensor->reading()->x();
    if (m_x != aX) {
        m_x = aX;
        Q_EMIT xChanged();
    }
    qreal aY = m_sensor->reading()->y();
    if (m_y != aY) {
        m_y = aY;
        Q_EMIT yChanged();
    }
    qreal aZ = m_sensor->reading()->z();
    if (m_z != aZ) {
        m_z = aZ;
        Q_EMIT zChanged();
    }
}
