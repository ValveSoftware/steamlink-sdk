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

#include "osx/osxbtconnectionmonitor_p.h"
#include "qbluetoothlocaldevice_p.h"
#include "qbluetoothlocaldevice.h"
#include "osx/osxbtdevicepair_p.h"
#include "osx/osxbtutility_p.h"
#include "osx/osxbluetooth_p.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qstring.h>
#include <QtCore/qglobal.h>
#include <QtCore/qdebug.h>
#include <QtCore/qmap.h>


#include <Foundation/Foundation.h>

#include <algorithm>

QT_BEGIN_NAMESPACE

class QBluetoothLocalDevicePrivate : public OSXBluetooth::PairingDelegate,
                                     public OSXBluetooth::ConnectionMonitor
{
    friend class QBluetoothLocalDevice;
public:
    typedef QBluetoothLocalDevice::Pairing Pairing;

    QBluetoothLocalDevicePrivate(QBluetoothLocalDevice *, const QBluetoothAddress & =
                                 QBluetoothAddress());

    bool isValid() const;
    void requestPairing(const QBluetoothAddress &address, Pairing pairing);
    Pairing pairingStatus(const QBluetoothAddress &address) const;

private:

    // PairingDelegate:
    void connecting(ObjCPairingRequest *pair) Q_DECL_OVERRIDE;
    void requestPIN(ObjCPairingRequest *pair) Q_DECL_OVERRIDE;
    void requestUserConfirmation(ObjCPairingRequest *pair,
                                 BluetoothNumericValue) Q_DECL_OVERRIDE;
    void passkeyNotification(ObjCPairingRequest *pair,
                             BluetoothPasskey passkey) Q_DECL_OVERRIDE;
    void error(ObjCPairingRequest *pair, IOReturn errorCode) Q_DECL_OVERRIDE;
    void pairingFinished(ObjCPairingRequest *pair) Q_DECL_OVERRIDE;

    // ConnectionMonitor
    void deviceConnected(const QBluetoothAddress &deviceAddress) Q_DECL_OVERRIDE;
    void deviceDisconnected(const QBluetoothAddress &deviceAddress) Q_DECL_OVERRIDE;

    void emitPairingFinished(const QBluetoothAddress &deviceAddress, Pairing pairing, bool queued);
    void emitError(QBluetoothLocalDevice::Error error, bool queued);

    void unpair(const QBluetoothAddress &deviceAddress);

    QBluetoothLocalDevice *q_ptr;

    typedef OSXBluetooth::ObjCScopedPointer<IOBluetoothHostController> HostController;
    HostController hostController;

    typedef OSXBluetooth::ObjCStrongReference<ObjCPairingRequest> PairingRequest;
    typedef QMap<QBluetoothAddress, PairingRequest> RequestMap;

    RequestMap pairingRequests;

    OSXBluetooth::ObjCScopedPointer<ObjCConnectionMonitor> connectionMonitor;
    QList<QBluetoothAddress> discoveredDevices;
};

QBluetoothLocalDevicePrivate::QBluetoothLocalDevicePrivate(QBluetoothLocalDevice *q,
                                                           const QBluetoothAddress &address) :
        q_ptr(q)
{
    registerQBluetoothLocalDeviceMetaType();

    Q_ASSERT_X(q, Q_FUNC_INFO, "invalid q_ptr (null)");

    QT_BT_MAC_AUTORELEASEPOOL;

    HostController defaultController([[IOBluetoothHostController defaultController] retain]);
    if (!defaultController) {
        qCCritical(QT_BT_OSX) << "failed to init a host controller object";
        return;
    }

    if (!address.isNull()) {
        NSString *const hciAddress = [defaultController addressAsString];
        if (!hciAddress) {
            qCCritical(QT_BT_OSX) << "failed to obtain an address";
            return;
        }

        BluetoothDeviceAddress iobtAddress = {};
        if (IOBluetoothNSStringToDeviceAddress(hciAddress, &iobtAddress) != kIOReturnSuccess) {
            qCCritical(QT_BT_OSX) << "invalid local device's address";
            return;
        }

        if (address != OSXBluetooth::qt_address(&iobtAddress)) {
            qCCritical(QT_BT_OSX) << "invalid local device's address";
            return;
        }
    }

    hostController.reset(defaultController.take());

    // This one is optional, if it fails to initialize, we do not care at all.
    connectionMonitor.reset([[ObjCConnectionMonitor alloc] initWithMonitor:this]);
}

