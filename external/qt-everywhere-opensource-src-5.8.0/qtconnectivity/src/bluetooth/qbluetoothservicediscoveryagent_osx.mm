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

#include "qbluetoothservicediscoveryagent.h"
#include "qbluetoothdevicediscoveryagent.h"
#include "qbluetoothlocaldevice.h"
#include "osx/osxbtsdpinquiry_p.h"
#include "qbluetoothhostinfo.h"
#include "osx/osxbtutility_p.h"
#include "osx/osxbluetooth_p.h"
#include "osx/uistrings_p.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qstring.h>
#include <QtCore/qglobal.h>
#include <QtCore/qdebug.h>
#include <QtCore/qlist.h>

#include <Foundation/Foundation.h>

QT_BEGIN_NAMESPACE

class QBluetoothServiceDiscoveryAgentPrivate : public QObject, public OSXBluetooth::SDPInquiryDelegate
{
    friend class QBluetoothServiceDiscoveryAgent;
public:
    enum DiscoveryState {
        Inactive,
        DeviceDiscovery,
        ServiceDiscovery,
    };

    QBluetoothServiceDiscoveryAgentPrivate(QBluetoothServiceDiscoveryAgent *qp,
                                           const QBluetoothAddress &localAddress);

    void startDeviceDiscovery();
    void stopDeviceDiscovery();

    void startServiceDiscovery();
    void stopServiceDiscovery();

    DiscoveryState discoveryState();
    void setDiscoveryMode(QBluetoothServiceDiscoveryAgent::DiscoveryMode m);
    QBluetoothServiceDiscoveryAgent::DiscoveryMode DiscoveryMode();

    void _q_deviceDiscovered(const QBluetoothDeviceInfo &info);
    void _q_deviceDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error);
    void _q_deviceDiscoveryFinished();
    void _q_serviceDiscoveryFinished();


private:
    // SDPInquiryDelegate:
    void SDPInquiryFinished(IOBluetoothDevice *device) Q_DECL_OVERRIDE;
    void SDPInquiryError(IOBluetoothDevice *device, IOReturn errorCode) Q_DECL_OVERRIDE;

    void performMinimalServiceDiscovery(const QBluetoothAddress &deviceAddress);
    void setupDeviceDiscoveryAgent();
    bool isDuplicatedService(const QBluetoothServiceInfo &serviceInfo) const;
    void serviceDiscoveryFinished();

    QBluetoothServiceDiscoveryAgent *q_ptr;

    QBluetoothServiceDiscoveryAgent::Error error;
    QString errorString;

    QList<QBluetoothDeviceInfo> discoveredDevices;
    QList<QBluetoothServiceInfo> discoveredServices;
    QList<QBluetoothUuid> uuidFilter;

    bool singleDevice;
    QBluetoothAddress deviceAddress;
    QBluetoothAddress localAdapterAddress;

    DiscoveryState state;
    QBluetoothServiceDiscoveryAgent::DiscoveryMode discoveryMode;

    QScopedPointer<QBluetoothDeviceDiscoveryAgent> deviceDiscoveryAgent;
    OSXBluetooth::ObjCScopedPointer<ObjCServiceInquiry> serviceInquiry;
};

QBluetoothServiceDiscoveryAgentPrivate::QBluetoothServiceDiscoveryAgentPrivate(
    QBluetoothServiceDiscoveryAgent *qp, const QBluetoothAddress &localAddress) :
    q_ptr(qp),
    error(QBluetoothServiceDiscoveryAgent::NoError),
    singleDevice(false),
    localAdapterAddress(localAddress),
    state(Inactive),
    discoveryMode(QBluetoothServiceDiscoveryAgent::MinimalDiscovery)
{
    serviceInquiry.reset([[ObjCServiceInquiry alloc] initWithDelegate:this]);
}

