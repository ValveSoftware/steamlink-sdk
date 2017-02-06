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

#include "osx/osxbtsocketlistener_p.h"
#include "qbluetoothserver_osx_p.h"

// The order is important: a workround for
// a private header included by private header
// (incorrectly handled dependencies).
#include "qbluetoothsocket_p.h"
#include "qbluetoothsocket_osx_p.h"

#include "qbluetoothlocaldevice.h"
#include "osx/osxbtutility_p.h"
#include "osx/osxbluetooth_p.h"
#include "qbluetoothserver.h"
#include "qbluetoothsocket.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qvariant.h>
#include <QtCore/qglobal.h>
#include <QtCore/qmutex.h>

// Import, since Obj-C headers do not have inclusion guards.
#include <Foundation/Foundation.h>

#include <limits>

QT_BEGIN_NAMESPACE

namespace {

typedef QBluetoothServiceInfo QSInfo;

QMap<quint16, QBluetoothServerPrivate *> &busyPSMs()
{
    static QMap<quint16, QBluetoothServerPrivate *> psms;
    return psms;
}

QMap<quint16, QBluetoothServerPrivate *> &busyChannels()
{
    static QMap<quint16, QBluetoothServerPrivate *> channels;
    return channels;
}

typedef QMap<quint16, QBluetoothServerPrivate *>::iterator ServerMapIterator;

}


QBluetoothServerPrivate::QBluetoothServerPrivate(QSInfo::Protocol type, QBluetoothServer *q)
                            : serverType(type),
                              q_ptr(q),
                              lastError(QBluetoothServer::NoError),
                              port(0),
                              maxPendingConnections(1)
{
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");
    if (serverType == QSInfo::UnknownProtocol)
        qCWarning(QT_BT_OSX) << "unknown protocol";
}

QBluetoothServerPrivate::~QBluetoothServerPrivate()
{
    // Actually, not good, but lock must be acquired.
    // TODO: test this.
    const QMutexLocker lock(&channelMapMutex());
    unregisterServer(this);
}

void QBluetoothServerPrivate::_q_newConnection()
{
    // Noop, we have openNotify for this.
}

bool QBluetoothServerPrivate::startListener(quint16 realPort)
{
    Q_ASSERT_X(realPort, Q_FUNC_INFO, "invalid port");

    if (serverType == QSInfo::UnknownProtocol) {
        qCWarning(QT_BT_OSX) << "invalid protocol";
        return false;
    }

    if (!listener)
        listener.reset([[ObjCListener alloc] initWithListener:this]);

    bool result = false;
    if (serverType == QSInfo::RfcommProtocol)
        result = [listener listenRFCOMMConnectionsWithChannelID:realPort];
    else
        result = [listener listenL2CAPConnectionsWithPSM:realPort];

    if (!result)
        listener.reset(nil);

    return result;
}

void QBluetoothServerPrivate::stopListener()
{
    listener.reset(nil);
}

void QBluetoothServerPrivate::openNotify(IOBluetoothRFCOMMChannel *channel)
{
    Q_ASSERT_X(listener, Q_FUNC_INFO, "invalid listener (nil)");
    Q_ASSERT_X(channel, Q_FUNC_INFO, "invalid channel (nil)");
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");

    PendingConnection newConnection(channel, true);
    pendingConnections.append(newConnection);

    emit q_ptr->newConnection();
}

void QBluetoothServerPrivate::openNotify(IOBluetoothL2CAPChannel *channel)
{
    Q_ASSERT_X(listener, Q_FUNC_INFO, "invalid listener (nil)");
    Q_ASSERT_X(channel, Q_FUNC_INFO, "invalid channel (nil)");
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");

    PendingConnection newConnection(channel, true);
    pendingConnections.append(newConnection);

    emit q_ptr->newConnection();
}

QMutex &QBluetoothServerPrivate::channelMapMutex()
{
    static QMutex mutex;
    return mutex;
}

bool QBluetoothServerPrivate::channelIsBusy(quint16 channelID)
{
    // External lock is required.
    return busyChannels().contains(channelID);
}

quint16 QBluetoothServerPrivate::findFreeChannel()
{
    // External lock is required.
    for (quint16 i = 1; i <= 30; ++i) {
        if (!busyChannels().contains(i))
            return i;
    }

    return 0; //Invalid port.
}

bool QBluetoothServerPrivate::psmIsBusy(quint16 psm)
{
    // External lock is required.
    return busyPSMs().contains(psm);
}

quint16 QBluetoothServerPrivate::findFreePSM()
{
    // External lock is required.
    for (quint16 i = 1, e = std::numeric_limits<qint16>::max(); i < e; i += 2) {
        if (!psmIsBusy(i))
            return i;
    }

    return 0; // Invalid PSM.
}

