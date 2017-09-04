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
#include "qbluetoothhostinfo_p.h"

QT_BEGIN_NAMESPACE

/*!
    \class QBluetoothHostInfo
    \inmodule QtBluetooth
    \brief The QBluetoothHostInfo class encapsulates the details of a local
    QBluetooth device.

    \since 5.2

    This class holds the name and address of a local Bluetooth device.
*/

/*!
    Constructs a null QBluetoothHostInfo object.
*/
QBluetoothHostInfo::QBluetoothHostInfo() :
    d_ptr(new QBluetoothHostInfoPrivate)
{
}

/*!
    Constructs a new QBluetoothHostInfo which is a copy of \a other.
*/
QBluetoothHostInfo::QBluetoothHostInfo(const QBluetoothHostInfo &other) :
    d_ptr(new QBluetoothHostInfoPrivate)
{
    Q_D(QBluetoothHostInfo);

    d->m_address = other.d_func()->m_address;
    d->m_name = other.d_func()->m_name;
}

/*!
    Destroys the QBluetoothHostInfo.
*/
QBluetoothHostInfo::~QBluetoothHostInfo()
{
    delete d_ptr;
}

/*!
    Assigns \a other to this QBluetoothHostInfo instance.
*/
QBluetoothHostInfo &QBluetoothHostInfo::operator=(const QBluetoothHostInfo &other)
{
    Q_D(QBluetoothHostInfo);

    d->m_address = other.d_func()->m_address;
    d->m_name = other.d_func()->m_name;

    return *this;
}

/*!
    \since 5.5

    Returns true if \a other is equal to this QBluetoothHostInfo, otherwise false.
*/
bool QBluetoothHostInfo::operator==(const QBluetoothHostInfo &other) const
{
    if (d_ptr == other.d_ptr)
        return true;

    return d_ptr->m_address == other.d_ptr->m_address && d_ptr->m_name == other.d_ptr->m_name;
}

/*!
    \since 5.5

    Returns true if \a other is not equal to this QBluetoothHostInfo, otherwise false.
*/
bool QBluetoothHostInfo::operator!=(const QBluetoothHostInfo &other) const
{
    return !operator==(other);
}

/*!
    Returns the Bluetooth address as a QBluetoothAddress.
*/
QBluetoothAddress QBluetoothHostInfo::address() const
{
    Q_D(const QBluetoothHostInfo);
    return d->m_address;
}

/*!
    Sets the Bluetooth \a address for this Bluetooth host info object.
*/
void QBluetoothHostInfo::setAddress(const QBluetoothAddress &address)
{
    Q_D(QBluetoothHostInfo);
    d->m_address = address;
}

/*!
    Returns the user visible name of the host info object.
*/
QString QBluetoothHostInfo::name() const
{
    Q_D(const QBluetoothHostInfo);
    return d->m_name;
}

/*!
    Sets the \a name of the host info object.
*/
void QBluetoothHostInfo::setName(const QString &name)
{
    Q_D(QBluetoothHostInfo);
    d->m_name = name;
}

QT_END_NAMESPACE