void QBluetoothServiceDiscoveryAgentPrivate::startDeviceDiscovery()
{
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");
    Q_ASSERT_X(state == Inactive, Q_FUNC_INFO, "invalid state");
    Q_ASSERT_X(error != QBluetoothServiceDiscoveryAgent::InvalidBluetoothAdapterError,
               Q_FUNC_INFO, "invalid bluetooth adapter");

    Q_ASSERT_X(deviceDiscoveryAgent.isNull(), "startDeviceDiscovery()",
               "discovery agent already exists");

    state = DeviceDiscovery;

    setupDeviceDiscoveryAgent();
    deviceDiscoveryAgent->start();
}

void QBluetoothServiceDiscoveryAgentPrivate::stopDeviceDiscovery()
{
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");
    Q_ASSERT_X(!deviceDiscoveryAgent.isNull(), Q_FUNC_INFO,
               "invalid device discovery agent (null)");
    Q_ASSERT_X(state == DeviceDiscovery, Q_FUNC_INFO, "invalid state");

    deviceDiscoveryAgent->stop();
    deviceDiscoveryAgent.reset(Q_NULLPTR);
    state = Inactive;

    emit q_ptr->canceled();
}

void QBluetoothServiceDiscoveryAgentPrivate::startServiceDiscovery()
{
    // Any of 'Inactive'/'DeviceDiscovery'/'ServiceDiscovery' states
    // are possible.

    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");
    Q_ASSERT_X(error != QBluetoothServiceDiscoveryAgent::InvalidBluetoothAdapterError,
               Q_FUNC_INFO, "invalid bluetooth adapter");

    if (discoveredDevices.isEmpty()) {
        state = Inactive;
        emit q_ptr->finished();
        return;
    }

    QT_BT_MAC_AUTORELEASEPOOL;

    state = ServiceDiscovery;
    const QBluetoothAddress &address(discoveredDevices.at(0).address());

    if (address.isNull()) {
        // This can happen: LE scan works with CoreBluetooth, but CBPeripherals
        // do not expose hardware addresses.
        // Pop the current QBluetoothDeviceInfo and decide what to do next.
        return serviceDiscoveryFinished();
    }

    // Autoreleased object.
    IOBluetoothHostController *const hc = [IOBluetoothHostController defaultController];
    if (![hc powerState]) {
        discoveredDevices.clear();
        if (singleDevice) {
            error = QBluetoothServiceDiscoveryAgent::PoweredOffError;
            errorString = QCoreApplication::translate(SERVICE_DISCOVERY, SD_LOCAL_DEV_OFF);
            emit q_ptr->error(error);
        }

        return serviceDiscoveryFinished();
    }

    if (DiscoveryMode() == QBluetoothServiceDiscoveryAgent::MinimalDiscovery) {
        performMinimalServiceDiscovery(address);
    } else {
        IOReturn result = kIOReturnSuccess;
        if (uuidFilter.size())
            result = [serviceInquiry performSDPQueryWithDevice:address filters:uuidFilter];
        else
            result = [serviceInquiry performSDPQueryWithDevice:address];

        if (result != kIOReturnSuccess) {
            // Failed immediately to perform an SDP inquiry on IOBluetoothDevice:
            SDPInquiryError(nil, result);
        }
    }
}

void QBluetoothServiceDiscoveryAgentPrivate::stopServiceDiscovery()
{
    Q_ASSERT_X(state != Inactive, Q_FUNC_INFO, "invalid state");
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");

    discoveredDevices.clear();
    state = Inactive;

    // "Stops" immediately.
    [serviceInquiry stopSDPQuery];

    emit q_ptr->canceled();
}

QBluetoothServiceDiscoveryAgentPrivate::DiscoveryState
    QBluetoothServiceDiscoveryAgentPrivate::discoveryState()
{
    return state;
}

void QBluetoothServiceDiscoveryAgentPrivate::setDiscoveryMode(
    QBluetoothServiceDiscoveryAgent::DiscoveryMode m)
{
    discoveryMode = m;

}

QBluetoothServiceDiscoveryAgent::DiscoveryMode
    QBluetoothServiceDiscoveryAgentPrivate::DiscoveryMode()
{
    return discoveryMode;
}

