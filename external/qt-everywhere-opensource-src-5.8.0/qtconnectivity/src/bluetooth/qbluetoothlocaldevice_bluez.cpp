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
#include <QtDBus/QDBusContext>

#include "qbluetoothlocaldevice.h"
#include "qbluetoothaddress.h"
#include "qbluetoothlocaldevice_p.h"

#include "bluez/manager_p.h"
#include "bluez/adapter_p.h"
#include "bluez/agent_p.h"
#include "bluez/device_p.h"
#include "bluez/bluez5_helper_p.h"
#include "bluez/objectmanager_p.h"
#include "bluez/properties_p.h"
#include "bluez/adapter1_bluez5_p.h"
#include "bluez/device1_bluez5_p.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

static const QLatin1String agentPath("/qt/agent");

inline uint qHash(const QBluetoothAddress &address)
{
    return ::qHash(address.toUInt64());
}

QBluetoothLocalDevice::QBluetoothLocalDevice(QObject *parent) :
    QObject(parent),
    d_ptr(new QBluetoothLocalDevicePrivate(this))
{
}

QBluetoothLocalDevice::QBluetoothLocalDevice(const QBluetoothAddress &address, QObject *parent) :
    QObject(parent),
    d_ptr(new QBluetoothLocalDevicePrivate(this, address))
{
}

QString QBluetoothLocalDevice::name() const
{
    if (d_ptr->adapter) {
        QDBusPendingReply<QVariantMap> reply = d_ptr->adapter->GetProperties();
        reply.waitForFinished();
        if (reply.isError())
            return QString();

        return reply.value().value(QStringLiteral("Name")).toString();
    } else if (d_ptr->adapterBluez5) {
        return d_ptr->adapterBluez5->alias();
    }

    return QString();
}

QBluetoothAddress QBluetoothLocalDevice::address() const
{
    if (d_ptr->adapter) {
        QDBusPendingReply<QVariantMap> reply = d_ptr->adapter->GetProperties();
        reply.waitForFinished();
        if (reply.isError())
            return QBluetoothAddress();

        return QBluetoothAddress(reply.value().value(QStringLiteral("Address")).toString());
    } else if (d_ptr->adapterBluez5) {
        return QBluetoothAddress(d_ptr->adapterBluez5->address());
    }

    return QBluetoothAddress();
}

void QBluetoothLocalDevice::powerOn()
{
    if (d_ptr->adapter)
        d_ptr->adapter->SetProperty(QStringLiteral("Powered"), QDBusVariant(QVariant::fromValue(true)));
    else if (d_ptr->adapterBluez5)
        d_ptr->adapterBluez5->setPowered(true);
}

void QBluetoothLocalDevice::setHostMode(QBluetoothLocalDevice::HostMode mode)
{
    if (!isValid())
        return;

    Q_D(QBluetoothLocalDevice);

    switch (mode) {
    case HostDiscoverableLimitedInquiry:
    case HostDiscoverable:
        if (hostMode() == HostPoweredOff) {
            // We first have to wait for BT to be powered on,
            // then we can set the host mode correctly
            d->pendingHostModeChange = static_cast<int>(HostDiscoverable);
            if (d->adapter) {
                d->adapter->SetProperty(QStringLiteral("Powered"),
                                        QDBusVariant(QVariant::fromValue(true)));
            } else {
                d->adapterBluez5->setPowered(true);
            }
        } else {
            if (d->adapter) {
                d->adapter->SetProperty(QStringLiteral("Discoverable"),
                                        QDBusVariant(QVariant::fromValue(true)));
            } else {
                d->adapterBluez5->setDiscoverable(true);
            }
        }
        break;
    case HostConnectable:
        if (hostMode() == HostPoweredOff) {
            d->pendingHostModeChange = static_cast<int>(HostConnectable);
            if (d->adapter) {
                d->adapter->SetProperty(QStringLiteral("Powered"),
                                        QDBusVariant(QVariant::fromValue(true)));
            } else {
                d->adapterBluez5->setPowered(true);
            }
        } else {
            if (d->adapter) {
                d->adapter->SetProperty(QStringLiteral("Discoverable"),
                                        QDBusVariant(QVariant::fromValue(false)));
            } else {
                d->adapterBluez5->setDiscoverable(false);
            }
        }
        break;
    case HostPoweredOff:
        if (d->adapter) {
            d->adapter->SetProperty(QStringLiteral("Powered"),
                                    QDBusVariant(QVariant::fromValue(false)));
        } else {
            d->adapterBluez5->setPowered(false);
        }
        break;
    }
}

QBluetoothLocalDevice::HostMode QBluetoothLocalDevice::hostMode() const
{
    if (d_ptr->adapter) {
        QDBusPendingReply<QVariantMap> reply = d_ptr->adapter->GetProperties();
        reply.waitForFinished();
        if (reply.isError())
            return HostPoweredOff;

        if (!reply.value().value(QStringLiteral("Powered")).toBool())
            return HostPoweredOff;
        else if (reply.value().value(QStringLiteral("Discoverable")).toBool())
            return HostDiscoverable;
        else if (reply.value().value(QStringLiteral("Powered")).toBool())
            return HostConnectable;
    } else if (d_ptr->adapterBluez5) {
        if (!d_ptr->adapterBluez5->powered())
            return HostPoweredOff;
        else if (d_ptr->adapterBluez5->discoverable())
            return HostDiscoverable;
        else if (d_ptr->adapterBluez5->powered())
            return HostConnectable;
    }

    return HostPoweredOff;
}

QList<QBluetoothAddress> QBluetoothLocalDevice::connectedDevices() const
{
    return d_ptr->connectedDevices();
}

