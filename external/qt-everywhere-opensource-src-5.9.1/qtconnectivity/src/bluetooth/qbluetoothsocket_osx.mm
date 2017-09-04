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

#include "qbluetoothservicediscoveryagent.h"
// The order is important (the first header contains
// the base class for a private socket) - workaround for
// dependencies problem.
#include "qbluetoothsocket_p.h"
#include "qbluetoothsocket_osx_p.h"
//
#include "qbluetoothsocket_osx_p.h"
#include "qbluetoothlocaldevice.h"
#include "qbluetoothdeviceinfo.h"
#include "osx/osxbtutility_p.h"
#include "osx/uistrings_p.h"
#include "qbluetoothsocket.h"

#include <QtCore/qloggingcategory.h>
#include <QtCore/qmetaobject.h>

#include <algorithm>
#include <limits>

QT_BEGIN_NAMESPACE

QBluetoothSocketPrivate::QBluetoothSocketPrivate()
  : q_ptr(Q_NULLPTR),
    writeChunk(std::numeric_limits<UInt16>::max()),
    openMode(QIODevice::NotOpen), // That's what is set in public class' ctors.
    state(QBluetoothSocket::UnconnectedState),
    socketType(QBluetoothServiceInfo::UnknownProtocol),
    socketError(QBluetoothSocket::NoSocketError),
    isConnecting(false)
{
}

QBluetoothSocketPrivate::~QBluetoothSocketPrivate()
{
    // "Empty" dtor to make a shared pointer happy (parametrized with
    // incomplete type in the header file).
}

void QBluetoothSocketPrivate::connectToService(const QBluetoothAddress &address, quint16 port,
                                               QIODevice::OpenMode mode)
{
    Q_ASSERT_X(state == QBluetoothSocket::ServiceLookupState
               || state == QBluetoothSocket::UnconnectedState,
               Q_FUNC_INFO, "invalid state");

    socketError = QBluetoothSocket::NoSocketError;
    errorString.clear();
    buffer.clear();
    txBuffer.clear();

    IOReturn status = kIOReturnError;
    // Setting socket state on q_ptr will emit a signal,
    // we'd like to avoid any signal until this function completes.
    const QBluetoothSocket::SocketState oldState = state;
    // To prevent other connectToService calls (from QBluetoothSocket):
    // and also avoid signals in delegate callbacks.
    state = QBluetoothSocket::ConnectingState;
    // We're still inside this function:
    isConnecting = true;

    // We'll later (or now) have to set an open mode on QBluetoothSocket.
    openMode = mode;

    if (socketType == QBluetoothServiceInfo::RfcommProtocol) {
        rfcommChannel.reset([[ObjCRFCOMMChannel alloc] initWithDelegate:this]);
        if (rfcommChannel)
            status = [rfcommChannel connectAsyncToDevice:address withChannelID:port];
        else
            status = kIOReturnNoMemory;
    } else if (socketType == QBluetoothServiceInfo::L2capProtocol) {
        l2capChannel.reset([[ObjCL2CAPChannel alloc] initWithDelegate:this]);
        if (l2capChannel)
            status = [l2capChannel connectAsyncToDevice:address withPSM:port];
        else
            status = kIOReturnNoMemory;
    }

    // We're probably still connecting, but at least are leaving this function:
    isConnecting = false;

    // QBluetoothSocket will change the state and also emit
    // a signal later if required.
    if (status == kIOReturnSuccess && socketError == QBluetoothSocket::NoSocketError) {
        if (state == QBluetoothSocket::ConnectedState) {
            // Callback 'channelOpenComplete' fired before
            // connectToService finished:
            state = oldState;
            // Connected, setOpenMode on a QBluetoothSocket.
            q_ptr->setOpenMode(openMode);
            q_ptr->setSocketState(QBluetoothSocket::ConnectedState);
            emit q_ptr->connected();
            if (buffer.size()) // We also have some data already ...
                emit q_ptr->readyRead();
        } else if (state == QBluetoothSocket::UnconnectedState) {
            // Even if we have some data, we can not read it if
            // state != ConnectedState.
            buffer.clear();
            state = oldState;
            q_ptr->setSocketError(QBluetoothSocket::UnknownSocketError);
        } else {
            // No error and we're connecting ...
            state = oldState;
            q_ptr->setSocketState(QBluetoothSocket::ConnectingState);
        }
    } else {
        state = oldState;
        if (status != kIOReturnSuccess)
            errorString = OSXBluetooth::qt_error_string(status);
        q_ptr->setSocketError(QBluetoothSocket::UnknownSocketError);
    }
}