void QBluetoothServiceDiscoveryAgentPrivate::_q_deviceDiscovered(const QBluetoothDeviceInfo &info)
{
    // Look for duplicates, and cached entries
    for (int i = 0; i < discoveredDevices.count(); i++) {
        if (discoveredDevices.at(i).address() == info.address()) {
            discoveredDevices.removeAt(i);
            break;
        }
    }

    discoveredDevices.prepend(info);
}

void QBluetoothServiceDiscoveryAgentPrivate::_q_deviceDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error)
{
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");

    error = QBluetoothServiceDiscoveryAgent::UnknownError;
    errorString = QCoreApplication::translate(DEV_DISCOVERY, DD_UNKNOWN_ERROR);

    deviceDiscoveryAgent->stop();
    deviceDiscoveryAgent.reset(Q_NULLPTR);

    state = QBluetoothServiceDiscoveryAgentPrivate::Inactive;
    emit q_ptr->error(error);
    emit q_ptr->finished();
}

void QBluetoothServiceDiscoveryAgentPrivate::_q_deviceDiscoveryFinished()
{
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");

    if (deviceDiscoveryAgent->error() != QBluetoothDeviceDiscoveryAgent::NoError) {
        //Forward the device discovery error
        error = static_cast<QBluetoothServiceDiscoveryAgent::Error>(deviceDiscoveryAgent->error());
        errorString = deviceDiscoveryAgent->errorString();
        deviceDiscoveryAgent.reset(Q_NULLPTR);
        state = Inactive;
        emit q_ptr->error(error);
        emit q_ptr->finished();
    } else {
        deviceDiscoveryAgent.reset(Q_NULLPTR);
        startServiceDiscovery();
    }
}

void QBluetoothServiceDiscoveryAgentPrivate::_q_serviceDiscoveryFinished()
{
    // See SDPInquiryFinished.
}

void QBluetoothServiceDiscoveryAgentPrivate::SDPInquiryFinished(IOBluetoothDevice *device)
{
    Q_ASSERT_X(device, Q_FUNC_INFO, "invalid IOBluetoothDevice (nil)");

    if (state == Inactive)
        return;

    QT_BT_MAC_AUTORELEASEPOOL;

    NSArray *const records = device.services;
    for (IOBluetoothSDPServiceRecord *record in records) {
        QBluetoothServiceInfo serviceInfo;
        Q_ASSERT_X(discoveredDevices.size() >= 1, Q_FUNC_INFO, "invalid number of devices");

        serviceInfo.setDevice(discoveredDevices.at(0));
        OSXBluetooth::extract_service_record(record, serviceInfo);

        if (!serviceInfo.isValid())
            continue;

        if (!isDuplicatedService(serviceInfo)) {
            discoveredServices.append(serviceInfo);
            emit q_ptr->serviceDiscovered(serviceInfo);
            // Here a user code can ... interrupt us by calling
            // stop. Check this.
            if (state == Inactive)
                break;
        }
    }

    serviceDiscoveryFinished();
}

void QBluetoothServiceDiscoveryAgentPrivate::SDPInquiryError(IOBluetoothDevice *device, IOReturn errorCode)
{
    Q_UNUSED(device)

    qCWarning(QT_BT_OSX) << "inquiry failed with IOKit code:" << int(errorCode);

    discoveredDevices.clear();
    // TODO: find a better mapping from IOReturn to QBluetoothServiceDiscoveryAgent::Error.
    if (singleDevice) {
        error = QBluetoothServiceDiscoveryAgent::UnknownError;
        errorString = QCoreApplication::translate(DEV_DISCOVERY, DD_UNKNOWN_ERROR);
        emit q_ptr->error(error);
    }

    serviceDiscoveryFinished();
}

