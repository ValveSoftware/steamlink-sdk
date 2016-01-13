/***************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
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

#include <QtBluetooth/QLowEnergyService>
#include "qlowenergyserviceprivate_p.h"
#include "qlowenergydescriptor.h"

QT_BEGIN_NAMESPACE

/*!
    \class QLowEnergyDescriptor
    \inmodule QtBluetooth
    \brief The QLowEnergyDescriptor class stores information about the Bluetooth
    Low Energy descriptor.
    \since 5.4

    QLowEnergyDescriptor provides information about a Bluetooth Low Energy
    descriptor's \l name(), \l uuid(), \l value() and \l handle(). Descriptors are
    encapsulated by Bluetooth Low Energy characteristics and provide additional
    centextual information about the characteristic (data format, notification activation
    and so on).

    The descriptor value may be written via the \l QLowEnergyService instance
    that manages the service to which this descriptor belongs. The
    \l {QLowEnergyService::writeDescriptor()} function writes the new value.
    The \l {QLowEnergyService::descriptorWritten()} signal
    is emitted upon success. The cahced \l value() of this object is updated accordingly.

    \note This class is provided by Qt 5.4 as part of a Bluetooth Low Energy Tech Preview.
    Some API elements may change until the final release of the feature.

    \sa QLowEnergyService, QLowEnergyCharacteristic
*/

struct QLowEnergyDescriptorPrivate
{
    QLowEnergyHandle charHandle;
    QLowEnergyHandle descHandle;
};

/*!
    Construct a new QLowEnergyDescriptor. A default-constructed instance
    of this class is always invalid.
*/
QLowEnergyDescriptor::QLowEnergyDescriptor():
    d_ptr(0), data(0)
{
}

/*!
    Construct a new QLowEnergyDescriptor that is a copy of \a other.

    The two copies continue to share the same underlying data which does not detach
    upon write.
*/
QLowEnergyDescriptor::QLowEnergyDescriptor(const QLowEnergyDescriptor &other):
    d_ptr(other.d_ptr), data(0)
{
    if (other.data) {
        data = new QLowEnergyDescriptorPrivate();
        data->charHandle = other.data->charHandle;
        data->descHandle = other.data->descHandle;
    }
}

/*!
    \internal

*/
QLowEnergyDescriptor::QLowEnergyDescriptor(QSharedPointer<QLowEnergyServicePrivate> p,
                                           QLowEnergyHandle charHandle,
                                           QLowEnergyHandle descHandle):
    d_ptr(p)
{
    data = new QLowEnergyDescriptorPrivate();
    data->charHandle = charHandle;
    data->descHandle = descHandle;

}

/*!
    Destroys the QLowEnergyDescriptor object.
*/
QLowEnergyDescriptor::~QLowEnergyDescriptor()
{
    delete data;
}

/*!
    Makes a copy of \a other and assigns it to this QLowEnergyDescriptor object.
    The two copies continue to share the same service and controller details.
*/
QLowEnergyDescriptor &QLowEnergyDescriptor::operator=(const QLowEnergyDescriptor &other)
{
    d_ptr = other.d_ptr;

    if (!other.data) {
        if (data) {
            delete data;
            data = 0;
        }
    } else {
        if (!data)
            data = new QLowEnergyDescriptorPrivate();

        data->charHandle = other.data->charHandle;
        data->descHandle = other.data->descHandle;
    }

    return *this;
}

/*!
    Returns \c true if \a other is equal to this QLowEnergyCharacteristic; otherwise \c false.

    Two QLowEnergyDescriptor instances are considered to be equal if they refer to
    the same descriptor on the same remote Bluetooth Low Energy device or both
    instances have been default-constructed.
 */
bool QLowEnergyDescriptor::operator==(const QLowEnergyDescriptor &other) const
{
    if (d_ptr != other.d_ptr)
        return false;

    if ((data && !other.data) || (!data && other.data))
        return false;

    if (!data)
        return true;

    if (data->charHandle != other.data->charHandle
        || data->descHandle != other.data->descHandle) {
        return false;
    }

    return true;
}

/*!
    Returns \c true if \a other is not equal to this QLowEnergyCharacteristic; otherwise \c false.

    Two QLowEnergyDescriptor instances are considered to be equal if they refer to
    the same descriptor on the same remote Bluetooth Low Energy device  or both
    instances have been default-constructed.
 */