void QBluetoothSocketPrivate::close()
{
    // Can never be called while we're in connectToService:
    Q_ASSERT_X(!isConnecting, Q_FUNC_INFO, "internal inconsistency - "
               "still in connectToService()");

    if (!txBuffer.size())
        abort();
}

void QBluetoothSocketPrivate::abort()
{
    // Can never be called while we're in connectToService:
    Q_ASSERT_X(!isConnecting, Q_FUNC_INFO, "internal inconsistency - "
               "still in connectToService()");

    if (socketType == QBluetoothServiceInfo::RfcommProtocol)
        rfcommChannel.reset(nil);
    else if (socketType == QBluetoothServiceInfo::L2capProtocol)
        l2capChannel.reset(nil);
}

quint64 QBluetoothSocketPrivate::bytesAvailable() const
{
    return buffer.size();
}

QString QBluetoothSocketPrivate::peerName() const
{
    QT_BT_MAC_AUTORELEASEPOOL;

    NSString *nsName = nil;
    if (socketType == QBluetoothServiceInfo::RfcommProtocol) {
        if (rfcommChannel)
            nsName = [rfcommChannel peerName];
    } else if (socketType == QBluetoothServiceInfo::L2capProtocol) {
        if (l2capChannel)
            nsName = [l2capChannel peerName];
    }

    if (nsName)
        return QString::fromNSString(nsName);

    return QString();
}

QBluetoothAddress QBluetoothSocketPrivate::peerAddress() const
{
    BluetoothDeviceAddress addr = {};
    if (socketType == QBluetoothServiceInfo::RfcommProtocol) {
        if (rfcommChannel)
            addr = [rfcommChannel peerAddress];
    } else if (socketType == QBluetoothServiceInfo::L2capProtocol) {
        if (l2capChannel)
            addr = [l2capChannel peerAddress];
    }

    return OSXBluetooth::qt_address(&addr);
}

quint16 QBluetoothSocketPrivate::peerPort() const
{
    if (socketType == QBluetoothServiceInfo::RfcommProtocol) {
        if (rfcommChannel)
            return [rfcommChannel getChannelID];
    } else if (socketType == QBluetoothServiceInfo::L2capProtocol) {
        if (l2capChannel)
            return [l2capChannel getPSM];
    }

    return 0;
}

void QBluetoothSocketPrivate::_q_readNotify()
{
    // Noop.
}

void QBluetoothSocketPrivate::_q_writeNotify()
{
    Q_ASSERT_X(socketType == QBluetoothServiceInfo::L2capProtocol
               || socketType == QBluetoothServiceInfo::RfcommProtocol,
               Q_FUNC_INFO, "invalid socket type");
    Q_ASSERT_X(l2capChannel || rfcommChannel, Q_FUNC_INFO,
               "invalid socket (no open channel)");
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");

    if (txBuffer.size()) {
        const bool isL2CAP = socketType == QBluetoothServiceInfo::L2capProtocol;
        writeChunk.resize(isL2CAP ? std::numeric_limits<UInt16>::max() :
                          [rfcommChannel getMTU]);

        const int size = txBuffer.read(writeChunk.data(), writeChunk.size());
        IOReturn status = kIOReturnError;
        if (!isL2CAP)
            status = [rfcommChannel writeAsync:writeChunk.data() length:UInt16(size)];
        else
            status = [l2capChannel writeAsync:writeChunk.data() length:UInt16(size)];

        if (status != kIOReturnSuccess) {
            errorString = QCoreApplication::translate(SOCKET, SOC_NETWORK_ERROR);
            q_ptr->setSocketError(QBluetoothSocket::NetworkError);
            return;
        } else {
            emit q_ptr->bytesWritten(size);
        }
    }

    if (!txBuffer.size() && state == QBluetoothSocket::ClosingState)
        close();
}

