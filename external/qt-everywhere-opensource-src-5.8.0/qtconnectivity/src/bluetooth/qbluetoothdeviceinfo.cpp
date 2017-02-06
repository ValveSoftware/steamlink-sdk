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

#include "qbluetoothdeviceinfo.h"
#include "qbluetoothdeviceinfo_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QBluetoothDeviceInfo
    \inmodule QtBluetooth
    \brief The QBluetoothDeviceInfo class stores information about the Bluetooth
    device.

    \since 5.2

    QBluetoothDeviceInfo provides information about a Bluetooth device's name, address and class of device.
*/

/*!
    \enum QBluetoothDeviceInfo::MajorDeviceClass

    This enum describes a Bluetooth device's major device class.

    \value MiscellaneousDevice  A miscellaneous device.
    \value ComputerDevice       A computer device or PDA.
    \value PhoneDevice          A telephone device.
    \value LANAccessDevice      A device that provides access to a local area network.
    \value AudioVideoDevice     A device capable of playback or capture of audio and/or video.
    \value PeripheralDevice     A peripheral device such as a keyboard, mouse, and so on.
    \value ImagingDevice        An imaging device such as a display, printer, scanner or camera.
    \value WearableDevice       A wearable device such as a watch or pager.
    \value ToyDevice            A toy.
    \value HealthDevice         A health reated device such as heart rate or temperature monitor.
    \value UncategorizedDevice  A device that does not fit into any of the other device classes.
*/

/*!
    \enum QBluetoothDeviceInfo::MinorMiscellaneousClass

    This enum describes the minor device classes for miscellaneous Bluetooth devices.

    \value UncategorizedMiscellaneous   An uncategorized miscellaneous device.
*/

/*!
    \enum QBluetoothDeviceInfo::MinorComputerClass

    This enum describes the minor device classes for computer devices.

    \value UncategorizedComputer        An uncategorized computer device.
    \value DesktopComputer              A desktop computer.
    \value ServerComputer               A server computer.
    \value LaptopComputer               A laptop computer.
    \value HandheldClamShellComputer    A clamshell handheld computer or PDA.
    \value HandheldComputer             A handheld computer or PDA.
    \value WearableComputer             A wearable computer.
*/

/*!
    \enum QBluetoothDeviceInfo::MinorPhoneClass

    This enum describes the minor device classes for phone devices.

    \value UncategorizedPhone               An uncategorized phone device.
    \value CellularPhone                    A cellular phone.
    \value CordlessPhone                    A cordless phone.
    \value SmartPhone                       A smart phone.
    \value WiredModemOrVoiceGatewayPhone    A wired modem or voice gateway.
    \value CommonIsdnAccessPhone            A device that provides ISDN access.
*/

/*!
    \enum QBluetoothDeviceInfo::MinorNetworkClass

    This enum describes the minor device classes for local area network access devices. Local area
    network access devices use the minor device class to specify the current network utilization.

    \value NetworkFullService       100% of the total bandwidth is available.
    \value NetworkLoadFactorOne     0 - 17% of the total bandwidth is currently being used.
    \value NetworkLoadFactorTwo     17 - 33% of the total bandwidth is currently being used.
    \value NetworkLoadFactorThree   33 - 50% of the total bandwidth is currently being used.
    \value NetworkLoadFactorFour    50 - 67% of the total bandwidth is currently being used.
    \value NetworkLoadFactorFive    67 - 83% of the total bandwidth is currently being used.
    \value NetworkLoadFactorSix     83 - 99% of the total bandwidth is currently being used.
    \value NetworkNoService         No network service available.
*/

/*!
    \enum QBluetoothDeviceInfo::MinorAudioVideoClass

    This enum describes the minor device classes for audio/video devices.

    \value UncategorizedAudioVideoDevice    An uncategorized audio/video device.
    \value WearableHeadsetDevice            A wearable headset device.
    \value HandsFreeDevice                  A handsfree device.
    \value Microphone                       A microphone.
    \value Loudspeaker                      A loudspeaker.
    \value Headphones                       Headphones.
    \value PortableAudioDevice              A portable audio device.
    \value CarAudio                         A car audio device.
    \value SetTopBox                        A settop box.
    \value HiFiAudioDevice                  A HiFi audio device.
    \value Vcr                              A video cassette recorder.
    \value VideoCamera                      A video camera.
    \value Camcorder                        A video camera.
    \value VideoMonitor                     A video monitor.
    \value VideoDisplayAndLoudspeaker       A video display with built-in loudspeaker.
    \value VideoConferencing                A video conferencing device.
    \value GamingDevice                     A gaming device.
*/

