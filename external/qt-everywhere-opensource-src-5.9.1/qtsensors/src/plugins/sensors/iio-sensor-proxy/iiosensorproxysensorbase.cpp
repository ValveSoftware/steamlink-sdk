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

#include "iiosensorproxysensorbase.h"
#include "sensorproxy_interface.h"
#include "properties_interface.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusServiceWatcher>
#include <QtDBus/QDBusConnectionInterface>

#include <time.h>

quint64 IIOSensorProxySensorBase::produceTimestamp()
{
    struct timespec tv;
    int ok;

#ifdef CLOCK_MONOTONIC_RAW
    ok = clock_gettime(CLOCK_MONOTONIC_RAW, &tv);
    if (ok != 0)
#endif
    ok = clock_gettime(CLOCK_MONOTONIC, &tv);
    Q_ASSERT(ok == 0);

    quint64 result = (tv.tv_sec * 1000000ULL) + (tv.tv_nsec * 0.001); // scale to microseconds
    return result;
}

IIOSensorProxySensorBase::IIOSensorProxySensorBase(const QString& dbusPath, const QString dbusIface, QSensor *sensor)
    : QSensorBackend(sensor)
    , m_dbusInterface(dbusIface)
{
    QDBusServiceWatcher *watcher = new QDBusServiceWatcher(serviceName(), QDBusConnection::systemBus(),
                                                           QDBusServiceWatcher::WatchForRegistration |
                                                           QDBusServiceWatcher::WatchForUnregistration, this);
    connect(watcher, SIGNAL(serviceRegistered(QString)),
            this, SLOT(serviceRegistered()));
    connect(watcher, SIGNAL(serviceUnregistered(QString)),
            this, SLOT(serviceUnregistered()));

    m_serviceRunning = QDBusConnection::systemBus().interface()->isServiceRegistered(serviceName());

    m_propertiesInterface = new OrgFreedesktopDBusPropertiesInterface(serviceName(), dbusPath,
                                                                      QDBusConnection::systemBus(), this);
    connect(m_propertiesInterface, SIGNAL(PropertiesChanged(QString,QVariantMap,QStringList)),
            this, SLOT(propertiesChanged(QString,QVariantMap,QStringList)));
}

IIOSensorProxySensorBase::~IIOSensorProxySensorBase()
{
}

QString IIOSensorProxySensorBase::serviceName() const
{
    return QLatin1String("net.hadess.SensorProxy");
}

void IIOSensorProxySensorBase::serviceRegistered()
{
    m_serviceRunning = true;
}

void IIOSensorProxySensorBase::serviceUnregistered()
{
    m_serviceRunning = false;
    sensorStopped();
}

void IIOSensorProxySensorBase::propertiesChanged(const QString &interface,
                                                 const QVariantMap &changedProperties,
                                                 const QStringList &/*invalidatedProperties*/)
{
    if (interface == m_dbusInterface)
        updateProperties(changedProperties);
}
