/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qmodbusdeviceidentification.h"
#include "qmodbus_symbols_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QModbusDeviceIdentification
    \inmodule QtSerialBus
    \since 5.8

    \brief The QModbusDeviceIdentification is a container class representing
    the physical and functional description of a Modbus server.

    The Device Identification interface is modeled as an address space composed
    of a set of addressable data elements. The data elements are called objects
    and an \l ObjectId identifies them.
*/

/*!
    \enum QModbusDeviceIdentification::ObjectId

    This enum describes the possible server objects. The interface consists of
    three categories of objects:

    Basic Device Identification. All objects of this category are mandatory.

    \value VendorNameObjectId           The vendor name of the device.
    \value ProductCodeObjectId          The product code of the device.
    \value MajorMinorRevisionObjectId   The product version numbering.

    Regular Device Identification. All objects of this category are standard
    defined and optional.

    \value VendorUrlObjectId            The vendor URL of the device.
    \value ProductNameObjectId          The product name of the device.
    \value ModelNameObjectId            The model name of the device.
    \value UserApplicationNameObjectId  The user application name of the device.

    Reserved range (i.e., ReservedObjectId >= ObjectId < ProductDependentObjectId).
    Do not use.

    \value ReservedObjectId             First value of reserved object Ids.

    Extended Device Identification. All of these data are device dependent and
    optional.

    \value ProductDependentObjectId     First possible value of product dependent
                                        identifiers.
    \value UndefinedObjectId            Do not use.
*/

/*!
    \enum QModbusDeviceIdentification::ReadDeviceIdCode

    Defines the access type of the read identification request.

    Stream access:

    \value BasicReadDeviceIdCode        Request to get the basic device
                                        identification.
    \value RegularReadDeviceIdCode      Request to get the regular device
                                        identification.
    \value ExtendedReadDeviceIdCode     Request to get the extended device
                                        identification.

    Individual access:

    \value IndividualReadDeviceIdCode   Request to get one specific identification
                                        object.
*/

/*!
    \enum QModbusDeviceIdentification::ConformityLevel

    Defines the identification conformity level of the device and type of
    supported access.

    \value BasicConformityLevel                 Basic identification (stream access).
    \value RegularConformityLevel               Regular identification (stream access).
    \value ExtendedConformityLevel              Extended identification (stream access).
    \value BasicIndividualConformityLevel       Basic identification (stream access and
                                                individual access).
    \value RegularIndividualConformityLevel     Regular identification (stream access
                                                and individual access).
    \value ExtendedIndividualConformityLevel    Extended identification (stream access
                                                and individual access).

    \sa ReadDeviceIdCode
*/

/*!
    \fn QModbusDeviceIdentification::QModbusDeviceIdentification()

    Constructs an invalid QModbusDeviceIdentification object.
*/

/*!
    \fn bool QModbusDeviceIdentification::isValid() const

    Returns \c true if the device identification object is valid; otherwise
    \c false.

    A device identification object is considered valid if \l ProductNameObjectId,
    \l ProductCodeObjectId and \l MajorMinorRevisionObjectId are set to a
    non-empty value. Still the object can contain valid object id's and
    associated data.

    \note A default constructed device identification object is invalid.
*/

/*!
    \fn QList<int> QModbusDeviceIdentification::objectIds() const

    Returns a list containing all the object id's in the
    \c QModbusDeviceIdentification object in ascending order.

    \sa ObjectId
*/

/*!
    \fn void QModbusDeviceIdentification::remove(uint objectId)

    Removes the item for the given \a objectId.

    \sa ObjectId
*/

/*!
    \fn bool QModbusDeviceIdentification::contains(uint objectId) const

    Returns \c true if there is an item for the given \a objectId; otherwise \c
    false.

    \sa ObjectId
*/

/*!
    \fn QByteArray QModbusDeviceIdentification::value(uint objectId) const

    Returns the value associated with the \a objectId. If there is no item with
    the \a objectId, the function returns a \l{default-constructed value}.

    \sa ObjectId
*/

/*!
    \fn bool QModbusDeviceIdentification::insert(uint objectId, const QByteArray &value)

    Inserts a new item with the \a objectId and a value of \a value. If there
    is already an item with the \a objectId, that item's value is replaced with
    \a value.

    Returns \c true if the size of \a value is less than 245 bytes and the
    \a objectId is less then \l QModbusDeviceIdentification::UndefinedObjectId.

    \sa ObjectId
*/

/*!
    \fn ConformityLevel QModbusDeviceIdentification::conformityLevel() const

    Returns the identification conformity level of the device and type of
    supported access.
*/

/*!
    \fn void QModbusDeviceIdentification::setConformityLevel(ConformityLevel level)

    Sets the identification conformity level of the device and type of
    supported access to \a level.
*/

/*!
    Converts the byte array \a ba to a QModbusDeviceIdentification object.

    \note: The returned object might be empty or even invalid if some error
    occurs while processing the byte array.

    \sa isValid()
*/
QModbusDeviceIdentification QModbusDeviceIdentification::fromByteArray(const QByteArray &ba)
{
    QModbusDeviceIdentification qmdi;
    // header 6 bytes: mei type + read device id + conformity level + more follows
    //                 + next object id + number of object
    // data   2 bytes: + object id + object size of the first object -> 8
    if (ba.size() >= 8) {
        if (ba[0] != EncapsulatedInterfaceTransport::ReadDeviceIdentification)
            return qmdi;
        if (ba.size() < (8 + quint8(ba[7])))
            return qmdi;
    } else {
        return qmdi;
    }

    ConformityLevel level = ConformityLevel(quint8(ba[2]));
    switch (level) {
    case BasicConformityLevel:
    case RegularConformityLevel:
    case ExtendedConformityLevel:
    case BasicIndividualConformityLevel:
    case RegularIndividualConformityLevel:
    case ExtendedIndividualConformityLevel:
        qmdi.setConformityLevel(level);
        break;
    default:
        return qmdi;
    }

    quint8 numOfObjects = ba[5];
    quint8 objectSize = quint8(ba[7]);
    qmdi.insert(quint8(ba[6]), ba.mid(8, objectSize));

    // header + object id + object size + second object id (9 bytes) + first object size
    int nextSizeField = 9 + objectSize;
    for (int i = 1; i < numOfObjects; ++i) {
        if (ba.size() <= nextSizeField)
            break;
        objectSize = ba[nextSizeField];
        if (ba.size() < (nextSizeField + objectSize))
            break;
        qmdi.insert(quint8(ba[nextSizeField - 1]), ba.mid(nextSizeField + 1, objectSize));
        nextSizeField += objectSize + 2; // object size + object id field + object size field
    }
    return qmdi;
}

QT_END_NAMESPACE