/*!
    \enum QBluetoothDeviceInfo::MinorPeripheralClass

    This enum describes the minor device classes for peripheral devices.

    \value UncategorizedPeripheral              An uncategorized peripheral device.
    \value KeyboardPeripheral                   A keyboard.
    \value PointingDevicePeripheral             A pointing device, for example a mouse.
    \value KeyboardWithPointingDevicePeripheral A keyboard with built-in pointing device.
    \value JoystickPeripheral                   A joystick.
    \value GamepadPeripheral                    A game pad.
    \value RemoteControlPeripheral              A remote control.
    \value SensingDevicePeripheral              A sensing device.
    \value DigitizerTabletPeripheral            A digitizer tablet peripheral.
    \value CardReaderPeripheral                 A card reader peripheral.
*/

/*!
    \enum QBluetoothDeviceInfo::MinorImagingClass

    This enum describes the minor device classes for imaging devices.

    \value UncategorizedImagingDevice   An uncategorized imaging device.
    \value ImageDisplay                 A device capable of displaying images.
    \value ImageCamera                  A camera.
    \value ImageScanner                 An image scanner.
    \value ImagePrinter                 A printer.
*/

/*!
    \enum QBluetoothDeviceInfo::MinorWearableClass

    This enum describes the minor device classes for wearable devices.

    \value UncategorizedWearableDevice  An uncategorized wearable device.
    \value WearableWristWatch           A wristwatch.
    \value WearablePager                A pager.
    \value WearableJacket               A jacket.
    \value WearableHelmet               A helmet.
    \value WearableGlasses              A pair of glasses.
*/

/*!
    \enum QBluetoothDeviceInfo::MinorToyClass

    This enum describes the minor device classes for toy devices.

    \value UncategorizedToy     An uncategorized toy.
    \value ToyRobot             A toy robot.
    \value ToyVehicle           A toy vehicle.
    \value ToyDoll              A toy doll or action figure.
    \value ToyController        A controller.
    \value ToyGame              A game.
*/

/*!
    \enum QBluetoothDeviceInfo::MinorHealthClass

    This enum describes the minor device classes for health devices.

    \value UncategorizedHealthDevice    An uncategorized health device.
    \value HealthBloodPressureMonitor   A blood pressure monitor.
    \value HealthThermometer            A Thermometer.
    \value HealthWeightScale            A scale.
    \value HealthGlucoseMeter           A glucose meter.
    \value HealthPulseOximeter          A blood oxygen saturation meter.
    \value HealthDataDisplay            A data display.
    \value HealthStepCounter            A pedometer.
*/

/*!
    \enum QBluetoothDeviceInfo::ServiceClass

    This enum describes the service class of the Bluetooth device. The service class is used as a
    rudimentary form of service discovery. It is meant to provide a list of the types
    of services that the device might provide.

    \value NoService                The device does not provide any services.
    \value PositioningService       The device provides positioning services.
    \value NetworkingService        The device provides networking services.
    \value RenderingService         The device provides rendering services.
    \value CapturingService         The device provides capturing services.
    \value ObjectTransferService    The device provides object transfer services.
    \value AudioService             The device provides audio services.
    \value TelephonyService         The device provides telephony services.
    \value InformationService       The device provides information services.
    \value AllServices              The device provides services of all types.
*/

/*!
    \enum QBluetoothDeviceInfo::DataCompleteness

    This enum describes the completeness of the received data.

    \value DataComplete     The data is complete.
    \value DataIncomplete   The data is incomplete. Addition datum is available via other
                            interfaces.
    \value DataUnavailable  No data is available.
*/