QList<QBluetoothHostInfo> QBluetoothLocalDevice::allDevices()
{
    QList<QBluetoothHostInfo> localDevices;

    if (isBluez5()) {
        OrgFreedesktopDBusObjectManagerInterface manager(QStringLiteral("org.bluez"),
                                                         QStringLiteral("/"),
                                                         QDBusConnection::systemBus());
        QDBusPendingReply<ManagedObjectList> reply = manager.GetManagedObjects();
        reply.waitForFinished();
        if (reply.isError())
            return localDevices;

        ManagedObjectList managedObjectList = reply.value();
        for (ManagedObjectList::const_iterator it = managedObjectList.constBegin(); it != managedObjectList.constEnd(); ++it) {
            const InterfaceList &ifaceList = it.value();

            for (InterfaceList::const_iterator jt = ifaceList.constBegin(); jt != ifaceList.constEnd(); ++jt) {
                const QString &iface = jt.key();
                const QVariantMap &ifaceValues = jt.value();

                if (iface == QStringLiteral("org.bluez.Adapter1")) {
                    QBluetoothHostInfo hostInfo;
                    const QString temp = ifaceValues.value(QStringLiteral("Address")).toString();

                    hostInfo.setAddress(QBluetoothAddress(temp));
                    if (hostInfo.address().isNull())
                        continue;
                    hostInfo.setName(ifaceValues.value(QStringLiteral("Name")).toString());
                    localDevices.append(hostInfo);
                }
            }
        }
   } else {
        OrgBluezManagerInterface manager(QStringLiteral("org.bluez"), QStringLiteral("/"),
                                         QDBusConnection::systemBus());

        QDBusPendingReply<QList<QDBusObjectPath> > reply = manager.ListAdapters();
        reply.waitForFinished();
        if (reply.isError())
            return localDevices;

        foreach (const QDBusObjectPath &path, reply.value()) {
            QBluetoothHostInfo hostinfo;
            OrgBluezAdapterInterface adapter(QStringLiteral("org.bluez"), path.path(),
                                             QDBusConnection::systemBus());

            QDBusPendingReply<QVariantMap> reply = adapter.GetProperties();
            reply.waitForFinished();
            if (reply.isError())
                continue;

            hostinfo.setAddress(QBluetoothAddress(
                                    reply.value().value(QStringLiteral("Address")).toString()));
            hostinfo.setName(reply.value().value(QStringLiteral("Name")).toString());

            localDevices.append(hostinfo);
        }
    }

    return localDevices;
}

static inline OrgBluezDeviceInterface *getDevice(const QBluetoothAddress &address,
                                                 QBluetoothLocalDevicePrivate *d_ptr)
{
    if (!d_ptr || !d_ptr->adapter)
        return 0;
    QDBusPendingReply<QDBusObjectPath> reply = d_ptr->adapter->FindDevice(address.toString());
    reply.waitForFinished();
    if (reply.isError()) {
        qCWarning(QT_BT_BLUEZ) << Q_FUNC_INFO << "reply failed" << reply.error();
        return 0;
    }

    QDBusObjectPath path = reply.value();

    return new OrgBluezDeviceInterface(QStringLiteral("org.bluez"), path.path(),
                                       QDBusConnection::systemBus());
}

void QBluetoothLocalDevice::requestPairing(const QBluetoothAddress &address, Pairing pairing)
{
    if (!isValid() || address.isNull()) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                  Q_ARG(QBluetoothLocalDevice::Error,
                                        QBluetoothLocalDevice::PairingError));
        return;
    }

    const Pairing current_pairing = pairingStatus(address);
    if (current_pairing == pairing) {
        if (d_ptr->adapterBluez5) {
            // A possibly running discovery or pending pairing request should be canceled
            if (d_ptr->pairingDiscoveryTimer && d_ptr->pairingDiscoveryTimer->isActive()) {
                d_ptr->pairingDiscoveryTimer->stop();
            }

            if (d_ptr->pairingTarget) {
                qCDebug(QT_BT_BLUEZ) << "Cancelling pending pairing request to" << d_ptr->pairingTarget->address();
                QDBusPendingReply<> cancelReply = d_ptr->pairingTarget->CancelPairing();
                cancelReply.waitForFinished();
                delete d_ptr->pairingTarget;
                d_ptr->pairingTarget = 0;
            }

        }
        QMetaObject::invokeMethod(this, "pairingFinished", Qt::QueuedConnection,
                                  Q_ARG(QBluetoothAddress, address),
                                  Q_ARG(QBluetoothLocalDevice::Pairing, pairing));
        return;
    }

    if (d_ptr->adapterBluez5) {
        d_ptr->requestPairingBluez5(address, pairing);
        return;
    }

    if (pairing == Paired || pairing == AuthorizedPaired) {
        d_ptr->address = address;
        d_ptr->pairing = pairing;

        if (!d_ptr->agent) {
            d_ptr->agent = new OrgBluezAgentAdaptor(d_ptr);
            bool res = QDBusConnection::systemBus().registerObject(d_ptr->agent_path, d_ptr);
            if (!res) {
                QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                          Q_ARG(QBluetoothLocalDevice::Error,
                                                QBluetoothLocalDevice::PairingError));
                qCWarning(QT_BT_BLUEZ) << "Failed to register agent";
                return;
            }
        }

        if (current_pairing == Paired && pairing == AuthorizedPaired) {
            OrgBluezDeviceInterface *device = getDevice(address, d_ptr);
            if (!device) {
                QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                          Q_ARG(QBluetoothLocalDevice::Error,
                                                QBluetoothLocalDevice::PairingError));
                return;
            }
            QDBusPendingReply<> deviceReply
                = device->SetProperty(QStringLiteral("Trusted"), QDBusVariant(true));
            deviceReply.waitForFinished();
            if (deviceReply.isError()) {
                qCWarning(QT_BT_BLUEZ) << Q_FUNC_INFO << "reply failed" << deviceReply.error();
                QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                          Q_ARG(QBluetoothLocalDevice::Error,
                                                QBluetoothLocalDevice::PairingError));
                delete device;
                return;
            }
            delete device;
            QMetaObject::invokeMethod(this, "pairingFinished", Qt::QueuedConnection,
                                      Q_ARG(QBluetoothAddress, address),
                                      Q_ARG(QBluetoothLocalDevice::Pairing,
                                            QBluetoothLocalDevice::AuthorizedPaired));
        } else if (current_pairing == AuthorizedPaired && pairing == Paired) {
            OrgBluezDeviceInterface *device = getDevice(address, d_ptr);
            if (!device) {
                QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                          Q_ARG(QBluetoothLocalDevice::Error,
                                                QBluetoothLocalDevice::PairingError));
                return;
            }
            QDBusPendingReply<> deviceReply
                = device->SetProperty(QStringLiteral("Trusted"), QDBusVariant(false));
            deviceReply.waitForFinished();
            if (deviceReply.isError()) {
                qCWarning(QT_BT_BLUEZ) << Q_FUNC_INFO << "reply failed" << deviceReply.error();
                QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                          Q_ARG(QBluetoothLocalDevice::Error,
                                                QBluetoothLocalDevice::PairingError));
                delete device;
                return;
            }
            delete device;
            QMetaObject::invokeMethod(this, "pairingFinished", Qt::QueuedConnection,
                                      Q_ARG(QBluetoothAddress, address),
                                      Q_ARG(QBluetoothLocalDevice::Pairing,
                                            QBluetoothLocalDevice::Paired));
        } else {
            QDBusPendingReply<QDBusObjectPath> reply
                = d_ptr->adapter->CreatePairedDevice(address.toString(),
                                                     QDBusObjectPath(d_ptr->agent_path),
                                                     QStringLiteral("NoInputNoOutput"));

            QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
            connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher *)), d_ptr,
                    SLOT(pairingCompleted(QDBusPendingCallWatcher *)));

            if (reply.isError())
                qCWarning(QT_BT_BLUEZ) << Q_FUNC_INFO << reply.error() << d_ptr->agent_path;
        }
    } else if (pairing == Unpaired) {
        QDBusPendingReply<QDBusObjectPath> reply = this->d_ptr->adapter->FindDevice(
            address.toString());
        reply.waitForFinished();
        if (reply.isError()) {
            qCWarning(QT_BT_BLUEZ) << Q_FUNC_INFO << "failed to find device" << reply.error();
            QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                      Q_ARG(QBluetoothLocalDevice::Error,
                                            QBluetoothLocalDevice::PairingError));
            return;
        }
        QDBusPendingReply<> removeReply = this->d_ptr->adapter->RemoveDevice(reply.value());
        removeReply.waitForFinished();
        if (removeReply.isError()) {
            qCWarning(QT_BT_BLUEZ) << Q_FUNC_INFO << "failed to remove device"
                                   << removeReply.error();
            QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection,
                                      Q_ARG(QBluetoothLocalDevice::Error,
                                            QBluetoothLocalDevice::PairingError));
        } else {
            QMetaObject::invokeMethod(this, "pairingFinished", Qt::QueuedConnection,
                                      Q_ARG(QBluetoothAddress, address),
                                      Q_ARG(QBluetoothLocalDevice::Pairing,
                                            QBluetoothLocalDevice::Unpaired));
        }
    }
}

