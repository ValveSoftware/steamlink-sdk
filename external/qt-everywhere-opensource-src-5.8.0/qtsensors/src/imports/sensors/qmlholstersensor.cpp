/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#include "qmlholstersensor.h"
#include <QtSensors/QHolsterSensor>

/*!
    \qmltype HolsterSensor
    \instantiates QmlHolsterSensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.1
    \inherits Sensor
    \brief The HolsterSensor element reports on whether a device is holstered.

    The HolsterSensor element reports on whether a device is holstered.

    This element wraps the QHolsterSensor class. Please see the documentation for
    QHolsterSensor for details.

    \sa HolsterReading
*/

QmlHolsterSensor::QmlHolsterSensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QHolsterSensor(this))
{
}

QmlHolsterSensor::~QmlHolsterSensor()
{
}

QmlSensorReading *QmlHolsterSensor::createReading() const
{
    return new QmlHolsterReading(m_sensor);
}

QSensor *QmlHolsterSensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype HolsterReading
    \instantiates QmlHolsterReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.1
    \inherits SensorReading
    \brief The HolsterReading element holds the most recent HolsterSensor reading.

    The HolsterReading element holds the most recent HolsterSensor reading.

    This element wraps the QHolsterReading class. Please see the documentation for
    QHolsterReading for details.

    This element cannot be directly created.
*/

QmlHolsterReading::QmlHolsterReading(QHolsterSensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
    , m_holstered(false)
{
}

QmlHolsterReading::~QmlHolsterReading()
{
}

/*!
    \qmlproperty qreal HolsterReading::holstered
    This property holds whether the device is holstered.

    Please see QHolsterReading::holstered for information about this property.
*/

bool QmlHolsterReading::holstered() const
{
    return m_holstered;
}

QSensorReading *QmlHolsterReading::reading() const
{
    return m_sensor->reading();
}

void QmlHolsterReading::readingUpdate()
{
    const bool holstered = m_sensor->reading()->holstered();
    if (m_holstered != holstered) {
        m_holstered = holstered;
        Q_EMIT holsteredChanged();
    }
}
