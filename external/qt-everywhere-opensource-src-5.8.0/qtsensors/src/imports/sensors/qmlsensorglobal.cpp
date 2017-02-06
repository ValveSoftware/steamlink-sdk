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

#include "qmlsensorglobal.h"
#include <QtSensors/QSensor>

QT_BEGIN_NAMESPACE

/*!
    \qmltype SensorGlobal
    \instantiates QmlSensorGlobal
    \inqmlmodule QtSensors
    \since QtSensors 5.0
    \brief The SensorGlobal element provides the module API.

    The SensorGlobal element provides the module API.

    This element cannot be directly created. It can only be accessed via a namespace import.

    \code
    import QtSensors 5.0
    import QtSensors 5.0 as Sensors
    ...
        Component.onCompleted: {
            var types = Sensors.QmlSensors.sensorTypes();
            console.log(types.join(", "));
        }
    \endcode
*/

QmlSensorGlobal::QmlSensorGlobal(QObject *parent)
    : QObject(parent)
    , m_sensor(new QSensor(QByteArray(), this))
{
    connect(m_sensor, SIGNAL(availableSensorsChanged()), this, SIGNAL(availableSensorsChanged()));
}

QmlSensorGlobal::~QmlSensorGlobal()
{
}

/*!
    \qmlmethod list<string> SensorGlobal::sensorTypes()
    Returns a list of the sensor types that have been registered.

    Please see QSensor::sensorTypes() for information.
*/
QStringList QmlSensorGlobal::sensorTypes() const
{
    QStringList ret;
    const QList<QByteArray> sensorTypes = QSensor::sensorTypes();
    ret.reserve(sensorTypes.count());
    foreach (const QByteArray &type, sensorTypes)
        ret << QString::fromLocal8Bit(type);
    return ret;
}

/*!
    \qmlmethod list<string> SensorGlobal::sensorsForType(type)
    Returns a list of the sensor identifiers that have been registered for \a type.

    Please see QSensor::sensorsForType() for information.
*/
QStringList QmlSensorGlobal::sensorsForType(const QString &type) const
{
    QStringList ret;
    const QList<QByteArray> sensors = QSensor::sensorsForType(type.toLocal8Bit());
    ret.reserve(sensors.count());
    foreach (const QByteArray &identifier, sensors)
        ret << QString::fromLocal8Bit(identifier);
    return ret;
}

/*!
    \qmlmethod string SensorGlobal::defaultSensorForType(type)
    Returns the default sensor identifier that has been registered for \a type.

    Please see QSensor::defaultSensorForType() for information.
*/
QString QmlSensorGlobal::defaultSensorForType(const QString &type) const
{
    return QString::fromLocal8Bit(QSensor::defaultSensorForType(type.toLocal8Bit()));
}

QT_END_NAMESPACE
