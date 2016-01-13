/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Copyright (C) 2013 BlackBerry Limited. All rights reserved.
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

#include "qbluetoothsocket.h"
#include "qbluetoothsocket_p.h"

#include "qbluetoothdeviceinfo.h"
#include "qbluetoothserviceinfo.h"
#include "qbluetoothservicediscoveryagent.h"


#include <QtCore/QLoggingCategory>
#include <QSocketNotifier>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_BT)
Q_DECLARE_LOGGING_CATEGORY(QT_BT_QNX)

/*!
    \class QBluetoothSocket
    \inmodule QtBluetooth
    \brief The QBluetoothSocket class enables connection to a Bluetooth device
    running a bluetooth server.

    \since 5.2

    QBluetoothSocket supports two socket types, \l {QBluetoothServiceInfo::L2capProtocol}{L2CAP} and
    \l {QBluetoothServiceInfo::RfcommProtocol}{RFCOMM}.

    \l {QBluetoothServiceInfo::L2capProtocol}{L2CAP} is a low level datagram-oriented Bluetooth socket.
    Android and BlackBerry do not support \l {QBluetoothServiceInfo::L2capProtocol}{L2CAP} for socket
    connections.

    \l {QBluetoothServiceInfo::RfcommProtocol}{RFCOMM} is a reliable, stream-oriented socket. RFCOMM
    sockets emulate an RS-232 serial port.

    To create a connection to a Bluetooth service, create a socket of the appropriate type and call
    connectToService() passing the Bluetooth address and port number. QBluetoothSocket will emit
    the connected() signal when the connection is established.

    If the \l {QBluetoothServiceInfo::Protocol}{Protocol} is not supported on a platform, calling
    \l connectToService() will emit a \l {QBluetoothSocket::UnsupportedProtocolError}{UnsupportedProtocolError} error.

*/

/*!
    \enum QBluetoothSocket::SocketState

    This enum describes the state of the Bluetooth socket.

    \value UnconnectedState     Socket is not connected.
    \value ServiceLookupState   Socket is querying connection parameters.
    \value ConnectingState      Socket is attempting to connect to a device.
    \value ConnectedState       Socket is connected to a device.
    \value BoundState           Socket is bound to a local address and port.
    \value ClosingState         Socket is connected and will be closed once all pending data is
    written to the socket.
    \value ListeningState       Socket is listening for incoming connections.
*/

/*!
    \enum QBluetoothSocket::SocketError

    This enum describes Bluetooth socket error types.

    \value UnknownSocketError       An unknown error has occurred.
    \value NoSocketError            No error. Used for testing.
    \value HostNotFoundError        Could not find the remote host.
    \value ServiceNotFoundError     Could not find the service UUID on remote host.
    \value NetworkError             Attempt to read or write from socket returned an error
    \value UnsupportedProtocolError The \l {QBluetoothServiceInfo::Protocol}{Protocol} is not
                                    supported on this platform.
    \value OperationError           An operation was attempted while the socket was in a state
                                    that did not permit it.
*/

/*!
    \fn void QBluetoothSocket::connected()

    This signal is emitted when a connection is established.

    \sa QBluetoothSocket::ConnectedState, stateChanged()
*/

/*!
    \fn void QBluetoothSocket::disconnected()

    This signal is emitted when the socket is disconnected.

    \sa QBluetoothSocket::UnconnectedState, stateChanged()
*/

/*!
    \fn void QBluetoothSocket::error(QBluetoothSocket::SocketError error)

    This signal is emitted when an \a error occurs.

    \sa error()
*/

/*!
    \fn QBluetoothSocket::stateChanged(QBluetoothSocket::SocketState state)

    This signal is emitted when the socket state changes to \a state.

    \sa connected(), disconnected(), state(), QBluetoothSocket::SocketState
*/

/*!
    \fn void QBluetoothSocket::abort()

    Aborts the current connection and resets the socket. Unlike disconnectFromService(), this
    function immediately closes the socket, discarding any pending data in the write buffer.

    \note On Android, aborting the socket requires asynchronous interaction with Android threads.
    Therefore the associated \l disconnected() and \l stateChanged() signals are delayed
    until the threads have finished the closure.

    \sa disconnectFromService(), close()
*/

/*!
    \fn void QBluetoothSocket::close()

    Disconnects the socket's connection with the device.

    \note On Android, closing the socket requires asynchronous interaction with Android threads.
    Therefore the associated \l disconnected() and \l stateChanged() signals are delayed
    until the threads have finished the closure.

*/

