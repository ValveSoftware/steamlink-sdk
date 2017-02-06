/***************************************************************************
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

#include "qlowenergydescriptordata.h"

#include <QtCore/qbytearray.h>

QT_BEGIN_NAMESPACE

struct QLowEnergyDescriptorDataPrivate : public QSharedData
{
    QLowEnergyDescriptorDataPrivate() : readable(true), writable(true) {}

    QBluetoothUuid uuid;
    QByteArray value;
    QBluetooth::AttAccessConstraints readConstraints;
    QBluetooth::AttAccessConstraints writeConstraints;
    bool readable;
    bool writable;
};

/*!
    \since 5.7
    \class QLowEnergyDescriptorData
    \brief The QLowEnergyDescriptorData class is used to create GATT service data.
    \inmodule QtBluetooth
    \ingroup shared

    An object of this class provides a descriptor to be added to a
    \l QLowEnergyCharacteristicData object via \l QLowEnergyCharacteristicData::addDescriptor().

    \note The member functions related to access permissions are only applicable to those
          types of descriptors for which the Bluetooth specification does not prescribe if
          and how their values can be accessed.

    \sa QLowEnergyCharacteristicData
    \sa QLowEnergyServiceData
    \sa QLowEnergyController::addService
*/

/*! Creates a new invalid object of this class. */
QLowEnergyDescriptorData::QLowEnergyDescriptorData() : d(new QLowEnergyDescriptorDataPrivate)
{
}

/*!
  Creates a new object of this class with UUID and value being provided by \a uuid and \a value,
  respectively.
 */
QLowEnergyDescriptorData::QLowEnergyDescriptorData(const QBluetoothUuid &uuid,
                                                   const QByteArray &value)
    : d(new QLowEnergyDescriptorDataPrivate)
{
    setUuid(uuid);
    setValue(value);
}

/*! Constructs a new object of this class that is a copy of \a other. */
QLowEnergyDescriptorData::QLowEnergyDescriptorData(const QLowEnergyDescriptorData &other)
    : d(other.d)
{
}

/*! Destroys this object. */
QLowEnergyDescriptorData::~QLowEnergyDescriptorData()
{
}

/*! Makes this object a copy of \a other and returns the new value of this object. */
QLowEnergyDescriptorData &QLowEnergyDescriptorData::operator=(const QLowEnergyDescriptorData &other)
{
    d = other.d;
    return *this;
}

/*! Returns the value of this descriptor. */
QByteArray QLowEnergyDescriptorData::value() const
{
    return d->value;
}

/*!
  Sets the value of this descriptor to \a value. It will be sent to a peer device
  exactly the way it is provided here, so callers need to take care of things such as endianness.
 */
void QLowEnergyDescriptorData::setValue(const QByteArray &value)
{
    d->value = value;
}

/*! Returns the UUID of this descriptor. */
QBluetoothUuid QLowEnergyDescriptorData::uuid() const
{
    return d->uuid;
}

/*! Sets the UUID of this descriptor to \a uuid. */
void QLowEnergyDescriptorData::setUuid(const QBluetoothUuid &uuid)
{
    d->uuid = uuid;
}

/*! Returns true if and only if this object has a non-null UUID. */
bool QLowEnergyDescriptorData::isValid() const
{
    return !uuid().isNull();
}

/*!
  Specifies whether the value of this descriptor is \a readable and if so, under which
  \a constraints.
  \sa setWritePermissions()
 */
void QLowEnergyDescriptorData::setReadPermissions(bool readable,
                                                  QBluetooth::AttAccessConstraints constraints)
{
    d->readable = readable;
    d->readConstraints = constraints;
}

/*! Returns \c true if the value of this descriptor is readable and \c false otherwise. */
bool QLowEnergyDescriptorData::isReadable() const
{
    return d->readable;
}

/*!
  Returns the constraints under which the value of this descriptor can be read. This value
  is only relevant if \l isReadable() returns \c true.
 */
QBluetooth::AttAccessConstraints QLowEnergyDescriptorData::readConstraints() const
{
    return d->readConstraints;
}

/*!
  Specifies whether the value of this descriptor is \a writable and if so, under which
  \a constraints.
  \sa setReadPermissions()
 */
void QLowEnergyDescriptorData::setWritePermissions(bool writable,
                                                   QBluetooth::AttAccessConstraints constraints)
{
    d->writable = writable;
    d->writeConstraints = constraints;
}

/*! Returns \c true if the value of this descriptor is writable and \c false otherwise. */
bool QLowEnergyDescriptorData::isWritable() const
{
    return d->writable;
}

/*!
  Returns the constraints under which the value of this descriptor can be written. This value
  is only relevant if \l isWritable() returns \c true.
 */
QBluetooth::AttAccessConstraints QLowEnergyDescriptorData::writeConstraints() const
{
    return d->writeConstraints;
}

/*!
   \fn void QLowEnergyDescriptorData::swap(QLowEnergyDescriptorData &other)
    Swaps this object with \a other.
 */

/*!
   Returns \c true if \a d1 and \a d2 are equal with respect to their public state,
   otherwise returns \c false.
 */
bool operator==(const QLowEnergyDescriptorData &d1, const QLowEnergyDescriptorData &d2)
{
    return d1.d == d2.d || (
                d1.uuid() == d2.uuid()
                && d1.value() == d2.value()
                && d1.isReadable() == d2.isReadable()
                && d1.isWritable() == d2.isWritable()
                && d1.readConstraints() == d2.readConstraints()
                && d1.writeConstraints() == d2.writeConstraints());
}

/*!
   \fn bool operator!=(const QLowEnergyDescriptorData &d1, const QLowEnergyDescriptorData &d2)
   Returns \c true if \a d1 and \a d2 are not equal with respect to their public state,
   otherwise returns \c false.
 */

QT_END_NAMESPACE
