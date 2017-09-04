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

#include "qmlambientlightsensor.h"
#include <QtSensors/QAmbientLightSensor>

/*!
    \qmltype AmbientLightSensor
    \instantiates QmlAmbientLightSensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits Sensor
    \brief The AmbientLightSensor element repors on ambient lighting conditions.

    The AmbientLightSensor element repors on ambient lighting conditions.

    This element wraps the QAmbientLightSensor class. Please see the documentation for
    QAmbientLightSensor for details.

    \sa AmbientLightReading
*/

QmlAmbientLightSensor::QmlAmbientLightSensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QAmbientLightSensor(this))
{
}

QmlAmbientLightSensor::~QmlAmbientLightSensor()
{
}

QmlSensorReading *QmlAmbientLightSensor::createReading() const
{
    return new QmlAmbientLightSensorReading(m_sensor);
}

QSensor *QmlAmbientLightSensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype AmbientLightReading
    \instantiates QmlAmbientLightSensorReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits SensorReading
    \brief The AmbientLightReading element holds the most AmbientLightSensor reading.

    The AmbientLightReading element holds the most AmbientLightSensor reading.

    This element wraps the QAmbientLightReading class. Please see the documentation for
    QAmbientLightReading for details.

    This element cannot be directly created.
*/

QmlAmbientLightSensorReading::QmlAmbientLightSensorReading(QAmbientLightSensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
{
}

QmlAmbientLightSensorReading::~QmlAmbientLightSensorReading()
{
}

/*!
    \qmlproperty LightLevel AmbientLightReading::lightLevel
    This property holds the ambient light level.

    Please see QAmbientLightReading::lightLevel for information about this property.

    Note that LightLevel constants are exposed through the AmbientLightReading class.
    \code
        AmbientLightSensor {
            onReadingChanged: {
                if (reading.lightLevel == AmbientLightReading.Dark)
                    // do something
            }
        }
    \endcode
*/

QAmbientLightReading::LightLevel QmlAmbientLightSensorReading::lightLevel() const
{
    return m_lightLevel;
}

QSensorReading *QmlAmbientLightSensorReading::reading() const
{
    return m_sensor->reading();
}

void QmlAmbientLightSensorReading::readingUpdate()
{
    QAmbientLightReading::LightLevel ll = m_sensor->reading()->lightLevel();
    if (m_lightLevel != ll) {
        m_lightLevel = ll;
        Q_EMIT lightLevelChanged();
    }
}
