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

#include <QtCore/QLoggingCategory>

#include "qbluetoothhostinfo.h"
#include "qbluetoothlocaldevice.h"
#include "qbluetoothserviceinfo.h"
#include "qbluetoothserviceinfo_p.h"
#include "qbluetoothserver_p.h"
#include "qbluetoothserver.h"

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_ANDROID)

extern QHash<QBluetoothServerPrivate*, int> __fakeServerPorts;

QBluetoothServiceInfoPrivate::QBluetoothServiceInfoPrivate()
:  registered(false)
{
}

QBluetoothServiceInfoPrivate::~QBluetoothServiceInfoPrivate()
{
}

bool QBluetoothServiceInfoPrivate::isRegistered() const
{
    return registered;
}

bool QBluetoothServiceInfoPrivate::unregisterService()
{
    if (!registered)
        return false;

    QBluetoothServerPrivate *sPriv = __fakeServerPorts.key(serverChannel());
    if (!sPriv) {
        //QBluetoothServer::close() was called without prior call to unregisterService().
        //Now it is unregistered anyway.
        registered = false;
        return true;
    }

    bool result = sPriv->deactivateActiveListening();
    if (!result)
        return false;

    registered = false;
    return true;
}

bool QBluetoothServiceInfoPrivate::registerService(const QBluetoothAddress& localAdapter)
{
    const QList<QBluetoothHostInfo> localDevices = QBluetoothLocalDevice::allDevices();
    if (!localDevices.count())
        return false; //no Bluetooth device

    if (!localAdapter.isNull()) {
        bool found = false;
        foreach (const QBluetoothHostInfo &hostInfo, localDevices) {
            if (hostInfo.address() == localAdapter) {
                found = true;
                break;
            }
        }

        if (!found) {
            qCWarning(QT_BT_ANDROID) << localAdapter.toString() << "is not a valid local Bt adapter";
            return false;
        }
    }

    //already registered on local adapter => nothing to do
    if (registered)
        return false;

    if (protocolDescriptor(QBluetoothUuid::Rfcomm).isEmpty()) {
        qCWarning(QT_BT_ANDROID) << Q_FUNC_INFO << "Only RFCOMM services can be registered on Android";
        return false;
    }

    QBluetoothServerPrivate *sPriv = __fakeServerPorts.key(serverChannel());
    if (!sPriv)
        return false;

    //tell the server what service name and uuid our listener should have
    //and start the real listener
    bool result = sPriv->initiateActiveListening(
                attributes.value(QBluetoothServiceInfo::ServiceId).value<QBluetoothUuid>(),
                attributes.value(QBluetoothServiceInfo::ServiceName).toString());
    if (!result) {
        return false;
    }


    registered = true;
    return true;
}

QT_END_NAMESPACE
