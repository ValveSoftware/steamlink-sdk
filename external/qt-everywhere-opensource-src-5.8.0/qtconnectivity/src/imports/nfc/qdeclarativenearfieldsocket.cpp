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

#include "qdeclarativenearfieldsocket_p.h"

#include <qllcpserver.h>

/*!
    \qmltype NearFieldSocket
    \instantiates QDeclarativeNearFieldSocket
    \since 5.2
    \brief Represents an LLCP socket.

    \ingroup nfc-qml
    \inqmlmodule QtNfc

    The NearFieldSocket type can be used to create a peer-to-peer connection over NFC LLCP
    sockets.  NearfieldSocket can be used for both client and server side sockets.

    \internal
*/

/*!
    \qmlproperty string NearFieldSocket::uri

    This property hold the URI of the socket.  The URI uniquely identifies a remote service (for
    client sockets) or to register a service (for server sockets).
*/

/*!
    \qmlproperty bool NearFieldSocket::connected

    This property holds the connected state of the socket.
*/

/*!
    \qmlproperty string NearFieldSocket::error

    This property holds the last error that occurred.
*/

/*!
    \qmlproperty string NearFieldSocket::state

    This property holds the state of the socket.
*/

/*!
    \qmlproperty bool NearFieldSocket::listening

    This property holds whether the socket is listening for incoming connections.
*/

/*!
    \qmlproperty string NearFieldSocket::stringData

    This property returns the available string data read from the socket.  Seting this property
    sends the data to the remote socket.
*/

class QDeclarativeNearFieldSocketPrivate
{
    Q_DECLARE_PUBLIC(QDeclarativeNearFieldSocket)

public:
    QDeclarativeNearFieldSocketPrivate(QDeclarativeNearFieldSocket *q)
    :   q_ptr(q), m_socket(0), m_server(0),
        m_error(QLatin1String("No Error")),
        m_state(QLatin1String("No Service Set")),
        m_componentCompleted(false),
        m_connected(false), m_listen(false)
    {
    }

    ~QDeclarativeNearFieldSocketPrivate()
    {
        delete m_socket;
    }

    void connect()
    {
        Q_ASSERT(!uri.isEmpty());

        m_error = QLatin1String("No Error");

        if (m_socket)
            m_socket->deleteLater();

        m_socket = new QLlcpSocket;

        Q_Q(const QDeclarativeNearFieldSocket);

        QObject::connect(m_socket, SIGNAL(connected()), q, SLOT(socket_connected()));
        QObject::connect(m_socket, SIGNAL(disconnected()), q, SLOT(socket_disconnected()));
        QObject::connect(m_socket, SIGNAL(error(QLlcpSocket::SocketError)),
                         q, SLOT(socket_error(QLlcpSocket::SocketError)));
        QObject::connect(m_socket, SIGNAL(stateChanged(QLlcpSocket::SocketState)),
                         q, SLOT(socket_state(QLlcpSocket::SocketState)));
        QObject::connect(m_socket, SIGNAL(readyRead()), q, SLOT(socket_readyRead()));

        m_socket->connectToService(0, uri);
    }

    QDeclarativeNearFieldSocket *q_ptr;
    QString uri;
    QLlcpSocket *m_socket;
    QLlcpServer *m_server;
    QString m_error;
    QString m_state;
    bool m_componentCompleted;
    bool m_connected;
    bool m_listen;
};

QDeclarativeNearFieldSocket::QDeclarativeNearFieldSocket(QObject *parent)
:   QObject(parent), d_ptr(new QDeclarativeNearFieldSocketPrivate(this))
{
}

QDeclarativeNearFieldSocket::~QDeclarativeNearFieldSocket()
{
    delete d_ptr;
}

void QDeclarativeNearFieldSocket::componentComplete()
{
    Q_D(QDeclarativeNearFieldSocket);

    d->m_componentCompleted = true;

    if (d->m_connected && !d->uri.isEmpty())
        d->connect();
    else if (d->m_listen)
        setListening(true);
}

QString QDeclarativeNearFieldSocket::uri() const
{
    Q_D(const QDeclarativeNearFieldSocket);

    return d->uri;
}

void QDeclarativeNearFieldSocket::setUri(const QString &uri)
{
    Q_D(QDeclarativeNearFieldSocket);

    d->uri = uri;

    if (!d->m_componentCompleted)
        return;

    if (d->m_connected)
        d->connect();

    emit uriChanged();
}

bool QDeclarativeNearFieldSocket::connected() const
{
    Q_D(const QDeclarativeNearFieldSocket);

    if (!d->m_socket)
        return false;

    return d->m_socket->state() == QLlcpSocket::ConnectedState;
}