/*!
    \fn void QBluetoothSocket::disconnectFromService()

    Attempts to close the socket. If there is pending data waiting to be written QBluetoothSocket
    will enter ClosingState and wait until all data has been written. Eventually, it will enter
    UnconnectedState and emit the disconnected() signal.

    \sa connectToService()
*/

/*!
    \fn QString QBluetoothSocket::localName() const

    Returns the name of the local device.

    Although some platforms may differ the socket must generally be connected to guarantee
    the return of a valid name. In particular, this is true when dealing with platforms
    that support multiple local Bluetooth adapters.
*/

/*!
    \fn QBluetoothAddress QBluetoothSocket::localAddress() const

    Returns the address of the local device.

    Although some platforms may differ the socket must generally be connected to guarantee
    the return of a valid address. In particular, this is true when dealing with platforms
    that support multiple local Bluetooth adapters.
*/

/*!
    \fn quint16 QBluetoothSocket::localPort() const

    Returns the port number of the local socket if available, otherwise returns 0.
    Although some platforms may differ the socket must generally be connected to guarantee
    the return of a valid port number.

    On BlackBerry and Android, this feature is not supported and returns 0.
*/

/*!
    \fn QString QBluetoothSocket::peerName() const

    Returns the name of the peer device.
*/

/*!
    \fn QBluetoothAddress QBluetoothSocket::peerAddress() const

    Returns the address of the peer device.
*/

/*!
    \fn quint16 QBluetoothSocket::peerPort() const

    Return the port number of the peer socket if available, otherwise returns 0.
    On BlackBerry and Android, this feature is not supported.
*/

/*!
    \fn qint64 QBluetoothSocket::readData(char *data, qint64 maxSize)

    \reimp
*/

/*!
    \fn qint64 QBluetoothSocket::writeData(const char *data, qint64 maxSize)

    \reimp
*/

/*!
    Constructs a Bluetooth socket of \a socketType type, with \a parent.
*/
QBluetoothSocket::QBluetoothSocket(QBluetoothServiceInfo::Protocol socketType, QObject *parent)
: QIODevice(parent), d_ptr(new QBluetoothSocketPrivate)
{
    d_ptr->q_ptr = this;

    Q_D(QBluetoothSocket);
    d->ensureNativeSocket(socketType);

    setOpenMode(QIODevice::NotOpen);
}

/*!
    Constructs a Bluetooth socket with \a parent.
*/
QBluetoothSocket::QBluetoothSocket(QObject *parent)
  : QIODevice(parent), d_ptr(new QBluetoothSocketPrivate)
{
    d_ptr->q_ptr = this;
    setOpenMode(QIODevice::NotOpen);
}

/*!
    Destroys the Bluetooth socket.
*/
QBluetoothSocket::~QBluetoothSocket()
{
    delete d_ptr;
    d_ptr = 0;
}

/*!
    \reimp
*/
bool QBluetoothSocket::isSequential() const
{
    return true;
}

/*!
    Returns the number of incoming bytes that are waiting to be read.

    \sa bytesToWrite(), read()
*/
qint64 QBluetoothSocket::bytesAvailable() const
{
    Q_D(const QBluetoothSocket);
    return QIODevice::bytesAvailable() + d->bytesAvailable();
}

/*!
    Returns the number of bytes that are waiting to be written. The bytes are written when control
    goes back to the event loop.
*/
qint64 QBluetoothSocket::bytesToWrite() const
{
    Q_D(const QBluetoothSocket);
    return d->txBuffer.size();
}

