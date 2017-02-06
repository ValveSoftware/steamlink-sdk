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

#include "qlowenergycontroller.h"
#include "qlowenergycontroller_p.h"

#include "qlowenergycharacteristicdata.h"
#include "qlowenergyconnectionparameters.h"
#include "qlowenergydescriptordata.h"
#include "qlowenergyservicedata.h"

#include <QtBluetooth/QBluetoothLocalDevice>
#include <QtCore/QLoggingCategory>

#include <algorithm>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT)

/*!
    \class QLowEnergyController
    \inmodule QtBluetooth
    \brief The QLowEnergyController class provides access to Bluetooth
    Low Energy Devices.

    \since 5.4

    QLowEnergyController acts as the entry point for Bluetooth Low Energy
    development.

    Bluetooth Low Energy defines two types of devices; the peripheral and
    the central. Each role performs a different task. The peripheral device
    provides data which is utilized by central devices. An example might be a
    humidity sensor which measures the moisture in a winter garden. A device
    such as a mobile phone might read the sensor's value and display it to the user
    in the greater context of all sensors in the same environment. In this case
    the sensor is the peripheral device and the mobile phone acts as the
    central device.

    A controller in the central role is created via the \l createCentral() factory method.
    Such an object essentially acts as a placeholder towards a remote Low Energy peripheral
    device, enabling features such as service discovery and state tracking.

    After having created a controller object in the central role, the first step is to establish
    a connection via \l connectToDevice().
    Once the connection has been established, the controller's \l state()
    changes to \l QLowEnergyController::ConnectedState and the \l connected()
    signal is emitted. It is important to mention that some platforms such as
    a BlueZ based Linux cannot maintain two connected instances of
    \l QLowEnergyController to the same remote device. In such cases the second
    call to \l connectToDevice() may fail. This limitation may disappear at some
    stage in the future. The \l disconnectFromDevice() function is used to break
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

    A controller in the peripheral role is created via the \l createPeripheral() factory method.
    Such an object acts as a peripheral device itself, enabling features such as advertising
    services and allowing clients to get notified about changes to characteristic values.

    After having created a controller object in the peripheral role, the first step is to
    populate the set of GATT services offered to client devices via calls to \l addService().
    Afterwards, one would call \l startAdvertising() to let the device broadcast some data
    and, depending on the type of advertising being done, also listen for incoming connections
    from GATT clients.

    \sa QLowEnergyService, QLowEnergyCharacteristic, QLowEnergyDescriptor
    \sa QLowEnergyAdvertisingParameters, QLowEnergyAdvertisingData
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
    \value ConnectionError              The attempt to connect to the remote device failed.
                                        This value was introduced by Qt 5.5.
    \value AdvertisingError             The attempt to start advertising failed.
                                        This value was introduced by Qt 5.7.
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
    \value AdvertisingState   The controller is currently advertising data.
                              This value was introduced by Qt 5.7.
*/

/*!
    \enum QLowEnergyController::RemoteAddressType

    Indicates what type of Bluetooth address the remote device uses.

    \value PublicAddress The remote device uses a public Bluetooth address.
    \value RandomAddress A random address is a Bluetooth Low Energy security feature.
                         Peripherals using such addresses may frequently change their
                         Bluetooth address. This information is needed when trying to
                         connect to a peripheral.
 */

/*!
    \enum QLowEnergyController::Role

    Indicates the role of the controller object.

    \value CentralRole
       The controller acts as a client interacting with a remote device which is in the peripheral
       role. The controller can initiate connections, discover services and
       read and write characteristics.
    \value PeripheralRole
       The controller can be used to advertise services and handle incoming
       connections and client requests, acting as a GATT server. A remote device connected to
       the controller is in the central role.

   \sa QLowEnergyController::createCentral()
   \sa QLowEnergyController::createPeripheral()
   \since 5.7
   \note The peripheral role is currently only supported on Linux. In addition, handling the
         "Signed Write" ATT command on the server side requires BlueZ 5 and kernel version 3.7
         or newer.
 */