bool QBluetoothSocketPrivate::setChannel(IOBluetoothRFCOMMChannel *channel)
{
    // A special case "constructor": on OS X we do not have a real listening socket,
    // instead a bluetooth server "listens" for channel open notifications and
    // creates (if asked by a user later) a "socket" object
    // for this connection. This function initializes
    // a "socket" from such an external channel (reported by a notification).

    // It must be a newborn socket!
    Q_ASSERT_X(socketError == QBluetoothSocket::NoSocketError
               && state == QBluetoothSocket::UnconnectedState && !rfcommChannel && !l2capChannel,
               Q_FUNC_INFO, "unexpected socket state");

    openMode = QIODevice::ReadWrite;
    rfcommChannel.reset([[ObjCRFCOMMChannel alloc] initWithDelegate:this channel:channel]);
    if (rfcommChannel) {// We do not handle errors, up to an external user.
        q_ptr->setOpenMode(QIODevice::ReadWrite);
        state = QBluetoothSocket::ConnectedState;
        socketType = QBluetoothServiceInfo::RfcommProtocol;
    }

    return rfcommChannel;
}

bool QBluetoothSocketPrivate::setChannel(IOBluetoothL2CAPChannel *channel)
{
    // A special case "constructor": on OS X we do not have a real listening socket,
    // instead a bluetooth server "listens" for channel open notifications and
    // creates (if asked by a user later) a "socket" object
    // for this connection. This function initializes
    // a "socket" from such an external channel (reported by a notification).

    // It must be a newborn socket!
    Q_ASSERT_X(socketError == QBluetoothSocket::NoSocketError
               && state == QBluetoothSocket::UnconnectedState && !l2capChannel && !rfcommChannel,
               Q_FUNC_INFO, "unexpected socket state");

    openMode = QIODevice::ReadWrite;
    l2capChannel.reset([[ObjCL2CAPChannel alloc] initWithDelegate:this channel:channel]);
    if (l2capChannel) {// We do not handle errors, up to an external user.
        q_ptr->setOpenMode(QIODevice::ReadWrite);
        state = QBluetoothSocket::ConnectedState;
        socketType = QBluetoothServiceInfo::L2capProtocol;
    }

    return l2capChannel;
}


void QBluetoothSocketPrivate::setChannelError(IOReturn errorCode)
{
    Q_UNUSED(errorCode)

    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");

    if (isConnecting) {
        // The delegate's method was called while we are still in
        // connectToService ... will emit a moment later.
        socketError = QBluetoothSocket::UnknownSocketError;
    } else {
        q_ptr->setSocketError(QBluetoothSocket::UnknownSocketError);
    }
}

void QBluetoothSocketPrivate::channelOpenComplete()
{
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");

    if (!isConnecting) {
        q_ptr->setSocketState(QBluetoothSocket::ConnectedState);
        q_ptr->setOpenMode(openMode);
        emit q_ptr->connected();
    } else {
        state = QBluetoothSocket::ConnectedState;
        // We are still in connectToService, it'll care
        // about signals!
    }
}

void QBluetoothSocketPrivate::channelClosed()
{
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");

    // Channel was closed by IOBluetooth and we can not write any data
    // (thus close/abort probably will not work).

    if (!isConnecting) {
        q_ptr->setSocketState(QBluetoothSocket::UnconnectedState);
        q_ptr->setOpenMode(QIODevice::NotOpen);
        emit q_ptr->disconnected();
    } else {
        state = QBluetoothSocket::UnconnectedState;
        // We are still in connectToService and do not want
        // to emit any signals yet.
    }
}

void QBluetoothSocketPrivate::readChannelData(void *data, std::size_t size)
{
    Q_ASSERT_X(data, Q_FUNC_INFO, "invalid data (null)");
    Q_ASSERT_X(size, Q_FUNC_INFO, "invalid data size (0)");
    Q_ASSERT_X(q_ptr, Q_FUNC_INFO, "invalid q_ptr (null)");

    const char *src = static_cast<char *>(data);
    char *dst = buffer.reserve(size);
    std::copy(src, src + size, dst);

    if (!isConnecting) {
        // If we're still in connectToService, do not emit.
        emit q_ptr->readyRead();
    }   // else connectToService must check and emit readyRead!
}

void QBluetoothSocketPrivate::writeComplete()
{
    _q_writeNotify();
}

