/****************************************************************************
**
** Copyright (C) 2016 Alexander Volkov <a.volkov@rusbitech.ru>
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

#include "iiosensorproxyorientationsensor.h"
#include "sensorproxy_interface.h"

#include <QtDBus/QDBusPendingReply>

char const * const IIOSensorProxyOrientationSensor::id("iio-sensor-proxy.orientationsensor");

static inline QString dbusPath() { return QStringLiteral("/net/hadess/SensorProxy"); }

IIOSensorProxyOrientationSensor::IIOSensorProxyOrientationSensor(QSensor *sensor)
    : IIOSensorProxySensorBase(dbusPath(), NetHadessSensorProxyInterface::staticInterfaceName(), sensor)
{
    setReading<QOrientationReading>(&m_reading);
    m_sensorProxyInterface = new NetHadessSensorProxyInterface(serviceName(), dbusPath(),
                                                               QDBusConnection::systemBus(), this);
}

IIOSensorProxyOrientationSensor::~IIOSensorProxyOrientationSensor()
{
}

void IIOSensorProxyOrientationSensor::start()
{
    if (isServiceRunning()) {
        if (m_sensorProxyInterface->hasAccelerometer()) {
            QDBusPendingReply<> reply = m_sensorProxyInterface->ClaimAccelerometer();
            reply.waitForFinished();
            if (!reply.isError()) {
                QString orientation = m_sensorProxyInterface->accelerometerOrientation();
                updateOrientation(orientation);
                return;
            }
        }
    }
    sensorStopped();
}

void IIOSensorProxyOrientationSensor::stop()
{
    if (isServiceRunning()) {
        QDBusPendingReply<> reply = m_sensorProxyInterface->ReleaseAccelerometer();
        reply.waitForFinished();
    }
    sensorStopped();
}

void IIOSensorProxyOrientationSensor::updateProperties(const QVariantMap &changedProperties)
{
    if (changedProperties.contains("AccelerometerOrientation")) {
        QString orientation = changedProperties.value("AccelerometerOrientation").toString();
        updateOrientation(orientation);
    }
}

void IIOSensorProxyOrientationSensor::updateOrientation(const QString &orientation)
{
    QOrientationReading::Orientation o = QOrientationReading::Undefined;
    if (orientation == QLatin1String("normal"))
        o = QOrientationReading::TopUp;
    else if (orientation == QLatin1String("bottom-up"))
        o = QOrientationReading::TopDown;
    else if (orientation == QLatin1String("left-up"))
        o = QOrientationReading::LeftUp;
    else if (orientation == QLatin1String("right-up"))
        o = QOrientationReading::RightUp;

    m_reading.setOrientation(o);
    m_reading.setTimestamp(produceTimestamp());
    newReadingAvailable();
}
