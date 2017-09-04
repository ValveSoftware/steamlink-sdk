/***************************************************************************
**
** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
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

#include "qlowenergycharacteristic.h"
#include "qlowenergyserviceprivate_p.h"
#include <QHash>

QT_BEGIN_NAMESPACE

/*!
    \class QLowEnergyCharacteristic
    \inmodule QtBluetooth
    \brief The QLowEnergyCharacteristic class stores information about a Bluetooth
    Low Energy service characteristic.

    \since 5.4

    QLowEnergyCharacteristic provides information about a Bluetooth Low Energy
    service characteristic's \l name(), \l uuid(), \l value(), \l properties(),
    \l handle() and \l descriptors(). To obtain the characteristic's specification
    and information, it is necessary to connect to the device using the
    \l QLowEnergyService and \l QLowEnergyController classes.

    The characteristic value may be written via the \l QLowEnergyService instance
    that manages the service to which this characteristic belongs. The
    \l {QLowEnergyService::writeCharacteristic()} function writes the new value.
    The \l {QLowEnergyService::characteristicWritten()} signal is emitted upon success.
    The \l value() of this object is automatically updated accordingly.

    Characteristics may contain none, one or more descriptors. They can be individually
    retrieved using the \l descriptor() function. The \l descriptors() function returns
    all descriptors as a list. The general purpose of a descriptor is to add contextual
    information to the characteristic. For example, the descriptor might provide
    format or range information specifying how the characteristic's value is to be\
    interpreted.

    \sa QLowEnergyService, QLowEnergyDescriptor
*/

/*!
    \enum QLowEnergyCharacteristic::PropertyType

    This enum describes the properties of a characteristic.

    \value Unknown                  The type is not known.
    \value Broadcasting             Allow for the broadcasting of Generic Attributes (GATT) characteristic values.
    \value Read                     Allow the characteristic values to be read.
    \value WriteNoResponse          Allow characteristic values without responses to be written.
    \value Write                    Allow for characteristic values to be written.
    \value Notify                   Permits notification of characteristic values.
    \value Indicate                 Permits indications of characteristic values.
    \value WriteSigned              Permits signed writes of the GATT characteristic values.
    \value ExtendedProperty         Additional characteristic properties are defined in the characteristic's
                                    extended properties descriptor.

    \sa properties()
*/

struct QLowEnergyCharacteristicPrivate
{
    QLowEnergyHandle handle;
};

/*!
    Construct a new QLowEnergyCharacteristic. A default-constructed instance of
    this class is always invalid.

    \sa isValid()
*/
QLowEnergyCharacteristic::QLowEnergyCharacteristic():
    d_ptr(0), data(0)
{

}

/*!
    Construct a new QLowEnergyCharacteristic that is a copy of \a other.

    The two copies continue to share the same underlying data which does not detach
    upon write.
*/
QLowEnergyCharacteristic::QLowEnergyCharacteristic(const QLowEnergyCharacteristic &other):
    d_ptr(other.d_ptr), data(0)
{
    if (other.data) {
        data = new QLowEnergyCharacteristicPrivate();
        data->handle = other.data->handle;
    }
}

/*!
    \internal
*/
QLowEnergyCharacteristic::QLowEnergyCharacteristic(
        QSharedPointer<QLowEnergyServicePrivate> p, QLowEnergyHandle handle):
    d_ptr(p)
{
    data = new QLowEnergyCharacteristicPrivate();
    data->handle = handle;
}

/*!
    Destroys the QLowEnergyCharacteristic object.
*/
QLowEnergyCharacteristic::~QLowEnergyCharacteristic()
{
    delete data;
}

/*!
    Returns the human-readable name of the characteristic.

    The name is based on the characteristic's \l uuid() which must have been
    standardized. The complete list of characteristic types can be found
    under \l {https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicsHome.aspx}{Bluetooth.org Characteristics}.

    The returned string is empty if the \l uuid() is unknown.

    \sa QBluetoothUuid::characteristicToString()
*/
QString QLowEnergyCharacteristic::name() const
{
    return QBluetoothUuid::characteristicToString(
                static_cast<QBluetoothUuid::CharacteristicType>(uuid().toUInt16()));
}

/*!
    Returns the UUID of the characteristic if \l isValid() returns \c true; otherwise a
    \l {QUuid::isNull()}{null} UUID.
*/
QBluetoothUuid QLowEnergyCharacteristic::uuid() const
{
    if (d_ptr.isNull() || !data
        || !d_ptr->characteristicList.contains(data->handle))
        return QBluetoothUuid();

    return d_ptr->characteristicList[data->handle].uuid;
}

/*!
    Returns the properties of the characteristic.

    The properties define the access permissions for the characteristic.
*/
QLowEnergyCharacteristic::PropertyTypes QLowEnergyCharacteristic::properties() const
{
    if (d_ptr.isNull() || !data
        || !d_ptr->characteristicList.contains(data->handle))
        return QLowEnergyCharacteristic::Unknown;

    return d_ptr->characteristicList[data->handle].properties;
}

/*!
    Returns the cached value of the characteristic.

    If the characteristic's \l properties() permit writing of new values,
    the value can be updated using \l QLowEnergyService::writeCharacteristic().

    The cache is updated during the associated service's
    \l {QLowEnergyService::discoverDetails()} {detail discovery}, a successful
    \l {QLowEnergyService::readCharacteristic()}{read}/\l {QLowEnergyService::writeCharacteristic()}{write}
    operation or when an update notification is received.

    The returned \l QByteArray always remains empty if the characteristic does not
    have the \l {QLowEnergyCharacteristic::Read}{read permission}. In such cases only
    the \l QLowEnergyService::characteristicChanged() or
    \l QLowEnergyService::characteristicWritten() may provice information about the
    value of this characteristic.
*/
QByteArray QLowEnergyCharacteristic::value() const
{
    if (d_ptr.isNull() || !data
        || !d_ptr->characteristicList.contains(data->handle))
        return QByteArray();

    return d_ptr->characteristicList[data->handle].value;
}