qint64 QBluetoothSocketPrivate::writeData(const char *data, qint64 maxSize)
{
    Q_ASSERT_X(data, Q_FUNC_INFO, "invalid data (null)");
    Q_ASSERT_X(maxSize > 0, Q_FUNC_INFO, "invalid data size");

    if (state != QBluetoothSocket::ConnectedState) {
        errorString = QCoreApplication::translate(SOCKET, SOC_NOWRITE);
        q_ptr->setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    // We do not have a real socket API under the hood,
    // IOBluetoothL2CAPChannel buffered (writeAsync).

    if (!txBuffer.size())
        QMetaObject::invokeMethod(this, "_q_writeNotify", Qt::QueuedConnection);

    char *dst = txBuffer.reserve(maxSize);
    std::copy(data, data + maxSize, dst);

    return maxSize;
}

QBluetoothSocket::QBluetoothSocket(QBluetoothServiceInfo::Protocol socketType, QObject *parent)
  : QIODevice(parent),
    d_ptr(new QBluetoothSocketPrivate)
{
    d_ptr->q_ptr = this;
    d_ptr->socketType = socketType;

    setOpenMode(NotOpen);
}

QBluetoothSocket::QBluetoothSocket(QObject *parent)
  : QIODevice(parent),
    d_ptr(new QBluetoothSocketPrivate)
{
    d_ptr->q_ptr = this;
    setOpenMode(NotOpen);
}

QBluetoothSocket::~QBluetoothSocket()
{
    delete d_ptr;
}

bool QBluetoothSocket::isSequential() const
{
    return true;
}

qint64 QBluetoothSocket::bytesAvailable() const
{
    return QIODevice::bytesAvailable() + d_ptr->bytesAvailable();
}

qint64 QBluetoothSocket::bytesToWrite() const
{
    return d_ptr->txBuffer.size();
}

void QBluetoothSocket::connectToService(const QBluetoothServiceInfo &service, OpenMode openMode)
{
    // Report this problem early, potentially avoid device discovery:
    if (socketType() == QBluetoothServiceInfo::UnknownProtocol) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "cannot connect with 'UnknownProtocol' type";
        d_ptr->errorString = QCoreApplication::translate(SOCKET, SOC_NETWORK_ERROR);
        setSocketError(QBluetoothSocket::UnsupportedProtocolError);
        return;
    }

    if (state() != UnconnectedState && state() != ServiceLookupState) {
        qCWarning(QT_BT_OSX)  << "called on a busy socket";
        d_ptr->errorString = QCoreApplication::translate(SOCKET, SOC_CONNECT_IN_PROGRESS);
        setSocketError(OperationError);
        return;
    }

    if (service.protocolServiceMultiplexer() > 0) {
        d_ptr->connectToService(service.device().address(),
                                service.protocolServiceMultiplexer(),
                                openMode);
    } else if (service.serverChannel() > 0) {
        d_ptr->connectToService(service.device().address(),
                                service.serverChannel(), openMode);
    } else {
        // Try service discovery.
        if (service.serviceUuid().isNull()) {
            qCWarning(QT_BT_OSX) << "No port, no PSM, and no "
                                    "UUID provided, unable to connect";
            return;
        }

        doDeviceDiscovery(service, openMode);
    }
}

void QBluetoothSocket::connectToService(const QBluetoothAddress &address, const QBluetoothUuid &uuid,
                                        OpenMode openMode)
{
    // Report this problem early, avoid device discovery:
    if (socketType() == QBluetoothServiceInfo::UnknownProtocol) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "cannot connect with 'UnknownProtocol' type";
        d_ptr->errorString = QCoreApplication::translate(SOCKET, SOC_NETWORK_ERROR);
        setSocketError(QBluetoothSocket::UnsupportedProtocolError);
        return;
    }

    if (state() != QBluetoothSocket::UnconnectedState) {
        qCWarning(QT_BT_OSX) << "called on a busy socket";
        d_ptr->errorString = QCoreApplication::translate(SOCKET, SOC_CONNECT_IN_PROGRESS);
        setSocketError(QBluetoothSocket::OperationError);
        return;
    }

    QBluetoothDeviceInfo device(address, QString(), QBluetoothDeviceInfo::MiscellaneousDevice);
    QBluetoothServiceInfo service;
    service.setDevice(device);
    service.setServiceUuid(uuid);
    doDeviceDiscovery(service, openMode);
}

void QBluetoothSocket::connectToService(const QBluetoothAddress &address, quint16 port,
                                        OpenMode openMode)
{
    if (socketType() == QBluetoothServiceInfo::UnknownProtocol) {
        qCWarning(QT_BT_OSX) << Q_FUNC_INFO << "cannot connect with 'UnknownProtocol' type";
        d_ptr->errorString = QCoreApplication::translate(SOCKET, SOC_NETWORK_ERROR);
        setSocketError(QBluetoothSocket::UnsupportedProtocolError);
        return;
    }

    if (state() != QBluetoothSocket::UnconnectedState) {
        qCWarning(QT_BT_OSX) << "called on a busy socket";
        d_ptr->errorString = QCoreApplication::translate(SOCKET, SOC_CONNECT_IN_PROGRESS);
        setSocketError(OperationError);
        return;
    }

    setOpenMode(openMode);
    d_ptr->connectToService(address, port, openMode);
}