bool QLowEnergyDescriptor::operator!=(const QLowEnergyDescriptor &other) const
{
    return !(*this == other);
}

/*!
    Returns \c true if the QLowEnergyDescriptor object is valid, otherwise returns \c false.

    An invalid descriptor instance is not associated with any service (default-constructed)
    or the associated service is no longer valid due to a disconnect from
    the underlying Bluetooth Low Energy device, for example. Once the object is invalid
    it cannot become valid anymore.

    \note If a QLowEnergyDescriptor instance turns invalid due to a disconnect
    from the underlying device, the information encapsulated by the current
    instance remains as it was at the time of the disconnect. Therefore it can be
    retrieved after the disconnect event.
*/
bool QLowEnergyDescriptor::isValid() const
{
    if (d_ptr.isNull() || !data)
        return false;

    if (d_ptr->state == QLowEnergyService::InvalidService)
        return false;

    return true;
}

/*!
    Returns the UUID of this descriptor if \l isValid() returns \c true; otherwise a
    \l {QUuid::isNull()}{null} UUID.
*/
QBluetoothUuid QLowEnergyDescriptor::uuid() const
{
    if (d_ptr.isNull() || !data
                       || !d_ptr->characteristicList.contains(data->charHandle)
                       || !d_ptr->characteristicList[data->charHandle].
                                        descriptorList.contains(data->descHandle)) {
        return QBluetoothUuid();
    }

    return d_ptr->characteristicList[data->charHandle].descriptorList[data->descHandle].uuid;
}

/*!
    Returns the handle of the descriptor or \c 0 if the handle
    cannot be accessed on the platform or the descriptor is invalid.
*/
QLowEnergyHandle QLowEnergyDescriptor::handle() const
{
    if (!data)
        return 0;

    return data->descHandle;
}

/*!
    Returns the cached value of the descriptor.

    A descriptor value may be updated using
    \l QLowEnergyService::writeDescriptor().
*/
QByteArray QLowEnergyDescriptor::value() const
{
    if (d_ptr.isNull() || !data
                       || !d_ptr->characteristicList.contains(data->charHandle)
                       || !d_ptr->characteristicList[data->charHandle].
                                        descriptorList.contains(data->descHandle)) {
        return QByteArray();
    }

    return d_ptr->characteristicList[data->charHandle].descriptorList[data->descHandle].value;
}

/*!
    Returns the human-readable name of the descriptor.

    The name is based on the descriptor's \l type(). The complete list
    of descriptor types can be found under
    \l {https://developer.bluetooth.org/gatt/descriptors/Pages/DescriptorsHomePage.aspx}{Bluetooth.org Descriptors}.

    The returned string is empty if the \l type() is unknown.

    \sa type(), QBluetoothUuid::descriptorToString()
*/

QString QLowEnergyDescriptor::name() const
{
    return QBluetoothUuid::descriptorToString(type());
}

/*!
    Returns the type of the descriptor.

    \sa name()
 */
QBluetoothUuid::DescriptorType QLowEnergyDescriptor::type() const
{
    const QBluetoothUuid u = uuid();
    bool ok = false;
    quint16 shortUuid = u.toUInt16(&ok);

    if (!ok)
        return QBluetoothUuid::UnknownDescriptorType;

    switch (shortUuid) {
    case QBluetoothUuid::CharacteristicExtendedProperties:
    case QBluetoothUuid::CharacteristicUserDescription:
    case QBluetoothUuid::ClientCharacteristicConfiguration:
    case QBluetoothUuid::ServerCharacteristicConfiguration:
    case QBluetoothUuid::CharacteristicPresentationFormat:
    case QBluetoothUuid::CharacteristicAggregateFormat:
    case QBluetoothUuid::ValidRange:
    case QBluetoothUuid::ExternalReportReference:
    case QBluetoothUuid::ReportReference:
        return (QBluetoothUuid::DescriptorType) shortUuid;
    default:
        break;
    }

    return QBluetoothUuid::UnknownDescriptorType;
}

/*!
    \internal

    Returns the handle of the characteristic to which this descriptor belongs
 */
QLowEnergyHandle QLowEnergyDescriptor::characteristicHandle() const
{
    if (d_ptr.isNull() || !data)
        return 0;

    return data->charHandle;
}

QT_END_NAMESPACE
