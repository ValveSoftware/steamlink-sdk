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

#include "qmlirproximitysensor.h"
#include <QtSensors/QIRProximitySensor>

/*!
    \qmltype IRProximitySensor
    \instantiates QmlIRProximitySensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits Sensor
    \brief The IRProximitySensor element reports on infra-red reflectance values.

    This element wraps the QIRProximitySensor class. Please see the documentation for
    QIRProximitySensor for details.

    \sa IRProximityReading
*/

QmlIRProximitySensor::QmlIRProximitySensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QIRProximitySensor(this))
{
}

QmlIRProximitySensor::~QmlIRProximitySensor()
{
}

QmlSensorReading *QmlIRProximitySensor::createReading() const
{
    return new QmlIRProximitySensorReading(m_sensor);
}

QSensor *QmlIRProximitySensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype IRProximityReading
    \instantiates QmlIRProximitySensorReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits SensorReading
    \brief The IRProximityReading element holds the most recent IR proximity reading.

    The IRProximityReading element holds the most recent IR proximity reading.

    This element wraps the QIRProximityReading class. Please see the documentation for
    QIRProximityReading for details.

    This element cannot be directly created.
*/

QmlIRProximitySensorReading::QmlIRProximitySensorReading(QIRProximitySensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
{
}

QmlIRProximitySensorReading::~QmlIRProximitySensorReading()
{
}

/*!
    \qmlproperty qreal IRProximityReading::reflectance
    This property holds the reflectance value.

    Please see QIRProximityReading::reflectance for information about this property.
*/

qreal QmlIRProximitySensorReading::reflectance() const
{
    return m_reflectance;
}

QSensorReading *QmlIRProximitySensorReading::reading() const
{
    return m_sensor->reading();
}

void QmlIRProximitySensorReading::readingUpdate()
{
    qreal fl = m_sensor->reading()->reflectance();
    if (m_reflectance != fl) {
        m_reflectance = fl;
        Q_EMIT reflectanceChanged();
    }
}
