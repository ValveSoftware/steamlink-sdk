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

#include "qbluetoothhostinfo.h"
#include "qbluetoothlocaldevice.h"
#include "qbluetoothservicediscoveryagent.h"
#include "qbluetoothservicediscoveryagent_p.h"

#include "qbluetoothdevicediscoveryagent.h"

QT_BEGIN_NAMESPACE

/*!
    \class QBluetoothServiceDiscoveryAgent
    \inmodule QtBluetooth
    \brief The QBluetoothServiceDiscoveryAgent class enables you to query for
    Bluetooth services.

    \since 5.2

    The discovery process relies on the Bluetooth Service Discovery Process (SDP).
    The following steps are required to query the services provided by all contactable
    Bluetooth devices:

    \list
    \li create an instance of QBluetoothServiceDiscoveryAgent,
    \li connect to either the serviceDiscovered() or finished() signals,
    \li and call start().
    \endlist

    \snippet doc_src_qtbluetooth.cpp service_discovery

    By default a minimal service discovery is performed. In this mode, the returned \l QBluetoothServiceInfo
    objects are guaranteed to contain only device and service UUID information. Depending
    on platform and device capabilities, other service information may also be available.
    The minimal service discovery mode relies on cached SDP data of the platform. Therefore it
    is possible that this discovery does not find a device although it is physically available.
    In such cases a full discovery must be performed to force an update of the platform cache.
    However for most use cases a minimal discovery is adequate as it is much quicker and other
    classes which require up-to-date information such as QBluetoothSocket::connectToService()
    will perform additional discovery if required.  If the full service information is required,
    pass \l FullDiscovery as the discoveryMode parameter to start().

    This class may internally utilize \l QBluetoothDeviceDiscoveryAgent to find unknown devices.

    The service discovery may find Bluetooth Low Energy services too if the target device
    is a combination of a classic and Low Energy device. Those devices are required to advertise
    their Low Energy services via SDP. If the target device only supports Bluetooth Low
    Energy services, it is likely to not advertise them via SDP. The \l QLowEnergyController class
    should be utilized to perform the service discovery on Low Energy devices.

    \sa QBluetoothDeviceDiscoveryAgent, QLowEnergyController
*/

/*!
    \enum QBluetoothServiceDiscoveryAgent::Error

    This enum describes errors that can occur during service discovery.

    \value NoError          No error has occurred.
    \value PoweredOffError  The Bluetooth adaptor is powered off, power it on before doing discovery.
    \value InputOutputError    Writing or reading from the device resulted in an error.
    \value InvalidBluetoothAdapterError The passed local adapter address does not match the physical
                                        adapter address of any local Bluetooth device. This value
                                        was introduced by Qt 5.3.
    \value UnknownError     An unknown error has occurred.
*/

/*!
    \enum QBluetoothServiceDiscoveryAgent::DiscoveryMode

    This enum describes the service discovery mode.

    \value MinimalDiscovery     Performs a minimal service discovery. The QBluetoothServiceInfo
    objects returned may be incomplete and are only guaranteed to contain device and service UUID information.
    Since a minimal discovery relies on cached SDP data it may not find a physically existing
    device until a \c FullDiscovery is performed.
    \value FullDiscovery        Performs a full service discovery.
*/

/*!
    \fn QBluetoothServiceDiscoveryAgent::serviceDiscovered(const QBluetoothServiceInfo &info)

    This signal is emitted when the Bluetooth service described by \a info is discovered.

    \note The passed \l QBluetoothServiceInfo parameter may contain a Bluetooth Low Energy
    service if the target device advertises the service via SDP. This is required from device
    which support both, classic Bluetooth (BaseRate) and Low Energy services.

    \sa QBluetoothDeviceInfo::coreConfigurations()
*/

/*!
    \fn QBluetoothServiceDiscoveryAgent::finished()

    This signal is emitted when the Bluetooth service discovery completes.

    Unlike the \l QBluetoothDeviceDiscoveryAgent::finished() signal this
    signal will even be emitted when an error occurred during the service discovery. Therefore
    it is recommended to check the \l error() signal to evaluate the success of the
    service discovery discovery.
*/