void QBluetoothServerPrivate::registerServer(QBluetoothServerPrivate *server, quint16 port)
{
    // External lock is required + port must be free.
    Q_ASSERT_X(server, Q_FUNC_INFO, "invalid server (null)");

    const QSInfo::Protocol type = server->serverType;
    if (type == QSInfo::RfcommProtocol) {
        Q_ASSERT_X(!channelIsBusy(port), Q_FUNC_INFO, "port is busy");
        busyChannels()[port] = server;
    } else if (type == QSInfo::L2capProtocol) {
        Q_ASSERT_X(!psmIsBusy(port), Q_FUNC_INFO, "port is busy");
        busyPSMs()[port] = server;
    } else {
        qCWarning(QT_BT_OSX) << "can not register a server "
                                "with unknown protocol type";
    }
}

QBluetoothServerPrivate *QBluetoothServerPrivate::registeredServer(quint16 port, QBluetoothServiceInfo::Protocol protocol)
{
    // Eternal lock is required.
    if (protocol == QSInfo::RfcommProtocol) {
        ServerMapIterator it = busyChannels().find(port);
        if (it != busyChannels().end())
            return it.value();
    } else if (protocol == QSInfo::L2capProtocol) {
        ServerMapIterator it = busyPSMs().find(port);
        if (it != busyPSMs().end())
            return it.value();
    } else {
        qCWarning(QT_BT_OSX) << "invalid protocol";
    }

    return Q_NULLPTR;
}

void QBluetoothServerPrivate::unregisterServer(QBluetoothServerPrivate *server)
{
    // External lock is required.
    const QSInfo::Protocol type = server->serverType;
    const quint16 port = server->port;

    if (type == QSInfo::RfcommProtocol) {
        ServerMapIterator it = busyChannels().find(port);
        if (it != busyChannels().end()) {
            busyChannels().erase(it);
        } else {
            qCWarning(QT_BT_OSX) << "server is not registered";
        }
    } else if (type == QSInfo::L2capProtocol) {
        ServerMapIterator it = busyPSMs().find(port);
        if (it != busyPSMs().end()) {
            busyPSMs().erase(it);
        } else {
            qCWarning(QT_BT_OSX) << "server is not registered";
        }
    } else {
        qCWarning(QT_BT_OSX) << "invalid protocol";
    }
}


QBluetoothServer::QBluetoothServer(QSInfo::Protocol serverType, QObject *parent)
                    : QObject(parent),
                      d_ptr(new QBluetoothServerPrivate(serverType, this))
{
}

QBluetoothServer::~QBluetoothServer()
{
    delete d_ptr;
}

void QBluetoothServer::close()
{
    d_ptr->listener.reset(nil);

    // Needs a lock :(
    const QMutexLocker lock(&d_ptr->channelMapMutex());
    d_ptr->unregisterServer(d_ptr);
    d_ptr->port = 0;
}

bool QBluetoothServer::listen(const QBluetoothAddress &address, quint16 port)
{
    typedef QBluetoothServerPrivate::ObjCListener ObjCListener;

    if (d_ptr->listener) {
        qCWarning(QT_BT_OSX) << "already in listen mode, close server first";
        return false;
    }

    const QBluetoothLocalDevice device(address);
    if (!device.isValid()) {
        qCWarning(QT_BT_OSX) << "device does not support Bluetooth or"
                             << address.toString()
                             << "is not a valid local adapter";
        d_ptr->lastError = UnknownError;
        emit error(UnknownError);
        return false;
    }

    const QBluetoothLocalDevice::HostMode hostMode = device.hostMode();
    if (hostMode == QBluetoothLocalDevice::HostPoweredOff) {
        qCWarning(QT_BT_OSX) << "Bluetooth device is powered off";
        d_ptr->lastError = PoweredOffError;
        emit error(PoweredOffError);
        return false;
    }

    const QSInfo::Protocol type = d_ptr->serverType;

    if (type == QSInfo::UnknownProtocol) {
        qCWarning(QT_BT_OSX) << "invalid protocol";
        d_ptr->lastError = UnsupportedProtocolError;
        emit error(d_ptr->lastError);
        return false;
    }

    d_ptr->lastError = QBluetoothServer::NoError;

    // Now we have to register a (fake) port, doing a proper (?) lock.
    const QMutexLocker lock(&d_ptr->channelMapMutex());

    if (port) {
        if (type == QSInfo::RfcommProtocol) {
            if (d_ptr->channelIsBusy(port)) {
                qCWarning(QT_BT_OSX) << "server port:" << port
                                     << "already registered";
                d_ptr->lastError = ServiceAlreadyRegisteredError;
            }
        } else {
            if (d_ptr->psmIsBusy(port)) {
                qCWarning(QT_BT_OSX) << "server port:" << port
                                     << "already registered";
                d_ptr->lastError = ServiceAlreadyRegisteredError;
            }
        }
    } else {
        type == QSInfo::RfcommProtocol ? port = d_ptr->findFreeChannel()
                                       : port = d_ptr->findFreePSM();
    }

    if (d_ptr->lastError != QBluetoothServer::NoError) {
        emit error(d_ptr->lastError);
        return false;
    }

    if (!port) {
        qCWarning(QT_BT_OSX) << "all ports are busy";
        d_ptr->lastError = ServiceAlreadyRegisteredError;
        emit error(d_ptr->lastError);
        return false;
    }

    // It's a fake port, the real one will be different
    // (provided after a service was registered).
    d_ptr->port = port;
    d_ptr->registerServer(d_ptr, port);
    d_ptr->listener.reset([[ObjCListener alloc] initWithListener:d_ptr]);

    return true;
}

