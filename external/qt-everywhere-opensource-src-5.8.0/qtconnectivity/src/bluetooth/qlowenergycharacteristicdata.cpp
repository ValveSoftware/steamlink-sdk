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

#include "qlowenergycharacteristicdata.h"

#include "qlowenergydescriptordata.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qdebug.h>

#include <climits>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT)

struct QLowEnergyCharacteristicDataPrivate : public QSharedData
{
    QLowEnergyCharacteristicDataPrivate()
        : properties(QLowEnergyCharacteristic::Unknown)
        , minimumValueLength(0)
        , maximumValueLength(INT_MAX)
    {}

    QBluetoothUuid uuid;
    QLowEnergyCharacteristic::PropertyTypes properties;
    QList<QLowEnergyDescriptorData> descriptors;
    QByteArray value;
    QBluetooth::AttAccessConstraints readConstraints;
    QBluetooth::AttAccessConstraints writeConstraints;
    int minimumValueLength;
    int maximumValueLength;
};

/*!
    \since 5.7
    \class QLowEnergyCharacteristicData
    \brief The QLowEnergyCharacteristicData class is used to set up GATT service data.
    \inmodule QtBluetooth
    \ingroup shared

    An Object of this class provides a characteristic to be added to a
    \l QLowEnergyServiceData object via \l QLowEnergyServiceData::addCharacteristic().

    \sa QLowEnergyServiceData
    \sa QLowEnergyController::addService
*/

/*! Creates a new invalid object of this class. */
QLowEnergyCharacteristicData::QLowEnergyCharacteristicData()
    : d(new QLowEnergyCharacteristicDataPrivate)
{
}

/*! Constructs a new object of this class that is a copy of \a other. */
QLowEnergyCharacteristicData::QLowEnergyCharacteristicData(const QLowEnergyCharacteristicData &other)
    : d(other.d)
{
}

/*! Destroys this object. */
QLowEnergyCharacteristicData::~QLowEnergyCharacteristicData()
{
}

/*! Makes this object a copy of \a other and returns the new value of this object. */
QLowEnergyCharacteristicData &QLowEnergyCharacteristicData::operator=(const QLowEnergyCharacteristicData &other)
{
    d = other.d;
    return *this;
}

/*! Returns the UUID of this characteristic. */
QBluetoothUuid QLowEnergyCharacteristicData::uuid() const
{
    return d->uuid;
}

/*! Sets the UUID of this characteristic to \a uuid. */
void QLowEnergyCharacteristicData::setUuid(const QBluetoothUuid &uuid)
{
    d->uuid = uuid;
}

/*! Returns the value of this characteristic. */
QByteArray QLowEnergyCharacteristicData::value() const
{
    return d->value;
}

/*! Sets the value of this characteristic to \a value. */
void QLowEnergyCharacteristicData::setValue(const QByteArray &value)
{
    d->value = value;
}

/*! Returns the properties of this characteristic. */
QLowEnergyCharacteristic::PropertyTypes QLowEnergyCharacteristicData::properties() const
{
    return d->properties;
}

/*! Sets the properties of this characteristic to \a properties. */
void QLowEnergyCharacteristicData::setProperties(QLowEnergyCharacteristic::PropertyTypes properties)
{
    d->properties = properties;
}

/*! Returns the descriptors of this characteristic. */
QList<QLowEnergyDescriptorData> QLowEnergyCharacteristicData::descriptors() const
{
    return d->descriptors;
}

/*!
  Sets the descriptors of this characteristic to \a descriptors. Only valid descriptors
  are considered.
  \sa addDescriptor()
  */
void QLowEnergyCharacteristicData::setDescriptors(const QList<QLowEnergyDescriptorData> &descriptors)
{
    d->descriptors.clear();
    foreach (const QLowEnergyDescriptorData &desc, descriptors)
        addDescriptor(desc);
}

/*!
  Adds \a descriptor to the list of descriptors of this characteristic, if it is valid.
  \sa setDescriptors()
 */
