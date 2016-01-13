/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtBluetooth module of the Qt Toolkit.
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

#include "qlowenergycontroller.h"
#include "qlowenergycontroller_p.h"

#include <QtBluetooth/QBluetoothLocalDevice>

#include <algorithm>

QT_BEGIN_NAMESPACE

/*!
    \class QLowEnergyController
    \inmodule QtBluetooth
    \brief The QLowEnergyController class provides access to Bluetooth
    Low Energy Devices.

    \since 5.4

    QLowEnergyController acts as the entry point for Bluetooth Low Energy
    development. Each QLowEnergyController instance acts as placeholder
    towards a remote Low Energy device enabling connection control,
    service discovery and state tracking.

    Bluetooth Low Energy defines two types of devices; the peripheral and
    the central. Each role performs a different task. The peripheral device
    provides data which is utilized by central devices. An example might be a
    humidity sensor which measures the moisture in a winter garden. A device
    such as a mobile phone might read the sensor's value and display it to the user
    in the greater context of all sensors in the same environment. In this case
    the sensor is the peripheral device and the mobile phone acts as the
    central device.

    At the moment Qt only supports the central role and therefore the remote
    device can only be a device acting as a peripheral. This implies that the local
    device acts within the boundaries of the central role as per the Bluetooth 4.0
    specification.

    The first step is to establish a connection via \l connectToDevice().
    Once the connection has been established, the controller's \l state()
    changes to \l QLowEnergyController::ConnectedState and the \l connected()
    signal is emitted. It is important to mention that the remote device can
    usually only be connected to a single device. Therefore it is not
    possible to have multiple instances of this class being connected to the
    same remote device. The \l disconnectFromDevice() function is used to break
    the existing connection.

    The second step after establishing the connection is to discover the services
    offered by the remote peripheral device. This process is started via
    \l discoverServices() and has finished once the \l discoveryFinished() signal
    has been emitted. The discovered services can be enumerated via
    \l services().

    The last step is to create service objects. The \l createServiceObject()
    function acts as factory for each service object and expects the service
    UUID as parameter. The calling context should take ownership of the returned
    \l QLowEnergyService instance.

    Any \l QLowEnergyService, \l QLowEnergyCharacteristic or
    \l QLowEnergyDescriptor instance which is later created from this controller's
    connection becomes invalid as soon as the controller disconnects from the
    remote Bluetooth Low Energy device.

    \note This class is provided by Qt 5.4 as part of a Bluetooth Low Energy Tech Preview.
    Some API elements may change until the final release of the feature.

    \sa QLowEnergyService, QLowEnergyCharacteristic, QLowEnergyDescriptor
*/

/*!
    \enum QLowEnergyController::Error

    Indicates all possible error conditions found during the controller's
    existence.

    \value NoError                      No error has occurred.
    \value UnknownError                 An unknown error has occurred.
    \value UnknownRemoteDeviceError     The remote Bluetooth Low Energy device with the address passed to
                                        the constructor of this class cannot be found.
    \value NetworkError                 The attempt to read from or write to the
                                        remote device failed.
    \value InvalidBluetoothAdapterError The local Bluetooth device with the address passed to
                                        the constructor of this class cannot be found or
                                        there is no local Bluetooth device.
*/

/*!
    \enum QLowEnergyController::ControllerState

    Indicates the state of the controller object.

    \value UnconnectedState   The controller is not connected to a remote device.
    \value ConnectingState    The controller is attempting to connect to a remote device.
    \value ConnectedState     The controller is connected to a remote device.
    \value DiscoveringState   The controller is retrieving the list of services offered
                              by the remote device.
    \value DiscoveredState    The controller has discovered all services offered by the
                              remote device.
    \value ClosingState       The controller is about to be disconnected from the remote device.
*/

/*!
    \enum QLowEnergyController::RemoteAddressType

    Indicates what type of Bluetooth address the remote device uses.

    \value PublicAddress The peripheral uses a public Bluetooth address.
    \value RandomAddress A random address is a Bluetooth Low Energy security feature.
                         Peripherals using such addresses may frequently change their
                         Bluetooth address. This information is needed when trying to
                         connect to a peripheral.
 */


/*!
    \fn void QLowEnergyController::connected()

    This signal is emitted when the controller successfully connects to the remote
    Low Energy device.
*/