/*!
    \enum QBluetoothDeviceInfo::CoreConfiguration
    \since 5.4

    This enum describes the configuration of the device.

    \value UnknownCoreConfiguration             The type of the Bluetooth device cannot be determined.
    \value BaseRateCoreConfiguration            The device is a standard Bluetooth device.
    \value BaseRateAndLowEnergyCoreConfiguration    The device is a Bluetooth Smart device with support
                                                for standard and Low Energy device.
    \value LowEnergyCoreConfiguration           The device is a Bluetooth Low Energy device.
*/
QBluetoothDeviceInfoPrivate::QBluetoothDeviceInfoPrivate() :
    valid(false),
    cached(false),
    rssi(1),
    serviceClasses(QBluetoothDeviceInfo::NoService),
    majorDeviceClass(QBluetoothDeviceInfo::MiscellaneousDevice),
    minorDeviceClass(0),
    serviceUuidsCompleteness(QBluetoothDeviceInfo::DataUnavailable),
    deviceCoreConfiguration(QBluetoothDeviceInfo::UnknownCoreConfiguration)
{
}

/*!
    Constructs an invalid QBluetoothDeviceInfo object.
*/
QBluetoothDeviceInfo::QBluetoothDeviceInfo() :
    d_ptr(new QBluetoothDeviceInfoPrivate)
{
}

/*!
    Constructs a QBluetoothDeviceInfo object with Bluetooth address \a address, device name
    \a name and the encoded class of device \a classOfDevice.

    The \a classOfDevice parameter is encoded in the following format

    \table
        \header \li Bits \li Size \li Description
        \row \li 0 - 1 \li 2 \li Unused, set to 0.
        \row \li 2 - 7 \li 6 \li Minor device class.
        \row \li 8 - 12 \li 5 \li Major device class.
        \row \li 13 - 23 \li 11 \li Service class.
    \endtable
*/
QBluetoothDeviceInfo::QBluetoothDeviceInfo(const QBluetoothAddress &address, const QString &name,
                                           quint32 classOfDevice) :
    d_ptr(new QBluetoothDeviceInfoPrivate)
{
    Q_D(QBluetoothDeviceInfo);

    d->address = address;
    d->name = name;

    d->minorDeviceClass = static_cast<quint8>((classOfDevice >> 2) & 0x3f);
    d->majorDeviceClass = static_cast<MajorDeviceClass>((classOfDevice >> 8) & 0x1f);
    d->serviceClasses = static_cast<ServiceClasses>((classOfDevice >> 13) & 0x7ff);

    d->serviceUuidsCompleteness = DataUnavailable;

    d->valid = true;
    d->cached = false;
    d->rssi = 0;
}

/*!
    Constructs a QBluetoothDeviceInfo object with unique \a uuid, device name
    \a name and the encoded class of device \a classOfDevice.

    This constructor is required for Low Energy devices on \macos and iOS. CoreBluetooth
    API hides addresses and provides unique UUIDs to identify a device. This UUID is
    not the same thing as a service UUID and is required to work later with CoreBluetooth API
    and discovered devices.

    \since 5.5
*/
QBluetoothDeviceInfo::QBluetoothDeviceInfo(const QBluetoothUuid &uuid, const QString &name,
                                           quint32 classOfDevice) :
    d_ptr(new QBluetoothDeviceInfoPrivate)
{
    Q_D(QBluetoothDeviceInfo);

    d->name = name;
    d->deviceUuid = uuid;

    d->minorDeviceClass = static_cast<quint8>((classOfDevice >> 2) & 0x3f);
    d->majorDeviceClass = static_cast<MajorDeviceClass>((classOfDevice >> 8) & 0x1f);
    d->serviceClasses = static_cast<ServiceClasses>((classOfDevice >> 13) & 0x7ff);

    d->serviceUuidsCompleteness = DataUnavailable;

    d->valid = true;
    d->cached = false;
    d->rssi = 0;
}

/*!
    Constructs a QBluetoothDeviceInfo that is a copy of \a other.
*/
QBluetoothDeviceInfo::QBluetoothDeviceInfo(const QBluetoothDeviceInfo &other) :
    d_ptr(new QBluetoothDeviceInfoPrivate)
{
    *this = other;
}

/*!
    Destroys the QBluetoothDeviceInfo.
*/
QBluetoothDeviceInfo::~QBluetoothDeviceInfo()
{
    delete d_ptr;
}

/*!
    Returns true if the QBluetoothDeviceInfo object is valid, otherwise returns false.
*/
bool QBluetoothDeviceInfo::isValid() const
{
    Q_D(const QBluetoothDeviceInfo);

    return d->valid;
}

/*!
  Returns the signal strength when the device was last scanned
  */