/*!
    \fn void QBluetoothServiceDiscoveryAgent::error(QBluetoothServiceDiscoveryAgent::Error error)

    This signal is emitted when an \a error occurs. The \a error parameter describes the error that
    occurred.
*/

/*!
    Constructs a new QBluetoothServiceDiscoveryAgent with \a parent. The search is performed via the
    local default Bluetooth adapter.
*/
QBluetoothServiceDiscoveryAgent::QBluetoothServiceDiscoveryAgent(QObject *parent)
  : QObject(parent),
    d_ptr(new QBluetoothServiceDiscoveryAgentPrivate(this, QBluetoothAddress()))
{
}

/*!
    Constructs a new QBluetoothServiceDiscoveryAgent for \a deviceAdapter and with \a parent.

    It uses \a deviceAdapter for the service search. If \a deviceAdapter is default constructed
    the resulting QBluetoothServiceDiscoveryAgent object will use the local default Bluetooth adapter.

    If a \a deviceAdapter is specified that is not a local adapter \l error() will be set to
    \l InvalidBluetoothAdapterError. Therefore it is recommended to test the error flag immediately after
    using this constructor.

    \sa error()
*/
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
        d_ptr->errorString = tr("Invalid Bluetooth adapter address");
    }
}

/*!

  Destructor for QBluetoothServiceDiscoveryAgent

*/

QBluetoothServiceDiscoveryAgent::~QBluetoothServiceDiscoveryAgent()
{
    if (isActive()) {
        disconnect(); //don't emit any signals due to stop()
        stop();
    }

    delete d_ptr;
}

/*!
    Returns the list of all discovered services.

    This list of services accumulates newly discovered services from multiple calls
    to \l start(). Unless \l clear() is called the list cannot decrease in size. This implies
    that if a remote Bluetooth device moves out of range in between two subsequent calls
    to \l start() the list may contain stale entries.

    \note The list of services should always be cleared before the discovery mode is changed.

    \sa clear()
*/
QList<QBluetoothServiceInfo> QBluetoothServiceDiscoveryAgent::discoveredServices() const
{
    Q_D(const QBluetoothServiceDiscoveryAgent);

    return d->discoveredServices;
}
/*!
    Sets the UUID filter to \a uuids.  Only services matching the UUIDs in \a uuids will be
    returned. The matching applies to the service's
    \l {QBluetoothServiceInfo::ServiceId}{ServiceId} and \l {QBluetoothServiceInfo::ServiceClassIds} {ServiceClassIds}
    attributes.

    An empty UUID list is equivalent to a list containing only QBluetoothUuid::PublicBrowseGroup.

    \sa uuidFilter()
*/
void QBluetoothServiceDiscoveryAgent::setUuidFilter(const QList<QBluetoothUuid> &uuids)
{
    Q_D(QBluetoothServiceDiscoveryAgent);

    d->uuidFilter = uuids;
}

/*!
    This is an overloaded member function, provided for convenience.

    Sets the UUID filter to a list containing the single element \a uuid.
    The matching applies to the service's \l {QBluetoothServiceInfo::ServiceId}{ServiceId}
    and \l {QBluetoothServiceInfo::ServiceClassIds} {ServiceClassIds}
    attributes.

    \sa uuidFilter()
*/
void QBluetoothServiceDiscoveryAgent::setUuidFilter(const QBluetoothUuid &uuid)
{
    Q_D(QBluetoothServiceDiscoveryAgent);

    d->uuidFilter.clear();
    d->uuidFilter.append(uuid);
}

/*!
    Returns the UUID filter.

    \sa setUuidFilter()
*/
QList<QBluetoothUuid> QBluetoothServiceDiscoveryAgent::uuidFilter() const
{
    Q_D(const QBluetoothServiceDiscoveryAgent);

    return d->uuidFilter;
}

/*!
    Sets the remote device address to \a address. If \a address is default constructed,
    services will be discovered on all contactable Bluetooth devices. A new remote
    address can only be set while there is no service discovery in progress; otherwise
    this function returns false.

    On some platforms the service discovery might lead to pairing requests.
    Therefore it is not recommended to do service discoveries on all devices.
    This function can be used to restrict the service discovery to a particular device.

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

/*!
    Returns the remote device address. If \l setRemoteAddress() is not called, the function
    will return a default constructed \l QBluetoothAddress.

    \sa setRemoteAddress()
*/
QBluetoothAddress QBluetoothServiceDiscoveryAgent::remoteAddress() const
{
    if (d_ptr->singleDevice == true)
        return d_ptr->deviceAddress;
    else
        return QBluetoothAddress();
}

