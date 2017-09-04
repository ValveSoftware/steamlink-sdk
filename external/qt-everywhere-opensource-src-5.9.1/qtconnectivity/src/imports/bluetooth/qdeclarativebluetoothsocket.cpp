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

#include "qdeclarativebluetoothsocket_p.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QPointer>
#include <QtCore/QStringList>
#include <QtCore/QDataStream>
#include <QtCore/QByteArray>

#include <QtBluetooth/QBluetoothDeviceInfo>
#include <QtBluetooth/QBluetoothAddress>
#include <QtBluetooth/QBluetoothSocket>

/*!
    \qmltype BluetoothSocket
    \instantiates QDeclarativeBluetoothSocket
    \inqmlmodule QtBluetooth
    \since 5.2
    \brief Enables connecting and communicating with a Bluetooth service or
    device.

   \sa QBluetoothSocket
   \sa QDataStream

    It allows a QML class connect to another Bluetooth device and exchange strings
    with it. Data is sent and received using a QDataStream object allowing type
    safe transfers of QStrings. QDataStream is a well known format and can be
    decoded by non-Qt applications. Note that for the ease of use, BluetoothSocket
    is only well suited for use with strings. If you want to
    use a binary protocol for your application's communication you should
    consider using its C++ counterpart QBluetoothSocket.

    Connections to remote devices can be over RFCOMM or L2CAP.  Either the remote port
    or service UUID is required.  This is specified by creating a BluetoothService,
    or passing in the service return from BluetoothDiscoveryModel.
 */

Q_DECLARE_LOGGING_CATEGORY(QT_BT_QML)

class QDeclarativeBluetoothSocketPrivate
{
public:
    QDeclarativeBluetoothSocketPrivate(QDeclarativeBluetoothSocket *bs)
        : m_dbs(bs), m_service(0), m_socket(0),
          m_error(QDeclarativeBluetoothSocket::NoError),
          m_state(QDeclarativeBluetoothSocket::NoServiceSet),
          m_componentCompleted(false),
          m_connected(false)
    {

    }

    ~QDeclarativeBluetoothSocketPrivate()
    {
        delete m_socket;
    }

    void connect()
    {
        Q_ASSERT(m_service);
        //qDebug() << "Connecting to: " << m_service->serviceInfo()->device().address().toString();
        m_error = QDeclarativeBluetoothSocket::NoError;

        if (m_socket)
            m_socket->deleteLater();

        QBluetoothServiceInfo::Protocol socketProtocol;
        if (m_service->serviceInfo()->socketProtocol() == QBluetoothServiceInfo::L2capProtocol)
            socketProtocol = QBluetoothServiceInfo::L2capProtocol;
        else if (m_service->serviceInfo()->socketProtocol() == QBluetoothServiceInfo::RfcommProtocol)
            socketProtocol = QBluetoothServiceInfo::RfcommProtocol;
        else
            socketProtocol = QBluetoothServiceInfo::UnknownProtocol;

        m_socket = new QBluetoothSocket(socketProtocol);
        m_socket->connectToService(*m_service->serviceInfo());
        QObject::connect(m_socket, SIGNAL(connected()), m_dbs, SLOT(socket_connected()));
        QObject::connect(m_socket, SIGNAL(disconnected()), m_dbs, SLOT(socket_disconnected()));
        QObject::connect(m_socket, SIGNAL(error(QBluetoothSocket::SocketError)), m_dbs, SLOT(socket_error(QBluetoothSocket::SocketError)));
        QObject::connect(m_socket, SIGNAL(stateChanged(QBluetoothSocket::SocketState)), m_dbs, SLOT(socket_state(QBluetoothSocket::SocketState)));
        QObject::connect(m_socket, SIGNAL(readyRead()), m_dbs, SLOT(socket_readyRead()));
    }

    QDeclarativeBluetoothSocket *m_dbs;
    QDeclarativeBluetoothService *m_service;
    QBluetoothSocket *m_socket;
    QDeclarativeBluetoothSocket::Error m_error;
    QDeclarativeBluetoothSocket::SocketState m_state;
    bool m_componentCompleted;
    bool m_connected;
};

QDeclarativeBluetoothSocket::QDeclarativeBluetoothSocket(QObject *parent) :
    QObject(parent)
{
    d = new QDeclarativeBluetoothSocketPrivate(this);
}

QDeclarativeBluetoothSocket::QDeclarativeBluetoothSocket(QDeclarativeBluetoothService *service, QObject *parent)
    : QObject(parent)
{
    d = new QDeclarativeBluetoothSocketPrivate(this);
    d->m_service = service;
}

QDeclarativeBluetoothSocket::QDeclarativeBluetoothSocket(QBluetoothSocket *socket, QDeclarativeBluetoothService *service, QObject *parent)
    : QObject(parent)
{
    d = new QDeclarativeBluetoothSocketPrivate(this);
    d->m_service = service;
    d->m_socket = socket;
    d->m_connected = true;
    d->m_componentCompleted = true;

    QObject::connect(socket, SIGNAL(connected()), this, SLOT(socket_connected()));
    QObject::connect(socket, SIGNAL(disconnected()), this, SLOT(socket_disconnected()));
    QObject::connect(socket, SIGNAL(error(QBluetoothSocket::SocketError)), this, SLOT(socket_error(QBluetoothSocket::SocketError)));
    QObject::connect(socket, SIGNAL(stateChanged(QBluetoothSocket::SocketState)), this, SLOT(socket_state(QBluetoothSocket::SocketState)));
    QObject::connect(socket, SIGNAL(readyRead()), this, SLOT(socket_readyRead()));
}


QDeclarativeBluetoothSocket::~QDeclarativeBluetoothSocket()
{
    delete d;
}

