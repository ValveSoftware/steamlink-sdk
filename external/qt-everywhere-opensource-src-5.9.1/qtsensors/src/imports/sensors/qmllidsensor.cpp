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
#include "qmllidsensor.h"
#include <QtSensors/QLidSensor>

/*!
    \qmltype LidSensor
    \instantiates QmlLidSensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.9
    \inherits Sensor
    \brief The LidSensor element reports on whether a device is closed.

    The LidSensor element reports on whether a device is closed.

    This element wraps the QLidSensor class. Please see the documentation for
    QLidSensor for details.

    \sa LidReading
*/

QmlLidSensor::QmlLidSensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QLidSensor(this))
{
}

QmlLidSensor::~QmlLidSensor()
{
}

QmlSensorReading *QmlLidSensor::createReading() const
{
    return new QmlLidReading(m_sensor);
}

QSensor *QmlLidSensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype LidReading
    \instantiates QmlLidReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.9
    \inherits SensorReading
    \brief The LidReading element holds the most recent LidSensor reading.

    The LidReading element holds the most recent LidSensor reading.

    This element wraps the QLidReading class. Please see the documentation for
    QLidReading for details.

    This element cannot be directly created.
*/

QmlLidReading::QmlLidReading(QLidSensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
    , m_backClosed(false)
    , m_frontClosed(true)
{
}

QmlLidReading::~QmlLidReading()
{
}

/*!
    \qmlproperty qreal LidReading::backLidClosed
    This property holds whether the back lid is closed.

    Please see QLidReading::backLidClosed for information about this property.
*/

bool QmlLidReading::backLidChanged() const
{
    return m_backClosed;
}

/*!
    \qmlproperty qreal LidReading::frontLidClosed
    This property holds whether the front lid is closed.

    Please see QLidReading::frontLidClosed for information about this property.
*/

bool QmlLidReading::frontLidClosed() const
{
    return m_frontClosed;
}

QSensorReading *QmlLidReading::reading() const
{
    return m_sensor->reading();
}

void QmlLidReading::readingUpdate()
{
    const bool backClosed = m_sensor->reading()->backLidClosed();
    if (m_backClosed != backClosed) {
        m_backClosed = backClosed;
        Q_EMIT backLidChanged(m_backClosed);
    }
    const bool frontClosed = m_sensor->reading()->frontLidClosed();
    if (m_frontClosed != frontClosed) {
        m_frontClosed = frontClosed;
        Q_EMIT frontLidChanged(m_frontClosed);
    }
}