void QDeclarativeNearFieldSocket::setConnected(bool connected)
{
    Q_D(QDeclarativeNearFieldSocket);

    d->m_connected = connected;
    if (connected && d->m_componentCompleted) {
        if (!d->uri.isEmpty())
            d->connect();
        else
            qWarning() << "NearFieldSocket::setConnected called before a uri was set";
    }

    if (!connected && d->m_socket)
        d->m_socket->close();
}

QString QDeclarativeNearFieldSocket::error() const
{
    Q_D(const QDeclarativeNearFieldSocket);

    return d->m_error;
}

void QDeclarativeNearFieldSocket::socket_connected()
{
    emit connectedChanged();
}

void QDeclarativeNearFieldSocket::socket_disconnected()
{
    Q_D(QDeclarativeNearFieldSocket);

    d->m_socket->deleteLater();
    d->m_socket = 0;
    emit connectedChanged();
}

void QDeclarativeNearFieldSocket::socket_error(QLlcpSocket::SocketError err)
{
    Q_D(QDeclarativeNearFieldSocket);

    if (err == QLlcpSocket::RemoteHostClosedError)
        d->m_error = QLatin1String("Connection Closed by Remote Host");
    else
        d->m_error = QLatin1String("Unknown Error");

    emit errorChanged();
}

void QDeclarativeNearFieldSocket::socket_state(QLlcpSocket::SocketState state)
{
    Q_D(QDeclarativeNearFieldSocket);

    switch (state) {
    case QLlcpSocket::UnconnectedState:
        d->m_state = QLatin1String("Unconnected");
        break;
    case QLlcpSocket::ConnectingState:
        d->m_state = QLatin1String("Connecting");
        break;
    case QLlcpSocket::ConnectedState:
        d->m_state = QLatin1String("Connected");
        break;
    case QLlcpSocket::ClosingState:
        d->m_state = QLatin1String("Closing");
        break;
    case QLlcpSocket::ListeningState:
        d->m_state = QLatin1String("Listening");
        break;
    case QLlcpSocket::BoundState:
        d->m_state = QLatin1String("Bound");
        break;
    }

    emit stateChanged();
}

QString QDeclarativeNearFieldSocket::state() const
{
    Q_D(const QDeclarativeNearFieldSocket);

    return d->m_state;
}

bool QDeclarativeNearFieldSocket::listening() const
{
    Q_D(const QDeclarativeNearFieldSocket);

    if (d->m_server)
        return true;

    return false;
}

void QDeclarativeNearFieldSocket::setListening(bool listen)
{
    Q_D(QDeclarativeNearFieldSocket);

    if (listen == false && d->m_server) {
        qWarning() << "Once socket is in listening state, can not be returned to client socket";
        return;
    }

    if (!d->m_componentCompleted){
        d->m_listen = listen;
        return;
    }

    if (d->uri.isEmpty()) {
        qWarning() << "Can not put socket into listening state without an assigned uri";
        return;
    }

    d->m_server = new QLlcpServer;

    connect(d->m_server, SIGNAL(newConnection()), this, SLOT(llcp_connection()));

    d->m_server->listen(d->uri);



    emit listeningChanged();
}

void QDeclarativeNearFieldSocket::socket_readyRead()
{
    emit dataAvailable();
}

QString QDeclarativeNearFieldSocket::stringData()
{
    Q_D(QDeclarativeNearFieldSocket);

    if (!d->m_socket|| !d->m_socket->bytesAvailable())
        return QString();

    const QByteArray data = d->m_socket->readAll();
    return QString::fromUtf8(data.constData(), data.size());
}

void QDeclarativeNearFieldSocket::sendStringData(const QString &data)
{
    Q_D(QDeclarativeNearFieldSocket);

    if (!d->m_connected || !d->m_socket) {
        qWarning() << "Writing data to unconnected socket";
        return;
    }

    d->m_socket->write(data.toUtf8());
}

void QDeclarativeNearFieldSocket::llcp_connection()
{
    Q_D(QDeclarativeNearFieldSocket);

    QLlcpSocket *socket = d->m_server->nextPendingConnection();
    if (!socket)
        return;

    if (d->m_socket) {
        socket->close();
        return;
    }

    d->m_socket = socket;

    connect(socket, SIGNAL(disconnected()), this, SLOT(socket_disconnected()));
    connect(socket, SIGNAL(error(QLlcpSocket::SocketError)),
            this, SLOT(socket_error(QLlcpSocket::SocketError)));
    connect(socket, SIGNAL(stateChanged(QLlcpSocket::SocketState)),
            this, SLOT(socket_state(QLlcpSocket::SocketState)));
    connect(socket, SIGNAL(readyRead()), this, SLOT(socket_readyRead()));

    void connectedChanged();
}