void QBluetoothServiceDiscoveryAgentPrivate::performMinimalServiceDiscovery(const QBluetoothAddress &deviceAddress)
{
    Q_ASSERT_X(!deviceAddress.isNull(), Q_FUNC_INFO, "invalid device address");

    QT_BT_MAC_AUTORELEASEPOOL;

    const BluetoothDeviceAddress iobtAddress = OSXBluetooth::iobluetooth_address(deviceAddress);
    IOBluetoothDevice *const device = [IOBluetoothDevice deviceWithAddress:&iobtAddress];
    if (!device || !device.services) {
        if (singleDevice) {
            error = QBluetoothServiceDiscoveryAgent::UnknownError;
            errorString = QCoreApplication::translate(SERVICE_DISCOVERY, SD_MINIMAL_FAILED);
            emit q_ptr->error(error);
        }
    } else {

        NSArray *const records = device.services;
        for (IOBluetoothSDPServiceRecord *record in records) {
            QBluetoothServiceInfo serviceInfo;
            Q_ASSERT_X(discoveredDevices.size() >= 1, Q_FUNC_INFO,
                       "invalid number of devices");

            serviceInfo.setDevice(discoveredDevices.at(0));
            OSXBluetooth::extract_service_record(record, serviceInfo);

            if (!serviceInfo.isValid())
                continue;

            if (!uuidFilter.isEmpty() && !uuidFilter.contains(serviceInfo.serviceUuid()))
                continue;

            if (!isDuplicatedService(serviceInfo)) {
                discoveredServices.append(serviceInfo);
                emit q_ptr->serviceDiscovered(serviceInfo);
            }
        }
    }

    serviceDiscoveryFinished();
}

void QBluetoothServiceDiscoveryAgentPrivate::setupDeviceDiscoveryAgent()
{
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");
    Q_ASSERT_X(deviceDiscoveryAgent.isNull() || !deviceDiscoveryAgent->isActive(),
               Q_FUNC_INFO, "device discovery agent is active");

    deviceDiscoveryAgent.reset(new QBluetoothDeviceDiscoveryAgent(localAdapterAddress, q_ptr));

    QObject::connect(deviceDiscoveryAgent.data(), SIGNAL(deviceDiscovered(const QBluetoothDeviceInfo &)),
                     q_ptr, SLOT(_q_deviceDiscovered(const QBluetoothDeviceInfo &)));
    QObject::connect(deviceDiscoveryAgent.data(), SIGNAL(finished()),
                     q_ptr, SLOT(_q_deviceDiscoveryFinished()));
    QObject::connect(deviceDiscoveryAgent.data(), SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)),
                     q_ptr, SLOT(_q_deviceDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error)));
}

bool QBluetoothServiceDiscoveryAgentPrivate::isDuplicatedService(const QBluetoothServiceInfo &serviceInfo) const
{
    //check the service is not already part of our known list
    for (int j = 0; j < discoveredServices.count(); j++) {
        const QBluetoothServiceInfo &info = discoveredServices.at(j);
        if (info.device() == serviceInfo.device()
                && info.serviceClassUuids() == serviceInfo.serviceClassUuids()
                && info.serviceUuid() == serviceInfo.serviceUuid()) {
            return true;
        }
    }

    return false;
}

void QBluetoothServiceDiscoveryAgentPrivate::serviceDiscoveryFinished()
{
    if (!discoveredDevices.isEmpty())
        discoveredDevices.removeFirst();

    if (state == ServiceDiscovery)
        startServiceDiscovery();
}

QBluetoothServiceDiscoveryAgent::QBluetoothServiceDiscoveryAgent(QObject *parent)
: QObject(parent),
  d_ptr(new QBluetoothServiceDiscoveryAgentPrivate(this, QBluetoothAddress()))
{
}

QBluetoothServiceDiscoveryAgent::QBluetoothServiceDiscoveryAgent(const QBluetoothAddress &deviceAdapter, QObject *parent)
: QObject(parent),
  d_ptr(new QBluetoothServiceDiscoveryAgentPrivate(this, deviceAdapter))
{
    if (!deviceAdapter.isNull()) {
        const QList<QBluetoothHostInfo> localDevices = QBluetoothLocalDevice::allDevices();
        foreach (const QBluetoothHostInfo &hostInfo, localDevices) {
            if (hostInfo.address() == deviceAdapter)
                return;
        }
        d_ptr->error = InvalidBluetoothAdapterError;
        d_ptr->errorString = QCoreApplication::translate(SERVICE_DISCOVERY, SD_INVALID_ADDRESS);
    }
}

