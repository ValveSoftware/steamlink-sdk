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
#include "qmlambienttemperaturesensor.h"
#include <QtSensors/QAmbientTemperatureSensor>

/*!
    \qmltype AmbientTemperatureSensor
    \instantiates QmlAmbientTemperatureSensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.1
    \inherits Sensor
    \brief The AmbientTemperatureSensor element reports on the ambient temperature.

    The AmbientTemperatureSensor element reports on the ambient temperature.

    This element wraps the QAmbientTemperatureSensor class. Please see the documentation for
    QAmbientTemperatureSensor for details.

    \sa AmbientTemperatureReading
*/

QmlAmbientTemperatureSensor::QmlAmbientTemperatureSensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QAmbientTemperatureSensor(this))
{
}

QmlAmbientTemperatureSensor::~QmlAmbientTemperatureSensor()
{
}

QmlSensorReading *QmlAmbientTemperatureSensor::createReading() const
{
    return new QmlAmbientTemperatureReading(m_sensor);
}

QSensor *QmlAmbientTemperatureSensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype AmbientTemperatureReading
    \instantiates QmlAmbientTemperatureReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.1
    \inherits SensorReading
    \brief The AmbientTemperatureReading element holds the most recent temperature reading.

    The AmbientTemperatureReading element holds the most recent temperature reading.

    This element wraps the QAmbientTemperatureReading class. Please see the documentation for
    QAmbientTemperatureReading for details.

    This element cannot be directly created.
*/

QmlAmbientTemperatureReading::QmlAmbientTemperatureReading(QAmbientTemperatureSensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
    , m_temperature(0)
{
}

QmlAmbientTemperatureReading::~QmlAmbientTemperatureReading()
{
}

/*!
    \qmlproperty qreal AmbientTemperatureReading::temperature
    This property holds the ambient temperature in degree Celsius.

    Please see QAmbientTemperatureReading::temperature for information about this property.
*/

qreal QmlAmbientTemperatureReading::temperature() const
{
    return m_temperature;
}

QSensorReading *QmlAmbientTemperatureReading::reading() const
{
    return m_sensor->reading();
}

void QmlAmbientTemperatureReading::readingUpdate()
{
    const qreal temperature = m_sensor->reading()->temperature();
    if (m_temperature != temperature) {
        m_temperature = temperature;
        Q_EMIT temperatureChanged();
    }
}
