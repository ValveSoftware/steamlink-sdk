/****************************************************************************
**
** Copyright (C) 2016 Canonical Ltd
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

#include "qmlhumiditysensor.h"
#include <QtSensors/QHumiditySensor>

/*!
    \qmltype HumiditySensor
    \instantiates QmlHumiditySensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.9
    \inherits Sensor
    \brief The HumiditySensor element reports on humidity.

    The HumiditySensor element reports on humidity.

    This element wraps the QHumiditySensor class. Please see the documentation for
    QHumiditySensor for details.

    \sa HumidityReading
*/

QmlHumiditySensor::QmlHumiditySensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QHumiditySensor(this))
{
}

QmlHumiditySensor::~QmlHumiditySensor()
{
}

QmlSensorReading *QmlHumiditySensor::createReading() const
{
    return new QmlHumidityReading(m_sensor);
}

QSensor *QmlHumiditySensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype HumidityReading
    \instantiates QmlHumidityReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.9
    \inherits SensorReading
    \brief The HumidityReading element holds the most recent HumiditySensor reading.

    The HumidityReading element holds the most recent HumiditySensor reading.

    This element wraps the QHumidityReading class. Please see the documentation for
    QHumidityReading for details.

    This element cannot be directly created.
*/

QmlHumidityReading::QmlHumidityReading(QHumiditySensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
    , m_relativeHumidity(0)
    , m_absoluteHumidity(0)
{
}

QmlHumidityReading::~QmlHumidityReading()
{
}

/*!
    \qmlproperty qreal HumidityReading::relativeHumidity
    This property holds the relative humidity as a percentage.

    Please see QHumidityReading::relativeHumidity for information about this property.
*/

qreal QmlHumidityReading::relativeHumidity() const
{
    return m_relativeHumidity;
}

/*!
    \qmlproperty qreal HumidityReading::absoluteHumidity
    This property holds the absolute humidity in grams per cubic meter (g/m3).

    Please see QHumidityReading::absoluteHumidity for information about this property.
*/

qreal QmlHumidityReading::absoluteHumidity() const
{
    return m_absoluteHumidity;
}

QSensorReading *QmlHumidityReading::reading() const
{
    return m_sensor->reading();
}

void QmlHumidityReading::readingUpdate()
{
    qreal humidity = m_sensor->reading()->relativeHumidity();
    if (m_relativeHumidity != humidity) {
        m_relativeHumidity = humidity;
        Q_EMIT relativeHumidityChanged();
    }
    qreal abs = m_sensor->reading()->absoluteHumidity();
    if (m_absoluteHumidity != abs) {
        m_absoluteHumidity = abs;
        Q_EMIT absoluteHumidityChanged();
    }
}