qint16 QBluetoothDeviceInfo::rssi() const
{
    Q_D(const QBluetoothDeviceInfo);

    return d->rssi;
}

/*!
  Set the \a signal strength value, used internally.
  */
void QBluetoothDeviceInfo::setRssi(qint16 signal)
{
    Q_D(QBluetoothDeviceInfo);
    d->rssi = signal;
}

/*!
    Makes a copy of the \a other and assigns it to this QBluetoothDeviceInfo object.
*/
QBluetoothDeviceInfo &QBluetoothDeviceInfo::operator=(const QBluetoothDeviceInfo &other)
{
    Q_D(QBluetoothDeviceInfo);

    d->address = other.d_func()->address;
    d->name = other.d_func()->name;
    d->minorDeviceClass = other.d_func()->minorDeviceClass;
    d->majorDeviceClass = other.d_func()->majorDeviceClass;
    d->serviceClasses = other.d_func()->serviceClasses;
    d->valid = other.d_func()->valid;
    d->cached = other.d_func()->cached;
    d->serviceUuidsCompleteness = other.d_func()->serviceUuidsCompleteness;
    d->serviceUuids = other.d_func()->serviceUuids;
    d->rssi = other.d_func()->rssi;
    d->deviceCoreConfiguration = other.d_func()->deviceCoreConfiguration;
    d->deviceUuid = other.d_func()->deviceUuid;

    return *this;
}

/*!
  Returns true if the \a other QBluetoothDeviceInfo object and this are identical.
  */
bool QBluetoothDeviceInfo::operator==(const QBluetoothDeviceInfo &other) const
{
    Q_D(const QBluetoothDeviceInfo);

    if (d->cached != other.d_func()->cached)
        return false;
    if (d->valid != other.d_func()->valid)
        return false;
    if (d->majorDeviceClass != other.d_func()->majorDeviceClass)
        return false;
    if (d->minorDeviceClass != other.d_func()->minorDeviceClass)
        return false;
    if (d->serviceClasses != other.d_func()->serviceClasses)
        return false;
    if (d->name != other.d_func()->name)
        return false;
    if (d->address != other.d_func()->address)
        return false;
    if (d->serviceUuidsCompleteness != other.d_func()->serviceUuidsCompleteness)
        return false;
    if (d->serviceUuids.count() != other.d_func()->serviceUuids.count())
        return false;
    if (d->serviceUuids != other.d_func()->serviceUuids)
        return false;
    if (d->deviceCoreConfiguration != other.d_func()->deviceCoreConfiguration)
        return false;
    if (d->deviceUuid != other.d_func()->deviceUuid)
        return false;

    return true;
}

/*!
    Returns true if this object is different from \a other, or false otherwise.

    \sa operator==()
*/
bool QBluetoothDeviceInfo::operator!=(const QBluetoothDeviceInfo &other) const
{
    return !(*this == other);
}

/*!
    Returns the address of the device.

    \note On iOS and \macos this address is invalid. Instead \l deviceUuid() should be used.
    Those two platforms do not expose Bluetooth addresses for found Bluetooth devices
    and utilize unique device identifiers.

    \sa deviceUuid()
*/
QBluetoothAddress QBluetoothDeviceInfo::address() const
{
    Q_D(const QBluetoothDeviceInfo);

    return d->address;
}

/*!
    Returns the name assigned to the device.
*/
QString QBluetoothDeviceInfo::name() const
{
    Q_D(const QBluetoothDeviceInfo);

    return d->name;
}

/*!
    Returns the service class of the device.
*/
QBluetoothDeviceInfo::ServiceClasses QBluetoothDeviceInfo::serviceClasses() const
{
    Q_D(const QBluetoothDeviceInfo);

    return d->serviceClasses;
}

/*!
    Returns the major device class of the device.
*/
QBluetoothDeviceInfo::MajorDeviceClass QBluetoothDeviceInfo::majorDeviceClass() const
{
    Q_D(const QBluetoothDeviceInfo);

    return d->majorDeviceClass;
}

/*!
    Returns the minor device class of the device. The actual information
    is context dependent on the value of \l majorDeviceClass().

    \sa MinorAudioVideoClass, MinorComputerClass, MinorHealthClass, MinorImagingClass,
    MinorMiscellaneousClass, MinorNetworkClass, MinorPeripheralClass, MinorPhoneClass,
    MinorToyClass, MinorWearableClass
*/
quint8 QBluetoothDeviceInfo::minorDeviceClass() const
{
    Q_D(const QBluetoothDeviceInfo);

    return d->minorDeviceClass;
}