bool QBluetoothLocalDevicePrivate::isValid() const
{
    return hostController;
}

void QBluetoothLocalDevicePrivate::requestPairing(const QBluetoothAddress &address, Pairing pairing)
{
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid local device");
    Q_ASSERT_X(!address.isNull(), Q_FUNC_INFO, "invalid device address");

    using OSXBluetooth::device_with_address;
    using OSXBluetooth::ObjCStrongReference;

    // That's a really special case on OS X.
    if (pairing == QBluetoothLocalDevice::Unpaired)
        return unpair(address);

    QT_BT_MAC_AUTORELEASEPOOL;

    if (pairing == QBluetoothLocalDevice::AuthorizedPaired)
        pairing = QBluetoothLocalDevice::Paired;

    RequestMap::iterator pos = pairingRequests.find(address);
    if (pos != pairingRequests.end()) {
        if ([pos.value() isActive]) // Still trying to pair, continue.
            return;

        // 'device' is autoreleased:
        IOBluetoothDevice *const device = [pos.value() targetDevice];
        if ([device isPaired]) {
            emitPairingFinished(address, pairing, true);
        } else if ([pos.value() start] != kIOReturnSuccess) {
            qCCritical(QT_BT_OSX) << "failed to start a new pairing request";
            emitError(QBluetoothLocalDevice::PairingError, true);
        }
        return;
    }

    // That's a totally new request ('Paired', since we are here).
    // Even if this device is paired (not by our local device), I still create a pairing request,
    // it'll just finish with success (skipping any intermediate steps).
    PairingRequest newRequest([[ObjCPairingRequest alloc] initWithTarget:address delegate:this], false);
    if (!newRequest) {
        qCCritical(QT_BT_OSX) << "failed to allocate a new pairing request";
        emitError(QBluetoothLocalDevice::PairingError, true);
        return;
    }

    pos = pairingRequests.insert(address, newRequest);
    const IOReturn result = [newRequest start];
    if (result != kIOReturnSuccess) {
        pairingRequests.erase(pos);
        qCCritical(QT_BT_OSX) << "failed to start a new pairing request";
        emitError(QBluetoothLocalDevice::PairingError, true);
    }
}

QBluetoothLocalDevice::Pairing QBluetoothLocalDevicePrivate::pairingStatus(const QBluetoothAddress &address)const
{
    Q_ASSERT_X(isValid(), Q_FUNC_INFO, "invalid local device");
    Q_ASSERT_X(!address.isNull(), Q_FUNC_INFO, "invalid address");

    using OSXBluetooth::device_with_address;
    using OSXBluetooth::ObjCStrongReference;

    QT_BT_MAC_AUTORELEASEPOOL;

    RequestMap::const_iterator it = pairingRequests.find(address);
    if (it != pairingRequests.end()) {
        // All Obj-C objects are autoreleased.
        IOBluetoothDevice *const device = [it.value() targetDevice];
        if (device && [device isPaired])
            return QBluetoothLocalDevice::Paired;
    } else {
        // Try even if device was not paired by this local device ...
        const ObjCStrongReference<IOBluetoothDevice> device(device_with_address(address));
        if (device && [device isPaired])
            return QBluetoothLocalDevice::Paired;
    }

    return QBluetoothLocalDevice::Unpaired;
}