QBluetoothServiceDiscoveryAgent::~QBluetoothServiceDiscoveryAgent()
{
    delete d_ptr;
}

QList<QBluetoothServiceInfo> QBluetoothServiceDiscoveryAgent::discoveredServices() const
{
    return d_ptr->discoveredServices;
}

/*
    Sets the UUID filter to \a uuids.  Only services matching the UUIDs in \a uuids will be
    returned.

    An empty UUID list is equivalent to a list containing only QBluetoothUuid::PublicBrowseGroup.

    \sa uuidFilter()
*/
void QBluetoothServiceDiscoveryAgent::setUuidFilter(const QList<QBluetoothUuid> &uuids)
{
    d_ptr->uuidFilter = uuids;
}

/*
    This is an overloaded member function, provided for convenience.

    Sets the UUID filter to a list containing the single element \a uuid.

    \sa uuidFilter()
*/
void QBluetoothServiceDiscoveryAgent::setUuidFilter(const QBluetoothUuid &uuid)
{
    d_ptr->uuidFilter.clear();
    d_ptr->uuidFilter.append(uuid);
}

/*
    Returns the UUID filter.

    \sa setUuidFilter()
*/
QList<QBluetoothUuid> QBluetoothServiceDiscoveryAgent::uuidFilter() const
{
    return d_ptr->uuidFilter;
}

/*
    Sets the remote device address to \a address. If \a address is default constructed,
    services will be discovered on all contactable Bluetooth devices. A new remote
    address can only be set while there is no service discovery in progress; otherwise
    this function returns false.

    \sa remoteAddress()
*/
bool QBluetoothServiceDiscoveryAgent::setRemoteAddress(const QBluetoothAddress &address)
{
    if (isActive())
        return false;

    if (!address.isNull())
        d_ptr->singleDevice = true;

    d_ptr->deviceAddress = address;
    return true;
}

QBluetoothAddress QBluetoothServiceDiscoveryAgent::remoteAddress() const
{
    if (d_ptr->singleDevice)
        return d_ptr->deviceAddress;

    return QBluetoothAddress();
}

void QBluetoothServiceDiscoveryAgent::start(DiscoveryMode mode)
{
    if (d_ptr->discoveryState() == QBluetoothServiceDiscoveryAgentPrivate::Inactive
        && d_ptr->error != InvalidBluetoothAdapterError)
    {
        d_ptr->setDiscoveryMode(mode);
        if (d_ptr->deviceAddress.isNull()) {
            d_ptr->startDeviceDiscovery();
        } else {
            d_ptr->discoveredDevices.append(QBluetoothDeviceInfo(d_ptr->deviceAddress, QString(), 0));
            d_ptr->startServiceDiscovery();
        }
    }
}

void QBluetoothServiceDiscoveryAgent::stop()
{
    if (d_ptr->error == InvalidBluetoothAdapterError || !isActive())
        return;

    switch (d_ptr->discoveryState()) {
    case QBluetoothServiceDiscoveryAgentPrivate::DeviceDiscovery:
        d_ptr->stopDeviceDiscovery();
        break;
    case QBluetoothServiceDiscoveryAgentPrivate::ServiceDiscovery:
        d_ptr->stopServiceDiscovery();
    default:;
    }

    d_ptr->discoveredDevices.clear();
}

void QBluetoothServiceDiscoveryAgent::clear()
{
    // Don't clear the list while the search is ongoing
    if (isActive())
        return;

    d_ptr->discoveredDevices.clear();
    d_ptr->discoveredServices.clear();
    d_ptr->uuidFilter.clear();
}

bool QBluetoothServiceDiscoveryAgent::isActive() const
{
    return d_ptr->state != QBluetoothServiceDiscoveryAgentPrivate::Inactive;
}

QBluetoothServiceDiscoveryAgent::Error QBluetoothServiceDiscoveryAgent::error() const
{
    return d_ptr->error;
}

QString QBluetoothServiceDiscoveryAgent::errorString() const
{
    return d_ptr->errorString;
}

#include "moc_qbluetoothservicediscoveryagent.cpp"

QT_END_NAMESPACE
