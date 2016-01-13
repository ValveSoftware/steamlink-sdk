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
