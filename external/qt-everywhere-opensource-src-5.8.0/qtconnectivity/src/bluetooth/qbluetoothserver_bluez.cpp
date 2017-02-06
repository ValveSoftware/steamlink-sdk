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

#include "qbluetoothserver.h"
#include "qbluetoothserver_p.h"
#include "qbluetoothsocket.h"
#include "qbluetoothlocaldevice.h"
#include "bluez/bluez_data_p.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QSocketNotifier>

#include <errno.h>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

static inline void convertAddress(quint64 from, quint8 (&to)[6])
{
    to[0] = (from >> 0) & 0xff;
    to[1] = (from >> 8) & 0xff;
    to[2] = (from >> 16) & 0xff;
    to[3] = (from >> 24) & 0xff;
    to[4] = (from >> 32) & 0xff;
    to[5] = (from >> 40) & 0xff;
}

QBluetoothServerPrivate::QBluetoothServerPrivate(QBluetoothServiceInfo::Protocol sType)
    :   maxPendingConnections(1), securityFlags(QBluetooth::Authorization), serverType(sType),
        m_lastError(QBluetoothServer::NoError), socketNotifier(0)
{
    if (sType == QBluetoothServiceInfo::RfcommProtocol)
        socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol);
    else
        socket = new QBluetoothSocket(QBluetoothServiceInfo::L2capProtocol);
}

QBluetoothServerPrivate::~QBluetoothServerPrivate()
{
    delete socketNotifier;

    delete socket;
}

void QBluetoothServerPrivate::_q_newConnection()
{
    // disable socket notifier until application calls nextPendingConnection().
    socketNotifier->setEnabled(false);

    emit q_ptr->newConnection();
}

void QBluetoothServerPrivate::setSocketSecurityLevel(
        QBluetooth::SecurityFlags requestedSecLevel, int *errnoCode)
{
    if (requestedSecLevel == QBluetooth::NoSecurity) {
        qCWarning(QT_BT_BLUEZ) << "Cannot set NoSecurity on server socket";
        return;
    }

    struct bt_security security;
    memset(&security, 0, sizeof(security));

    // ignore QBluetooth::Authentication -> not used anymore
    if (requestedSecLevel & QBluetooth::Authorization)
        security.level = BT_SECURITY_LOW;
    if (requestedSecLevel & QBluetooth::Encryption)
        security.level = BT_SECURITY_MEDIUM;
    if (requestedSecLevel & QBluetooth::Secure)
        security.level = BT_SECURITY_HIGH;

    if (setsockopt(socket->socketDescriptor(), SOL_BLUETOOTH, BT_SECURITY,
                   &security, sizeof(security)) != 0) {
        if (errnoCode)
            *errnoCode = errno;
    }
}

QBluetooth::SecurityFlags QBluetoothServerPrivate::socketSecurityLevel() const
{
    struct bt_security security;
    memset(&security, 0, sizeof(security));
    socklen_t length = sizeof(security);

    if (getsockopt(socket->socketDescriptor(), SOL_BLUETOOTH, BT_SECURITY,
                   &security, &length) != 0) {
        qCWarning(QT_BT_BLUEZ) << "Failed to get security flags" << qt_error_string(errno);
        return QBluetooth::NoSecurity;
    }

    switch (security.level) {
    case BT_SECURITY_LOW:
        return QBluetooth::Authorization;
    case BT_SECURITY_MEDIUM:
        return QBluetooth::Encryption;
    case BT_SECURITY_HIGH:
        return QBluetooth::Secure;
    default:
        qCWarning(QT_BT_BLUEZ) << "Unknown server socket security level" << security.level;
        return QBluetooth::NoSecurity;
    }
}

void QBluetoothServer::close()
{
    Q_D(QBluetoothServer);

    delete d->socketNotifier;
    d->socketNotifier = 0;

    d->socket->close();
}

