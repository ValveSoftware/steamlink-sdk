/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include <QtCore/QLoggingCategory>
#include "qbluetoothdevicediscoveryagent.h"
#include "qbluetoothdevicediscoveryagent_p.h"
#include "qbluetoothaddress.h"
#include "qbluetoothuuid.h"

#include "bluez/manager_p.h"
#include "bluez/adapter_p.h"
#include "bluez/device_p.h"
#include "bluez/bluez5_helper_p.h"
#include "bluez/objectmanager_p.h"
#include "bluez/adapter1_bluez5_p.h"
#include "bluez/device1_bluez5_p.h"
#include "bluez/properties_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

QBluetoothDeviceDiscoveryAgentPrivate::QBluetoothDeviceDiscoveryAgentPrivate(
    const QBluetoothAddress &deviceAdapter, QBluetoothDeviceDiscoveryAgent *parent) :
    lastError(QBluetoothDeviceDiscoveryAgent::NoError),
    m_adapterAddress(deviceAdapter),
    pendingCancel(false),
    pendingStart(false),
    manager(0),
    adapter(0),
    managerBluez5(0),
    adapterBluez5(0),
    discoveryTimer(0),
    useExtendedDiscovery(false),
    lowEnergySearchTimeout(-1), // remains -1 on BlueZ 4 -> timeout not supported
    q_ptr(parent)
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);
    if (isBluez5()) {
        lowEnergySearchTimeout = 20000;
        managerBluez5 = new OrgFreedesktopDBusObjectManagerInterface(
                                           QStringLiteral("org.bluez"),
                                           QStringLiteral("/"),
                                           QDBusConnection::systemBus(), parent);
        QObject::connect(managerBluez5,
                         SIGNAL(InterfacesAdded(QDBusObjectPath,InterfaceList)),
                         q, SLOT(_q_InterfacesAdded(QDBusObjectPath,InterfaceList)));

    } else {
        manager = new OrgBluezManagerInterface(QStringLiteral("org.bluez"), QStringLiteral("/"),
                                           QDBusConnection::systemBus(), parent);
        QObject::connect(&extendedDiscoveryTimer,
                         SIGNAL(timeout()),
                         q, SLOT(_q_extendedDeviceDiscoveryTimeout()));
        extendedDiscoveryTimer.setInterval(10000);
        extendedDiscoveryTimer.setSingleShot(true);
    }
    inquiryType = QBluetoothDeviceDiscoveryAgent::GeneralUnlimitedInquiry;
}

QBluetoothDeviceDiscoveryAgentPrivate::~QBluetoothDeviceDiscoveryAgentPrivate()
{
    delete adapter;
    delete adapterBluez5;
}

//TODO: Qt6 remove the pendingCancel/pendingStart logic as it is cumbersome.
//      It is a behavior change across all platforms and was initially done
//      for Bluez. The behavior should be similar to QBluetoothServiceDiscoveryAgent
//      PendingCancel creates issues whereby the agent is still shutting down
//      but isActive() below already returns false. This means the isActive() is
//      out of sync with the finished() and cancel() signal.

bool QBluetoothDeviceDiscoveryAgentPrivate::isActive() const
{
    if (pendingStart)
        return true;
    if (pendingCancel)
        return false; //TODO Qt6: remove pending[Cancel|Start] logic (see comment above)

    return (adapter || adapterBluez5);
}

QBluetoothDeviceDiscoveryAgent::DiscoveryMethods QBluetoothDeviceDiscoveryAgent::supportedDiscoveryMethods()
{
    return (ClassicMethod | LowEnergyMethod);
}