/*!
    Starts service discovery. \a mode specifies the type of service discovery to perform.

    On some platforms, device discovery may lead to pairing requests.

    \sa DiscoveryMode
*/
void QBluetoothServiceDiscoveryAgent::start(DiscoveryMode mode)
{
    Q_D(QBluetoothServiceDiscoveryAgent);

    if (d->discoveryState() == QBluetoothServiceDiscoveryAgentPrivate::Inactive
            && d->error != InvalidBluetoothAdapterError) {
#ifdef QT_BLUEZ_BLUETOOTH
        // done to avoid repeated parsing for adapter address
        // on Bluez5
        d->foundHostAdapterPath.clear();
#endif
        d->setDiscoveryMode(mode);
        if (d->deviceAddress.isNull()) {
            d->startDeviceDiscovery();
        } else {
            d->discoveredDevices << QBluetoothDeviceInfo(d->deviceAddress, QString(), 0);
            d->startServiceDiscovery();
        }
    }
}

/*!
    Stops the service discovery process. The \l canceled() signal will be emitted once
    the search has stopped.
*/
void QBluetoothServiceDiscoveryAgent::stop()
{
    Q_D(QBluetoothServiceDiscoveryAgent);

    if (d->error == InvalidBluetoothAdapterError || !isActive())
        return;

    switch (d->discoveryState()) {
    case QBluetoothServiceDiscoveryAgentPrivate::DeviceDiscovery:
        d->stopDeviceDiscovery();
        break;
    case QBluetoothServiceDiscoveryAgentPrivate::ServiceDiscovery:
        d->stopServiceDiscovery();
    default:
        ;
    }

    d->discoveredDevices.clear();
}

/*!
    Clears the results of previous service discoveries and resets \l uuidFilter().
    This function does nothing during an ongoing service discovery (see \l isActive()).

    \sa discoveredServices()
*/
void QBluetoothServiceDiscoveryAgent::clear()
{
    Q_D(QBluetoothServiceDiscoveryAgent);

    //don't clear the list while the search is ongoing
    if (isActive())
        return;

    d->discoveredDevices.clear();
    d->discoveredServices.clear();
    d->uuidFilter.clear();
}

/*!
    Returns \c true if the service discovery is currently active; otherwise returns \c false.
    An active discovery can be stopped by calling \l stop().
*/
bool QBluetoothServiceDiscoveryAgent::isActive() const
{
    Q_D(const QBluetoothServiceDiscoveryAgent);

    return d->state != QBluetoothServiceDiscoveryAgentPrivate::Inactive;
}

/*!
    Returns the type of error that last occurred. If the service discovery is done
    for a single \l remoteAddress() it will return errors that occurred while trying to discover
    services on that device. If the \l remoteAddress() is not set and devices are
    discovered by a scan, errors during service discovery on individual
    devices are not saved and no signals are emitted. In this case, errors are
    fairly normal as some devices may not respond to discovery or
    may no longer be in range.  Such errors are surpressed.  If no services
    are returned, it can be assumed no services could be discovered.

*/
QBluetoothServiceDiscoveryAgent::Error QBluetoothServiceDiscoveryAgent::error() const
{
    Q_D(const QBluetoothServiceDiscoveryAgent);

    return d->error;
}

/*!
    Returns a human-readable description of the last error that occurred during the
    service discovery.
*/
QString QBluetoothServiceDiscoveryAgent::errorString() const
{
    Q_D(const QBluetoothServiceDiscoveryAgent);
    return d->errorString;
}


/*!
    \fn QBluetoothServiceDiscoveryAgent::canceled()

    This signal is triggered when the service discovery was canceled via a call to \l stop().
 */