void QBluetoothLocalDevicePrivate::requestPairingBluez5(const QBluetoothAddress &targetAddress,
                                                        QBluetoothLocalDevice::Pairing targetPairing)
{
    if (!managerBluez5)
        return;

    //are we already discovering something? -> abort those attempts
    if (pairingDiscoveryTimer && pairingDiscoveryTimer->isActive()) {
        pairingDiscoveryTimer->stop();
        QtBluezDiscoveryManager::instance()->unregisterDiscoveryInterest(
                    adapterBluez5->path());
    }

    if (pairingTarget) {
        delete pairingTarget;
        pairingTarget = 0;
    }

    // pairing implies that the device was found
    // if we cannot find it we may have to turn on Discovery mode for a limited amount of time

    // check device doesn't already exist
    QDBusPendingReply<ManagedObjectList> reply = managerBluez5->GetManagedObjects();
    reply.waitForFinished();
    if (reply.isError()) {
        emit q_ptr->error(QBluetoothLocalDevice::PairingError);
        return;
    }

    ManagedObjectList managedObjectList = reply.value();
    for (ManagedObjectList::const_iterator it = managedObjectList.constBegin(); it != managedObjectList.constEnd(); ++it) {
        const QDBusObjectPath &path = it.key();
        const InterfaceList &ifaceList = it.value();

        for (InterfaceList::const_iterator jt = ifaceList.constBegin(); jt != ifaceList.constEnd(); ++jt) {
            const QString &iface = jt.key();

            if (iface == QStringLiteral("org.bluez.Device1")) {

                OrgBluezDevice1Interface device(QStringLiteral("org.bluez"),
                                                path.path(),
                                                QDBusConnection::systemBus());
                if (targetAddress == QBluetoothAddress(device.address())) {
                    qCDebug(QT_BT_BLUEZ) << "Initiating direct pair to" << targetAddress.toString();
                    //device exist -> directly work with it
                    processPairingBluez5(path.path(), targetPairing);
                    return;
                }
            }
        }
    }

    //no device matching -> turn on discovery
    QtBluezDiscoveryManager::instance()->registerDiscoveryInterest(adapterBluez5->path());

    address = targetAddress;
    pairing = targetPairing;
    if (!pairingDiscoveryTimer) {
        pairingDiscoveryTimer = new QTimer(this);
        pairingDiscoveryTimer->setSingleShot(true);
        pairingDiscoveryTimer->setInterval(20000); //20s
        connect(pairingDiscoveryTimer, SIGNAL(timeout()),
                SLOT(pairingDiscoveryTimedOut()));
    }

    qCDebug(QT_BT_BLUEZ) << "Initiating discovery for pairing on" << targetAddress.toString();
    pairingDiscoveryTimer->start();
}

/*!
 * \internal
 *
 * Found a matching device. Now we must ensure its pairing/trusted state is as desired.
 * If it has to be paired then we need another roundtrip through the event loop
 * while we wait for the user to accept the pairing dialogs.
 */