/*!
    \fn void QLowEnergyController::disconnected()

    This signal is emitted when the controller disconnects from the remote
    Low Energy device.
*/

/*!
    \fn void QLowEnergyController::stateChanged(ControllerState state)

    This signal is emitted when the controller's state changes. The new
    \a state can also be retrieved via \l state().

    \sa state()
*/

/*!
    \fn void QLowEnergyController::error(QLowEnergyController::Error newError)

    This signal is emitted when an error occurs.
    The \a newError parameter describes the error that occurred.

    \sa error(), errorString()
*/

/*!
    \fn void QLowEnergyController::serviceDiscovered(const QBluetoothUuid &newService)

    This signal is emitted each time a new service is discovered. The
    \a newService parameter contains the UUID of the found service.

    \sa discoverServices(), discoveryFinished()
*/

/*!
    \fn void QLowEnergyController::discoveryFinished()

    This signal is emitted when the running service discovery finishes.
    The signal is not emitted if the discovery process finishes with
    an error.

    \sa discoverServices(), error()
*/

void QLowEnergyControllerPrivate::setError(
        QLowEnergyController::Error newError)
{
    Q_Q(QLowEnergyController);
    error = newError;

    switch (newError) {
    case QLowEnergyController::UnknownRemoteDeviceError:
        errorString = QLowEnergyController::tr("Remote device cannot be found");
        break;
    case QLowEnergyController::InvalidBluetoothAdapterError:
        errorString = QLowEnergyController::tr("Cannot find local adapter");
        break;
    case QLowEnergyController::NetworkError:
        errorString = QLowEnergyController::tr("Error occurred during connection I/O");
        break;
    case QLowEnergyController::UnknownError:
    default:
        errorString = QLowEnergyController::tr("Unknown Error");
        break;
    }

    emit q->error(newError);
}

bool QLowEnergyControllerPrivate::isValidLocalAdapter()
{
    if (localAdapter.isNull())
        return false;

    const QList<QBluetoothHostInfo> foundAdapters = QBluetoothLocalDevice::allDevices();
    bool adapterFound = false;

    foreach (const QBluetoothHostInfo &info, foundAdapters) {
        if (info.address() == localAdapter) {
            adapterFound = true;
            break;
        }
    }

    return adapterFound;
}

void QLowEnergyControllerPrivate::setState(
        QLowEnergyController::ControllerState newState)
{
    Q_Q(QLowEnergyController);
    if (state == newState)
        return;

    state = newState;
    emit q->stateChanged(state);
}

void QLowEnergyControllerPrivate::invalidateServices()
{
    foreach (const QSharedPointer<QLowEnergyServicePrivate> service, serviceList.values()) {
        service->setController(0);
        service->setState(QLowEnergyService::InvalidService);
    }

    serviceList.clear();
}

QSharedPointer<QLowEnergyServicePrivate> QLowEnergyControllerPrivate::serviceForHandle(
        QLowEnergyHandle handle)
{
    foreach (QSharedPointer<QLowEnergyServicePrivate> service, serviceList.values())
        if (service->startHandle <= handle && handle <= service->endHandle)
            return service;

    return QSharedPointer<QLowEnergyServicePrivate>();
}

/*!
    Returns a valid characteristic if the given handle is the
    handle of the characteristic itself or one of its descriptors
 */
QLowEnergyCharacteristic QLowEnergyControllerPrivate::characteristicForHandle(
        QLowEnergyHandle handle)
{
    QSharedPointer<QLowEnergyServicePrivate> service = serviceForHandle(handle);
    if (service.isNull())
        return QLowEnergyCharacteristic();

    if (service->characteristicList.isEmpty())
        return QLowEnergyCharacteristic();

    // check whether it is the handle of a characteristic header
    if (service->characteristicList.contains(handle))
        return QLowEnergyCharacteristic(service, handle);

    // check whether it is the handle of the characteristic value or its descriptors
    QList<QLowEnergyHandle> charHandles = service->characteristicList.keys();
    std::sort(charHandles.begin(), charHandles.end());
    for (int i = charHandles.size() - 1; i >= 0; i--) {
        if (charHandles.at(i) > handle)
            continue;

        return QLowEnergyCharacteristic(service, charHandles.at(i));
    }

    return QLowEnergyCharacteristic();
}

/*!
    Returns a valid descriptor if \a handle belongs to a descriptor;
    otherwise an invalid one.
 */
