/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#include "qllcpsocket_p.h"

#if defined(QT_SIMULATOR)
#include "qllcpsocket_simulator_p.h"
#else
#include "qllcpsocket_p_p.h"
#endif

QT_BEGIN_NAMESPACE

/*!
    \class QLlcpSocket
    \brief The QLlcpSocket class provides an NFC LLCP socket.
    \internal

    \ingroup connectivity-nfc
    \inmodule QtNfc

    NFC LLCP protocol is a peer-to-peer communication protocol between two NFC compliant devices.
*/

/*!
    \enum QLlcpSocket::SocketError

    This enum describes the errors that can occur. The most recent error can be retrieved through a
    call to error().

    \value UnknownSocketError       An unidentified error has occurred.
    \value RemoteHostClosedError    The remote host closed the connection.
    \value SocketAccessError        The socket operation failed because the application lacked the
                                    required privileges.
    \value SocketResourceError      The local system ran out of resources (e.g., too many sockets).
*/

/*!
    \enum QLlcpSocket::SocketState

    This enum describes the different state in which a socket can be.

    \value UnconnectedState The socket is not connected.
    \value ConnectingState  The socket has started establishing a connection.
    \value ConnectedState   A connection is established.
    \value ClosingState     The socket is about to close.
    \value BoundState       The socket is bound to a local port (for servers).
    \value ListeningState   The socket is listening for incoming connections (for internal use).
*/

/*!
    \fn QLlcpSocket::connected()

    This signal is emitted after connectToService() has been called and a connection has been
    successfully established.

    \sa connectToService(), disconnected()
*/

/*!
    \fn QLlcpSocket::disconnected()

    This signal is emitted when the socket has been disconnected.

    \sa disconnectFromService(),
*/

/*!
    \fn QLlcpSocket::error(QLlcpSocket::SocketError socketError)

    This signal is emitted when an error occurs. The \a socketError parameter describes the error.
*/

/*!
    \fn QLlcpSocket::stateChanged(QLlcpSocket::SocketState socketState)

    This signal is emitted when the state of the socket changes. The \a socketState parameter
    describes the new state.
*/

/*!
    Construct a new unconnected LLCP socket with \a parent.
*/
QLlcpSocket::QLlcpSocket(QObject *parent)
:   QIODevice(parent), d_ptr(new QLlcpSocketPrivate(this))
{
    setOpenMode(QIODevice::NotOpen);
}

/*!
    \internal
*/
QLlcpSocket::QLlcpSocket(QLlcpSocketPrivate *d, QObject *parent)
:   QIODevice(parent), d_ptr(d)
{
    setOpenMode(QIODevice::ReadWrite);
    d_ptr->q_ptr = this;
}

/*!
    Destroys the LLCP socket.
*/
QLlcpSocket::~QLlcpSocket()
{
    delete d_ptr;
}

/*!
    Connects to the service identified by the URI \a serviceUri on \a target.
*/
void QLlcpSocket::connectToService(QNearFieldTarget *target, const QString &serviceUri)
{
    Q_D(QLlcpSocket);

    d->connectToService(target, serviceUri);
}

/*!
    Disconnects the socket.
*/
void QLlcpSocket::disconnectFromService()
{
    Q_D(QLlcpSocket);

    d->disconnectFromService();
}

/*!
    Disconnects the socket.
*/
void QLlcpSocket::close()
{
    Q_D(QLlcpSocket);

    QIODevice::close();

    d->disconnectFromService();
}

/*!
    Binds the LLCP socket to local \a port. Returns true on success; otherwise returns false.
*/
bool QLlcpSocket::bind(quint8 port)
{
    Q_D(QLlcpSocket);

    return d->bind(port);
}

/*!
    Returns true if at least one datagram (service data units) is waiting to be read; otherwise
    returns false.

    \sa pendingDatagramSize(), readDatagram()
*/
bool QLlcpSocket::hasPendingDatagrams() const
{
    Q_D(const QLlcpSocket);

    return d->hasPendingDatagrams();
}

/*!
    Returns the size of the first pending datagram (service data unit). If there is no datagram
    available, this function returns -1.

    \sa hasPendingDatagrams(), readDatagram()
*/
qint64 QLlcpSocket::pendingDatagramSize() const
{
    Q_D(const QLlcpSocket);

    return d->pendingDatagramSize();
}

/*!
    Sends the datagram at \a data of size \a size to the service that this socket is connected to.
    Returns the number of bytes sent on success; otherwise return -1;
*/
qint64 QLlcpSocket::writeDatagram(const char *data, qint64 size)
{
    Q_D(QLlcpSocket);

    return d->writeDatagram(data, size);
}

