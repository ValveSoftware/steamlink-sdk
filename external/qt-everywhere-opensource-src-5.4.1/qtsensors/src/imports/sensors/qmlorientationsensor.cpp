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

#include "qmlorientationsensor.h"
#include <QtSensors/QOrientationSensor>

/*!
    \qmltype OrientationSensor
    \instantiates QmlOrientationSensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits Sensor
    \brief The OrientationSensor element reports device orientation.

    The OrientationSensor element reports device orientation.

    This element wraps the QOrientationSensor class. Please see the documentation for
    QOrientationSensor for details.

    \sa OrientationReading
*/

QmlOrientationSensor::QmlOrientationSensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QOrientationSensor(this))
{
}

QmlOrientationSensor::~QmlOrientationSensor()
{
}

QmlSensorReading *QmlOrientationSensor::createReading() const
{
    return new QmlOrientationSensorReading(m_sensor);
}

QSensor *QmlOrientationSensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype OrientationReading
    \instantiates QmlOrientationSensorReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits SensorReading
    \brief The OrientationReading element holds the most recent OrientationSensor reading.

    The OrientationReading element holds the most recent OrientationSensor reading.

    This element wraps the QOrientationReading class. Please see the documentation for
    QOrientationReading for details.

    This element cannot be directly created.
*/

QmlOrientationSensorReading::QmlOrientationSensorReading(QOrientationSensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
{
}

QmlOrientationSensorReading::~QmlOrientationSensorReading()
{
}

/*!
    \qmlproperty Orientation OrientationReading::orientation
    This property holds the orientation of the device.

    Please see QOrientationReading::orientation for information about this property.

    Note that Orientation constants are exposed through the OrientationReading class.
    \code
        OrientationSensor {
            onReadingChanged: {
                if (reading.orientation == OrientationReading.TopUp)
                    // do something
            }
        }
    \endcode
*/

QOrientationReading::Orientation QmlOrientationSensorReading::orientation() const
{
    return m_orientation;
}

QSensorReading *QmlOrientationSensorReading::reading() const
{
    return m_sensor->reading();
}

void QmlOrientationSensorReading::readingUpdate()
{
    QOrientationReading::Orientation o = m_sensor->reading()->orientation();
    if (m_orientation != o) {
        m_orientation = o;
        Q_EMIT orientationChanged();
    }
}