QBluetoothServiceInfo::Protocol QBluetoothSocket::socketType() const
{
    return d_ptr->socketType;
}

QBluetoothSocket::SocketState QBluetoothSocket::state() const
{
    return d_ptr->state;
}

QBluetoothSocket::SocketError QBluetoothSocket::error() const
{
    return d_ptr->socketError;
}

QString QBluetoothSocket::errorString() const
{
    return d_ptr->errorString;
}

void QBluetoothSocket::setSocketState(QBluetoothSocket::SocketState state)
{
    const SocketState oldState = d_ptr->state;
    d_ptr->state = state;
    if (oldState != d_ptr->state)
        emit stateChanged(state);

    if (state == ListeningState) {
        // We can register for L2CAP/RFCOMM open notifications,
        // that's different from 'listen' and is implemented
        // in QBluetoothServer.
        qCWarning(QT_BT_OSX) << "listening sockets are not supported";
    }
}

bool QBluetoothSocket::canReadLine() const
{
    return d_ptr->buffer.canReadLine() || QIODevice::canReadLine();
}

void QBluetoothSocket::setSocketError(QBluetoothSocket::SocketError socketError)
{
    d_ptr->socketError = socketError;
    emit error(socketError);
}

void QBluetoothSocket::doDeviceDiscovery(const QBluetoothServiceInfo &service, OpenMode openMode)
{
    setSocketState(ServiceLookupState);

    if (d_ptr->discoveryAgent)
        d_ptr->discoveryAgent->stop();

    d_ptr->discoveryAgent.reset(new QBluetoothServiceDiscoveryAgent(this));
    d_ptr->discoveryAgent->setRemoteAddress(service.device().address());

    connect(d_ptr->discoveryAgent.data(), SIGNAL(serviceDiscovered(QBluetoothServiceInfo)),
            this, SLOT(serviceDiscovered(QBluetoothServiceInfo)));
    connect(d_ptr->discoveryAgent.data(), SIGNAL(finished()),
            this, SLOT(discoveryFinished()));

    d_ptr->openMode = openMode;

    if (!service.serviceUuid().isNull())
        d_ptr->discoveryAgent->setUuidFilter(service.serviceUuid());

    if (!service.serviceClassUuids().isEmpty())
        d_ptr->discoveryAgent->setUuidFilter(service.serviceClassUuids());

    Q_ASSERT_X(!d_ptr->discoveryAgent->uuidFilter().isEmpty(), Q_FUNC_INFO,
               "invalid service info");

    d_ptr->discoveryAgent->start(QBluetoothServiceDiscoveryAgent::FullDiscovery);
}

void QBluetoothSocket::serviceDiscovered(const QBluetoothServiceInfo &service)
{
    if (service.protocolServiceMultiplexer() != 0 || service.serverChannel() != 0) {
        d_ptr->discoveryAgent->stop();
        connectToService(service, d_ptr->openMode);
    }
}

void QBluetoothSocket::discoveryFinished()
{
    d_ptr->errorString = QCoreApplication::translate(SOCKET, SOC_SERVICE_NOT_FOUND);
    setSocketState(UnconnectedState);
    setSocketError(ServiceNotFoundError);
}

void QBluetoothSocket::abort()
{
    if (state() == UnconnectedState)
        return;

    setOpenMode(NotOpen);

    if (state() == ServiceLookupState && d_ptr->discoveryAgent) {
        d_ptr->discoveryAgent->disconnect();
        d_ptr->discoveryAgent->stop();
        d_ptr->discoveryAgent.reset();
    }

    setSocketState(QBluetoothSocket::ClosingState);
    d_ptr->abort();

    setSocketState(QBluetoothSocket::UnconnectedState);
    emit disconnected();
}

void QBluetoothSocket::disconnectFromService()
{
    close();
}

QString QBluetoothSocket::localName() const
{
    const QBluetoothLocalDevice device;
    return device.name();
}

QBluetoothAddress QBluetoothSocket::localAddress() const
{
    const QBluetoothLocalDevice device;
    return device.address();
}

quint16 QBluetoothSocket::localPort() const
{
    return 0;
}

