/***************************************************************************
**
** Copyright (C) 2012 Research In Motion
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

#include "qbluetoothserviceinfo.h"
#include "qbluetoothserviceinfo_p.h"

#include "qbluetoothserver_p.h"
#include "qbluetoothserver.h"

#ifdef QT_QNX_BT_BLUETOOTH
#include <btapi/btspp.h>
#include <errno.h>
#endif

QT_BEGIN_NAMESPACE

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

extern QHash<QBluetoothServerPrivate*, int> __fakeServerPorts;

bool QBluetoothServiceInfoPrivate::unregisterService()
{
    if (!registered)
        return false;
    if (serverChannel() == -1)
        return false;
    if ( __fakeServerPorts.key(serverChannel()) != 0) {
#ifdef QT_QNX_BT_BLUETOOTH
        QByteArray b_uuid = attributes.value(QBluetoothServiceInfo::ServiceId).
                value<QBluetoothUuid>().toByteArray();
        b_uuid = b_uuid.mid(1, b_uuid.length() - 2);
        if (bt_spp_close_server(b_uuid.data()) == -1)
            return false;
#else
        if (!ppsSendControlMessage("deregister_server", 0x1101, attributes.value(QBluetoothServiceInfo::ServiceId).value<QBluetoothUuid>(), QString(),
                                   attributes.value(QBluetoothServiceInfo::ServiceName).toString(),
                                   __fakeServerPorts.key(serverChannel()), BT_SPP_SERVER_SUBTYPE)) {
            return false;
        }
#endif
        else {
            __fakeServerPorts.remove(__fakeServerPorts.key(serverChannel()));
            registered = false;
            return true;
        }
    }
    else {
        return false;
    }
}

bool QBluetoothServiceInfoPrivate::registerService(const QBluetoothAddress& localAdapter)
{
    Q_UNUSED(localAdapter); //QNX always uses default local adapter
    if (protocolDescriptor(QBluetoothUuid::Rfcomm).isEmpty()) {
        qCWarning(QT_BT_QNX) << Q_FUNC_INFO << "Only SPP services can be registered on QNX";
        return false;
    }

    if (serverChannel() == -1)
        return false;

    if (__fakeServerPorts.key(serverChannel()) != 0) {
#ifdef QT_QNX_BT_BLUETOOTH
        QByteArray b_uuid = attributes.value(QBluetoothServiceInfo::ServiceId)
                .value<QBluetoothUuid>().toByteArray();
        b_uuid = b_uuid.mid(1, b_uuid.length() - 2);
        qCDebug(QT_BT_QNX) << "Registering server. " << b_uuid.data()
                           << attributes.value(QBluetoothServiceInfo::ServiceName)
                              .toString();
        if (bt_spp_open_server(attributes.value(QBluetoothServiceInfo::ServiceName)
                               .toString().toUtf8().data(),
                               b_uuid.data(), true, &QBluetoothServerPrivate::btCallback,
                               reinterpret_cast<long>(__fakeServerPorts.key(serverChannel()))) == -1) {
            qCDebug(QT_BT_QNX) << "Could not open the server. "
                               << qt_error_string(errno) << errno;
            bt_spp_close_server(b_uuid.data());
            return false;
        }
#else
        if (!ppsSendControlMessage("register_server", 0x1101, attributes.value(QBluetoothServiceInfo::ServiceId).value<QBluetoothUuid>(), QString(),
                                   attributes.value(QBluetoothServiceInfo::ServiceName).toString(),
                              __fakeServerPorts.key(serverChannel()), BT_SPP_SERVER_SUBTYPE))
            return false;
#endif
        //The server needs to know the service name for the socket mount point path
        __fakeServerPorts.key(serverChannel())->m_serviceName = attributes.value(QBluetoothServiceInfo::ServiceName).toString();
    } else {
        return false;
    }

    registered = true;
    return true;
}

QT_END_NAMESPACE