/*!
    Starts device discovery.
*/
void QBluetoothServiceDiscoveryAgentPrivate::startDeviceDiscovery()
{
    Q_Q(QBluetoothServiceDiscoveryAgent);

    if (!deviceDiscoveryAgent) {
#ifdef QT_BLUEZ_BLUETOOTH
        deviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(m_deviceAdapterAddress, q);
#else
        deviceDiscoveryAgent = new QBluetoothDeviceDiscoveryAgent(q);
#endif
        QObject::connect(deviceDiscoveryAgent, SIGNAL(finished()),
                         q, SLOT(_q_deviceDiscoveryFinished()));
        QObject::connect(deviceDiscoveryAgent, SIGNAL(deviceDiscovered(QBluetoothDeviceInfo)),
                         q, SLOT(_q_deviceDiscovered(QBluetoothDeviceInfo)));
        QObject::connect(deviceDiscoveryAgent, SIGNAL(error(QBluetoothDeviceDiscoveryAgent::Error)),
                         q, SLOT(_q_deviceDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error)));

    }

    setDiscoveryState(DeviceDiscovery);

    deviceDiscoveryAgent->start();
}

/*!
    Stops device discovery.
*/
void QBluetoothServiceDiscoveryAgentPrivate::stopDeviceDiscovery()
{
    deviceDiscoveryAgent->stop();
    delete deviceDiscoveryAgent;
    deviceDiscoveryAgent = 0;

    setDiscoveryState(Inactive);

    Q_Q(QBluetoothServiceDiscoveryAgent);
    emit q->canceled();
}

/*!
    Called when device discovery finishes.
*/
void QBluetoothServiceDiscoveryAgentPrivate::_q_deviceDiscoveryFinished()
{
    if (deviceDiscoveryAgent->error() != QBluetoothDeviceDiscoveryAgent::NoError) {
        //Forward the device discovery error
        error = static_cast<QBluetoothServiceDiscoveryAgent::Error>(deviceDiscoveryAgent->error());
        errorString = deviceDiscoveryAgent->errorString();
        setDiscoveryState(Inactive);
        Q_Q(QBluetoothServiceDiscoveryAgent);
        emit q->error(error);
        emit q->finished();
        return;
    }

    delete deviceDiscoveryAgent;
    deviceDiscoveryAgent = 0;

    startServiceDiscovery();
}

void QBluetoothServiceDiscoveryAgentPrivate::_q_deviceDiscovered(const QBluetoothDeviceInfo &info)
{
    // look for duplicates, and cached entries
    for (int i = 0; i < discoveredDevices.count(); i++) {
        if (discoveredDevices.at(i).address() == info.address())
            discoveredDevices.removeAt(i);
    }
    discoveredDevices.prepend(info);
}

void QBluetoothServiceDiscoveryAgentPrivate::_q_deviceDiscoveryError(QBluetoothDeviceDiscoveryAgent::Error newError)
{
    error = static_cast<QBluetoothServiceDiscoveryAgent::Error>(newError);
    errorString = deviceDiscoveryAgent->errorString();

    deviceDiscoveryAgent->stop();
    delete deviceDiscoveryAgent;
    deviceDiscoveryAgent = 0;

    setDiscoveryState(Inactive);
    Q_Q(QBluetoothServiceDiscoveryAgent);
    emit q->error(error);
    emit q->finished();
}

/*!
    Starts service discovery for the next device.
*/
void QBluetoothServiceDiscoveryAgentPrivate::startServiceDiscovery()
{
    Q_Q(QBluetoothServiceDiscoveryAgent);

    if (discoveredDevices.isEmpty()) {
        setDiscoveryState(Inactive);
        emit q->finished();
        return;
    }

    setDiscoveryState(ServiceDiscovery);
    start(discoveredDevices.at(0).address());
}

/*!
    Stops service discovery.
*/
void QBluetoothServiceDiscoveryAgentPrivate::stopServiceDiscovery()
{
    stop();

    setDiscoveryState(Inactive);
}

void QBluetoothServiceDiscoveryAgentPrivate::_q_serviceDiscoveryFinished()
{
    if(!discoveredDevices.isEmpty()) {
        discoveredDevices.removeFirst();
    }

    startServiceDiscovery();
}

bool QBluetoothServiceDiscoveryAgentPrivate::isDuplicatedService(
        const QBluetoothServiceInfo &serviceInfo) const
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

#include "moc_qbluetoothservicediscoveryagent.cpp"

QT_END_NAMESPACE
