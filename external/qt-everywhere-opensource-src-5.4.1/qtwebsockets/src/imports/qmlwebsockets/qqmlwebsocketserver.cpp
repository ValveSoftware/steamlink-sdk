/****************************************************************************
**
** Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Milian Wolff <milian.wolff@kdab.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebSocket module of the Qt Toolkit.
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

#include "qqmlwebsocketserver.h"
#include "qqmlwebsocket.h"

QT_USE_NAMESPACE

/*!
    \qmltype WebSocketServer
    \instantiates QQmlWebSocketServer
    \since 5.3

    \inqmlmodule Qt.WebSockets
    \ingroup websockets-qml
    \brief QML interface to QWebSocketServer.
*/

/*!
  \qmlproperty QUrl WebSocketServer::url
  Server url that client WebSockets can connect to. The url uses the \e ws:// scheme and includes the
  port the server listens to and the host address of the server.
  */

/*!
  \qmlproperty QString WebSocketServer::host
  The host address of the server. By default, localhost is used.
  */

/*!
  \qmlproperty int WebSocketServer::port
  The port this server is listening on. By default, a port is chosen automatically.
  */

/*!
  \qmlproperty QString WebSocketServer::name
  The name of this server used during the http handshake phase.
  */

/*!
  \qmlproperty QString WebSocketServer::errorString
  The stringified error message in case an error occurred.
  */

/*!
  \qmlproperty bool WebSocketServer::listen
  Set to true when the server should listen to client connections or false otherwise.
  When set to true, the server will listen on the specified url defined by host and port
  and, when accept is true, accepts incoming client connections. Otherwise the server is closed.
  By default, the server is not listening.
  */

/*!
  \qmlproperty bool WebSocketServer::accept
  Set to true to accept incoming client connections when the server is listening. When set to false,
  incoming connections are rejected. By default, connections are accepted.
  */

/*!
  \qmlsignal WebSocketServer::clientConnected(WebSocket webSocket)
  This signal is emitted when a client connects to this server.
  */

QQmlWebSocketServer::QQmlWebSocketServer(QObject *parent)
    : QObject(parent)
    , m_host(QHostAddress(QHostAddress::LocalHost).toString())
    , m_port(0)
    , m_listen(false)
    , m_accept(true)
    , m_componentCompleted(true)
{
}

QQmlWebSocketServer::~QQmlWebSocketServer()
{

}

void QQmlWebSocketServer::classBegin()
{
    m_componentCompleted = false;
}

void QQmlWebSocketServer::componentComplete()
{
    init();
    m_componentCompleted = true;
}

QUrl QQmlWebSocketServer::url() const
{
    QUrl url;
    url.setPort(m_port);
    url.setHost(m_host);
    url.setScheme("ws");
    return url;
}

QString QQmlWebSocketServer::host() const
{
    return m_host;
}

void QQmlWebSocketServer::setHost(const QString &host)
{
    if (host == m_host) {
        return;
    }

    m_host = host;
    emit hostChanged(host);
    emit urlChanged(url());

    updateListening();
}

quint16 QQmlWebSocketServer::port() const
{
    return m_port;
}

void QQmlWebSocketServer::setPort(quint16 port)
{
    if (port == m_port) {
        return;
    }

    m_port = port;
    emit portChanged(port);
    emit urlChanged(url());

    if (m_componentCompleted && m_server->isListening()) {
        updateListening();
    }
}

QString QQmlWebSocketServer::errorString() const
{
    return m_server ? m_server->errorString() : tr("QQmlWebSocketServer is not ready.");
}

QString QQmlWebSocketServer::name() const
{
    return m_name;
}

void QQmlWebSocketServer::setName(const QString &name)
{
    if (name == m_name) {
        return;
    }

    m_name = name;
    emit nameChanged(name);

    if (m_componentCompleted) {
        init();
    }
}

bool QQmlWebSocketServer::listen() const
{
    return m_listen;
}

void QQmlWebSocketServer::setListen(bool listen)
{
    if (listen == m_listen) {
        return;
    }

    m_listen = listen;
    emit listenChanged(listen);

    updateListening();
}

bool QQmlWebSocketServer::accept() const
{
    return m_accept;
}

void QQmlWebSocketServer::setAccept(bool accept)
{
    if (accept == m_accept) {
        return;
    }

    m_accept = accept;
    emit acceptChanged(accept);

    if (m_componentCompleted) {
        if (!accept) {
            m_server->pauseAccepting();
        } else {
            m_server->resumeAccepting();
        }
    }
}

void QQmlWebSocketServer::init()
{
    // TODO: add support for wss, requires ssl configuration to be set from QML - realistic?
    m_server.reset(new QWebSocketServer(m_name, QWebSocketServer::NonSecureMode));

    connect(m_server.data(), &QWebSocketServer::newConnection,
            this, &QQmlWebSocketServer::newConnection);
    connect(m_server.data(), &QWebSocketServer::serverError,
            this, &QQmlWebSocketServer::serverError);
    connect(m_server.data(), &QWebSocketServer::closed,
            this, &QQmlWebSocketServer::closed);

    updateListening();
}

void QQmlWebSocketServer::updateListening()
{
    if (!m_server) {
        return;
    }

    if (m_server->isListening()) {
        m_server->close();
    }

    if (!m_listen || !m_server->listen(QHostAddress(m_host), m_port)) {
        return;
    }
    setPort(m_server->serverPort());
    setHost(m_server->serverAddress().toString());
}

void QQmlWebSocketServer::newConnection()
{
    emit clientConnected(new QQmlWebSocket(m_server->nextPendingConnection(), this));
}

void QQmlWebSocketServer::serverError()
{
    emit errorStringChanged(errorString());
}

void QQmlWebSocketServer::closed()
{
    setListen(false);
}