void QDeclarativeBluetoothSocket::componentComplete()
{
    d->m_componentCompleted = true;

    if (d->m_connected && d->m_service)
        d->connect();
}

/*!
  \qmlproperty BluetoothService BluetoothSocket::service

  This property holds the details of the remote service to connect to. It can be
  set to a static BluetoothService with a fixed description, or a service returned
  by service discovery.
  */


QDeclarativeBluetoothService *QDeclarativeBluetoothSocket::service()
{
    return d->m_service;
}

void QDeclarativeBluetoothSocket::setService(QDeclarativeBluetoothService *service)
{
    d->m_service = service;

    if (!d->m_componentCompleted)
        return;

    if (d->m_connected)
        d->connect();
    emit serviceChanged();
}

/*!
  \qmlproperty bool BluetoothSocket::connected

  This property holds the connection state of the socket. If the socket is
  connected to peer, it returns true. It can be set true or false to control the
  connection. When set to true, the property will not return true until the
  connection is established.

  */


bool QDeclarativeBluetoothSocket::connected() const
{
    if (!d->m_socket)
        return false;

    return d->m_socket->state() == QBluetoothSocket::ConnectedState;
}

void QDeclarativeBluetoothSocket::setConnected(bool connected)
{
    d->m_connected = connected;
    if (connected && d->m_componentCompleted) {
        if (d->m_service) {
            d->connect();
        }
        else {
            qCWarning(QT_BT_QML) << "BluetoothSocket::setConnected called before a service was set";
        }
    }

    if (!connected && d->m_socket){
        d->m_socket->close();
    }
}

/*!
    \qmlproperty enumeration BluetoothSocket::error

    This property holds the last error that happened.
    \list
        \li \c{NoError}
        \li \c{UnknownSocketError}
        \li \c{HostNotFoundError}
        \li \c{ServiceNotFoundError}
        \li \c{NetworkError}
        \li \c{UnsupportedProtocolError}
    \endlist

    The errors are derived from \l QBluetoothSocket::SocketError. This property is read-only.
*/


QDeclarativeBluetoothSocket::Error QDeclarativeBluetoothSocket::error() const
{
    return d->m_error;
}

void QDeclarativeBluetoothSocket::socket_connected()
{
    emit connectedChanged();
}

void QDeclarativeBluetoothSocket::socket_disconnected()
{
    d->m_socket->deleteLater();
    d->m_socket = 0;
    emit connectedChanged();
}

void QDeclarativeBluetoothSocket::socket_error(QBluetoothSocket::SocketError error)
{
    d->m_error = static_cast<QDeclarativeBluetoothSocket::Error>(error);

    emit errorChanged();
}

void QDeclarativeBluetoothSocket::socket_state(QBluetoothSocket::SocketState state)
{
    d->m_state = static_cast<QDeclarativeBluetoothSocket::SocketState>(state);

    emit stateChanged();
}

/*!
    \qmlproperty enumeration BluetoothSocket::state

    This property holds the current state of the socket.

    \list
        \li \c{NoServiceSet}
        \li \c{Unconnected}
        \li \c{ServiceLookup}
        \li \c{Connecting}
        \li \c{Connected}
        \li \c{Closing}
        \li \c{Listening}
        \li \c{Bound}
    \endlist

    The states are derived from \l QBluetoothSocket::SocketState. This property is read-only.
*/
QDeclarativeBluetoothSocket::SocketState QDeclarativeBluetoothSocket::state() const
{
    return d->m_state;
}

void QDeclarativeBluetoothSocket::socket_readyRead()
{
    emit dataAvailable();
}

/*!
  \qmlproperty string BluetoothSocket::stringData

  This property receives or sends data to a remote Bluetooth device. Arrival of
  data can be detected by connecting to this properties changed signal and can be read via
  stringData. Setting stringData will transmit the string.
  If excessive amounts of data are sent, the function may block sending. Reading will
  never block.
  */

QString QDeclarativeBluetoothSocket::stringData()
{
    if (!d->m_socket|| !d->m_socket->bytesAvailable())
        return QString();

    QString data;
    while (d->m_socket->canReadLine()) {
        QByteArray line = d->m_socket->readLine();
        data += QString::fromUtf8(line.constData(), line.length());
    }
    return data;
}

/*!
  This method transmits the string data passed with "data" to the remote device.
  If excessive amounts of data are sent, the function may block sending.
 */

void QDeclarativeBluetoothSocket::sendStringData(const QString &data)
{
    if (!d->m_connected || !d->m_socket){
        qCWarning(QT_BT_QML) << "Writing data to unconnected socket";
        return;
    }

    QByteArray text = data.toUtf8() + '\n';
    d->m_socket->write(text);
}

void QDeclarativeBluetoothSocket::newSocket(QBluetoothSocket *socket, QDeclarativeBluetoothService *service)
{
    if (d->m_socket){
        delete d->m_socket;
    }

    d->m_service = service;
    d->m_socket = socket;
    d->m_connected = true;
    d->m_componentCompleted = true;
    d->m_error = NoError;

    QObject::connect(socket, SIGNAL(connected()), this, SLOT(socket_connected()));
    QObject::connect(socket, SIGNAL(disconnected()), this, SLOT(socket_disconnected()));
    QObject::connect(socket, SIGNAL(error(QBluetoothSocket::SocketError)), this, SLOT(socket_error(QBluetoothSocket::SocketError)));
    QObject::connect(socket, SIGNAL(stateChanged(QBluetoothSocket::SocketState)), this, SLOT(socket_state(QBluetoothSocket::SocketState)));
    QObject::connect(socket, SIGNAL(readyRead()), this, SLOT(socket_readyRead()));

    socket_state(socket->state());

    emit connectedChanged();
}
