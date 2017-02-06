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

#include <QtCore/QLoggingCategory>
#include <QtBluetooth/qbluetooth.h>

QT_BEGIN_NAMESPACE

namespace QBluetooth {
/*!
    \namespace QBluetooth
    \inmodule QtBluetooth
    \brief The QBluetooth namespace provides classes and functions related to
    Bluetooth.

    \since 5.2
*/

/*!
    \enum QBluetooth::Security

    This enum describe the security requirements of a Bluetooth service.

    \value NoSecurity    The service does not require any security.

    \value Authorization The service requires authorization by the user,
    unless the device is Authorized-Paired.

    \value Authentication The service requires authentication. Device must
    be paired, and the user is prompted on connection unless the device is
    Authorized-Paired.

    \value Encryption The service requires the communication link to be
    encrypted. This requires the device to be paired.

    \value Secure The service requires the communication link to be secure.
    Simple Pairing from Bluetooth 2.1 or greater is required.
    Legacy pairing is not permitted.
*/

/*!
    \enum QBluetooth::AttAccessConstraint
    \since 5.7

    This enum describes the possible requirements for reading or writing an ATT attribute.

    \value AttAuthorizationRequired
        The client needs authorization from the ATT server to access the attribute.
    \value AttAuthenticationRequired
        The client needs to be authenticated to access the attribute.
    \value AttEncryptionRequired
        The attribute can only be accessed if the connection is encrypted.
*/

}

/*!
    \typedef QLowEnergyHandle
    \relates QBluetooth
    \since 5.4

    Typedef for Bluetooth Low Energy ATT attribute handles.
*/

Q_LOGGING_CATEGORY(QT_BT, "qt.bluetooth")
Q_LOGGING_CATEGORY(QT_BT_ANDROID, "qt.bluetooth.android")
Q_LOGGING_CATEGORY(QT_BT_BLUEZ, "qt.bluetooth.bluez")
Q_LOGGING_CATEGORY(QT_BT_WINRT, "qt.bluetooth.winphone")

QT_END_NAMESPACE