void QBluetoothLocalDevicePrivate::processPairingBluez5(const QString &objectPath,
                                                        QBluetoothLocalDevice::Pairing target)
{
    if (pairingTarget)
        delete pairingTarget;

    //stop possibly running discovery
    if (pairingDiscoveryTimer && pairingDiscoveryTimer->isActive()) {
        pairingDiscoveryTimer->stop();

        QtBluezDiscoveryManager::instance()->unregisterDiscoveryInterest(
                    adapterBluez5->path());
    }

    pairingTarget = new OrgBluezDevice1Interface(QStringLiteral("org.bluez"), objectPath,
                                                 QDBusConnection::systemBus(), this);
    const QBluetoothAddress targetAddress(pairingTarget->address());

    Q_Q(QBluetoothLocalDevice);

    switch (target) {
    case QBluetoothLocalDevice::Unpaired: {
        delete pairingTarget;
        pairingTarget = 0;

        QDBusPendingReply<> removeReply = adapterBluez5->RemoveDevice(QDBusObjectPath(objectPath));
        removeReply.waitForFinished();

        if (removeReply.isError())
            emit q->error(QBluetoothLocalDevice::PairingError);
        else
            emit q->pairingFinished(targetAddress, QBluetoothLocalDevice::Unpaired);

        break;
    }
    case QBluetoothLocalDevice::Paired:
    case QBluetoothLocalDevice::AuthorizedPaired:
        pairing = target;

        if (!pairingTarget->paired()) {
            qCDebug(QT_BT_BLUEZ) << "Sending pairing request to" << pairingTarget->address();
            //initiate the pairing
            QDBusPendingReply<> pairReply = pairingTarget->Pair();
            QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pairReply, this);
            connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)),
                    SLOT(pairingCompleted(QDBusPendingCallWatcher*)));
            return;
        }

        //already paired but Trust level must be adjusted
        if (target == QBluetoothLocalDevice::AuthorizedPaired && !pairingTarget->trusted())
            pairingTarget->setTrusted(true);
        else if (target == QBluetoothLocalDevice::Paired && pairingTarget->trusted())
            pairingTarget->setTrusted(false);

        delete pairingTarget;
        pairingTarget = 0;

        emit q->pairingFinished(targetAddress, target);

        break;
    default:
        break;
    }
}

void QBluetoothLocalDevicePrivate::pairingDiscoveryTimedOut()
{
    qCWarning(QT_BT_BLUEZ) << "Discovery for pairing purposes failed. Cannot find parable device.";

    QtBluezDiscoveryManager::instance()->unregisterDiscoveryInterest(
                adapterBluez5->path());

    emit q_ptr->error(QBluetoothLocalDevice::PairingError);
}

QBluetoothLocalDevice::Pairing QBluetoothLocalDevice::pairingStatus(
    const QBluetoothAddress &address) const
{
    if (address.isNull())
        return Unpaired;

    if (d_ptr->adapter) {
        OrgBluezDeviceInterface *device = getDevice(address, d_ptr);

        if (!device)
            return Unpaired;

        QDBusPendingReply<QVariantMap> deviceReply = device->GetProperties();
        deviceReply.waitForFinished();
        if (deviceReply.isError()) {
            delete device;
            return Unpaired;
        }

        QVariantMap map = deviceReply.value();

        if (map.value(QStringLiteral("Trusted")).toBool() && map.value(QStringLiteral("Paired")).toBool()) {
            delete device;
            return AuthorizedPaired;
        } else if (map.value(QStringLiteral("Paired")).toBool()) {
            delete device;
            return Paired;
        }
        delete device;
    } else if (d_ptr->adapterBluez5) {

        QDBusPendingReply<ManagedObjectList> reply = d_ptr->managerBluez5->GetManagedObjects();
        reply.waitForFinished();
        if (reply.isError())
            return Unpaired;

        ManagedObjectList managedObjectList = reply.value();
        for (ManagedObjectList::const_iterator it = managedObjectList.constBegin(); it != managedObjectList.constEnd(); ++it) {
            const QDBusObjectPath &path = it.key();
            const InterfaceList &ifaceList = it.value();

            for (InterfaceList::const_iterator jt = ifaceList.constBegin(); jt != ifaceList.constEnd(); ++jt) {
                const QString &iface = jt.key();

                if (iface == QStringLiteral("org.bluez.Device1")) {

                    OrgBluezDevice1Interface device(QStringLiteral("org.bluez"),
                                                    path.path(),
                                                    QDBusConnection::systemBus());

                    if (address == QBluetoothAddress(device.address())) {
                        if (device.trusted() && device.paired())
                            return AuthorizedPaired;
                        else if (device.paired())
                            return Paired;
                        else
                            return Unpaired;
                    }
                }
            }
        }
    }

    return Unpaired;
}

QBluetoothLocalDevicePrivate::QBluetoothLocalDevicePrivate(QBluetoothLocalDevice *q,
                                                           QBluetoothAddress address) :
        adapter(0),
        adapterBluez5(0),
        adapterProperties(0),
        managerBluez5(0),
        agent(0),
        manager(0),
        localAddress(address),
        pairingTarget(0),
        pairingDiscoveryTimer(0),
        pendingHostModeChange(-1),
        msgConnection(0),
        q_ptr(q)
{
    registerQBluetoothLocalDeviceMetaType();

    if (isBluez5())
        initializeAdapterBluez5();
    else
        initializeAdapter();

    connectDeviceChanges();
}