/*!
    Sets the list of service UUIDs to \a uuids and the completeness of the data to \a completeness.
*/
void QBluetoothDeviceInfo::setServiceUuids(const QList<QBluetoothUuid> &uuids,
                                           DataCompleteness completeness)
{
    Q_D(QBluetoothDeviceInfo);

    d->serviceUuids = uuids;
    d->serviceUuidsCompleteness = completeness;
}

/*!
    Returns the list of service UUIDS supported by the device. If \a completeness is not 0 it will
    be set to DataComplete and the complete list of UUIDs supported by the device is returned.
    DataIncomplete if additional service UUIDs are supported by the device and DataUnavailable if
    no service UUID information is available.

    This function requires both the Bluetooth devices to support the 2.1 specification.
*/
QList<QBluetoothUuid> QBluetoothDeviceInfo::serviceUuids(DataCompleteness *completeness) const
{
    Q_D(const QBluetoothDeviceInfo);

    if (completeness)
        *completeness = d->serviceUuidsCompleteness;

    return d->serviceUuids;
}

/*!
    Returns the completeness of the service UUID list.  If DataComplete is returned,
    serviceUuids() returns the complete list of service UUIDs supported by the device, otherwise
    only the partial or empty list of service UUIDs. To get a list
    of all services supported by the device, a full service discovery needs to be performed.
*/
QBluetoothDeviceInfo::DataCompleteness QBluetoothDeviceInfo::serviceUuidsCompleteness() const
{
    Q_D(const QBluetoothDeviceInfo);

    return d->serviceUuidsCompleteness;
}

/*!
    Sets the CoreConfigurations of the device to \a coreConfigs. This will help to make a difference
    between regular and Low Energy devices.

    \sa coreConfigurations()
    \since 5.4
*/
void QBluetoothDeviceInfo::setCoreConfigurations(QBluetoothDeviceInfo::CoreConfigurations coreConfigs)
{
    Q_D(QBluetoothDeviceInfo);

    d->deviceCoreConfiguration = coreConfigs;
}

/*!

    Returns the configuration of the device. If device configuration is not set,
    basic rate device configuration will be returned.

    \sa setCoreConfigurations()
    \since 5.4
*/
QBluetoothDeviceInfo::CoreConfigurations QBluetoothDeviceInfo::coreConfigurations() const
{
    Q_D(const QBluetoothDeviceInfo);

    return d->deviceCoreConfiguration;
}

/*!
    Returns true if the QBluetoothDeviceInfo object is created from cached data.
*/
bool QBluetoothDeviceInfo::isCached() const
{
    Q_D(const QBluetoothDeviceInfo);

    return d->cached;
}

/*!
  Used by the system to set the \a cached flag if the QBluetoothDeviceInfo is created from cached data. Cached
  information may not be as accurate as data read from an active device.
  */
void QBluetoothDeviceInfo::setCached(bool cached)
{
    Q_D(QBluetoothDeviceInfo);

    d->cached = cached;
}

/*!
   Sets the unique identifier \a uuid for Bluetooth devices, that do not have addresses.
   This happens on \macos and iOS, where the CoreBluetooth API hides addresses, but provides
   UUIDs to identify devices/peripherals.

   This uuid is invalid on any other platform.

   \sa deviceUuid()
   \since 5.5
  */
void QBluetoothDeviceInfo::setDeviceUuid(const QBluetoothUuid &uuid)
{
    Q_D(QBluetoothDeviceInfo);

    d->deviceUuid = uuid;
}

/*!
   Returns a unique identifier for a Bluetooth device without an address.

   In general, this uuid is invalid on every platform but \macos and iOS.
   It is used as a workaround for those two platforms as they do not
   provide Bluetooth addresses for found Bluetooth Low Energy devices.
   Every other platform uses \l address() instead.

   \sa setDeviceUuid()
   \since 5.5
  */
QBluetoothUuid QBluetoothDeviceInfo::deviceUuid() const
{
    Q_D(const QBluetoothDeviceInfo);

    return d->deviceUuid;
}

QT_END_NAMESPACE