void QBluetoothLocalDevicePrivate::connecting(ObjCPairingRequest *pair)
{
    Q_UNUSED(pair)
}

void QBluetoothLocalDevicePrivate::requestPIN(ObjCPairingRequest *pair)
{
    Q_UNUSED(pair)
}

void QBluetoothLocalDevicePrivate::requestUserConfirmation(ObjCPairingRequest *pair, BluetoothNumericValue intPin)
{
    Q_UNUSED(pair)
    Q_UNUSED(intPin)
}

void QBluetoothLocalDevicePrivate::passkeyNotification(ObjCPairingRequest *pair,
                                                       BluetoothPasskey passkey)
{
    Q_UNUSED(pair)
    Q_UNUSED(passkey)
}

void QBluetoothLocalDevicePrivate::error(ObjCPairingRequest *pair, IOReturn errorCode)
{
    Q_UNUSED(pair)
    Q_UNUSED(errorCode)

    emitError(QBluetoothLocalDevice::PairingError, false);
}

void QBluetoothLocalDevicePrivate::pairingFinished(ObjCPairingRequest *pair)
{
    Q_ASSERT_X(pair, Q_FUNC_INFO, "invalid pairing request (nil)");

    const QBluetoothAddress &deviceAddress = [pair targetAddress];
    Q_ASSERT_X(!deviceAddress.isNull(), Q_FUNC_INFO,
               "invalid target address");

    emitPairingFinished(deviceAddress, QBluetoothLocalDevice::Paired, false);
}

void QBluetoothLocalDevicePrivate::deviceConnected(const QBluetoothAddress &deviceAddress)
{
    if (!discoveredDevices.contains(deviceAddress))
        discoveredDevices.append(deviceAddress);

    QMetaObject::invokeMethod(q_ptr, "deviceConnected", Qt::QueuedConnection,
                              Q_ARG(QBluetoothAddress, deviceAddress));
}

void QBluetoothLocalDevicePrivate::deviceDisconnected(const QBluetoothAddress &deviceAddress)
{
    QList<QBluetoothAddress>::iterator devicePos =std::find(discoveredDevices.begin(),
                                                            discoveredDevices.end(),
                                                            deviceAddress);

    if (devicePos != discoveredDevices.end())
        discoveredDevices.erase(devicePos);

    QMetaObject::invokeMethod(q_ptr, "deviceDisconnected", Qt::QueuedConnection,
                              Q_ARG(QBluetoothAddress, deviceAddress));
}

void QBluetoothLocalDevicePrivate::emitError(QBluetoothLocalDevice::Error error, bool queued)
{
    if (queued) {
        QMetaObject::invokeMethod(q_ptr, "error", Qt::QueuedConnection,
                                  Q_ARG(QBluetoothLocalDevice::Error, error));
    } else {
        emit q_ptr->error(QBluetoothLocalDevice::PairingError);
    }
}

void QBluetoothLocalDevicePrivate::emitPairingFinished(const QBluetoothAddress &deviceAddress,
                                                       Pairing pairing, bool queued)
{
    Q_ASSERT_X(!deviceAddress.isNull(), Q_FUNC_INFO, "invalid target device address");
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");

    if (queued) {
        QMetaObject::invokeMethod(q_ptr, "pairingFinished", Qt::QueuedConnection,
                                  Q_ARG(QBluetoothAddress, deviceAddress),
                                  Q_ARG(QBluetoothLocalDevice::Pairing, pairing));
    } else {
        emit q_ptr->pairingFinished(deviceAddress, pairing);
    }
}

