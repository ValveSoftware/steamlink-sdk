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

#include "qbluetoothtransferrequest.h"
#include "qbluetoothaddress.h"
#include "qbluetoothtransferrequest_p.h"


QT_BEGIN_NAMESPACE

/*!
    \class QBluetoothTransferRequest
    \inmodule QtBluetooth
    \brief The QBluetoothTransferRequest class stores information about a
    data transfer request.

    \since 5.2

    QBluetoothTransferRequest is part of the Bluetooth Transfer API and is the class holding the
    information necessary to initiate a transfer over Bluetooth.

    \sa QBluetoothTransferReply, QBluetoothTransferManager
*/

/*!
    \enum QBluetoothTransferRequest::Attribute

    Attribute codes for QBluetoothTransferRequest and QBluetoothTransferReply.

    \value DescriptionAttribute A textual description of the object being transferred.
    May be displayed in the UI of the remote device.
    \value TimeAttribute        Time attribute of the object being transferred.
    \value TypeAttribute        MIME type of the object being transferred.
    \value LengthAttribute      Length in bytes of the object being transferred.
    \value NameAttribute        Name of the object being transferred. May be displayed in the UI of
                                the remote device.
*/

/*!
    Constructs a new Bluetooth transfer request to the device with \a address.
*/
QBluetoothTransferRequest::QBluetoothTransferRequest(const QBluetoothAddress &address)
:d_ptr(new QBluetoothTransferRequestPrivate)
{
    Q_D(QBluetoothTransferRequest);

    d->m_address = address;
}

/*!
    Constructs a new Bluetooth transfer request that is a copy of \a other.
*/
QBluetoothTransferRequest::QBluetoothTransferRequest(const QBluetoothTransferRequest &other)
:d_ptr(new QBluetoothTransferRequestPrivate)
{
    *this = other;
}

/*!
    Destorys the Bluetooth transfer request.
*/
QBluetoothTransferRequest::~QBluetoothTransferRequest()
{
    delete d_ptr;
}

/*!
    Returns the attribute associated with \a code. If the attribute has not been set, it
    returns \a defaultValue.

    \sa setAttribute(), QBluetoothTransferRequest::Attribute
*/
QVariant QBluetoothTransferRequest::attribute(Attribute code, const QVariant &defaultValue) const
{
    Q_D(const QBluetoothTransferRequest);

    if (d->m_parameters.contains((int)code)) {
        return d->m_parameters.value((int)code);
    } else {
        return defaultValue;
    }
}

/*!
    Sets the attribute associated with \a code to \a value. If the attribute is
    already set, the previous value is discarded. If \a value is an invalid QVariant, the attribute
    is unset.

    \sa attribute(), QBluetoothTransferRequest::Attribute
*/
void QBluetoothTransferRequest::setAttribute(Attribute code, const QVariant &value)
{
    Q_D(QBluetoothTransferRequest);

    d->m_parameters.insert((int)code, value);
}

/*!
    Returns the address associated with the Bluetooth transfer request.
*/
QBluetoothAddress QBluetoothTransferRequest::address() const
{
    Q_D(const QBluetoothTransferRequest);

    return d->m_address;
}


/*!
    Returns true if this object is not the same as \a other.

    \sa operator==()
*/
bool QBluetoothTransferRequest::operator!=(const QBluetoothTransferRequest &other) const
{
    return !(*this == other);
}

/*!
    Creates a copy of \a other.
*/
QBluetoothTransferRequest &QBluetoothTransferRequest::operator=(const QBluetoothTransferRequest &other)
{
    Q_D(QBluetoothTransferRequest);

    d->m_address = other.d_func()->m_address;
    d->m_parameters = other.d_func()->m_parameters;

    return *this;
}

/*!
    Returns true if this object is the same as \a other.
*/
bool QBluetoothTransferRequest::operator==(const QBluetoothTransferRequest &other) const
{
    Q_D(const QBluetoothTransferRequest);
    if (d->m_address == other.d_func()->m_address && d->m_parameters == other.d_func()->m_parameters) {
        return true;
    }
    return false;
}

QBluetoothTransferRequestPrivate::QBluetoothTransferRequestPrivate()
{
}

QT_END_NAMESPACE
