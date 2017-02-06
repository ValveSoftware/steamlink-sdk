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

#include "qbluetoothsocket.h"
#include "qbluetoothsocket_p.h"

#include "bluez/manager_p.h"
#include "bluez/adapter_p.h"
#include "bluez/device_p.h"
#include "bluez/objectmanager_p.h"
#include <QtBluetooth/QBluetoothLocalDevice>
#include "bluez/bluez_data_p.h"

#include <qplatformdefs.h>
#include <QtCore/private/qcore_unix_p.h>

#include <QtCore/QLoggingCategory>

#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <QtCore/QSocketNotifier>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT_BLUEZ)

QBluetoothSocketPrivate::QBluetoothSocketPrivate()
    : socket(-1),
      socketType(QBluetoothServiceInfo::UnknownProtocol),
      state(QBluetoothSocket::UnconnectedState),
      socketError(QBluetoothSocket::NoSocketError),
      readNotifier(0),
      connectWriteNotifier(0),
      connecting(false),
      discoveryAgent(0),
      secFlags(QBluetooth::Authorization),
      lowEnergySocketType(0)
{
}

QBluetoothSocketPrivate::~QBluetoothSocketPrivate()
{
    delete readNotifier;
    readNotifier = 0;
    delete connectWriteNotifier;
    connectWriteNotifier = 0;
}

bool QBluetoothSocketPrivate::ensureNativeSocket(QBluetoothServiceInfo::Protocol type)
{
    if (socket != -1) {
        if (socketType == type)
            return true;

        delete readNotifier;
        readNotifier = 0;
        delete connectWriteNotifier;
        connectWriteNotifier = 0;
        QT_CLOSE(socket);
    }

    socketType = type;

    switch (type) {
    case QBluetoothServiceInfo::L2capProtocol:
        socket = ::socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
        break;
    case QBluetoothServiceInfo::RfcommProtocol:
        socket = ::socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
        break;
    default:
        socket = -1;
    }

    if (socket == -1)
        return false;

    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);

    Q_Q(QBluetoothSocket);
    readNotifier = new QSocketNotifier(socket, QSocketNotifier::Read);
    QObject::connect(readNotifier, SIGNAL(activated(int)), this, SLOT(_q_readNotify()));
    connectWriteNotifier = new QSocketNotifier(socket, QSocketNotifier::Write, q);
    QObject::connect(connectWriteNotifier, SIGNAL(activated(int)), this, SLOT(_q_writeNotify()));

    connectWriteNotifier->setEnabled(false);
    readNotifier->setEnabled(false);


    return true;
}

