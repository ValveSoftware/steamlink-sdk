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

#include "qmllightsensor.h"
#include <QtSensors/QLightSensor>

/*!
    \qmltype LightSensor
    \instantiates QmlLightSensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits Sensor
    \brief The LightSensor element reports on light levels using LUX.

    The LightSensor element reports on light levels using LUX.

    This element wraps the QLightSensor class. Please see the documentation for
    QLightSensor for details.

    \sa LightReading
*/

QmlLightSensor::QmlLightSensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QLightSensor(this))
{
    connect(m_sensor, SIGNAL(fieldOfViewChanged(qreal)),
            this, SIGNAL(fieldOfViewChanged(qreal)));
}

QmlLightSensor::~QmlLightSensor()
{
}

QmlSensorReading *QmlLightSensor::createReading() const
{
    return new QmlLightSensorReading(m_sensor);
}

QSensor *QmlLightSensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmlproperty qreal LightSensor::fieldOfView
    This property holds a value indicating the field of view.

    Please see QLightSensor::fieldOfView for information about this property.
*/

qreal QmlLightSensor::fieldOfView() const
{
    return m_sensor->fieldOfView();
}

/*!
    \qmltype LightReading
    \instantiates QmlLightSensorReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits SensorReading
    \brief The LightReading element holds the most recent LightSensor reading.

    The LightReading element holds the most recent LightSensor reading.

    This element wraps the QLightReading class. Please see the documentation for
    QLightReading for details.

    This element cannot be directly created.
*/

QmlLightSensorReading::QmlLightSensorReading(QLightSensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
{
}

QmlLightSensorReading::~QmlLightSensorReading()
{
}

/*!
    \qmlproperty qreal LightReading::illuminance
    This property holds the light level.

    Please see QLightReading::illuminance for information about this property.
*/

qreal QmlLightSensorReading::illuminance() const
{
    return m_illuminance;
}

QSensorReading *QmlLightSensorReading::reading() const
{
    return m_sensor->reading();
}

void QmlLightSensorReading::readingUpdate()
{
    qreal ill = m_sensor->reading()->lux();
    if (m_illuminance != ill) {
        m_illuminance = ill;
        Q_EMIT illuminanceChanged();
    }
}