/*!
    \fn void QLowEnergyController::connected()

    This signal is emitted when the controller successfully connects to the remote
    Low Energy device (if the controller is in the \l CentralRole) or if a remote Low Energy
    device connected to the controller (if the controller is in the \l PeripheralRole).
    On iOS and OS X this signal is not reliable if the controller is in the \l PeripheralRole
    - the controller only guesses that some central connected to our peripheral as
    soon as this central tries to write/read a characteristic/descriptor.
*/

/*!
    \fn void QLowEnergyController::disconnected()

    This signal is emitted when the controller disconnects from the remote
    Low Energy device or vice versa. On iOS and OS X this signal is unreliable
    if the controller is in the \l PeripheralRole.
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

    This signal can only be emitted if the controller is in the \c CentralRole.

    \sa discoverServices(), discoveryFinished()
*/

/*!
    \fn void QLowEnergyController::discoveryFinished()

    This signal is emitted when the running service discovery finishes.
    The signal is not emitted if the discovery process finishes with
    an error.

    This signal can only be emitted if the controller is in the \l CentralRole.

    \sa discoverServices(), error()
*/

/*!
    \fn void QLowEnergyController::connectionUpdated(const QLowEnergyConnectionParameters &newParameters)

    This signal is emitted when the connection parameters change. This can happen as a result
    of calling \l requestConnectionUpdate() or due to other reasons, for instance because
    the other side of the connection requested new parameters. The new values can be retrieved
    from \a newParameters.

    \since 5.7
    \sa requestConnectionUpdate()
*/


void registerQLowEnergyControllerMetaType()
{
    static bool initDone = false;
    if (!initDone) {
        qRegisterMetaType<QLowEnergyController::ControllerState>();
        qRegisterMetaType<QLowEnergyController::Error>();
        qRegisterMetaType<QLowEnergyConnectionParameters>();
        initDone = true;
    }
}


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
    case QLowEnergyController::ConnectionError:
        errorString = QLowEnergyController::tr("Error occurred trying to connect to remote device.");
        break;
    case QLowEnergyController::AdvertisingError:
        errorString = QLowEnergyController::tr("Error occurred trying to start advertising");
        break;
    case QLowEnergyController::NoError:
        return;
    default:
    case QLowEnergyController::UnknownError:
        errorString = QLowEnergyController::tr("Unknown Error");
        break;
    }

    emit q->error(newError);
}