void QBluetoothSocketPrivate::connectToService(const QBluetoothAddress &address, quint16 port, QIODevice::OpenMode openMode)
{
    Q_Q(QBluetoothSocket);
    int result = -1;

    if (socket == -1 && !ensureNativeSocket(socketType)) {
        errorString = QBluetoothSocket::tr("Unknown socket error");
        q->setSocketError(QBluetoothSocket::UnknownSocketError);
        return;
    }

    // apply preferred security level
    // ignore QBluetooth::Authentication -> not used anymore by kernel
    struct bt_security security;
    memset(&security, 0, sizeof(security));

    if (secFlags & QBluetooth::Authorization)
        security.level = BT_SECURITY_LOW;
    if (secFlags & QBluetooth::Encryption)
        security.level = BT_SECURITY_MEDIUM;
    if (secFlags & QBluetooth::Secure)
        security.level = BT_SECURITY_HIGH;

    if (setsockopt(socket, SOL_BLUETOOTH, BT_SECURITY,
                   &security, sizeof(security)) != 0) {
        qCWarning(QT_BT_BLUEZ) << "Failed to set socket option, closing socket for safety" << errno;
        qCWarning(QT_BT_BLUEZ) << "Error: " << qt_error_string(errno);
        errorString = QBluetoothSocket::tr("Cannot set connection security level");
        q->setSocketError(QBluetoothSocket::UnknownSocketError);
        return;
    }

    if (socketType == QBluetoothServiceInfo::RfcommProtocol) {
        sockaddr_rc addr;

        memset(&addr, 0, sizeof(addr));
        addr.rc_family = AF_BLUETOOTH;
        addr.rc_channel = port;

        convertAddress(address.toUInt64(), addr.rc_bdaddr.b);

        connectWriteNotifier->setEnabled(true);
        readNotifier->setEnabled(true);QString();

        result = ::connect(socket, (sockaddr *)&addr, sizeof(addr));
    } else if (socketType == QBluetoothServiceInfo::L2capProtocol) {
        sockaddr_l2 addr;

        memset(&addr, 0, sizeof(addr));
        addr.l2_family = AF_BLUETOOTH;
        // This is an ugly hack but the socket class does what's needed already.
        // For L2CP GATT we need a channel rather than a socket and the LE address type
        // We don't want to make this public API offering for now especially since
        // only Linux (of all platforms supported by this library) supports this type
        // of socket.

#if defined(QT_BLUEZ_BLUETOOTH) && !defined(QT_BLUEZ_NO_BTLE)
        if (lowEnergySocketType) {
            addr.l2_cid = htobs(port);
            addr.l2_bdaddr_type = lowEnergySocketType;
        } else {
            addr.l2_psm = htobs(port);
        }
#else
        addr.l2_psm = htobs(port);
#endif

        convertAddress(address.toUInt64(), addr.l2_bdaddr.b);

        connectWriteNotifier->setEnabled(true);
        readNotifier->setEnabled(true);

        result = ::connect(socket, (sockaddr *)&addr, sizeof(addr));
    }

    if (result >= 0 || (result == -1 && errno == EINPROGRESS)) {
        connecting = true;
        q->setSocketState(QBluetoothSocket::ConnectingState);
        q->setOpenMode(openMode);
    } else {
        errorString = qt_error_string(errno);
        q->setSocketError(QBluetoothSocket::UnknownSocketError);
    }
}

void QBluetoothSocketPrivate::_q_writeNotify()
{
    Q_Q(QBluetoothSocket);
    if(connecting && state == QBluetoothSocket::ConnectingState){
        int errorno, len;
        len = sizeof(errorno);
        ::getsockopt(socket, SOL_SOCKET, SO_ERROR, &errorno, (socklen_t*)&len);
        if(errorno) {
            errorString = qt_error_string(errorno);
            q->setSocketError(QBluetoothSocket::UnknownSocketError);
            return;
        }

        q->setSocketState(QBluetoothSocket::ConnectedState);
        emit q->connected();

        connectWriteNotifier->setEnabled(false);
        connecting = false;
    }
    else {
        if (txBuffer.size() == 0) {
            connectWriteNotifier->setEnabled(false);
            return;
        }

        char buf[1024];

        int size = txBuffer.read(buf, 1024);
        int writtenBytes = qt_safe_write(socket, buf, size);
        if (writtenBytes < 0) {
            switch (errno) {
            case EAGAIN:
                writtenBytes = 0;
                txBuffer.ungetBlock(buf, size);
                break;
            default:
                // every other case returns error
                errorString = QBluetoothSocket::tr("Network Error: %1");
                errorString.arg(qt_error_string(errno));
                q->setSocketError(QBluetoothSocket::NetworkError);
                break;
            }
        } else {
            if (writtenBytes < size) {
                // add remainder back to buffer
                char* remainder = buf + writtenBytes;
                txBuffer.ungetBlock(remainder, size - writtenBytes);
            }
            if (writtenBytes > 0)
                emit q->bytesWritten(writtenBytes);
        }

        if (txBuffer.size()) {
            connectWriteNotifier->setEnabled(true);
        }
        else if (state == QBluetoothSocket::ClosingState) {
            connectWriteNotifier->setEnabled(false);
            this->close();
        }
    }
}