QLowEnergyDescriptor QLowEnergyControllerPrivate::descriptorForHandle(
        QLowEnergyHandle handle)
{
    const QLowEnergyCharacteristic matchingChar = characteristicForHandle(handle);
    if (!matchingChar.isValid())
        return QLowEnergyDescriptor();

    const QLowEnergyServicePrivate::CharData charData = matchingChar.
            d_ptr->characteristicList[matchingChar.attributeHandle()];

    if (charData.descriptorList.contains(handle))
        return QLowEnergyDescriptor(matchingChar.d_ptr, matchingChar.attributeHandle(),
                                    handle);

    return QLowEnergyDescriptor();
}

/*!
    Returns the length of the updated characteristic value.
 */
quint16 QLowEnergyControllerPrivate::updateValueOfCharacteristic(
        QLowEnergyHandle charHandle,const QByteArray &value, bool appendValue)
{
    QSharedPointer<QLowEnergyServicePrivate> service = serviceForHandle(charHandle);
    if (!service.isNull() && service->characteristicList.contains(charHandle)) {
        if (appendValue)
            service->characteristicList[charHandle].value += value;
        else
            service->characteristicList[charHandle].value = value;

        return service->characteristicList[charHandle].value.size();
    }

    return 0;
}

/*!
    Returns the length of the updated descriptor value.
 */
quint16 QLowEnergyControllerPrivate::updateValueOfDescriptor(
        QLowEnergyHandle charHandle, QLowEnergyHandle descriptorHandle,
        const QByteArray &value, bool appendValue)
{
    QSharedPointer<QLowEnergyServicePrivate> service = serviceForHandle(charHandle);
    if (service.isNull() || !service->characteristicList.contains(charHandle))
        return 0;

    if (!service->characteristicList[charHandle].descriptorList.contains(descriptorHandle))
        return 0;

    if (appendValue)
        service->characteristicList[charHandle].descriptorList[descriptorHandle].value += value;
    else
        service->characteristicList[charHandle].descriptorList[descriptorHandle].value = value;

    return service->characteristicList[charHandle].descriptorList[descriptorHandle].value.size();
}

/*!
    Constructs a new instance of this class with \a parent.

    The \a remoteDevice must contain the address of the
    remote Bluetooth Low Energy device to which this object
    should attempt to connect later on.

    The controller uses the local default Bluetooth adapter for
    the connection management.
 */
QLowEnergyController::QLowEnergyController(
                            const QBluetoothAddress &remoteDevice,
                            QObject *parent)
    : QObject(parent), d_ptr(new QLowEnergyControllerPrivate())
{
    Q_D(QLowEnergyController);
    d->q_ptr = this;
    d->remoteDevice = remoteDevice;
    d->localAdapter = QBluetoothLocalDevice().address();
    d->addressType = QLowEnergyController::PublicAddress;
}

/*!
    Constructs a new instance of this class with \a parent.

    The \a remoteDevice must contain the address of the
    remote Bluetooth Low Energy device to which this object
    should attempt to connect later on.

    The connection is established via \a localDevice. If \a localDevice
    is invalid, the local default device is automatically selected. If
    \a localDevice specifies a local device that is not a local Bluetooth
    adapter, \l error() is set to \l InvalidBluetoothAdapterError once
    \l connectToDevice() is called.
 */
QLowEnergyController::QLowEnergyController(
                            const QBluetoothAddress &remoteDevice,
                            const QBluetoothAddress &localDevice,
                            QObject *parent)
    : QObject(parent), d_ptr(new QLowEnergyControllerPrivate())
{
    Q_D(QLowEnergyController);
    d->q_ptr = this;
    d->remoteDevice = remoteDevice;
    d->localAdapter = localDevice;
}

/*!
    Destroys the QLowEnergyController instance.
 */
QLowEnergyController::~QLowEnergyController()
{
    disconnectFromDevice(); //in case we were connected
    delete d_ptr;
}

/*!
  Returns the address of the local Bluetooth adapter being used for the
  communication.

  If this class instance was requested to use the default adapter
  but there was no default adapter when creating this
  class instance, the returned \l QBluetoothAddress will be null.

  \sa QBluetoothAddress::isNull()
 */
QBluetoothAddress QLowEnergyController::localAddress() const
{
    return d_ptr->localAdapter;
}