bool QLowEnergyControllerPrivate::isValidLocalAdapter()
{
#ifdef QT_WINRT_BLUETOOTH
    return true;
#endif
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
    if (state == QLowEnergyController::UnconnectedState
            && role == QLowEnergyController::PeripheralRole) {
        remoteDevice.clear();
    }
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
    if (!service.isNull()) {
        CharacteristicDataMap::iterator charIt = service->characteristicList.find(charHandle);
        if (charIt != service->characteristicList.end()) {
            QLowEnergyServicePrivate::CharData &charDetails = charIt.value();

            if (appendValue)
                charDetails.value += value;
            else
                charDetails.value = value;

            return charDetails.value.size();
        }
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
    if (!service.isNull()) {
        CharacteristicDataMap::iterator charIt = service->characteristicList.find(charHandle);
        if (charIt != service->characteristicList.end()) {
            QLowEnergyServicePrivate::CharData &charDetails = charIt.value();

            DescriptorDataMap::iterator descIt = charDetails.descriptorList.find(descriptorHandle);
            if (descIt != charDetails.descriptorList.end()) {
                QLowEnergyServicePrivate::DescData &descDetails = descIt.value();

                if (appendValue)
                    descDetails.value += value;
                else
                    descDetails.value = value;

                return descDetails.value.size();
            }
        }
    }

    return 0;
}

/*!
    Constructs a new instance of this class with \a parent.

    The \a remoteDevice must contain the address of the
    remote Bluetooth Low Energy device to which this object
    should attempt to connect later on.

    The controller uses the local default Bluetooth adapter for
    the connection management.

    \obsolete
 */
QLowEnergyController::QLowEnergyController(
                            const QBluetoothAddress &remoteDevice,
                            QObject *parent)
    : QObject(parent), d_ptr(new QLowEnergyControllerPrivate())
{
    Q_D(QLowEnergyController);
    d->q_ptr = this;
    d->role = CentralRole;
    d->remoteDevice = remoteDevice;
    d->localAdapter = QBluetoothLocalDevice().address();
    d->addressType = QLowEnergyController::PublicAddress;
    d->init();
}

/*!
    Constructs a new instance of this class with \a parent.

    The \a remoteDeviceInfo must contain the details of the
    remote Bluetooth Low Energy device to which this object
    should attempt to connect later on.

    The controller uses the local default Bluetooth adapter for
    the connection management.

    \since 5.5
    \obsolete
*/
QLowEnergyController::QLowEnergyController(
                            const QBluetoothDeviceInfo &remoteDeviceInfo,
                            QObject *parent)
    : QObject(parent), d_ptr(new QLowEnergyControllerPrivate())
{
    Q_D(QLowEnergyController);
    d->q_ptr = this;
    d->role = CentralRole;
    d->remoteDevice = remoteDeviceInfo.address();
    d->localAdapter = QBluetoothLocalDevice().address();
    d->addressType = QLowEnergyController::PublicAddress;
    d->remoteName = remoteDeviceInfo.name();
    d->init();
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

    \obsolete
 */
QLowEnergyController::QLowEnergyController(
                            const QBluetoothAddress &remoteDevice,
                            const QBluetoothAddress &localDevice,
                            QObject *parent)
    : QObject(parent), d_ptr(new QLowEnergyControllerPrivate())
{
    Q_D(QLowEnergyController);
    d->q_ptr = this;
    d->role = CentralRole;
    d->remoteDevice = remoteDevice;
    d->localAdapter = localDevice;
    d->init();
}

/*!
   Returns a new object of this class that is in the \l CentralRole and has the
   parent object \a parent.
   The \a remoteDevice refers to the device that a connection will be established to later.

   The controller uses the local default Bluetooth adapter for the connection management.

   \sa QLowEnergyController::CentralRole
   \since 5.7
 */
QLowEnergyController *QLowEnergyController::createCentral(const QBluetoothDeviceInfo &remoteDevice,
                                                          QObject *parent)
{
    return new QLowEnergyController(remoteDevice, parent);
}


/*!
   Returns a new object of this class that is in the \l PeripheralRole and has the
   parent object \a parent.
   Typically, the next step is to call \l startAdvertising() on the returned object.

   The controller uses the local default Bluetooth adapter for the connection management.

   \sa QLowEnergyController::PeripheralRole
   \since 5.7
 */
QLowEnergyController *QLowEnergyController::createPeripheral(QObject *parent)
{
    return new QLowEnergyController(parent);
}

QLowEnergyController::QLowEnergyController(QObject *parent)
    : QObject(parent), d_ptr(new QLowEnergyControllerPrivate())
{
    Q_D(QLowEnergyController);
    d->q_ptr = this;
    d->role = PeripheralRole;
    d->localAdapter = QBluetoothLocalDevice().address();
    d->init();
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

    For a controller in the \l CentralRole, this value will always be the one passed in when
    the controller object was created. For a controller in the \l PeripheralRole, this value
    is the address of the currently connected client device. In particular, this address will
    be invalid if the controller is not currently in the \l ConnectedState.
 */
QBluetoothAddress QLowEnergyController::remoteAddress() const
{
    return d_ptr->remoteDevice;
}

/*!
    Returns the unique identifier of the remote Bluetooth Low Energy device.

    On macOS/iOS/tvOS CoreBluetooth does not expose/accept hardware addresses for
    LE devices; instead developers are supposed to use unique 128-bit UUIDs, generated
    by CoreBluetooth. These UUIDS will stay constant for the same central <-> peripheral
    pair and we use them when connecting to a remote device. For a controller in the
    \l CentralRole, this value will always be the one passed in when the controller
    object was created. For a controller in the \l PeripheralRole, this value is invalid.

    \since 5.8
 */
QBluetoothUuid QLowEnergyController::remoteDeviceUuid() const
{
    return  QBluetoothUuid();
}

/*!
    Returns the name of the remote Bluetooth Low Energy device, if the controller is in the
    \l CentralRole. Otherwise the result is unspecified.

    \since 5.5
 */
QString QLowEnergyController::remoteName() const
{
    return d_ptr->remoteName;
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

    On Linux/BlueZ systems, it is not possible to connect to the same
    remote device using two instances of this class. The second call
    to this function may fail with an error. This limitation may
    be removed in future releases.

    \sa disconnectFromDevice()
 */
void QLowEnergyController::connectToDevice()
{
    Q_D(QLowEnergyController);

    if (role() != CentralRole) {
        qCWarning(QT_BT) << "Connection can only be established while in central role";
        return;
    }

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

    \note Some platforms internally cache the service list of a device
    which was discovered in the past. This can be problematic if the remote device
    changed its list of services or their inclusion tree. If this behavior is a
    problem, the best workaround is to temporarily turn Bluetooth off. This
    causes a reset of the cache data. Currently Android exhibits such a
    cache behavior.
 */
void QLowEnergyController::discoverServices()
{
    Q_D(QLowEnergyController);

    if (d->role != CentralRole) {
        qCWarning(QT_BT) << "Cannot discover services in peripheral role";
        return;
    }
    if (d->state != QLowEnergyController::ConnectedState)
        return;

    d->setState(QLowEnergyController::DiscoveringState);
    d->discoverServices();
}

/*!
    Returns the list of services offered by the remote device, if the controller is in
    the \l CentralRole. Otherwise, the result is unspecified.

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

    QLowEnergyService *service = Q_NULLPTR;

    ServiceDataMap::const_iterator it = d->serviceList.constFind(serviceUuid);
    if (it != d->serviceList.constEnd()) {
        const QSharedPointer<QLowEnergyServicePrivate> &serviceData = it.value();

        service = new QLowEnergyService(serviceData, parent);
    }

    return service;
}

/*!
   Starts advertising the data given in \a advertisingData and \a scanResponseData, using
   the parameters set in \a parameters. The controller has to be in the \l PeripheralRole.
   If \a parameters indicates that the advertisement should be connectable, then this function
   also starts listening for incoming client connections.

   Providing \a scanResponseData is not required, as it is not applicable for certain
   configurations of \c parameters. \a advertisingData and \a scanResponseData are limited
   to 31 byte user data. If, for example, several 128bit uuids are added to \a advertisingData,
   the advertised packets may not contain all uuids. The existing limit may have caused the truncation
   of uuids. In such cases \a scanResponseData may be used for additional information.

   If this object is currently not in the \l UnconnectedState, nothing happens.
   \note Advertising will stop automatically once a client connects to the local device.

   \since 5.7
   \sa stopAdvertising()
 */
void QLowEnergyController::startAdvertising(const QLowEnergyAdvertisingParameters &parameters,
                                            const QLowEnergyAdvertisingData &advertisingData,
                                            const QLowEnergyAdvertisingData &scanResponseData)
{
    Q_D(QLowEnergyController);
    if (role() != PeripheralRole) {
        qCWarning(QT_BT) << "Cannot start advertising in central role" << state();
        return;
    }
    if (state() != UnconnectedState) {
        qCWarning(QT_BT) << "Cannot start advertising in state" << state();
        return;
    }
    d->startAdvertising(parameters, advertisingData, scanResponseData);
}

/*!
   Stops advertising, if this object is currently in the advertising state.

   \since 5.7
   \sa startAdvertising()
 */
void QLowEnergyController::stopAdvertising()
{
    Q_D(QLowEnergyController);
    if (state() != AdvertisingState) {
        qCDebug(QT_BT) << "stopAdvertising called in state" << state();
        return;
    }
    d->stopAdvertising();
}

/*!
  Constructs and returns a \l QLowEnergyService object with \a parent from \a service.
  The controller must be in the \l PeripheralRole and in the \l UnconnectedState. The \a service
  object must be valid.

  \since 5.7
  \sa QLowEnergyServiceData::addIncludedService
 */
QLowEnergyService *QLowEnergyController::addService(const QLowEnergyServiceData &service,
                                                    QObject *parent)
{
    if (role() != PeripheralRole) {
        qCWarning(QT_BT) << "Services can only be added in the peripheral role";
        return nullptr;
    }
    if (state() != UnconnectedState) {
        qCWarning(QT_BT) << "Services can only be added in unconnected state";
        return nullptr;
    }
    if (!service.isValid()) {
        qCWarning(QT_BT) << "Not adding invalid service";
        return nullptr;
    }

    // Spec says services "should" be grouped by uuid length (16-bit first, then 128-bit).
    // Since this is not mandatory, we ignore it here and let the caller take responsibility
    // for it.

    const auto servicePrivate = QSharedPointer<QLowEnergyServicePrivate>::create();
    servicePrivate->state = QLowEnergyService::LocalService;
    servicePrivate->setController(d_ptr);
    servicePrivate->uuid = service.uuid();
    servicePrivate->type = service.type() == QLowEnergyServiceData::ServiceTypePrimary
            ? QLowEnergyService::PrimaryService : QLowEnergyService::IncludedService;
    foreach (QLowEnergyService * const includedService, service.includedServices()) {
        servicePrivate->includedServices << includedService->serviceUuid();
        includedService->d_ptr->type |= QLowEnergyService::IncludedService;
    }

    // Spec v4.2, Vol 3, Part G, Section 3.
    const QLowEnergyHandle oldLastHandle = d_ptr->lastLocalHandle;
    servicePrivate->startHandle = ++d_ptr->lastLocalHandle; // Service declaration.
    d_ptr->lastLocalHandle += servicePrivate->includedServices.count(); // Include declarations.
    foreach (const QLowEnergyCharacteristicData &cd, service.characteristics()) {
        const QLowEnergyHandle declHandle = ++d_ptr->lastLocalHandle;
        QLowEnergyServicePrivate::CharData charData;
        charData.valueHandle = ++d_ptr->lastLocalHandle;
        charData.uuid = cd.uuid();
        charData.properties = cd.properties();
        charData.value = cd.value();
        foreach (const QLowEnergyDescriptorData &dd, cd.descriptors()) {
            QLowEnergyServicePrivate::DescData descData;
            descData.uuid = dd.uuid();
            descData.value = dd.value();
            charData.descriptorList.insert(++d_ptr->lastLocalHandle, descData);
        }
        servicePrivate->characteristicList.insert(declHandle, charData);
    }
    servicePrivate->endHandle = d_ptr->lastLocalHandle;
    const bool handleOverflow = d_ptr->lastLocalHandle <= oldLastHandle;
    if (handleOverflow) {
        qCWarning(QT_BT) << "Not enough attribute handles left to create this service";
        d_ptr->lastLocalHandle = oldLastHandle;
        return nullptr;
    }

    d_ptr->localServices.insert(servicePrivate->uuid, servicePrivate);
    d_ptr->addToGenericAttributeList(service, servicePrivate->startHandle);
    return new QLowEnergyService(servicePrivate, parent);
}

/*!
  Requests the controller to update the connection according to \a parameters.
  If the request is successful, the \l connectionUpdated() signal will be emitted
  with the actual new parameters.
  See the  \l QLowEnergyConnectionParameters class for more information on connection parameters.
  \note Currently, this functionality is only implemented on Linux.

  \since 5.7
 */
void QLowEnergyController::requestConnectionUpdate(const QLowEnergyConnectionParameters &parameters)
{
    if (state() != ConnectedState) {
        qCWarning(QT_BT) << "Connection update request only possible in connected state";
        return;
    }
    d_ptr->requestConnectionUpdate(parameters);
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

/*!
   Returns the role that this controller object is in.

   The role is determined when constructing a QLowEnergyController instance
   using \l createCentral() or \l createPeripheral().

   \since 5.7
 */
QLowEnergyController::Role QLowEnergyController::role() const
{
    return d_ptr->role;
}

QT_END_NAMESPACE