void QBluetoothSocketPrivate::_q_readNotify()
{
    Q_Q(QBluetoothSocket);
    char *writePointer = buffer.reserve(QPRIVATELINEARBUFFER_BUFFERSIZE);
//    qint64 readFromDevice = q->readData(writePointer, QPRIVATELINEARBUFFER_BUFFERSIZE);
    int readFromDevice = ::read(socket, writePointer, QPRIVATELINEARBUFFER_BUFFERSIZE);
    buffer.chop(QPRIVATELINEARBUFFER_BUFFERSIZE - (readFromDevice < 0 ? 0 : readFromDevice));
    if(readFromDevice <= 0){
        int errsv = errno;
        readNotifier->setEnabled(false);
        connectWriteNotifier->setEnabled(false);
        errorString = qt_error_string(errsv);
        qCWarning(QT_BT_BLUEZ) << Q_FUNC_INFO << socket << "error:" << readFromDevice << errorString;
        if (errsv == EHOSTDOWN)
            q->setSocketError(QBluetoothSocket::HostNotFoundError);
        else if (errsv != ECONNRESET) // The other side closing the connection is not an error.
            q->setSocketError(QBluetoothSocket::UnknownSocketError);

        q->disconnectFromService();
    }
    else {
        emit q->readyRead();
    }
}

void QBluetoothSocketPrivate::abort()
{
    delete readNotifier;
    readNotifier = 0;
    delete connectWriteNotifier;
    connectWriteNotifier = 0;

    // We don't transition through Closing for abort, so
    // we don't call disconnectFromService or
    // QBluetoothSocket::close
    QT_CLOSE(socket);
    socket = -1;
}

QString QBluetoothSocketPrivate::localName() const
{
    const QBluetoothAddress address = localAddress();
    if (address.isNull())
        return QString();

    QBluetoothLocalDevice device(address);
    return device.name();
}

QBluetoothAddress QBluetoothSocketPrivate::localAddress() const
{
    if (socketType == QBluetoothServiceInfo::RfcommProtocol) {
        sockaddr_rc addr;
        socklen_t addrLength = sizeof(addr);

        if (::getsockname(socket, reinterpret_cast<sockaddr *>(&addr), &addrLength) == 0)
            return QBluetoothAddress(convertAddress(addr.rc_bdaddr.b));
    } else if (socketType == QBluetoothServiceInfo::L2capProtocol) {
        sockaddr_l2 addr;
        socklen_t addrLength = sizeof(addr);

        if (::getsockname(socket, reinterpret_cast<sockaddr *>(&addr), &addrLength) == 0)
            return QBluetoothAddress(convertAddress(addr.l2_bdaddr.b));
    }

    return QBluetoothAddress();
}

quint16 QBluetoothSocketPrivate::localPort() const
{
    if (socketType == QBluetoothServiceInfo::RfcommProtocol) {
        sockaddr_rc addr;
        socklen_t addrLength = sizeof(addr);

        if (::getsockname(socket, reinterpret_cast<sockaddr *>(&addr), &addrLength) == 0)
            return addr.rc_channel;
    } else if (socketType == QBluetoothServiceInfo::L2capProtocol) {
        sockaddr_l2 addr;
        socklen_t addrLength = sizeof(addr);

        if (::getsockname(socket, reinterpret_cast<sockaddr *>(&addr), &addrLength) == 0)
            return addr.l2_psm;
    }

    return 0;
}