void QBluetoothLocalDevicePrivate::connectDeviceChanges()
{
    if (adapter) { // invalid QBluetoothLocalDevice due to wrong local adapter address
        createCache();
        connect(adapter, SIGNAL(PropertyChanged(QString, QDBusVariant)),
                SLOT(PropertyChanged(QString, QDBusVariant)));
        connect(adapter, SIGNAL(DeviceCreated(QDBusObjectPath)),
                SLOT(_q_deviceCreated(QDBusObjectPath)));
        connect(adapter, SIGNAL(DeviceRemoved(QDBusObjectPath)),
                SLOT(_q_deviceRemoved(QDBusObjectPath)));
    } else if (adapterBluez5 && managerBluez5) {
        //setup property change notifications for all existing devices
        QDBusPendingReply<ManagedObjectList> reply = managerBluez5->GetManagedObjects();
        reply.waitForFinished();
        if (reply.isError())
            return;

        OrgFreedesktopDBusPropertiesInterface *monitor = 0;

        ManagedObjectList managedObjectList = reply.value();
        for (ManagedObjectList::const_iterator it = managedObjectList.constBegin(); it != managedObjectList.constEnd(); ++it) {
            const QDBusObjectPath &path = it.key();
            const InterfaceList &ifaceList = it.value();

            for (InterfaceList::const_iterator jt = ifaceList.constBegin(); jt != ifaceList.constEnd(); ++jt) {
                const QString &iface = jt.key();
                const QVariantMap &ifaceValues = jt.value();

                if (iface == QStringLiteral("org.bluez.Device1")) {
                    monitor = new OrgFreedesktopDBusPropertiesInterface(QStringLiteral("org.bluez"),
                                                                        path.path(),
                                                                        QDBusConnection::systemBus(), this);
                    connect(monitor, SIGNAL(PropertiesChanged(QString,QVariantMap,QStringList)),
                            SLOT(PropertiesChanged(QString,QVariantMap,QStringList)));
                    deviceChangeMonitors.insert(path.path(), monitor);

                    if (ifaceValues.value(QStringLiteral("Connected"), false).toBool()) {
                        QBluetoothAddress address(ifaceValues.value(QStringLiteral("Address")).toString());
                        connectedDevicesSet.insert(address);
                    }
                }
            }
        }
    }
}

QBluetoothLocalDevicePrivate::~QBluetoothLocalDevicePrivate()
{
    delete msgConnection;
    delete adapter;
    delete adapterBluez5;
    delete adapterProperties;
    delete managerBluez5;
    delete agent;
    delete pairingTarget;
    delete manager;

    qDeleteAll(devices);
    qDeleteAll(deviceChangeMonitors);
}

void QBluetoothLocalDevicePrivate::initializeAdapter()
{
    if (adapter)
        return;

    QScopedPointer<OrgBluezManagerInterface> man(new OrgBluezManagerInterface(
                            QStringLiteral("org.bluez"), QStringLiteral("/"),
                            QDBusConnection::systemBus()));

    if (localAddress == QBluetoothAddress()) {
        QDBusPendingReply<QDBusObjectPath> reply = man->DefaultAdapter();
        reply.waitForFinished();
        if (reply.isError())
            return;

        adapter = new OrgBluezAdapterInterface(QStringLiteral("org.bluez"),
                                               reply.value().path(), QDBusConnection::systemBus());
    } else {
        QDBusPendingReply<QList<QDBusObjectPath> > reply = man->ListAdapters();
        reply.waitForFinished();
        if (reply.isError())
            return;

        foreach (const QDBusObjectPath &path, reply.value()) {
            OrgBluezAdapterInterface *tmpAdapter
                = new OrgBluezAdapterInterface(QStringLiteral("org.bluez"),
                                               path.path(), QDBusConnection::systemBus());

            QDBusPendingReply<QVariantMap> reply = tmpAdapter->GetProperties();
            reply.waitForFinished();
            if (reply.isError()) {
                delete tmpAdapter;
                continue;
            }

            QBluetoothAddress path_address(reply.value().value(QStringLiteral("Address")).toString());

            if (path_address == localAddress) {
                adapter = tmpAdapter;
                break;
            } else {
                delete tmpAdapter;
            }
        }
    }

    // monitor case when local adapter is removed
    manager = man.take();
    connect(manager, SIGNAL(AdapterRemoved(QDBusObjectPath)),
            this, SLOT(adapterRemoved(QDBusObjectPath)));

    currentMode = static_cast<QBluetoothLocalDevice::HostMode>(-1);
    if (adapter) {
        connect(adapter, SIGNAL(PropertyChanged(QString, QDBusVariant)),
                SLOT(PropertyChanged(QString, QDBusVariant)));

        qsrand(QTime::currentTime().msec());
        agent_path = agentPath;
        agent_path.append(QString::fromLatin1("/%1").arg(qrand()));
    }
}

void QBluetoothLocalDevicePrivate::initializeAdapterBluez5()
{
    if (adapterBluez5)
        return;

    //get all local adapters
    if (!managerBluez5)
        managerBluez5 = new OrgFreedesktopDBusObjectManagerInterface(
                                                     QStringLiteral("org.bluez"),
                                                     QStringLiteral("/"),
                                                     QDBusConnection::systemBus(), this);

    connect(managerBluez5, SIGNAL(InterfacesAdded(QDBusObjectPath,InterfaceList)),
            SLOT(InterfacesAdded(QDBusObjectPath,InterfaceList)));
    connect(managerBluez5, SIGNAL(InterfacesRemoved(QDBusObjectPath,QStringList)),
            SLOT(InterfacesRemoved(QDBusObjectPath,QStringList)));

    bool ok = true;
    const QString adapterPath = findAdapterForAddress(localAddress, &ok);
    if (!ok || adapterPath.isEmpty())
        return;

    adapterBluez5 = new OrgBluezAdapter1Interface(QStringLiteral("org.bluez"),
                                                   adapterPath,
                                                   QDBusConnection::systemBus(), this);

    if (adapterBluez5) {
        //hook up propertiesChanged for current adapter
        adapterProperties = new OrgFreedesktopDBusPropertiesInterface(
                    QStringLiteral("org.bluez"), adapterBluez5->path(),
                    QDBusConnection::systemBus(), this);
        connect(adapterProperties, SIGNAL(PropertiesChanged(QString,QVariantMap,QStringList)),
                SLOT(PropertiesChanged(QString,QVariantMap,QStringList)));
    }

    currentMode = static_cast<QBluetoothLocalDevice::HostMode>(-1);
}