void QBluetoothDeviceDiscoveryAgentPrivate::start(QBluetoothDeviceDiscoveryAgent::DiscoveryMethods /*methods*/)
{
    // Currently both BlueZ backends do not distinguish discovery methods.
    // The DBus API's always return both device types. Therefore we ignore
    // the passed in methods.

    if (pendingCancel == true) {
        pendingStart = true;
        return;
    }

    discoveredDevices.clear();

    if (managerBluez5) {
        startBluez5();
        return;
    }

    QDBusPendingReply<QDBusObjectPath> reply;

    if (m_adapterAddress.isNull())
        reply = manager->DefaultAdapter();
    else
        reply = manager->FindAdapter(m_adapterAddress.toString());
    reply.waitForFinished();

    if (reply.isError()) {
        errorString = reply.error().message();
        qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << "ERROR: " << errorString;
        lastError = QBluetoothDeviceDiscoveryAgent::InputOutputError;
        Q_Q(QBluetoothDeviceDiscoveryAgent);
        emit q->error(lastError);
        return;
    }

    adapter = new OrgBluezAdapterInterface(QStringLiteral("org.bluez"), reply.value().path(),
                                           QDBusConnection::systemBus());

    Q_Q(QBluetoothDeviceDiscoveryAgent);
    QObject::connect(adapter, SIGNAL(DeviceFound(QString, QVariantMap)),
                     q, SLOT(_q_deviceFound(QString, QVariantMap)));
    QObject::connect(adapter, SIGNAL(PropertyChanged(QString, QDBusVariant)),
                     q, SLOT(_q_propertyChanged(QString, QDBusVariant)));

    QDBusPendingReply<QVariantMap> propertiesReply = adapter->GetProperties();
    propertiesReply.waitForFinished();
    if (propertiesReply.isError()) {
        errorString = propertiesReply.error().message();
        delete adapter;
        adapter = 0;
        qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << "ERROR: " << errorString;
        lastError = QBluetoothDeviceDiscoveryAgent::InputOutputError;
        Q_Q(QBluetoothDeviceDiscoveryAgent);
        delete adapter;
        adapter = 0;
        emit q->error(lastError);
        return;
    }

    if (!propertiesReply.value().value(QStringLiteral("Powered")).toBool()) {
        qCDebug(QT_BT_BLUEZ) << "Aborting device discovery due to offline Bluetooth Adapter";
        lastError = QBluetoothDeviceDiscoveryAgent::PoweredOffError;
        errorString = QBluetoothDeviceDiscoveryAgent::tr("Device is powered off");
        delete adapter;
        adapter = 0;
        emit q->error(lastError);
        return;
    }

    if (propertiesReply.value().value(QStringLiteral("Discovering")).toBool()) {
        /*  The discovery session is already ongoing. BTLE devices are advertised
            immediately after the start of the device discovery session. Hence if the
            session is already ongoing, we have just missed the BTLE device
            advertisement.

            This always happens during the second device discovery run in
            the current process. The first discovery doesn't have this issue.
            As to why the discovery session remains active despite the previous one
            being terminated is not known. This may be a bug in Bluez4.

            To workaround this issue we have to wait for two discovery
            sessions cycles.
        */
        qCDebug(QT_BT_BLUEZ) << "Using BTLE device discovery workaround.";
        useExtendedDiscovery = true;
    } else {
        useExtendedDiscovery = false;
    }

    QDBusPendingReply<> discoveryReply = adapter->StartDiscovery();
    discoveryReply.waitForFinished();
    if (discoveryReply.isError()) {
        delete adapter;
        adapter = 0;
        errorString = discoveryReply.error().message();
        lastError = QBluetoothDeviceDiscoveryAgent::InputOutputError;
        Q_Q(QBluetoothDeviceDiscoveryAgent);
        qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << "ERROR: " << errorString;
        emit q->error(lastError);
        return;
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::startBluez5()
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);


    bool ok = false;
    const QString adapterPath = findAdapterForAddress(m_adapterAddress, &ok);
    if (!ok || adapterPath.isEmpty()) {
        qCWarning(QT_BT_BLUEZ) << "Cannot find Bluez 5 adapter for device search" << ok;
        lastError = QBluetoothDeviceDiscoveryAgent::InputOutputError;
        errorString = QBluetoothDeviceDiscoveryAgent::tr("Cannot find valid Bluetooth adapter.");
        q->error(lastError);
        return;
    }

    adapterBluez5 = new OrgBluezAdapter1Interface(QStringLiteral("org.bluez"),
                                                  adapterPath,
                                                  QDBusConnection::systemBus());

    if (!adapterBluez5->powered()) {
        qCDebug(QT_BT_BLUEZ) << "Aborting device discovery due to offline Bluetooth Adapter";
        lastError = QBluetoothDeviceDiscoveryAgent::PoweredOffError;
        errorString = QBluetoothDeviceDiscoveryAgent::tr("Device is powered off");
        delete adapterBluez5;
        adapterBluez5 = 0;
        emit q->error(lastError);
        return;
    }


    QtBluezDiscoveryManager::instance()->registerDiscoveryInterest(adapterBluez5->path());
    QObject::connect(QtBluezDiscoveryManager::instance(), SIGNAL(discoveryInterrupted(QString)),
            q, SLOT(_q_discoveryInterrupted(QString)));

    // collect initial set of information
    QDBusPendingReply<ManagedObjectList> reply = managerBluez5->GetManagedObjects();
    reply.waitForFinished();
    if (!reply.isError()) {
        ManagedObjectList managedObjectList = reply.value();
        for (ManagedObjectList::const_iterator it = managedObjectList.constBegin(); it != managedObjectList.constEnd(); ++it) {
            const QDBusObjectPath &path = it.key();
            const InterfaceList &ifaceList = it.value();

            for (InterfaceList::const_iterator jt = ifaceList.constBegin(); jt != ifaceList.constEnd(); ++jt) {
                const QString &iface = jt.key();

                if (iface == QStringLiteral("org.bluez.Device1")) {

                    if (path.path().indexOf(adapterBluez5->path()) != 0)
                        continue; //devices whose path doesn't start with same path we skip

                    deviceFoundBluez5(path.path());
                    if (!isActive()) // Can happen if stop() was called from a slot in user code.
                      return;
                }
            }
        }
    }

    // wait interval and sum up what was found
    if (!discoveryTimer) {
        discoveryTimer = new QTimer(q);
        discoveryTimer->setSingleShot(true);
        QObject::connect(discoveryTimer, SIGNAL(timeout()),
                         q, SLOT(_q_discoveryFinished()));
    }

    if (lowEnergySearchTimeout > 0) { // otherwise no timeout and stop() required
        discoveryTimer->setInterval(lowEnergySearchTimeout);
        discoveryTimer->start();
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::stop()
{
    if (!adapter && !adapterBluez5)
        return;

    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO;
    pendingCancel = true;
    pendingStart = false;
    if (adapter) {
        QDBusPendingReply<> reply = adapter->StopDiscovery();
        reply.waitForFinished();
    } else {
        _q_discoveryFinished();
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::_q_deviceFound(const QString &address,
                                                           const QVariantMap &dict)
{
    const QBluetoothAddress btAddress(address);
    const QString btName = dict.value(QStringLiteral("Name")).toString();
    quint32 btClass = dict.value(QStringLiteral("Class")).toUInt();

    qCDebug(QT_BT_BLUEZ) << "Discovered: " << address << btName
                         << "Num UUIDs" << dict.value(QStringLiteral("UUIDs")).toStringList().count()
                         << "total device" << discoveredDevices.count() << "cached"
                         << dict.value(QStringLiteral("Cached")).toBool()
                         << "RSSI" << dict.value(QStringLiteral("RSSI")).toInt();

    QBluetoothDeviceInfo device(btAddress, btName, btClass);
    if (dict.value(QStringLiteral("RSSI")).isValid())
        device.setRssi(dict.value(QStringLiteral("RSSI")).toInt());
    QList<QBluetoothUuid> uuids;
    foreach (const QString &u, dict.value(QLatin1String("UUIDs")).toStringList())
        uuids.append(QBluetoothUuid(u));
    device.setServiceUuids(uuids, QBluetoothDeviceInfo::DataIncomplete);
    device.setCached(dict.value(QStringLiteral("Cached")).toBool());


    /*
     * Bluez v4.1 does not have extra bit which gives information if device is Bluetooth
     * Low Energy device and the way to discover it is with Class property of the Bluetooth device.
     * Low Energy devices do not have property Class.
     */
    if (btClass == 0)
        device.setCoreConfigurations(QBluetoothDeviceInfo::LowEnergyCoreConfiguration);
    else
        device.setCoreConfigurations(QBluetoothDeviceInfo::BaseRateCoreConfiguration);
    for (int i = 0; i < discoveredDevices.size(); i++) {
        if (discoveredDevices[i].address() == device.address()) {
            if (discoveredDevices[i] == device) {
                qCDebug(QT_BT_BLUEZ) << "Duplicate: " << address;
                return;
            }
            discoveredDevices.replace(i, device);
            Q_Q(QBluetoothDeviceDiscoveryAgent);
            qCDebug(QT_BT_BLUEZ) << "Updated: " << address;

            emit q->deviceDiscovered(device);
            return; // this works if the list doesn't contain duplicates. Don't let it.
        }
    }
    qCDebug(QT_BT_BLUEZ) << "Emit: " << address;
    discoveredDevices.append(device);
    Q_Q(QBluetoothDeviceDiscoveryAgent);
    emit q->deviceDiscovered(device);
}

void QBluetoothDeviceDiscoveryAgentPrivate::deviceFoundBluez5(const QString& devicePath)
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);

    if (!q->isActive())
        return;

    OrgBluezDevice1Interface device(QStringLiteral("org.bluez"), devicePath,
                                    QDBusConnection::systemBus());

    if (device.adapter().path() != adapterBluez5->path())
        return;

    const QBluetoothAddress btAddress(device.address());
    if (btAddress.isNull()) // no point reporting an empty address
        return;

    const QString btName = device.alias();
    quint32 btClass = device.classProperty();

    qCDebug(QT_BT_BLUEZ) << "Discovered: " << btAddress.toString() << btName
                         << "Num UUIDs" << device.uUIDs().count()
                         << "total device" << discoveredDevices.count() << "cached"
                         << "RSSI" << device.rSSI() << "Class" << btClass;

    OrgFreedesktopDBusPropertiesInterface *prop = new OrgFreedesktopDBusPropertiesInterface(
                QStringLiteral("org.bluez"), devicePath, QDBusConnection::systemBus(), q);
    QObject::connect(prop, SIGNAL(PropertiesChanged(QString,QVariantMap,QStringList)),
                     q, SLOT(_q_PropertiesChanged(QString,QVariantMap,QStringList)));
    // remember what we have to cleanup
    propertyMonitors.append(prop);

    // read information
    QBluetoothDeviceInfo deviceInfo(btAddress, btName, btClass);

    if (!btClass)
        deviceInfo.setCoreConfigurations(QBluetoothDeviceInfo::LowEnergyCoreConfiguration);
    else
        deviceInfo.setCoreConfigurations(QBluetoothDeviceInfo::BaseRateCoreConfiguration);

    deviceInfo.setRssi(device.rSSI());
    QList<QBluetoothUuid> uuids;
    foreach (const QString &u, device.uUIDs())
        uuids.append(QBluetoothUuid(u));
    deviceInfo.setServiceUuids(uuids, QBluetoothDeviceInfo::DataIncomplete);

    for (int i = 0; i < discoveredDevices.size(); i++) {
        if (discoveredDevices[i].address() == deviceInfo.address()) {
            if (discoveredDevices[i] == deviceInfo) {
                qCDebug(QT_BT_BLUEZ) << "Duplicate: " << btAddress.toString();
                return;
            }
            discoveredDevices.replace(i, deviceInfo);

            emit q->deviceDiscovered(deviceInfo);
            return; // this works if the list doesn't contain duplicates. Don't let it.
        }
    }

    discoveredDevices.append(deviceInfo);
    emit q->deviceDiscovered(deviceInfo);
}

void QBluetoothDeviceDiscoveryAgentPrivate::_q_propertyChanged(const QString &name,
                                                               const QDBusVariant &value)
{
    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << name << value.variant();

    if (name == QLatin1String("Discovering")) {
      if (!value.variant().toBool()) {
            Q_Q(QBluetoothDeviceDiscoveryAgent);
            if (pendingCancel && !pendingStart) {
                adapter->deleteLater();
                adapter = 0;

                pendingCancel = false;
                emit q->canceled();
            } else if (pendingStart) {
                adapter->deleteLater();
                adapter = 0;

                pendingStart = false;
                pendingCancel = false;
                // start parameter ignored since Bluez 4 doesn't distinguish them
                start(QBluetoothDeviceDiscoveryAgent::ClassicMethod
                      | QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
            } else {
                 // happens when agent is created while other agent called StopDiscovery()
                if (!adapter)
                    return;

                if (useExtendedDiscovery) {
                    useExtendedDiscovery = false;
                    /* We don't use the Start/StopDiscovery combo here
                       Using this combo surppresses the BTLE device.
                    */
                    extendedDiscoveryTimer.start();
                    return;
                }

                QDBusPendingReply<> reply = adapter->StopDiscovery();
                reply.waitForFinished();
                adapter->deleteLater();
                adapter = 0;
                emit q->finished();
            }
        } else {
            if (extendedDiscoveryTimer.isActive())
                extendedDiscoveryTimer.stop();
        }
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::_q_extendedDeviceDiscoveryTimeout()
{

    if (adapter) {
        adapter->deleteLater();
        adapter = 0;
    }
    if (isActive()) {
        Q_Q(QBluetoothDeviceDiscoveryAgent);
        emit q->finished();
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::_q_InterfacesAdded(const QDBusObjectPath &object_path,
                                                               InterfaceList interfaces_and_properties)
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);

    if (!q->isActive())
        return;

    if (interfaces_and_properties.contains(QStringLiteral("org.bluez.Device1"))) {
        // device interfaces belonging to different adapter
        // will be filtered out by deviceFoundBluez5();
        deviceFoundBluez5(object_path.path());
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::_q_discoveryFinished()
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);

    if (discoveryTimer)
        discoveryTimer->stop();

    QtBluezDiscoveryManager::instance()->disconnect(q);
    QtBluezDiscoveryManager::instance()->unregisterDiscoveryInterest(adapterBluez5->path());

    qDeleteAll(propertyMonitors);
    propertyMonitors.clear();

    delete adapterBluez5;
    adapterBluez5 = 0;

    if (pendingCancel && !pendingStart) {
        pendingCancel = false;
        emit q->canceled();
    } else if (pendingStart) {
        pendingStart = false;
        pendingCancel = false;
        start(QBluetoothDeviceDiscoveryAgent::ClassicMethod
              | QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    } else {
        emit q->finished();
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::_q_discoveryInterrupted(const QString &path)
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);

    if (!q->isActive())
        return;

    if (path == adapterBluez5->path()) {
        qCWarning(QT_BT_BLUEZ) << "Device discovery aborted due to unexpected adapter changes from another process.";

        if (discoveryTimer)
            discoveryTimer->stop();

        QtBluezDiscoveryManager::instance()->disconnect(q);
        // no need to call unregisterDiscoveryInterest since QtBluezDiscoveryManager
        // does this automatically when emitting discoveryInterrupted(QString) signal

        delete adapterBluez5;
        adapterBluez5 = 0;

        errorString = QBluetoothDeviceDiscoveryAgent::tr("Bluetooth adapter error");
        lastError = QBluetoothDeviceDiscoveryAgent::InputOutputError;
        emit q->error(lastError);
    }
}

void QBluetoothDeviceDiscoveryAgentPrivate::_q_PropertiesChanged(const QString &interface,
                                                                 const QVariantMap &changed_properties,
                                                                 const QStringList &)
{
    Q_Q(QBluetoothDeviceDiscoveryAgent);
    if (interface == QStringLiteral("org.bluez.Device1")
            && changed_properties.contains(QStringLiteral("RSSI"))) {
        OrgFreedesktopDBusPropertiesInterface *props =
                qobject_cast<OrgFreedesktopDBusPropertiesInterface *>(q->sender());
        if (!props)
            return;

        OrgBluezDevice1Interface device(QStringLiteral("org.bluez"), props->path(),
                                            QDBusConnection::systemBus());
        for (int i = 0; i < discoveredDevices.size(); i++) {
            if (discoveredDevices[i].address().toString() == device.address()) {
                qCDebug(QT_BT_BLUEZ) << "Updating RSSI for" << device.address()
                         << changed_properties.value(QStringLiteral("RSSI"));
                discoveredDevices[i].setRssi(
                            changed_properties.value(QStringLiteral("RSSI")).toInt());
                return;
            }
        }
    }
}

QT_END_NAMESPACE