/*!
    \reimp

    Always returns true.
*/
bool QLlcpSocket::isSequential() const
{
    return true;
}

/*!
    \overload

    Sends the datagram \a datagram to the service that this socket is connected to.
*/
qint64 QLlcpSocket::writeDatagram(const QByteArray &datagram)
{
    Q_D(QLlcpSocket);

    return d->writeDatagram(datagram);
}

/*!
    Receives a datagram no larger than \a maxSize bytes and stores it in \a data. The sender's
    details are stored in \a target and \a port (unless the pointers are 0).

    Returns the size of the datagram on success; otherwise returns -1.

    If maxSize is too small, the rest of the datagram will be lost. To avoid loss of data, call
    pendingDatagramSize() to determine the size of the pending datagram before attempting to read
    it. If maxSize is 0, the datagram will be discarded.

    \sa writeDatagram(), hasPendingDatagrams(), pendingDatagramSize()
*/
qint64 QLlcpSocket::readDatagram(char *data, qint64 maxSize, QNearFieldTarget **target,
                                 quint8 *port)
{
    Q_D(QLlcpSocket);

    return d->readDatagram(data, maxSize, target, port);
}

/*!
    Sends the datagram at \a data of size \a size to the service identified by the URI
    \a port on \a target. Returns the number of bytes sent on success; otherwise returns -1.

    \sa readDatagram()
*/
qint64 QLlcpSocket::writeDatagram(const char *data, qint64 size, QNearFieldTarget *target,
                                  quint8 port)
{
    Q_D(QLlcpSocket);

    return d->writeDatagram(data, size, target, port);
}

/*!
    \overload

    Sends the datagram \a datagram to the service identified by the URI \a port on \a target.
*/
qint64 QLlcpSocket::writeDatagram(const QByteArray &datagram, QNearFieldTarget *target,
                                  quint8 port)
{
    Q_D(QLlcpSocket);

    return d->writeDatagram(datagram, target, port);
}

/*!
    Returns the type of error that last occurred.
*/
QLlcpSocket::SocketError QLlcpSocket::error() const
{
    Q_D(const QLlcpSocket);

    return d->error();
}

/*!
    Returns the state of the socket.
*/
QLlcpSocket::SocketState QLlcpSocket::state() const
{
    Q_D(const QLlcpSocket);

    return d->state();
}

/*!
    \reimp
*/
qint64 QLlcpSocket::bytesAvailable() const
{
    Q_D(const QLlcpSocket);

    return d->bytesAvailable() + QIODevice::bytesAvailable();
}

/*!
    \reimp
*/
bool QLlcpSocket::canReadLine() const
{
    Q_D(const QLlcpSocket);

    return d->canReadLine() || QIODevice::canReadLine();
}

/*!
    \reimp
*/
bool QLlcpSocket::waitForReadyRead(int msecs)
{
    Q_D(QLlcpSocket);

    return d->waitForReadyRead(msecs);
}

/*!
    \reimp
*/
bool QLlcpSocket::waitForBytesWritten(int msecs)
{
    Q_D(QLlcpSocket);

    return d->waitForBytesWritten(msecs);
}

/*!
    Waits until the socket is connected, up to \a msecs milliseconds. If the connection has been
    established, this function returns true; otherwise it returns false. In the case where it
    returns false, you can call error() to determine the cause of the error.

    If msecs is -1, this function will not time out.
*/
bool QLlcpSocket::waitForConnected(int msecs)
{
    Q_D(QLlcpSocket);

    return d->waitForConnected(msecs);
}

/*!
    Waits until the socket is disconnected, up to \a msecs milliseconds. If the connection has been
    disconnected, this function returns true; otherwise it returns false. In the case where it
    returns false, you can call error() to determine the cause of the error.

    If msecs is -1, this function will not time out.
*/
bool QLlcpSocket::waitForDisconnected(int msecs)
{
    Q_D(QLlcpSocket);

    return d->waitForDisconnected(msecs);
}

/*!
    \internal
*/
qint64 QLlcpSocket::readData(char *data, qint64 maxlen)
{
    Q_D(QLlcpSocket);

    return d->readData(data, maxlen);
}

/*!
    \internal
*/
qint64 QLlcpSocket::writeData(const char *data, qint64 len)
{
    Q_D(QLlcpSocket);

    return d->writeData(data, len);
}

QT_END_NAMESPACE