// Bluez 5
void QBluetoothLocalDevicePrivate::PropertiesChanged(const QString &interface,
                                                     const QVariantMap &changed_properties,
                                                     const QStringList &/*invalidated_properties*/)
{
    //qDebug() << "Change" << interface << changed_properties;
    if (interface == QStringLiteral("org.bluez.Adapter1")) {
        //update host mode
        if (changed_properties.contains(QStringLiteral("Discoverable"))
                || changed_properties.contains(QStringLiteral("Powered"))) {

            QBluetoothLocalDevice::HostMode mode;

            if (!adapterBluez5->powered()) {
                mode = QBluetoothLocalDevice::HostPoweredOff;
            } else {
                if (adapterBluez5->discoverable())
                    mode = QBluetoothLocalDevice::HostDiscoverable;
                else
                    mode = QBluetoothLocalDevice::HostConnectable;

                if (pendingHostModeChange != -1) {

                    if (static_cast<int>(mode) != pendingHostModeChange) {
                        adapterBluez5->setDiscoverable(
                            pendingHostModeChange
                            == static_cast<int>(QBluetoothLocalDevice::HostDiscoverable));
                        pendingHostModeChange = -1;
                        return;
                    }
                    pendingHostModeChange = -1;
                }
            }

            if (mode != currentMode)
                emit q_ptr->hostModeStateChanged(mode);

            currentMode = mode;
        }
    } else if (interface == QStringLiteral("org.bluez.Device1")
               && changed_properties.contains(QStringLiteral("Connected"))) {
        // update list of connected devices
        OrgFreedesktopDBusPropertiesInterface *senderIface =
                qobject_cast<OrgFreedesktopDBusPropertiesInterface*>(sender());
        if (!senderIface)
            return;

        const QString currentPath = senderIface->path();
        bool isConnected = changed_properties.value(QStringLiteral("Connected"), false).toBool();
        OrgBluezDevice1Interface device(QStringLiteral("org.bluez"), currentPath,
                                        QDBusConnection::systemBus());
        const QBluetoothAddress changedAddress(device.address());
        bool isInSet = connectedDevicesSet.contains(changedAddress);
        if (isConnected && !isInSet) {
            connectedDevicesSet.insert(changedAddress);
            emit q_ptr->deviceConnected(changedAddress);
        } else if (!isConnected && isInSet) {
            connectedDevicesSet.remove(changedAddress);
            emit q_ptr->deviceDisconnected(changedAddress);
        }
    }
}

void QBluetoothLocalDevicePrivate::InterfacesAdded(const QDBusObjectPath &object_path, InterfaceList interfaces_and_properties)
{
    if (interfaces_and_properties.contains(QStringLiteral("org.bluez.Device1"))
        && !deviceChangeMonitors.contains(object_path.path())) {
        // a new device was added which we need to add to list of known devices
        OrgFreedesktopDBusPropertiesInterface *monitor = new OrgFreedesktopDBusPropertiesInterface(
                                           QStringLiteral("org.bluez"),
                                           object_path.path(),
                                           QDBusConnection::systemBus());
        connect(monitor, SIGNAL(PropertiesChanged(QString,QVariantMap,QStringList)),
                SLOT(PropertiesChanged(QString,QVariantMap,QStringList)));
        deviceChangeMonitors.insert(object_path.path(), monitor);

        const QVariantMap ifaceValues = interfaces_and_properties.value(QStringLiteral("org.bluez.Device1"));
        if (ifaceValues.value(QStringLiteral("Connected"), false).toBool()) {
            QBluetoothAddress address(ifaceValues.value(QStringLiteral("Address")).toString());
            connectedDevicesSet.insert(address);
            emit q_ptr->deviceConnected(address);
        }
    }

    if (pairingDiscoveryTimer && pairingDiscoveryTimer->isActive()
        && interfaces_and_properties.contains(QStringLiteral("org.bluez.Device1"))) {
        //device discovery for pairing found new remote device
        OrgBluezDevice1Interface device(QStringLiteral("org.bluez"),
                                        object_path.path(), QDBusConnection::systemBus());
        if (!address.isNull() && address == QBluetoothAddress(device.address()))
            processPairingBluez5(object_path.path(), pairing);
    }
}

void QBluetoothLocalDevicePrivate::InterfacesRemoved(const QDBusObjectPath &object_path,
                                                     const QStringList &interfaces)
{
    if (deviceChangeMonitors.contains(object_path.path())
            && interfaces.contains(QLatin1String("org.bluez.Device1"))) {

        //a device was removed
        delete deviceChangeMonitors.take(object_path.path());

        //the path contains the address (e.g.: /org/bluez/hci0/dev_XX_XX_XX_XX_XX_XX)
        //-> use it to update current list of connected devices
        QString addressString = object_path.path().right(17);
        addressString.replace(QStringLiteral("_"), QStringLiteral(":"));
        const QBluetoothAddress address(addressString);
        bool found = connectedDevicesSet.remove(address);
        if (found)
            emit q_ptr->deviceDisconnected(address);
    }

    if (adapterBluez5
            && object_path.path() == adapterBluez5->path()
            && interfaces.contains(QLatin1String("org.bluez.Adapter1"))) {
        qCDebug(QT_BT_BLUEZ) << "Adapter" << adapterBluez5->path() << "was removed";
        // current adapter was removed -> invalidate the instance
        delete adapterBluez5;
        adapterBluez5 = 0;
        managerBluez5->deleteLater();
        managerBluez5 = 0;
        delete adapterProperties;
        adapterProperties = 0;

        delete pairingTarget;
        pairingTarget = 0;

        // turn  off connectivity monitoring
        qDeleteAll(deviceChangeMonitors);
        deviceChangeMonitors.clear();
        connectedDevicesSet.clear();
    }
}

bool QBluetoothLocalDevicePrivate::isValid() const
{
    return adapter || adapterBluez5;
}