bool QBluetoothServer::listen(const QBluetoothAddress &address, quint16 port)
{
    Q_D(QBluetoothServer);

    if (d->socket->state() == QBluetoothSocket::ListeningState) {
        qCWarning(QT_BT_BLUEZ) << "Socket already in listen mode, close server first";
        return false; //already listening, nothing to do
    }

    QBluetoothLocalDevice device(address);
    if (!device.isValid()) {
        qCWarning(QT_BT_BLUEZ) << "Device does not support Bluetooth or"
                                 << address.toString() << "is not a valid local adapter";
        d->m_lastError = QBluetoothServer::UnknownError;
        emit error(d->m_lastError);
        return false;
    }

    QBluetoothLocalDevice::HostMode hostMode = device.hostMode();
    if (hostMode == QBluetoothLocalDevice::HostPoweredOff) {
        d->m_lastError = QBluetoothServer::PoweredOffError;
        emit error(d->m_lastError);
        qCWarning(QT_BT_BLUEZ) << "Bluetooth device is powered off";
        return false;
    }

    int sock = d->socket->socketDescriptor();
    if (sock < 0) {
        /* Negative socket descriptor is not always an error case
         * Another cause could be a call to close()/abort()
         * Check whether we can recover by re-creating the socket
         * we should really call Bluez::QBluetoothSocketPrivate::ensureNativeSocket
         * but a re-creation of the socket will do as well.
         */

        delete d->socket;
        if (serverType() == QBluetoothServiceInfo::RfcommProtocol)
            d->socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol);
        else
            d->socket = new QBluetoothSocket(QBluetoothServiceInfo::L2capProtocol);

        sock = d->socket->socketDescriptor();
        if (sock < 0) {
            d->m_lastError = InputOutputError;
            emit error(d->m_lastError);
            return false;
        }
    }

    if (d->serverType == QBluetoothServiceInfo::RfcommProtocol) {
        sockaddr_rc addr;

        addr.rc_family = AF_BLUETOOTH;
        addr.rc_channel = port;

        if (!address.isNull())
            convertAddress(address.toUInt64(), addr.rc_bdaddr.b);
        else
            convertAddress(device.address().toUInt64(), addr.rc_bdaddr.b);

        if (::bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(sockaddr_rc)) < 0) {
            if (errno == EADDRINUSE)
                d->m_lastError = ServiceAlreadyRegisteredError;
            else
                d->m_lastError = InputOutputError;
            emit error(d->m_lastError);
            return false;
        }
    } else {
        sockaddr_l2 addr;

        memset(&addr, 0, sizeof(sockaddr_l2));
        addr.l2_family = AF_BLUETOOTH;
        addr.l2_psm = port;

        if (!address.isNull())
            convertAddress(address.toUInt64(), addr.l2_bdaddr.b);
        else
            convertAddress(Q_UINT64_C(0), addr.l2_bdaddr.b);

        if (::bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(sockaddr_l2)) < 0) {
            d->m_lastError = InputOutputError;
            emit error(d->m_lastError);
            return false;
        }
    }

    d->setSocketSecurityLevel(d->securityFlags, 0);

    if (::listen(sock, d->maxPendingConnections) < 0) {
        d->m_lastError = InputOutputError;
        emit error(d->m_lastError);
        return false;
    }

    d->socket->setSocketState(QBluetoothSocket::ListeningState);

    if (!d->socketNotifier) {
        d->socketNotifier = new QSocketNotifier(d->socket->socketDescriptor(),
                                                QSocketNotifier::Read);
        connect(d->socketNotifier, SIGNAL(activated(int)), this, SLOT(_q_newConnection()));
    }

    return true;
}

void QBluetoothServer::setMaxPendingConnections(int numConnections)
{
    Q_D(QBluetoothServer);

    if (d->socket->state() == QBluetoothSocket::UnconnectedState)
        d->maxPendingConnections = numConnections;
}

bool QBluetoothServer::hasPendingConnections() const
{
    Q_D(const QBluetoothServer);

    if (!d || !d->socketNotifier)
        return false;

    // if the socket notifier is disabled there is a pending connection waiting for us to accept.
    return !d->socketNotifier->isEnabled();
}

QBluetoothSocket *QBluetoothServer::nextPendingConnection()
{
    Q_D(QBluetoothServer);

    if (!hasPendingConnections())
        return 0;

    int pending;
    if (d->serverType == QBluetoothServiceInfo::RfcommProtocol) {
        sockaddr_rc addr;
        socklen_t length = sizeof(sockaddr_rc);
        pending = ::accept(d->socket->socketDescriptor(),
                               reinterpret_cast<sockaddr *>(&addr), &length);
    } else {
        sockaddr_l2 addr;
        socklen_t length = sizeof(sockaddr_l2);
        pending = ::accept(d->socket->socketDescriptor(),
                               reinterpret_cast<sockaddr *>(&addr), &length);
    }

    if (pending >= 0) {
        QBluetoothSocket *newSocket = new QBluetoothSocket;
        if (d->serverType == QBluetoothServiceInfo::RfcommProtocol)
            newSocket->setSocketDescriptor(pending, QBluetoothServiceInfo::RfcommProtocol);
        else
            newSocket->setSocketDescriptor(pending, QBluetoothServiceInfo::L2capProtocol);

        d->socketNotifier->setEnabled(true);

        return newSocket;
    } else {
        d->socketNotifier->setEnabled(true);
    }

    return 0;
}

QBluetoothAddress QBluetoothServer::serverAddress() const
{
    Q_D(const QBluetoothServer);

    return d->socket->localAddress();
}

quint16 QBluetoothServer::serverPort() const
{
    Q_D(const QBluetoothServer);

    return d->socket->localPort();
}

void QBluetoothServer::setSecurityFlags(QBluetooth::SecurityFlags security)
{
    Q_D(QBluetoothServer);

    if (d->socket->state() == QBluetoothSocket::UnconnectedState) {
        // nothing to set beyond the fact to remember the sec level for the next listen()
        d->securityFlags = security;
        return;
    }

    int errorCode = 0;
    d->setSocketSecurityLevel(security, &errorCode);
    if (errorCode) {
        qCWarning(QT_BT_BLUEZ) << "Failed to set socket option, closing socket for safety" << errorCode;
        qCWarning(QT_BT_BLUEZ) << "Error: " << qt_error_string(errorCode);
        d->m_lastError = InputOutputError;
        emit error(d->m_lastError);
        d->socket->close();
    }
}

QBluetooth::SecurityFlags QBluetoothServer::securityFlags() const
{
    Q_D(const QBluetoothServer);

    if (d->socket->state() == QBluetoothSocket::UnconnectedState)
        return d->securityFlags;

    return d->socketSecurityLevel();
}

QT_END_NAMESPACE