QString QBluetoothSocketPrivate::peerName() const
{
    quint64 bdaddr;

    if (socketType == QBluetoothServiceInfo::RfcommProtocol) {
        sockaddr_rc addr;
        socklen_t addrLength = sizeof(addr);

        if (::getpeername(socket, reinterpret_cast<sockaddr *>(&addr), &addrLength) < 0)
            return QString();

        convertAddress(addr.rc_bdaddr.b, &bdaddr);
    } else if (socketType == QBluetoothServiceInfo::L2capProtocol) {
        sockaddr_l2 addr;
        socklen_t addrLength = sizeof(addr);

        if (::getpeername(socket, reinterpret_cast<sockaddr *>(&addr), &addrLength) < 0)
            return QString();

        convertAddress(addr.l2_bdaddr.b, &bdaddr);
    } else {
        qCWarning(QT_BT_BLUEZ) << "peerName() called on socket of unknown type";
        return QString();
    }

    const QString peerAddress = QBluetoothAddress(bdaddr).toString();
    const QString localAdapter = localAddress().toString();

    if (isBluez5()) {
        OrgFreedesktopDBusObjectManagerInterface manager(QStringLiteral("org.bluez"),
                                                         QStringLiteral("/"),
                                                         QDBusConnection::systemBus());
        QDBusPendingReply<ManagedObjectList> reply = manager.GetManagedObjects();
        reply.waitForFinished();
        if (reply.isError())
            return QString();

        ManagedObjectList managedObjectList = reply.value();
        for (ManagedObjectList::const_iterator it = managedObjectList.constBegin(); it != managedObjectList.constEnd(); ++it) {
            const InterfaceList &ifaceList = it.value();

            for (InterfaceList::const_iterator jt = ifaceList.constBegin(); jt != ifaceList.constEnd(); ++jt) {
                const QString &iface = jt.key();
                const QVariantMap &ifaceValues = jt.value();

                if (iface == QStringLiteral("org.bluez.Device1")) {
                    if (ifaceValues.value(QStringLiteral("Address")).toString() == peerAddress)
                        return ifaceValues.value(QStringLiteral("Alias")).toString();
                }
            }
        }
        return QString();
    } else {
        OrgBluezManagerInterface manager(QStringLiteral("org.bluez"), QStringLiteral("/"),
                                         QDBusConnection::systemBus());

        QDBusPendingReply<QDBusObjectPath> reply = manager.FindAdapter(localAdapter);
        reply.waitForFinished();
        if (reply.isError())
            return QString();

        OrgBluezAdapterInterface adapter(QStringLiteral("org.bluez"), reply.value().path(),
                                         QDBusConnection::systemBus());

        QDBusPendingReply<QDBusObjectPath> deviceObjectPath = adapter.FindDevice(peerAddress);
        deviceObjectPath.waitForFinished();
        if (deviceObjectPath.isError()) {
            if (deviceObjectPath.error().name() != QStringLiteral("org.bluez.Error.DoesNotExist"))
                return QString();

            deviceObjectPath = adapter.CreateDevice(peerAddress);
            deviceObjectPath.waitForFinished();
            if (deviceObjectPath.isError())
                return QString();
        }

        OrgBluezDeviceInterface device(QStringLiteral("org.bluez"), deviceObjectPath.value().path(),
                                       QDBusConnection::systemBus());

        QDBusPendingReply<QVariantMap> properties = device.GetProperties();
        properties.waitForFinished();
        if (properties.isError())
            return QString();

        return properties.value().value(QStringLiteral("Alias")).toString();
    }
}

QBluetoothAddress QBluetoothSocketPrivate::peerAddress() const
{
    if (socketType == QBluetoothServiceInfo::RfcommProtocol) {
        sockaddr_rc addr;
        socklen_t addrLength = sizeof(addr);

        if (::getpeername(socket, reinterpret_cast<sockaddr *>(&addr), &addrLength) == 0)
            return QBluetoothAddress(convertAddress(addr.rc_bdaddr.b));
    } else if (socketType == QBluetoothServiceInfo::L2capProtocol) {
        sockaddr_l2 addr;
        socklen_t addrLength = sizeof(addr);

        if (::getpeername(socket, reinterpret_cast<sockaddr *>(&addr), &addrLength) == 0)
            return QBluetoothAddress(convertAddress(addr.l2_bdaddr.b));
    }

    return QBluetoothAddress();
}