// Bluez 4
void QBluetoothLocalDevicePrivate::adapterRemoved(const QDBusObjectPath &devicePath)
{
    if (!adapter )
        return;

    if (adapter->path() != devicePath.path())
        return;

    qCDebug(QT_BT_BLUEZ) << "Adapter" << devicePath.path()
                         << "was removed. Invalidating object.";
    // the current adapter was removed
    delete adapter;
    adapter = 0;
    manager->deleteLater();
    manager = 0;

    // stop all pairing related activities
    if (agent) {
        QDBusConnection::systemBus().unregisterObject(agent_path);
        delete agent;
        agent = 0;
    }

    delete msgConnection;
    msgConnection = 0;

    // stop all connectivity monitoring
    qDeleteAll(devices);
    devices.clear();
    connectedDevicesSet.clear();
}

void QBluetoothLocalDevicePrivate::RequestConfirmation(const QDBusObjectPath &in0, uint in1)
{
    Q_UNUSED(in0);
    Q_Q(QBluetoothLocalDevice);
    setDelayedReply(true);
    msgConfirmation = message();
    msgConnection = new QDBusConnection(connection());
    emit q->pairingDisplayConfirmation(address, QString::fromLatin1("%1").arg(in1));
}

void QBluetoothLocalDevicePrivate::_q_deviceCreated(const QDBusObjectPath &device)
{
    OrgBluezDeviceInterface *deviceInterface
        = new OrgBluezDeviceInterface(QStringLiteral("org.bluez"),
                                      device.path(),
                                      QDBusConnection::systemBus(), this);
    connect(deviceInterface, SIGNAL(PropertyChanged(QString, QDBusVariant)),
            SLOT(_q_devicePropertyChanged(QString, QDBusVariant)));
    devices << deviceInterface;
    QDBusPendingReply<QVariantMap> properties
        = deviceInterface->asyncCall(QStringLiteral("GetProperties"));

    properties.waitForFinished();
    if (!properties.isValid()) {
        qCritical() << "Unable to get device properties from: " << device.path();
        return;
    }
    const QBluetoothAddress address
        = QBluetoothAddress(properties.value().value(QStringLiteral("Address")).toString());
    const bool connected = properties.value().value(QStringLiteral("Connected")).toBool();

    if (connected) {
        connectedDevicesSet.insert(address);
        emit q_ptr->deviceConnected(address);
    } else {
        connectedDevicesSet.remove(address);
        emit q_ptr->deviceDisconnected(address);
    }
}

void QBluetoothLocalDevicePrivate::_q_deviceRemoved(const QDBusObjectPath &device)
{
    foreach (OrgBluezDeviceInterface *deviceInterface, devices) {
        if (deviceInterface->path() == device.path()) {
            devices.remove(deviceInterface);
            delete deviceInterface; // deviceDisconnected is already emitted by _q_devicePropertyChanged
            break;
        }
    }
}

void QBluetoothLocalDevicePrivate::_q_devicePropertyChanged(const QString &property,
                                                            const QDBusVariant &value)
{
    OrgBluezDeviceInterface *deviceInterface = qobject_cast<OrgBluezDeviceInterface *>(sender());
    if (deviceInterface && property == QLatin1String("Connected")) {
        QDBusPendingReply<QVariantMap> propertiesReply = deviceInterface->GetProperties();
        propertiesReply.waitForFinished();
        if (propertiesReply.isError()) {
            qCWarning(QT_BT_BLUEZ) << propertiesReply.error().message();
            return;
        }
        const QVariantMap properties = propertiesReply.value();
        const QBluetoothAddress address
            = QBluetoothAddress(properties.value(QStringLiteral("Address")).toString());
        const bool connected = value.variant().toBool();

        if (connected) {
            connectedDevicesSet.insert(address);
            emit q_ptr->deviceConnected(address);
        } else {
            connectedDevicesSet.remove(address);
            emit q_ptr->deviceDisconnected(address);
        }
    }
}

void QBluetoothLocalDevicePrivate::createCache()
{
    if (!adapter)
        return;

    QDBusPendingReply<QList<QDBusObjectPath> > reply = adapter->ListDevices();
    reply.waitForFinished();
    if (reply.isError()) {
        qCWarning(QT_BT_BLUEZ) << reply.error().message();
        return;
    }
    foreach (const QDBusObjectPath &device, reply.value()) {
        OrgBluezDeviceInterface *deviceInterface =
                new OrgBluezDeviceInterface(QStringLiteral("org.bluez"),
                                            device.path(),
                                            QDBusConnection::systemBus(), this);
        connect(deviceInterface, SIGNAL(PropertyChanged(QString,QDBusVariant)),
                SLOT(_q_devicePropertyChanged(QString,QDBusVariant)));
        devices << deviceInterface;

        QDBusPendingReply<QVariantMap> properties
            = deviceInterface->asyncCall(QStringLiteral("GetProperties"));
        properties.waitForFinished();
        if (!properties.isValid()) {
            qCWarning(QT_BT_BLUEZ) << "Unable to get properties for device " << device.path();
            return;
        }

        if (properties.value().value(QStringLiteral("Connected")).toBool()) {
            connectedDevicesSet.insert(
                QBluetoothAddress(properties.value().value(QStringLiteral("Address")).toString()));
        }
    }
}

QList<QBluetoothAddress> QBluetoothLocalDevicePrivate::connectedDevices() const
{
    return connectedDevicesSet.toList();
}

void QBluetoothLocalDevice::pairingConfirmation(bool confirmation)
{
    if (!d_ptr
        || !d_ptr->msgConfirmation.isReplyRequired()
        || !d_ptr->msgConnection)
        return;

    if (confirmation) {
        QDBusMessage msg = d_ptr->msgConfirmation.createReply(QVariant(true));
        d_ptr->msgConnection->send(msg);
    } else {
        QDBusMessage error
            = d_ptr->msgConfirmation.createErrorReply(QDBusError::AccessDenied,
                                                      QStringLiteral("Pairing rejected"));
        d_ptr->msgConnection->send(error);
    }
    delete d_ptr->msgConnection;
    d_ptr->msgConnection = 0;
}

