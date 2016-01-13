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

#include "qmltapsensor.h"
#include <QtSensors/QTapSensor>

/*!
    \qmltype TapSensor
    \instantiates QmlTapSensor
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits Sensor
    \brief The TapSensor element reports tap and double tap events
           along the X, Y and Z axes.

    The TapSensor element reports tap and double tap events
    along the X, Y and Z axes.

    This element wraps the QTapSensor class. Please see the documentation for
    QTapSensor for details.

    \sa TapReading
*/

QmlTapSensor::QmlTapSensor(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QTapSensor(this))
{
    connect(m_sensor, SIGNAL(returnDoubleTapEventsChanged(bool)),
            this, SIGNAL(returnDoubleTapEventsChanged(bool)));
}

QmlTapSensor::~QmlTapSensor()
{
}

QmlSensorReading *QmlTapSensor::createReading() const
{
    return new QmlTapSensorReading(m_sensor);
}

QSensor *QmlTapSensor::sensor() const
{
    return m_sensor;
}

/*!
    \qmlproperty bool TapSensor::returnDoubleTapEvents
    This property holds a value indicating if double tap events should be reported.

    Please see QTapSensor::returnDoubleTapEvents for information about this property.
*/

bool QmlTapSensor::returnDoubleTapEvents() const
{
    return m_sensor->returnDoubleTapEvents();
}

void QmlTapSensor::setReturnDoubleTapEvents(bool ret)
{
    m_sensor->setReturnDoubleTapEvents(ret);
}

/*!
    \qmltype TapReading
    \instantiates QmlTapSensorReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits SensorReading
    \brief The TapReading element holds the most recent TapSensor reading.

    The TapReading element holds the most recent TapSensor reading.

    This element wraps the QTapReading class. Please see the documentation for
    QTapReading for details.

    This element cannot be directly created.
*/

QmlTapSensorReading::QmlTapSensorReading(QTapSensor *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
{
}

QmlTapSensorReading::~QmlTapSensorReading()
{
}

/*!
    \qmlproperty TapDirection TapReading::tapDirection
    This property holds the direction of the tap.

    Please see QTapReading::tapDirection for information about this property.

    Note that TapDirection constants are exposed through the TapReading class.
    \code
        TapSensor {
            onReadingChanged: {
                if ((reading.tapDirection & TapReading.X_Both))
                    // do something
            }
        }
    \endcode
*/

QTapReading::TapDirection QmlTapSensorReading::tapDirection() const
{
    return m_tapDirection;
}

/*!
    \qmlproperty bool TapReading::doubleTap
    This property holds a value indicating if there was a single or double tap.

    Please see QTapReading::doubleTap for information about this property.
*/

bool QmlTapSensorReading::isDoubleTap() const
{
    return m_isDoubleTap;
}

QSensorReading *QmlTapSensorReading::reading() const
{
    return const_cast<QTapSensor*>(m_sensor)->reading();
}

void QmlTapSensorReading::readingUpdate()
{
    QTapReading::TapDirection td = m_sensor->reading()->tapDirection();
    if (m_tapDirection != td) {
        m_tapDirection = td;
        Q_EMIT tapDirectionChanged();
    }
    bool dTap = m_sensor->reading()->isDoubleTap();
    if (m_isDoubleTap != dTap) {
        m_isDoubleTap = dTap;
        Q_EMIT isDoubleTapChanged();
    }
}
