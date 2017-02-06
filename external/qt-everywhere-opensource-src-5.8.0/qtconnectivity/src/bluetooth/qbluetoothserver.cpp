/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
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
#include "qbluetoothserviceinfo.h"

QT_BEGIN_NAMESPACE

/*!
    \class QBluetoothServer
    \inmodule QtBluetooth
    \brief The QBluetoothServer class uses the RFCOMM or L2cap protocol to communicate with
    a Bluetooth device.

    \since 5.2

    QBluetoothServer is used to implement Bluetooth services over RFCOMM or L2cap.

    Start listening for incoming connections with listen(). Wait till the newConnection() signal
    is emitted when a new connection is established, and call nextPendingConnection() to get a QBluetoothSocket
    for the new connection.

    To enable other devices to find your service, create a QBluetoothServiceInfo with the
    applicable attributes for your service and register it using QBluetoothServiceInfo::registerService().
    Call serverPort() to get the channel number that is being used.

    If the \l QBluetoothServiceInfo::Protocol is not supported by a platform, \l listen() will return \c false.

    \sa QBluetoothServiceInfo, QBluetoothSocket
*/

/*!
    \fn void QBluetoothServer::newConnection()

    This signal is emitted when a new connection is available.

    The connected slot should call nextPendingConnection() to get a QBluetoothSocket object to
    send and receive data over the connection.

    \sa nextPendingConnection(), hasPendingConnections()
*/

/*!
    \fn void QBluetoothServer::error(QBluetoothServer::Error error)

    This signal is emitted when an \a error occurs.

    \sa error(), QBluetoothServer::Error
*/

/*!
    \fn void QBluetoothServer::close()

    Closes and resets the listening socket. Any already established \l QBluetoothSocket
    continues to operate and must be separately \l {QBluetoothSocket::close()}{closed}.
*/

/*!
    \enum QBluetoothServer::Error

    This enum describes Bluetooth server error types.

    \value NoError                  No error.
    \value UnknownError             An unknown error occurred.
    \value PoweredOffError          The Bluetooth adapter is powered off.
    \value InputOutputError         An input output error occurred.
    \value ServiceAlreadyRegisteredError  The service or port was already registered
    \value UnsupportedProtocolError The \l {QBluetoothServiceInfo::Protocol}{Protocol} is not
                                    supported on this platform.
*/

/*!
    \fn bool QBluetoothServer::listen(const QBluetoothAddress &address, quint16 port)

    Start listening for incoming connections to \a address on \a port. \a address
    must be a local Bluetooth adapter address and \a port must be larger than zero
    and not be taken already by another Bluetooth server object. It is recommended
    to avoid setting a port number to enable the system to automatically choose
    a port.

    Returns \c true if the operation succeeded and the server is listening for
    incoming connections, otherwise returns \c false.

    If the server object is already listening for incoming connections this function
    always returns \c false. \l close() should be called before calling this function.

    \sa isListening(), newConnection()
*/

/*!
    \fn void QBluetoothServer::setMaxPendingConnections(int numConnections)

    Sets the maximum number of pending connections to \a numConnections. If
    the number of pending sockets exceeds this limit new sockets will be rejected.

    \sa maxPendingConnections()
*/

/*!
    \fn bool QBluetoothServer::hasPendingConnections() const
    Returns true if a connection is pending, otherwise false.
*/

/*!
    \fn QBluetoothSocket *QBluetoothServer::nextPendingConnection()

    Returns a pointer to the QBluetoothSocket for the next pending connection. It is the callers
    responsibility to delete the pointer.
*/

/*!
    \fn QBluetoothAddress QBluetoothServer::serverAddress() const

    Returns the server address.
*/

/*!
    \fn quint16 QBluetoothServer::serverPort() const

    Returns the server port number.
*/

/*!
    Constructs a bluetooth server with \a parent and \a serverType.
*/
QBluetoothServer::QBluetoothServer(QBluetoothServiceInfo::Protocol serverType, QObject *parent)
    : QObject(parent), d_ptr(new QBluetoothServerPrivate(serverType))
{
    d_ptr->q_ptr = this;
}

/*!
    Destroys the bluetooth server.
*/
QBluetoothServer::~QBluetoothServer()
{
    delete d_ptr;
}

