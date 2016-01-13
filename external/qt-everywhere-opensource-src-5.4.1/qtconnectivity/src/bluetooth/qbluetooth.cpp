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
Q_LOGGING_CATEGORY(QT_BT_QNX, "qt.bluetooth.qnx")

QT_END_NAMESPACE