QString QBluetoothSocket::peerName() const
{
    return d_ptr->peerName();
}

QBluetoothAddress QBluetoothSocket::peerAddress() const
{
    return d_ptr->peerAddress();
}

quint16 QBluetoothSocket::peerPort() const
{
    return d_ptr->peerPort();
}

qint64 QBluetoothSocket::writeData(const char *data, qint64 maxSize)
{
    if (!data || maxSize <= 0) {
        d_ptr->errorString = QCoreApplication::translate(SOCKET, SOC_INVAL_DATASIZE);
        setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    return d_ptr->writeData(data, maxSize);
}

qint64 QBluetoothSocketPrivate::readData(char *data, qint64 maxSize)
{
    if (state != QBluetoothSocket::ConnectedState) {
        errorString = QCoreApplication::translate(SOCKET, SOC_NOREAD);
        q_ptr->setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    if (!buffer.isEmpty())
        return buffer.read(data, maxSize);

    return 0;
}

qint64 QBluetoothSocket::readData(char *data, qint64 maxSize)
{
    return d_ptr->readData(data, maxSize);
}

void QBluetoothSocket::close()
{
    if (state() == UnconnectedState)
        return;

    setOpenMode(NotOpen);

    if (state() == ServiceLookupState && d_ptr->discoveryAgent) {
        d_ptr->discoveryAgent->disconnect();
        d_ptr->discoveryAgent->stop();
        d_ptr->discoveryAgent.reset();
    }

    setSocketState(ClosingState);

    d_ptr->close();

    setSocketState(UnconnectedState);
    emit disconnected();
}

bool QBluetoothSocket::setSocketDescriptor(int socketDescriptor, QBluetoothServiceInfo::Protocol socketType,
                                           SocketState socketState, OpenMode openMode)
{
    Q_UNUSED(socketDescriptor)
    Q_UNUSED(socketType)
    Q_UNUSED(socketState)
    Q_UNUSED(openMode)

    // Noop on OS X.
    return true;
}

int QBluetoothSocket::socketDescriptor() const
{
    return -1;
}

/* not supported on OS X */
void QBluetoothSocket::setPreferredSecurityFlags(QBluetooth::SecurityFlags flags)
{
    Q_UNUSED(flags)
}

/* not supported on OS X - platform always uses encryption */
QBluetooth::SecurityFlags QBluetoothSocket::preferredSecurityFlags() const
{
    return QBluetooth::Secure;
}

#ifndef QT_NO_DEBUG_STREAM

QDebug operator<<(QDebug debug, QBluetoothSocket::SocketError error)
{
    switch (error) {
    case QBluetoothSocket::UnknownSocketError:
        debug << "QBluetoothSocket::UnknownSocketError";
        break;
    case QBluetoothSocket::HostNotFoundError:
        debug << "QBluetoothSocket::HostNotFoundError";
        break;
    case QBluetoothSocket::ServiceNotFoundError:
        debug << "QBluetoothSocket::ServiceNotFoundError";
        break;
    case QBluetoothSocket::NetworkError:
        debug << "QBluetoothSocket::NetworkError";
        break;
    default:
        debug << "QBluetoothSocket::SocketError(" << (int)error << ")";
    }
    return debug;
}

QDebug operator<<(QDebug debug, QBluetoothSocket::SocketState state)
{
    switch (state) {
    case QBluetoothSocket::UnconnectedState:
        debug << "QBluetoothSocket::UnconnectedState";
        break;
    case QBluetoothSocket::ConnectingState:
        debug << "QBluetoothSocket::ConnectingState";
        break;
    case QBluetoothSocket::ConnectedState:
        debug << "QBluetoothSocket::ConnectedState";
        break;
    case QBluetoothSocket::BoundState:
        debug << "QBluetoothSocket::BoundState";
        break;
    case QBluetoothSocket::ClosingState:
        debug << "QBluetoothSocket::ClosingState";
        break;
    case QBluetoothSocket::ListeningState:
        debug << "QBluetoothSocket::ListeningState";
        break;
    case QBluetoothSocket::ServiceLookupState:
        debug << "QBluetoothSocket::ServiceLookupState";
        break;
    default:
        debug << "QBluetoothSocket::SocketState(" << (int)state << ")";
    }
    return debug;
}

#endif // QT_NO_DEBUG_STREAM

#include "moc_qbluetoothsocket.cpp"

QT_END_NAMESPACE