/*!
    \fn QBluetoothServiceInfo QBluetoothServer::listen(const QBluetoothUuid &uuid, const QString &serviceName)

    Convenience function for registering an SPP service with \a uuid and \a serviceName.
    Because this function already registers the service, the QBluetoothServiceInfo object
    which is returned can not be changed any more. To shutdown the server later on it is
    required to call \l QBluetoothServiceInfo::unregisterService() and \l close() on this
    server object.

    Returns a registered QBluetoothServiceInfo instance if sucessful otherwise an
    invalid QBluetoothServiceInfo. This function always assumes that the default Bluetooth adapter
    should be used.

    If the server object is already listening for incoming connections this function
    returns an invalid \l QBluetoothServiceInfo.

    For an RFCOMM server this function is equivalent to following code snippet.

    \snippet qbluetoothserver.cpp listen
    \snippet qbluetoothserver.cpp listen2
    \snippet qbluetoothserver.cpp listen3

    \sa isListening(), newConnection(), listen()
*/
QBluetoothServiceInfo QBluetoothServer::listen(const QBluetoothUuid &uuid, const QString &serviceName)
{
    Q_D(const QBluetoothServer);
    if (!listen())
        return QBluetoothServiceInfo();
//! [listen]
    QBluetoothServiceInfo serviceInfo;
    serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceName, serviceName);
    QBluetoothServiceInfo::Sequence browseSequence;
    browseSequence << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));
    serviceInfo.setAttribute(QBluetoothServiceInfo::BrowseGroupList,
                             browseSequence);

    QBluetoothServiceInfo::Sequence classId;
    classId << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));
    serviceInfo.setAttribute(QBluetoothServiceInfo::BluetoothProfileDescriptorList,
                             classId);

    //Android requires custom uuid to be set as service class
    classId.prepend(QVariant::fromValue(uuid));
    serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);
    serviceInfo.setServiceUuid(uuid);

    QBluetoothServiceInfo::Sequence protocolDescriptorList;
    QBluetoothServiceInfo::Sequence protocol;
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
    if (d->serverType == QBluetoothServiceInfo::L2capProtocol)
        protocol << QVariant::fromValue(serverPort());
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    protocol.clear();
//! [listen]
    if (d->serverType == QBluetoothServiceInfo::RfcommProtocol) {
//! [listen2]
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
             << QVariant::fromValue(quint8(serverPort()));
    protocolDescriptorList.append(QVariant::fromValue(protocol));
//! [listen2]
    }
//! [listen3]
    serviceInfo.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList,
                             protocolDescriptorList);
    bool result = serviceInfo.registerService();
//! [listen3]
    if (!result) {
        close(); //close the still listening socket
        return QBluetoothServiceInfo();
    }
    return serviceInfo;
}

/*!
    Returns true if the server is listening for incoming connections, otherwise false.
*/
bool QBluetoothServer::isListening() const
{
    Q_D(const QBluetoothServer);

#ifdef QT_ANDROID_BLUETOOTH
    return d->isListening();
#endif

    return d->socket->state() == QBluetoothSocket::ListeningState;
}

/*!
    Returns the maximum number of pending connections.

    \sa setMaxPendingConnections()
*/
int QBluetoothServer::maxPendingConnections() const
{
    Q_D(const QBluetoothServer);

    return d->maxPendingConnections;
}

/*!
    \fn QBluetoothServer::setSecurityFlags(QBluetooth::SecurityFlags security)
    Sets the Bluetooth security flags to \a security. This function must be called before calling listen().
    The Bluetooth link will always be encrypted when using Bluetooth 2.1 devices as encryption is
    mandatory.

    Android only supports two levels of security (secure and non-secure). If this flag is set to
    \l QBluetooth::NoSecurity the server object will not employ any authentication or encryption.
    Any other security flag combination will trigger a secure Bluetooth connection.

    On \macos, security flags are not supported and will be ignored.
*/

/*!
    \fn QBluetooth::SecurityFlags QBluetoothServer::securityFlags() const
    Returns the Bluetooth security flags.
*/

/*!
    \fn QBluetoothSocket::ServerType QBluetoothServer::serverType() const
    Returns the type of the QBluetoothServer.
*/
QBluetoothServiceInfo::Protocol QBluetoothServer::serverType() const
{
    Q_D(const QBluetoothServer);
    return d->serverType;
}

/*!
    \fn QBluetoothServer::Error QBluetoothServer::error() const
    Returns the last error of the QBluetoothServer.
*/
QBluetoothServer::Error QBluetoothServer::error() const
{
    Q_D(const QBluetoothServer);
    return d->m_lastError;
}

#include "moc_qbluetoothserver.cpp"

QT_END_NAMESPACE
