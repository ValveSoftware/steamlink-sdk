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

#include "qmlgyroscope.h"
#include <QtSensors/QGyroscope>

/*!
    \qmltype Gyroscope
    \instantiates QmlGyroscope
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits Sensor
    \brief The Gyroscope element reports on rotational acceleration
           around the X, Y and Z axes.

    This element wraps the QGyroscope class. Please see the documentation for
    QGyroscope for details.

    \sa GyroscopeReading
*/

QmlGyroscope::QmlGyroscope(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QGyroscope(this))
{
}

QmlGyroscope::~QmlGyroscope()
{
}

QmlSensorReading *QmlGyroscope::createReading() const
{
    return new QmlGyroscopeReading(m_sensor);
}

QSensor *QmlGyroscope::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype GyroscopeReading
    \instantiates QmlGyroscopeReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits SensorReading
    \brief The GyroscopeReading element holds the most recent Gyroscope reading.

    The GyroscopeReading element holds the most recent Gyroscope reading.

    This element wraps the QGyroscopeReading class. Please see the documentation for
    QGyroscopeReading for details.

    This element cannot be directly created.
*/

QmlGyroscopeReading::QmlGyroscopeReading(QGyroscope *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
{
}

QmlGyroscopeReading::~QmlGyroscopeReading()
{
}

/*!
    \qmlproperty qreal GyroscopeReading::x
    This property holds the angular velocity around the x axis.

    Please see QGyroscopeReading::x for information about this property.
*/

qreal QmlGyroscopeReading::x() const
{
    return m_x;
}

/*!
    \qmlproperty qreal GyroscopeReading::y
    This property holds the angular velocity around the y axis.

    Please see QGyroscopeReading::y for information about this property.
*/

qreal QmlGyroscopeReading::y() const
{
    return m_y;
}

/*!
    \qmlproperty qreal GyroscopeReading::z
    This property holds the angular velocity around the z axis.

    Please see QGyroscopeReading::z for information about this property.
*/

qreal QmlGyroscopeReading::z() const
{
    return m_z;
}

QSensorReading *QmlGyroscopeReading::reading() const
{
    return m_sensor->reading();
}

void QmlGyroscopeReading::readingUpdate()
{
    qreal gx = m_sensor->reading()->x();
    if (m_x != gx) {
        m_x = gx;
        Q_EMIT xChanged();
    }
    qreal gy = m_sensor->reading()->y();
    if (m_y != gy) {
        m_y = gy;
        Q_EMIT yChanged();
    }
    qreal gz = m_sensor->reading()->z();
    if (m_z != gz) {
        m_z = gz;
        Q_EMIT zChanged();
    }
}