/*!
    Returns the handle of the characteristic's value attribute;
    or \c 0 if the handle cannot be accessed on the platform or
    if the characteristic is invalid.

    \note On \macos and iOS handles can differ from 0, but these
    values have no special meaning outside of internal/private API.
*/
QLowEnergyHandle QLowEnergyCharacteristic::handle() const
{
    if (d_ptr.isNull() || !data
        || !d_ptr->characteristicList.contains(data->handle))
        return 0;

    return d_ptr->characteristicList[data->handle].valueHandle;
}

/*!
    Makes a copy of \a other and assigns it to this \l QLowEnergyCharacteristic object.
    The two copies continue to share the same service and controller details.
*/
QLowEnergyCharacteristic &QLowEnergyCharacteristic::operator=(const QLowEnergyCharacteristic &other)
{
    d_ptr = other.d_ptr;

    if (!other.data) {
        if (data) {
            delete data;
            data = 0;
        }
    } else {
        if (!data)
            data = new QLowEnergyCharacteristicPrivate();

        data->handle = other.data->handle;
    }
    return *this;
}

/*!
    Returns \c true if \a other is equal to this QLowEnergyCharacteristic; otherwise \c false.

    Two \l QLowEnergyCharacteristic instances are considered to be equal if they refer to
    the same characteristic on the same remote Bluetooth Low Energy device or both instances
    have been default-constructed.
 */
bool QLowEnergyCharacteristic::operator==(const QLowEnergyCharacteristic &other) const
{
    if (d_ptr != other.d_ptr)
        return false;

    if ((data && !other.data) || (!data && other.data))
        return false;

    if (!data)
        return true;

    if (data->handle != other.data->handle)
        return false;

    return true;
}

/*!
    Returns \c true if \a other is not equal to this QLowEnergyCharacteristic; otherwise \c false.

    Two QLowEnergyCharcteristic instances are considered to be equal if they refer to
    the same characteristic on the same remote Bluetooth Low Energy device or both instances
    have been default-constructed.
 */

bool QLowEnergyCharacteristic::operator!=(const QLowEnergyCharacteristic &other) const
{
    return !(*this == other);
}

/*!
    Returns \c true if the QLowEnergyCharacteristic object is valid, otherwise returns \c false.

    An invalid characteristic object is not associated with any service (default-constructed)
    or the associated service is no longer valid due to a disconnect from
    the underlying Bluetooth Low Energy device, for example. Once the object is invalid
    it cannot become valid anymore.

    \note If a \l QLowEnergyCharacteristic instance turns invalid due to a disconnect
    from the underlying device, the information encapsulated by the current
    instance remains as it was at the time of the disconnect. Therefore it can be
    retrieved after the disconnect event.
*/
bool QLowEnergyCharacteristic::isValid() const
{
    if (d_ptr.isNull() || !data)
        return false;

    if (d_ptr->state == QLowEnergyService::InvalidService)
        return false;

    return true;
}

/*!
    \internal

    Returns the handle of the characteristic or
    \c 0 if the handle cannot be accessed on the platform or if the
    characteristic is invalid.

    \note On \macos and iOS handles can differ from 0, but these
    values have no special meaning outside of internal/private API.

    \sa isValid()
 */
QLowEnergyHandle QLowEnergyCharacteristic::attributeHandle() const
{
    if (d_ptr.isNull() || !data)
        return 0;

    return data->handle;
}


/*!
    Returns the descriptor for \a uuid or an invalid \c QLowEnergyDescriptor instance.

    \sa descriptors()
*/
QLowEnergyDescriptor QLowEnergyCharacteristic::descriptor(const QBluetoothUuid &uuid) const
{
    if (d_ptr.isNull() || !data)
        return QLowEnergyDescriptor();

    CharacteristicDataMap::const_iterator charIt = d_ptr->characteristicList.constFind(data->handle);
    if (charIt != d_ptr->characteristicList.constEnd()) {
        const QLowEnergyServicePrivate::CharData &charDetails = charIt.value();

        DescriptorDataMap::const_iterator descIt = charDetails.descriptorList.constBegin();
        for ( ; descIt != charDetails.descriptorList.constEnd(); ++descIt) {
            const QLowEnergyHandle descHandle = descIt.key();
            const QLowEnergyServicePrivate::DescData &descDetails = descIt.value();

            if (descDetails.uuid == uuid)
                return QLowEnergyDescriptor(d_ptr, data->handle, descHandle);
        }
    }

    return QLowEnergyDescriptor();
}

/*!
    Returns the list of descriptors belonging to this characteristic; otherwise
    an empty list.

    \sa descriptor()
*/
QList<QLowEnergyDescriptor> QLowEnergyCharacteristic::descriptors() const
{
    QList<QLowEnergyDescriptor> result;

    if (d_ptr.isNull() || !data
                       || !d_ptr->characteristicList.contains(data->handle))
        return result;

    QList<QLowEnergyHandle> descriptorKeys = d_ptr->characteristicList[data->handle].
                                                    descriptorList.keys();

    std::sort(descriptorKeys.begin(), descriptorKeys.end());

    foreach (const QLowEnergyHandle descHandle, descriptorKeys) {
        QLowEnergyDescriptor descriptor(d_ptr, data->handle, descHandle);
        result.append(descriptor);
    }

    return result;
}

QT_END_NAMESPACE