void QLowEnergyCharacteristicData::addDescriptor(const QLowEnergyDescriptorData &descriptor)
{
    if (descriptor.isValid())
        d->descriptors << descriptor;
    else
        qCWarning(QT_BT) << "not adding invalid descriptor to characteristic";
}

/*!
  Specifies that clients need to fulfill \a constraints to read the value of this characteristic.
 */
void QLowEnergyCharacteristicData::setReadConstraints(QBluetooth::AttAccessConstraints constraints)
{
    d->readConstraints = constraints;
}

/*!
  Returns the constraints needed for a client to read the value of this characteristic.
  If \l properties() does not include \l QLowEnergyCharacteristic::Read, this value is irrelevant.
  By default, there are no read constraints.
 */
QBluetooth::AttAccessConstraints QLowEnergyCharacteristicData::readConstraints() const
{
    return d->readConstraints;
}

/*!
  Specifies that clients need to fulfill \a constraints to write the value of this characteristic.
 */
void QLowEnergyCharacteristicData::setWriteConstraints(QBluetooth::AttAccessConstraints constraints)
{
    d->writeConstraints = constraints;
}

/*!
  Returns the constraints needed for a client to write the value of this characteristic.
  If \l properties() does not include either of \l QLowEnergyCharacteristic::Write,
  \l QLowEnergyCharacteristic::WriteNoResponse and \l QLowEnergyCharacteristic::WriteSigned,
  this value is irrelevant.
  By default, there are no write constraints.
 */
QBluetooth::AttAccessConstraints QLowEnergyCharacteristicData::writeConstraints() const
{
    return d->writeConstraints;
}

/*!
  Specifies \a minimum and \a maximum to be the smallest and largest length, respectively,
  that the value of this characteristic can have. The unit is bytes. If \a minimum and
  \a maximum are equal, the characteristic has a fixed-length value.
 */
void QLowEnergyCharacteristicData::setValueLength(int minimum, int maximum)
{
    d->minimumValueLength = minimum;
    d->maximumValueLength = qMax(minimum, maximum);
}

/*!
  Returns the minimum length in bytes that the value of this characteristic can have.
  The default is zero.
 */
int QLowEnergyCharacteristicData::minimumValueLength() const
{
    return d->minimumValueLength;
}

/*!
  Returns the maximum length in bytes that the value of this characteristic can have.
  By default, there is no limit beyond the constraints of the data type.
 */
int QLowEnergyCharacteristicData::maximumValueLength() const
{
    return d->maximumValueLength;
}

/*!
  Returns true if and only if this characteristic is valid, that is, it has a non-null UUID.
 */
bool QLowEnergyCharacteristicData::isValid() const
{
    return !uuid().isNull();
}

/*!
   \fn void QLowEnergyCharacteristicData::swap(QLowEnergyCharacteristicData &other)
    Swaps this object with \a other.
 */

/*!
   Returns \c true if \a cd1 and \a cd2 are equal with respect to their public state,
   otherwise returns \c false.
 */
bool operator==(const QLowEnergyCharacteristicData &cd1, const QLowEnergyCharacteristicData &cd2)
{
    return cd1.d == cd2.d || (
                cd1.uuid() == cd2.uuid()
                && cd1.properties() == cd2.properties()
                && cd1.descriptors() == cd2.descriptors()
                && cd1.value() == cd2.value()
                && cd1.readConstraints() == cd2.readConstraints()
                && cd1.writeConstraints() == cd2.writeConstraints()
                && cd1.minimumValueLength() == cd2.maximumValueLength()
                && cd1.maximumValueLength() == cd2.maximumValueLength());
}

/*!
   \fn bool operator!=(const QLowEnergyCharacteristicData &cd1,
                       const QLowEnergyCharacteristicData &cd2)
   Returns \c true if \a cd1 and \a cd2 are not equal with respect to their public state,
   otherwise returns \c false.
 */

QT_END_NAMESPACE