void QBluetoothLocalDevicePrivate::unpair(const QBluetoothAddress &deviceAddress)
{
    Q_ASSERT_X(!deviceAddress.isNull(), Q_FUNC_INFO,
               "invalid target address");

    emitPairingFinished(deviceAddress, QBluetoothLocalDevice::Unpaired, true);
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

QBluetoothLocalDevice::~QBluetoothLocalDevice()
{
    delete d_ptr;
}

bool QBluetoothLocalDevice::isValid() const
{
    return d_ptr->isValid();
}


QString QBluetoothLocalDevice::name() const
{
    QT_BT_MAC_AUTORELEASEPOOL;

    if (isValid()) {
        if (NSString *const nsn = [d_ptr->hostController nameAsString])
            return QString::fromNSString(nsn);
        qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "failed to obtain a name";
    }

    return QString();
}

QBluetoothAddress QBluetoothLocalDevice::address() const
{
    QT_BT_MAC_AUTORELEASEPOOL;

    if (isValid()) {
        if (NSString *const nsa = [d_ptr->hostController addressAsString])
            return QBluetoothAddress(OSXBluetooth::qt_address(nsa));

        qCCritical(QT_BT_OSX) << Q_FUNC_INFO << "failed to obtain an address";
    } else {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "invalid local device";
    }

    return QBluetoothAddress();
}

void QBluetoothLocalDevice::powerOn()
{
    if (!isValid())
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "invalid local device";
}

void QBluetoothLocalDevice::setHostMode(QBluetoothLocalDevice::HostMode mode)
{
    Q_UNUSED(mode)

    if (!isValid())
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "invalid local device";
}

QBluetoothLocalDevice::HostMode QBluetoothLocalDevice::hostMode() const
{
    if (!isValid() || ![d_ptr->hostController powerState])
        return HostPoweredOff;

    return HostConnectable;
}

QList<QBluetoothAddress> QBluetoothLocalDevice::connectedDevices() const
{
    QT_BT_MAC_AUTORELEASEPOOL;

    QList<QBluetoothAddress> connectedDevices;

    // Take the devices known to IOBluetooth to be paired and connected first:
    NSArray *const pairedDevices = [IOBluetoothDevice pairedDevices];
    for (IOBluetoothDevice *device in pairedDevices) {
        if ([device isConnected]) {
            const QBluetoothAddress address(OSXBluetooth::qt_address([device getAddress]));
            if (!address.isNull())
                connectedDevices.append(address);
        }
    }

    // Add devices, discovered by the connection monitor:
    connectedDevices += d_ptr->discoveredDevices;
    // Find something more elegant? :)
    // But after all, addresses are integers.
    std::sort(connectedDevices.begin(), connectedDevices.end());
    connectedDevices.erase(std::unique(connectedDevices.begin(),
                                       connectedDevices.end()),
                           connectedDevices.end());

    return connectedDevices;
}

QList<QBluetoothHostInfo> QBluetoothLocalDevice::allDevices()
{
    QList<QBluetoothHostInfo> localDevices;

    QBluetoothLocalDevice defaultAdapter;
    if (!defaultAdapter.isValid() || defaultAdapter.address().isNull()) {
        qCCritical(QT_BT_OSX) << Q_FUNC_INFO <<"no valid device found";
        return localDevices;
    }

    QBluetoothHostInfo deviceInfo;
    deviceInfo.setName(defaultAdapter.name());
    deviceInfo.setAddress(defaultAdapter.address());

    localDevices.append(deviceInfo);

    return localDevices;
}

void QBluetoothLocalDevice::pairingConfirmation(bool confirmation)
{
    Q_UNUSED(confirmation)
}


void QBluetoothLocalDevice::requestPairing(const QBluetoothAddress &address, Pairing pairing)
{
    if (!isValid())
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "invalid local device";

    if (!isValid() || address.isNull()) {
        d_ptr->emitError(PairingError, true);
        return;
    }

    return d_ptr->requestPairing(address, pairing);
}

QBluetoothLocalDevice::Pairing QBluetoothLocalDevice::pairingStatus(const QBluetoothAddress &address) const
{
    if (!isValid())
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "invalid local device";

    if (!isValid() || address.isNull())
        return Unpaired;

    return d_ptr->pairingStatus(address);
}

QT_END_NAMESPACE
