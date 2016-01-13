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
    foreach (const QByteArray &type, QSensor::sensorTypes())
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
    foreach (const QByteArray &identifier, QSensor::sensorsForType(type.toLocal8Bit()))
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
