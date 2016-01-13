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

#include "qmlrotationsensor.h"
#include <QtSensors/QRotationSensor>

/*!
    \qmltype RotationSensor
    \instantiates QmlRotationSensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits Sensor
    \brief The RotationSensor element reports on device rotation
           around the X, Y and Z axes.

    The RotationSensor element reports on device rotation
    around the X, Y and Z axes.

    This element wraps the QRotationSensor class. Please see the documentation for
    QRotationSensor for details.

    \sa RotationReading
*/

QmlRotationSensor::QmlRotationSensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QRotationSensor(this))
{
    connect(m_sensor, SIGNAL(hasZChanged(bool)), this, SIGNAL(hasZChanged(bool)));
}

QmlRotationSensor::~QmlRotationSensor()
{
}

QmlSensorReading *QmlRotationSensor::createReading() const
{
    return new QmlRotationSensorReading(m_sensor);
}

QSensor *QmlRotationSensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmlproperty qreal RotationSensor::hasZ
    This property holds a value indicating if the z angle is available.

    Please see QRotationSensor::hasZ for information about this property.
*/

bool QmlRotationSensor::hasZ() const
{
    return m_sensor->hasZ();
}

void QmlRotationSensor::_update()
{
}

/*!
    \qmltype RotationReading
    \instantiates QmlRotationSensorReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits SensorReading
    \brief The RotationReading element holds the most recent RotationSensor reading.

    The RotationReading element holds the most recent RotationSensor reading.

    This element wraps the QRotationReading class. Please see the documentation for
    QRotationReading for details.

    This element cannot be directly created.
*/

QmlRotationSensorReading::QmlRotationSensorReading(QRotationSensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
{
}

QmlRotationSensorReading::~QmlRotationSensorReading()
{
}

/*!
    \qmlproperty qreal RotationReading::x
    This property holds the rotation around the x axis.

    Please see QRotationReading::x for information about this property.
*/

qreal QmlRotationSensorReading::x() const
{
    return m_x;
}

/*!
    \qmlproperty qreal RotationReading::y
    This property holds the rotation around the y axis.

    Please see QRotationReading::y for information about this property.
*/

qreal QmlRotationSensorReading::y() const
{
    return m_y;
}

/*!
    \qmlproperty qreal RotationReading::z
    This property holds the rotation around the z axis.

    Please see QRotationReading::z for information about this property.
*/

qreal QmlRotationSensorReading::z() const
{
    return m_z;
}

QSensorReading *QmlRotationSensorReading::reading() const
{
    return m_sensor->reading();
}

void QmlRotationSensorReading::readingUpdate()
{
    qreal rX = m_sensor->reading()->x();
    if (m_x != rX) {
        m_x = rX;
        Q_EMIT xChanged();
    }
    qreal rY = m_sensor->reading()->y();
    if (m_y != rY) {
        m_y = rY;
        Q_EMIT yChanged();
    }
    qreal rZ = m_sensor->reading()->z();
    if (m_z != rZ) {
        m_z = rZ;
        Q_EMIT zChanged();
    }
}