QBluetoothServiceInfo QBluetoothServer::listen(const QBluetoothUuid &uuid, const QString &serviceName)
{
    if (!listen())
        return QBluetoothServiceInfo();

    QBluetoothServiceInfo serviceInfo;
    serviceInfo.setAttribute(QSInfo::ServiceName, serviceName);
    serviceInfo.setAttribute(QSInfo::BrowseGroupList,
                             QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));

    QSInfo::Sequence classId;
    classId << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));
    serviceInfo.setAttribute(QSInfo::BluetoothProfileDescriptorList, classId);

    classId.prepend(QVariant::fromValue(uuid));
    serviceInfo.setAttribute(QSInfo::ServiceClassIds, classId);
    serviceInfo.setServiceUuid(uuid);

    QSInfo::Sequence protocolDescriptorList;
    QSInfo::Sequence protocol;
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
    if (d_ptr->serverType == QSInfo::L2capProtocol)
        protocol << QVariant::fromValue(serverPort());
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    protocol.clear();

    if (d_ptr->serverType == QBluetoothServiceInfo::RfcommProtocol) {
        protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
                 << QVariant::fromValue(quint8(serverPort()));
        protocolDescriptorList.append(QVariant::fromValue(protocol));
    }

    serviceInfo.setAttribute(QSInfo::ProtocolDescriptorList,
                             protocolDescriptorList);


    // It's now up to a service info to acquire a real PSM/channel ID
    // (provided by IOBluetooth) and start a listener.
    if (!serviceInfo.registerService())
        return QBluetoothServiceInfo();

    return serviceInfo;
}

bool QBluetoothServer::isListening() const
{
    if (d_ptr->serverType == QSInfo::UnknownProtocol)
        return false;

    const QMutexLocker lock(&QBluetoothServerPrivate::channelMapMutex());
    return QBluetoothServerPrivate::registeredServer(serverPort(), d_ptr->serverType);
}

void QBluetoothServer::setMaxPendingConnections(int numConnections)
{
    // That's a 'fake' limit, it affects nothing.
    d_ptr->maxPendingConnections = numConnections;
}

int QBluetoothServer::maxPendingConnections() const
{
    // That's a 'fake' limit, it affects nothing.
    return d_ptr->maxPendingConnections;
}

bool QBluetoothServer::hasPendingConnections() const
{
    return d_ptr->pendingConnections.size();
}

QBluetoothSocket *QBluetoothServer::nextPendingConnection()
{
    if (!d_ptr->pendingConnections.size())
        return Q_NULLPTR;

    QScopedPointer<QBluetoothSocket> newSocket(new QBluetoothSocket);
    QBluetoothServerPrivate::PendingConnection channel(d_ptr->pendingConnections.front());

    // Remove it even if we have some errors below.
    d_ptr->pendingConnections.pop_front();

    if (d_ptr->serverType == QSInfo::RfcommProtocol) {
        if (!newSocket->d_ptr->setChannel(static_cast<IOBluetoothRFCOMMChannel *>(channel)))
            return Q_NULLPTR;
    } else {
        if (!newSocket->d_ptr->setChannel(static_cast<IOBluetoothL2CAPChannel *>(channel)))
            return Q_NULLPTR;
    }

    return newSocket.take();
}

QBluetoothAddress QBluetoothServer::serverAddress() const
{
    return QBluetoothLocalDevice().address();
}

quint16 QBluetoothServer::serverPort() const
{
    return d_ptr->port;
}

void QBluetoothServer::setSecurityFlags(QBluetooth::SecurityFlags security)
{
    Q_UNUSED(security)
    // Not implemented (yet?)
}

QBluetooth::SecurityFlags QBluetoothServer::securityFlags() const
{
    // Not implemented (yet?)
    return QBluetooth::NoSecurity;
}

QSInfo::Protocol QBluetoothServer::serverType() const
{
    return d_ptr->serverType;
}

QBluetoothServer::Error QBluetoothServer::error() const
{
    return d_ptr->lastError;
}

#include "moc_qbluetoothserver.cpp"

QT_END_NAMESPACE