/*!
    Attempts to connect to the service described by \a service.

    The socket is opened in the given \a openMode. The \l socketType() may change
    depending on the protocol required by \a service.

    The socket first enters ConnectingState and attempts to connect to the device providing
    \a service. If a connection is established, QBluetoothSocket enters ConnectedState and
    emits connected().

    At any point, the socket can emit error() to signal that an error occurred.

    Note that most platforms require a pairing prior to connecting to the remote device. Otherwise
    the connection process may fail.

    \sa state(), disconnectFromService()
*/
void QBluetoothSocket::connectToService(const QBluetoothServiceInfo &service, OpenMode openMode)
{
    Q_D(QBluetoothSocket);

    if (state() != QBluetoothSocket::UnconnectedState && state() != QBluetoothSocket::ServiceLookupState) {
        qCWarning(QT_BT)  << "QBluetoothSocket::connectToService called on busy socket";
        d->errorString = QBluetoothSocket::tr("Trying to connect while connection is in progress");
        setSocketError(QBluetoothSocket::OperationError);
        return;
    }
#if defined(QT_QNX_BLUETOOTH) || defined(QT_ANDROID_BLUETOOTH)
    if (!d->ensureNativeSocket(service.socketProtocol())) {
        d->errorString = tr("Socket type not supported");
        setSocketError(QBluetoothSocket::UnsupportedProtocolError);
        return;
    }
    d->connectToService(service.device().address(), service.serviceUuid(), openMode);
#else
    if (service.protocolServiceMultiplexer() > 0) {
        if (!d->ensureNativeSocket(QBluetoothServiceInfo::L2capProtocol)) {
            d->errorString = tr("Unknown socket error");
            setSocketError(UnknownSocketError);
            return;
        }
        d->connectToService(service.device().address(), service.protocolServiceMultiplexer(), openMode);
    } else if (service.serverChannel() > 0) {
        if (!d->ensureNativeSocket(QBluetoothServiceInfo::RfcommProtocol)) {
            d->errorString = tr("Unknown socket error");
            setSocketError(UnknownSocketError);
            return;
        }
        d->connectToService(service.device().address(), service.serverChannel(), openMode);
    } else {
        // try doing service discovery to see if we can find the socket
        if(service.serviceUuid().isNull()){
            qCWarning(QT_BT) << "No port, no PSM, and no UUID provided, unable to connect";
            return;
        }
        qCDebug(QT_BT) << "Need a port/psm, doing discovery";
        doDeviceDiscovery(service, openMode);
    }
#endif
}

/*!
    Attempts to make a connection to the service identified by \a uuid on the device with address
    \a address.

    The socket is opened in the given \a openMode.

    For BlueZ, the socket first enters the \l ServiceLookupState and queries the connection parameters for
    \a uuid. If the service parameters are successfully retrieved the socket enters
    ConnectingState, and attempts to connect to \a address. If a connection is established,
    QBluetoothSocket enters Connected State and emits connected().

    On BlackBerry and Android, the service connection can directly be established
    using the UUID of the remote service. Therefore these platforms do not require
    the \l ServiceLookupState and \l socketType() is always set to
    \l QBluetoothServiceInfo::RfcommProtocol.

    At any point, the socket can emit error() to signal that an error occurred.

    Note that most platforms require a pairing prior to connecting to the remote device. Otherwise
    the connection process may fail.

    \sa state(), disconnectFromService()
*/
void QBluetoothSocket::connectToService(const QBluetoothAddress &address, const QBluetoothUuid &uuid, OpenMode openMode)
{
    Q_D(QBluetoothSocket);

    if (state() != QBluetoothSocket::UnconnectedState) {
        qCWarning(QT_BT)  << "QBluetoothSocket::connectToService called on busy socket";
        d->errorString = QBluetoothSocket::tr("Trying to connect while connection is in progress");
        setSocketError(QBluetoothSocket::OperationError);
        return;
    }

#if defined(QT_QNX_BLUETOOTH) || defined(QT_ANDROID_BLUETOOTH)
    if (!d->ensureNativeSocket(QBluetoothServiceInfo::RfcommProtocol)) {
        d->errorString = tr("Socket type not supported");
        setSocketError(QBluetoothSocket::UnsupportedProtocolError);
        return;
    }
    d->connectToService(address, uuid, openMode);
#else
    QBluetoothServiceInfo service;
    QBluetoothDeviceInfo device(address, QString(), QBluetoothDeviceInfo::MiscellaneousDevice);
    service.setDevice(device);
    service.setServiceUuid(uuid);
    doDeviceDiscovery(service, openMode);
#endif
}