QString QBluetoothLocalDevicePrivate::RequestPinCode(const QDBusObjectPath &in0)
{
    Q_UNUSED(in0)
    Q_Q(QBluetoothLocalDevice);
    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << in0.path();
    // seeded in constructor, 6 digit pin
    QString pin = QString::fromLatin1("%1").arg(qrand()&1000000);
    pin = QString::fromLatin1("%1").arg(pin, 6, QLatin1Char('0'));

    emit q->pairingDisplayPinCode(address, pin);
    return pin;
}

void QBluetoothLocalDevicePrivate::pairingCompleted(QDBusPendingCallWatcher *watcher)
{
    Q_Q(QBluetoothLocalDevice);
    QDBusPendingReply<> reply = *watcher;

    if (reply.isError()) {
        qCWarning(QT_BT_BLUEZ) << "Failed to create pairing" << reply.error().name();
        if (reply.error().name() != QStringLiteral("org.bluez.Error.AuthenticationCanceled"))
            emit q->error(QBluetoothLocalDevice::PairingError);
        watcher->deleteLater();
        return;
    }

    if (adapter) {
        QDBusPendingReply<QDBusObjectPath> findReply = adapter->FindDevice(address.toString());
        findReply.waitForFinished();
        if (findReply.isError()) {
            qCWarning(QT_BT_BLUEZ) << Q_FUNC_INFO << "failed to find device" << findReply.error();
            emit q->error(QBluetoothLocalDevice::PairingError);
            watcher->deleteLater();
            return;
        }

        OrgBluezDeviceInterface device(QStringLiteral("org.bluez"), findReply.value().path(),
                                       QDBusConnection::systemBus());

        if (pairing == QBluetoothLocalDevice::AuthorizedPaired) {
            device.SetProperty(QStringLiteral("Trusted"), QDBusVariant(QVariant(true)));
            emit q->pairingFinished(address, QBluetoothLocalDevice::AuthorizedPaired);
        }
        else {
            device.SetProperty(QStringLiteral("Trusted"), QDBusVariant(QVariant(false)));
            emit q->pairingFinished(address, QBluetoothLocalDevice::Paired);
        }
    } else if (adapterBluez5) {
        if (!pairingTarget) {
            qCWarning(QT_BT_BLUEZ) << "Pairing target expected but found null pointer.";
            emit q->error(QBluetoothLocalDevice::PairingError);
            watcher->deleteLater();
            return;
        }

        if (!pairingTarget->paired()) {
            qCWarning(QT_BT_BLUEZ) << "Device was not paired as requested";
            emit q->error(QBluetoothLocalDevice::PairingError);
            watcher->deleteLater();
            return;
        }

        const QBluetoothAddress targetAddress(pairingTarget->address());

        if (pairing == QBluetoothLocalDevice::AuthorizedPaired && !pairingTarget->trusted())
            pairingTarget->setTrusted(true);
        else if (pairing == QBluetoothLocalDevice::Paired && pairingTarget->trusted())
            pairingTarget->setTrusted(false);

        delete pairingTarget;
        pairingTarget = 0;

        emit q->pairingFinished(targetAddress, pairing);
    }

    watcher->deleteLater();
}

void QBluetoothLocalDevicePrivate::Authorize(const QDBusObjectPath &in0, const QString &in1)
{
    Q_UNUSED(in0)
    Q_UNUSED(in1)
    // TODO implement this
    qCDebug(QT_BT_BLUEZ) << "Got authorize for" << in0.path() << in1;
}

void QBluetoothLocalDevicePrivate::Cancel()
{
    // TODO implement this
    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO;
}

void QBluetoothLocalDevicePrivate::Release()
{
    // TODO implement this
    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO;
}

void QBluetoothLocalDevicePrivate::ConfirmModeChange(const QString &in0)
{
    Q_UNUSED(in0)
    // TODO implement this
    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << in0;
}

void QBluetoothLocalDevicePrivate::DisplayPasskey(const QDBusObjectPath &in0, uint in1, uchar in2)
{
    Q_UNUSED(in0)
    Q_UNUSED(in1)
    Q_UNUSED(in2)
    // TODO implement this
    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO << in0.path() << in1 << in2;
}

uint QBluetoothLocalDevicePrivate::RequestPasskey(const QDBusObjectPath &in0)
{
    Q_UNUSED(in0);
    qCDebug(QT_BT_BLUEZ) << Q_FUNC_INFO;
    return qrand()&0x1000000;
}

// Bluez 4
void QBluetoothLocalDevicePrivate::PropertyChanged(QString property, QDBusVariant value)
{
    Q_UNUSED(value);

    if (property != QLatin1String("Powered")
        && property != QLatin1String("Discoverable"))
        return;

    Q_Q(QBluetoothLocalDevice);
    QBluetoothLocalDevice::HostMode mode;

    QDBusPendingReply<QVariantMap> reply = adapter->GetProperties();
    reply.waitForFinished();
    if (reply.isError()) {
        qCWarning(QT_BT_BLUEZ) << "Failed to get bluetooth properties for mode change";
        return;
    }

    QVariantMap map = reply.value();

    if (!map.value(QStringLiteral("Powered")).toBool()) {
        mode = QBluetoothLocalDevice::HostPoweredOff;
    } else {
        if (map.value(QStringLiteral("Discoverable")).toBool())
            mode = QBluetoothLocalDevice::HostDiscoverable;
        else
            mode = QBluetoothLocalDevice::HostConnectable;

        if (pendingHostModeChange != -1) {
            if ((int)mode != pendingHostModeChange) {
                if (property == QStringLiteral("Powered"))
                    return;
                if (pendingHostModeChange == (int)QBluetoothLocalDevice::HostDiscoverable) {
                    adapter->SetProperty(QStringLiteral("Discoverable"),
                                         QDBusVariant(QVariant::fromValue(true)));
                } else {
                    adapter->SetProperty(QStringLiteral("Discoverable"),
                                         QDBusVariant(QVariant::fromValue(false)));
                }
                pendingHostModeChange = -1;
                return;
            }
        }
    }

    if (mode != currentMode)
        emit q->hostModeStateChanged(mode);

    currentMode = mode;
}

#include "moc_qbluetoothlocaldevice_p.cpp"

QT_END_NAMESPACE