/*!
    Returns the address of the remote Bluetooth Low Energy device.
 */
QBluetoothAddress QLowEnergyController::remoteAddress() const
{
    return d_ptr->remoteDevice;
}

/*!
    Returns the current state of the controller.

    \sa stateChanged()
 */
QLowEnergyController::ControllerState QLowEnergyController::state() const
{
    return d_ptr->state;
}

/*!
    Returns the type of \l remoteAddress(). By default, this value is initialized
    to \l PublicAddress.

    \sa setRemoteAddressType()
 */
QLowEnergyController::RemoteAddressType QLowEnergyController::remoteAddressType() const
{
    return d_ptr->addressType;
}

/*!
    Sets the remote address \a type. The type is required to connect
    to the remote Bluetooth Low Energy device.
 */
void QLowEnergyController::setRemoteAddressType(
                    QLowEnergyController::RemoteAddressType type)
{
    d_ptr->addressType = type;
}

/*!
    Connects to the remote Bluetooth Low Energy device.

    This function does nothing if the controller's \l state()
    is not equal to \l UnconnectedState. The \l connected() signal is emitted
    once the connection is successfully established.

    \sa disconnectFromDevice()
 */
void QLowEnergyController::connectToDevice()
{
    Q_D(QLowEnergyController);

    if (!d->isValidLocalAdapter()) {
        d->setError(QLowEnergyController::InvalidBluetoothAdapterError);
        return;
    }

    if (state() != QLowEnergyController::UnconnectedState)
        return;

    d->connectToDevice();
}

/*!
    Disconnects from the remote device.

    Any \l QLowEnergyService, \l QLowEnergyCharacteristic or \l QLowEnergyDescriptor
    instance that resulted from the current connection is automatically invalidated.
    Once any of those objects become invalid they remain invalid even if this
    controller object reconnects.

    This function does nothing if the controller is in the \l UnconnectedState.

    \sa connectToDevice()
 */
void QLowEnergyController::disconnectFromDevice()
{
    Q_D(QLowEnergyController);

    if (state() == QLowEnergyController::UnconnectedState)
        return;

    d->invalidateServices();
    d->disconnectFromDevice();
}

/*!
    Initiates the service discovery process.

    The discovery progress is indicated via the \l serviceDiscovered() signal.
    The \l discoveryFinished() signal is emitted when the process has finished.

    If the controller instance is not connected or the controller has performed
    the service discovery already this function will do nothing.
 */
void QLowEnergyController::discoverServices()
{
    Q_D(QLowEnergyController);

    if (d->state != QLowEnergyController::ConnectedState)
        return;

    d->setState(QLowEnergyController::DiscoveringState);
    d->discoverServices();
}

/*!
    Returns the list of services offered by the remote device.

    The list contains all primary and secondary services.

    \sa createServiceObject()
 */
QList<QBluetoothUuid> QLowEnergyController::services() const
{
    return d_ptr->serviceList.keys();
}

/*!
    Creates an instance of the service represented by \a serviceUuid.
    The \a serviceUuid parameter must have been obtained via
    \l services().

    The caller takes ownership of the returned pointer and may pass
    a \a parent parameter as default owner.

    This function returns a null pointer if no service with
    \a serviceUuid can be found on the remote device or the controller
    is disconnected.

    This function can return instances for secondary services
    too. The include relationships between services can be expressed
    via \l QLowEnergyService::includedServices().

    If this function is called multiple times using the same service UUID,
    the returned \l QLowEnergyService instances share their internal
    data. Therefore if one of the instances initiates the discovery
    of the service details, the other instances automatically
    transition into the discovery state too.

    \sa services()
 */
QLowEnergyService *QLowEnergyController::createServiceObject(
        const QBluetoothUuid &serviceUuid, QObject *parent)
{
    Q_D(QLowEnergyController);
    if (!d->serviceList.contains(serviceUuid))
        return 0;

    QLowEnergyService *service = new QLowEnergyService(
                d->serviceList.value(serviceUuid), parent);

    return service;
}

/*!
    Returns the last occurred error or \l NoError.
*/
QLowEnergyController::Error QLowEnergyController::error() const
{
    return d_ptr->error;
}

/*!
    Returns a textual representation of the last occurred error.
    The string is translated.
*/
QString QLowEnergyController::errorString() const
{
    return d_ptr->errorString;
}

QT_END_NAMESPACE