/*!
    Attempts to make a connection with \a address on the given \a port.

    The socket is opened in the given \a openMode.

    The socket first enters ConnectingState, and attempts to connect to \a address. If a
    connection is established, QBluetoothSocket enters ConnectedState and emits connected().

    At any point, the socket can emit error() to signal that an error occurred.

    On BlackBerry and Android, a connection to a service can not be established using a port. Calling this function
    will emit a \l {QBluetoothSocket::ServiceNotFoundError}{ServiceNotFoundError}

    Note that most platforms require a pairing prior to connecting to the remote device. Otherwise
    the connection process may fail.

    \sa state(), disconnectFromService()
*/
void QBluetoothSocket::connectToService(const QBluetoothAddress &address, quint16 port, OpenMode openMode)
{
    Q_D(QBluetoothSocket);
#if defined(QT_QNX_BLUETOOTH) || defined(QT_ANDROID_BLUETOOTH)
    Q_UNUSED(port);
    Q_UNUSED(openMode);
    Q_UNUSED(address);
    d->errorString = tr("Connecting to port is not supported");
    setSocketError(QBluetoothSocket::ServiceNotFoundError);
    qCWarning(QT_BT) << "Connecting to port is not supported";
#else
    if (state() != QBluetoothSocket::UnconnectedState) {
        qCWarning(QT_BT)  << "QBluetoothSocket::connectToService called on busy socket";
        d->errorString = QBluetoothSocket::tr("Trying to connect while connection is in progress");
        setSocketError(QBluetoothSocket::OperationError);
        return;
    }

    setOpenMode(openMode);
    d->connectToService(address, port, openMode);
#endif
}

/*!
    Returns the socket type. The socket automatically adjusts to the protocol
    offered by the remote service.

    Blackberry and Android only support \l{QBluetoothServiceInfo::RfcommProtocol}{RFCOMM}
    based sockets.
*/
QBluetoothServiceInfo::Protocol QBluetoothSocket::socketType() const
{
    Q_D(const QBluetoothSocket);
    return d->socketType;
}

/*!
    Returns the current state of the socket.
*/
QBluetoothSocket::SocketState QBluetoothSocket::state() const
{
    Q_D(const QBluetoothSocket);
    return d->state;
}

/*!
    Returns the last error.
*/
QBluetoothSocket::SocketError QBluetoothSocket::error() const
{
    Q_D(const QBluetoothSocket);
    return d->socketError;
}

/*!
    Returns a user displayable text string for the error.
 */
QString QBluetoothSocket::errorString() const
{
    Q_D(const QBluetoothSocket);
    return d->errorString;
}

/*!
    Sets the socket state to \a state.
*/
void QBluetoothSocket::setSocketState(QBluetoothSocket::SocketState state)
{
    Q_D(QBluetoothSocket);
    SocketState old = d->state;
    d->state = state;
    if(old != d->state)
        emit stateChanged(state);
    if(state == ListeningState){
        // TODO: look at this, is this really correct?
        // if we're a listening socket we can't handle connects?
        if (d->readNotifier) {
            d->readNotifier->setEnabled(false);
        }
    }
}

/*!
    Returns true if you can read at least one line from the device
 */

bool QBluetoothSocket::canReadLine() const
{
    Q_D(const QBluetoothSocket);
    return d->buffer.canReadLine() || QIODevice::canReadLine();
}

/*!
    Sets the type of error that last occurred to \a error_.
*/
void QBluetoothSocket::setSocketError(QBluetoothSocket::SocketError error_)
{
    Q_D(QBluetoothSocket);
    d->socketError = error_;
    emit error(error_);
}

/*!
  Start device discovery for \a service and open the socket with \a openMode.  If the socket
  is created with a service uuid device address, use service discovery to find the
  port number to connect to.
*/

void QBluetoothSocket::doDeviceDiscovery(const QBluetoothServiceInfo &service, OpenMode openMode)
{
    Q_D(QBluetoothSocket);

    setSocketState(QBluetoothSocket::ServiceLookupState);
    qCDebug(QT_BT) << "Starting discovery";

    if(d->discoveryAgent) {
        d->discoveryAgent->stop();
        delete d->discoveryAgent;
    }

    d->discoveryAgent = new QBluetoothServiceDiscoveryAgent(this);
    d->discoveryAgent->setRemoteAddress(service.device().address());

    //qDebug() << "Got agent";

    connect(d->discoveryAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)), this, SLOT(serviceDiscovered(QBluetoothServiceInfo)));
    connect(d->discoveryAgent, SIGNAL(finished()), this, SLOT(discoveryFinished()));

    d->openMode = openMode;

    if(!service.serviceUuid().isNull())
        d->discoveryAgent->setUuidFilter(service.serviceUuid());

    if(!service.serviceClassUuids().isEmpty())
        d->discoveryAgent->setUuidFilter(service.serviceClassUuids());

    // we have to ID the service somehow
    Q_ASSERT(!d->discoveryAgent->uuidFilter().isEmpty());

    qCDebug(QT_BT) << "UUID filter" << d->discoveryAgent->uuidFilter();

    d->discoveryAgent->start(QBluetoothServiceDiscoveryAgent::FullDiscovery);
}

