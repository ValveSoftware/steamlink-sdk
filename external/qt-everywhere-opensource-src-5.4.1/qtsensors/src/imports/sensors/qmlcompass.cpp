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

#include "qmlcompass.h"
#include <QtSensors/QCompass>

/*!
    \qmltype Compass
    \instantiates QmlCompass
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits Sensor
    \brief The Compass element reports on heading using magnetic north as a reference.

    The Compass element reports on heading using magnetic north as a reference.

    This element wraps the QCompass class. Please see the documentation for
    QCompass for details.

    \sa CompassReading
*/

QmlCompass::QmlCompass(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QCompass(this))
{
}

QmlCompass::~QmlCompass()
{
}

QmlSensorReading *QmlCompass::createReading() const
{
    return new QmlCompassReading(m_sensor);
}

QSensor *QmlCompass::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype CompassReading
    \instantiates QmlCompassReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \inherits SensorReading
    \brief The CompassReading element holds the most recent Compass reading.

    The CompassReading element holds the most recent Compass reading.

    This element wraps the QCompassReading class. Please see the documentation for
    QCompassReading for details.

    This element cannot be directly created.
*/

QmlCompassReading::QmlCompassReading(QCompass *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
{
}

QmlCompassReading::~QmlCompassReading()
{
}

/*!
    \qmlproperty qreal CompassReading::azimuth
    This property holds the azimuth of the device.

    Please see QCompassReading::azimuth for information about this property.
*/

qreal QmlCompassReading::azimuth() const
{
    return m_azimuth;
}

/*!
    \qmlproperty qreal CompassReading::calibrationLevel
    This property holds the calibration level of the reading.

    Please see QCompassReading::calibrationLevel for information about this property.
*/

qreal QmlCompassReading::calibrationLevel() const
{
    return m_calibrationLevel;
}

QSensorReading *QmlCompassReading::reading() const
{
    return m_sensor->reading();
}

void QmlCompassReading::readingUpdate()
{
    qreal azm = m_sensor->reading()->azimuth();
    if (m_azimuth != azm) {
        m_azimuth = azm;
        Q_EMIT azimuthChanged();
    }
    qreal calLevel = m_sensor->reading()->calibrationLevel();
    if (m_calibrationLevel != calLevel) {
        m_calibrationLevel = calLevel;
        Q_EMIT calibrationLevelChanged();
    }
}
