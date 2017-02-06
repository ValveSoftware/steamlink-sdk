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

#include "qmlproximitysensor.h"
#include <QtSensors/QProximitySensor>

/*!
    \qmltype ProximitySensor
    \instantiates QmlProximitySensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits Sensor
    \brief The ProximitySensor element reports on object proximity.

    The ProximitySensor element reports on object proximity.

    This element wraps the QProximitySensor class. Please see the documentation for
    QProximitySensor for details.

    \sa ProximityReading
*/

QmlProximitySensor::QmlProximitySensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QProximitySensor(this))
{
}

QmlProximitySensor::~QmlProximitySensor()
{
}

QmlSensorReading *QmlProximitySensor::createReading() const
{
    return new QmlProximitySensorReading(m_sensor);
}

QSensor *QmlProximitySensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype ProximityReading
    \instantiates QmlProximitySensorReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits SensorReading
    \brief The ProximityReading element holds the most recent ProximitySensor reading.

    The ProximityReading element holds the most recent ProximitySensor reading.

    This element wraps the QProximityReading class. Please see the documentation for
    QProximityReading for details.

    This element cannot be directly created.
*/

QmlProximitySensorReading::QmlProximitySensorReading(QProximitySensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
{
}

QmlProximitySensorReading::~QmlProximitySensorReading()
{
}

/*!
    \qmlproperty bool ProximityReading::near
    This property holds a value indicating if something is near.

    Please see QProximityReading::near for information about this property.
*/

bool QmlProximitySensorReading::near() const
{
    return m_near;
}

QSensorReading *QmlProximitySensorReading::reading() const
{
    return m_sensor->reading();
}

void QmlProximitySensorReading::readingUpdate()
{
    bool pNear = m_sensor->reading()->close();
    if (m_near != pNear) {
        m_near = pNear;
        Q_EMIT nearChanged();
    }
}