quint16 QBluetoothSocketPrivate::peerPort() const
{
    if (socketType == QBluetoothServiceInfo::RfcommProtocol) {
        sockaddr_rc addr;
        socklen_t addrLength = sizeof(addr);

        if (::getpeername(socket, reinterpret_cast<sockaddr *>(&addr), &addrLength) == 0)
            return addr.rc_channel;
    } else if (socketType == QBluetoothServiceInfo::L2capProtocol) {
        sockaddr_l2 addr;
        socklen_t addrLength = sizeof(addr);

        if (::getpeername(socket, reinterpret_cast<sockaddr *>(&addr), &addrLength) == 0)
            return addr.l2_psm;
    }

    return 0;
}

qint64 QBluetoothSocketPrivate::writeData(const char *data, qint64 maxSize)
{
    Q_Q(QBluetoothSocket);

    if (state != QBluetoothSocket::ConnectedState) {
        errorString = QBluetoothSocket::tr("Cannot write while not connected");
        q->setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    if (q->openMode() & QIODevice::Unbuffered) {
        int sz = ::qt_safe_write(socket, data, maxSize);
        if (sz < 0) {
            switch (errno) {
            case EAGAIN:
                sz = 0;
                break;
            default:
                errorString = QBluetoothSocket::tr("Network Error: %1");
                errorString.arg(qt_error_string(errno));
                q->setSocketError(QBluetoothSocket::NetworkError);
            }
        }

        if (sz > 0)
            emit q->bytesWritten(sz);

        return sz;
    }
    else {

        if(!connectWriteNotifier)
            return -1;

        if(txBuffer.size() == 0) {
            connectWriteNotifier->setEnabled(true);
            QMetaObject::invokeMethod(this, "_q_writeNotify", Qt::QueuedConnection);
        }

        char *txbuf = txBuffer.reserve(maxSize);
        memcpy(txbuf, data, maxSize);

        return maxSize;
    }
}

qint64 QBluetoothSocketPrivate::readData(char *data, qint64 maxSize)
{
    Q_Q(QBluetoothSocket);

    if (state != QBluetoothSocket::ConnectedState) {
        errorString = QBluetoothSocket::tr("Cannot read while not connected");
        q->setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    if (!buffer.isEmpty()) {
        int i = buffer.read(data, maxSize);
        return i;
    }

    return 0;
}

void QBluetoothSocketPrivate::close()
{
    if (txBuffer.size() > 0)
        connectWriteNotifier->setEnabled(true);
    else
        abort();
}

bool QBluetoothSocketPrivate::setSocketDescriptor(int socketDescriptor, QBluetoothServiceInfo::Protocol socketType_,
                                           QBluetoothSocket::SocketState socketState, QBluetoothSocket::OpenMode openMode)
{
    Q_Q(QBluetoothSocket);
    delete readNotifier;
    readNotifier = 0;
    delete connectWriteNotifier;
    connectWriteNotifier = 0;

    socketType = socketType_;
    socket = socketDescriptor;

    // ensure that O_NONBLOCK is set on new connections.
    int flags = fcntl(socket, F_GETFL, 0);
    if (!(flags & O_NONBLOCK))
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);

    readNotifier = new QSocketNotifier(socket, QSocketNotifier::Read);
    QObject::connect(readNotifier, SIGNAL(activated(int)), this, SLOT(_q_readNotify()));
    connectWriteNotifier = new QSocketNotifier(socket, QSocketNotifier::Write, q);
    QObject::connect(connectWriteNotifier, SIGNAL(activated(int)), this, SLOT(_q_writeNotify()));

    q->setSocketState(socketState);
    q->setOpenMode(openMode);

    return true;
}

qint64 QBluetoothSocketPrivate::bytesAvailable() const
{
    return buffer.size();
}

QT_END_NAMESPACE