void QBluetoothSocket::serviceDiscovered(const QBluetoothServiceInfo &service)
{
    Q_D(QBluetoothSocket);
    qCDebug(QT_BT) << "FOUND SERVICE!" << service;
    if (service.protocolServiceMultiplexer() > 0 || service.serverChannel() > 0) {
        connectToService(service, d->openMode);
        d->discoveryAgent->deleteLater();
        d->discoveryAgent = 0;
    } else {
        qCDebug(QT_BT) << "Could not find port/psm for potential remote service";
    }
}

void QBluetoothSocket::discoveryFinished()
{
    qCDebug(QT_BT) << "Socket discovery finished";
    Q_D(QBluetoothSocket);
    if (d->discoveryAgent){
        qCDebug(QT_BT) << "Didn't find any";
        d->errorString = tr("Service cannot be found");
        setSocketError(ServiceNotFoundError);
        setSocketState(QBluetoothSocket::UnconnectedState);
        d->discoveryAgent->deleteLater();
        d->discoveryAgent = 0;
    }
}

void QBluetoothSocket::abort()
{
    if (state() == UnconnectedState)
        return;

    Q_D(QBluetoothSocket);
    setOpenMode(QIODevice::NotOpen);
    setSocketState(ClosingState);
    d->abort();

#ifndef QT_ANDROID_BLUETOOTH
    //Android closes when the Java event loop comes around
    setSocketState(QBluetoothSocket::UnconnectedState);
    emit disconnected();
#endif
}

void QBluetoothSocket::disconnectFromService()
{
    close();
}

QString QBluetoothSocket::localName() const
{
    Q_D(const QBluetoothSocket);
    return d->localName();
}

QBluetoothAddress QBluetoothSocket::localAddress() const
{
    Q_D(const QBluetoothSocket);
    return d->localAddress();
}

quint16 QBluetoothSocket::localPort() const
{
    Q_D(const QBluetoothSocket);
    return d->localPort();
}

QString QBluetoothSocket::peerName() const
{
    Q_D(const QBluetoothSocket);
    return d->peerName();
}

QBluetoothAddress QBluetoothSocket::peerAddress() const
{
    Q_D(const QBluetoothSocket);
    return d->peerAddress();
}

quint16 QBluetoothSocket::peerPort() const
{
    Q_D(const QBluetoothSocket);
    return d->peerPort();
}

qint64 QBluetoothSocket::writeData(const char *data, qint64 maxSize)
{
    Q_D(QBluetoothSocket);

    if (!data || maxSize <= 0) {
        d_ptr->errorString = tr("Invalid data/data size");
        setSocketError(QBluetoothSocket::OperationError);
        return -1;
    }

    return d->writeData(data, maxSize);
}

qint64 QBluetoothSocket::readData(char *data, qint64 maxSize)
{
    Q_D(QBluetoothSocket);
    return d->readData(data, maxSize);
}

void QBluetoothSocket::close()
{
    if (state() == UnconnectedState)
        return;

    Q_D(QBluetoothSocket);
    setOpenMode(QIODevice::NotOpen);
    setSocketState(ClosingState);

    d->close();

#ifndef QT_ANDROID_BLUETOOTH
    //Android closes when the Java event loop comes around
    setSocketState(UnconnectedState);
    emit disconnected();
#endif
}

/*!
  Set the socket to use \a socketDescriptor with a type of \a socketType,
  which is in state, \a socketState, and mode, \a openMode.

  Returns true on success
*/


bool QBluetoothSocket::setSocketDescriptor(int socketDescriptor, QBluetoothServiceInfo::Protocol socketType,
                                           SocketState socketState, OpenMode openMode)
{
    Q_D(QBluetoothSocket);
    return d->setSocketDescriptor(socketDescriptor, socketType, socketState, openMode);
}

/*!
  Returns the platform-specific socket descriptor, if available.
  This function returns -1 if the descriptor is not available or an error has occurred.
*/

int QBluetoothSocket::socketDescriptor() const
{
    Q_D(const QBluetoothSocket);
    return d->socket;
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
#endif

#include "moc_qbluetoothsocket.cpp"

QT_END_NAMESPACE
