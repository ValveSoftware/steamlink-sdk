/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
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
#include "qmlaltimeter.h"
#include <QtSensors/QAltimeter>

/*!
    \qmltype Altimeter
    \instantiates QmlAltimeter
    \ingroup qml-sensors_type
    \inqmlmodule QtSensors
    \since QtSensors 5.1
    \inherits Sensor
    \brief The Altimeter element reports on altitude.

    The Altimeter element reports on altitude.

    This element wraps the QAltimeter class. Please see the documentation for
    QAltimeter for details.

    \sa AltimeterReading
*/

QmlAltimeter::QmlAltimeter(QObject *parent)
    : QmlSensor(parent)
    , m_sensor(new QAltimeter(this))
{
}

QmlAltimeter::~QmlAltimeter()
{
}

QmlSensorReading *QmlAltimeter::createReading() const
{
    return new QmlAltimeterReading(m_sensor);
}

QSensor *QmlAltimeter::sensor() const
{
    return m_sensor;
}

/*!
    \qmltype AltimeterReading
    \instantiates QmlAltimeterReading
    \ingroup qml-sensors_reading
    \inqmlmodule QtSensors
    \since QtSensors 5.1
    \inherits SensorReading
    \brief The AltimeterReading element holds the most recent Altimeter reading.

    The AltimeterReading element holds the most recent Altimeter reading.

    This element wraps the QAltimeterReading class. Please see the documentation for
    QAltimeterReading for details.

    This element cannot be directly created.
*/

QmlAltimeterReading::QmlAltimeterReading(QAltimeter *sensor)
    : QmlSensorReading(sensor)
    , m_sensor(sensor)
    , m_altitude(0)
{
}

QmlAltimeterReading::~QmlAltimeterReading()
{
}

/*!
    \qmlproperty qreal AltimeterReading::altitude
    This property holds the altitude of the device.

    Please see QAltimeterReading::altitude for information about this property.
*/

qreal QmlAltimeterReading::altitude() const
{
    return m_altitude;
}

QSensorReading *QmlAltimeterReading::reading() const
{
    return m_sensor->reading();
}

void QmlAltimeterReading::readingUpdate()
{
    qreal altitude = m_sensor->reading()->altitude();
    if (m_altitude != altitude) {
        m_altitude = altitude;
        Q_EMIT altitudeChanged();
    }
}
