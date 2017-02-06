/****************************************************************************
**
** Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebSockets module of the Qt Toolkit.
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

/*!
    \qmltype WebSocket
    \instantiates QQmlWebSocket
    \since 5.3

    \inqmlmodule QtWebSockets
    \ingroup websockets-qml
    \brief QML interface to QWebSocket.

    WebSockets is a web technology providing full-duplex communications channels over a
    single TCP connection.
    The WebSocket protocol was standardized by the IETF as \l {RFC 6455} in 2011.
*/

/*!
  \qmlproperty QUrl WebSocket::url
  Server url to connect to. The url must have one of 2 schemes: \e ws:// or \e wss://.
  When not supplied, then \e ws:// is used.
  */

/*!
  \qmlproperty Status WebSocket::status
  Status of the WebSocket.

  The status can have the following values:
  \list
  \li WebSockets.Connecting
  \li WebSockets.Open
  \li WebSockets.Closing
  \li WebSockets.Closed
  \li WebSockets.Error
  \endlist
  */

/*!
  \qmlproperty QString WebSocket::errorString
  Contains a description of the last error that occurred. When no error occurrred,
  this string is empty.
  */

/*!
  \qmlproperty bool WebSocket::active
  When set to true, a connection is made to the server with the given url.
  When set to false, the connection is closed.
  The default value is false.
  */

/*!
  \qmlsignal WebSocket::textMessageReceived(QString message)
  This signal is emitted when a text message is received.
  */

/*!
  \qmlsignal WebSocket::binaryMessageReceived(QString message)
  \since 5.8
  This signal is emitted when a binary message is received.
  */

/*!
  \qmlsignal WebSocket::statusChanged(Status status)
  This signal is emitted when the status of the WebSocket changes.
  the \l [QML]{WebSocket::status}{status} argument provides the current status.

  \sa {QtWebSockets::}{WebSocket::status}
  */

/*!
  \qmlmethod void WebSocket::sendTextMessage(string message)
  Sends \c message to the server.
  */

/*!
  \qmlmethod void WebSocket::sendBinaryMessage(ArrayBuffer message)
  \since 5.8
  Sends \c message to the server.
  */

#include "qqmlwebsocket.h"
#include <QtWebSockets/QWebSocket>

QT_BEGIN_NAMESPACE

QQmlWebSocket::QQmlWebSocket(QObject *parent) :
    QObject(parent),
    m_webSocket(),
    m_status(Closed),
    m_url(),
    m_isActive(false),
    m_componentCompleted(true),
    m_errorString()
{
}

QQmlWebSocket::QQmlWebSocket(QWebSocket *socket, QObject *parent) :
    QObject(parent),
    m_status(Closed),
    m_url(socket->requestUrl()),
    m_isActive(true),
    m_componentCompleted(true),
    m_errorString(socket->errorString())
{
    setSocket(socket);
    onStateChanged(socket->state());
}

QQmlWebSocket::~QQmlWebSocket()
{
}

qint64 QQmlWebSocket::sendTextMessage(const QString &message)
{
    if (m_status != Open) {
        setErrorString(tr("Messages can only be sent when the socket is open."));
        setStatus(Error);
        return 0;
    }
    return m_webSocket->sendTextMessage(message);
}

qint64 QQmlWebSocket::sendBinaryMessage(const QByteArray &message)
{
    if (m_status != Open) {
        setErrorString(tr("Messages can only be sent when the socket is open."));
        setStatus(Error);
        return 0;
    }
    return m_webSocket->sendBinaryMessage(message);
}

QUrl QQmlWebSocket::url() const
{
    return m_url;
}

void QQmlWebSocket::setUrl(const QUrl &url)
{
    if (m_url == url) {
        return;
    }
    if (m_webSocket && (m_status == Open)) {
        m_webSocket->close();
    }
    m_url = url;
    Q_EMIT urlChanged();
    if (m_webSocket) {
        m_webSocket->open(m_url);
    }
}

QQmlWebSocket::Status QQmlWebSocket::status() const
{
    return m_status;
}

QString QQmlWebSocket::errorString() const
{
    return m_errorString;
}

void QQmlWebSocket::classBegin()
{
    m_componentCompleted = false;
    m_errorString = tr("QQmlWebSocket is not ready.");
    m_status = Closed;
}

void QQmlWebSocket::componentComplete()
{
    setSocket(new QWebSocket);

    m_componentCompleted = true;

    open();
}

void QQmlWebSocket::setSocket(QWebSocket *socket)
{
    m_webSocket.reset(socket);
    if (m_webSocket) {
        // explicit ownership via QScopedPointer
        m_webSocket->setParent(Q_NULLPTR);
        connect(m_webSocket.data(), &QWebSocket::textMessageReceived,
                this, &QQmlWebSocket::textMessageReceived);
        connect(m_webSocket.data(), &QWebSocket::binaryMessageReceived,
                this, &QQmlWebSocket::binaryMessageReceived);
        typedef void (QWebSocket::* ErrorSignal)(QAbstractSocket::SocketError);
        connect(m_webSocket.data(), static_cast<ErrorSignal>(&QWebSocket::error),
                this, &QQmlWebSocket::onError);
        connect(m_webSocket.data(), &QWebSocket::stateChanged,
                this, &QQmlWebSocket::onStateChanged);
    }
}

void QQmlWebSocket::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    setErrorString(m_webSocket->errorString());
    setStatus(Error);
}

void QQmlWebSocket::onStateChanged(QAbstractSocket::SocketState state)
{
    switch (state)
    {
        case QAbstractSocket::ConnectingState:
        case QAbstractSocket::BoundState:
        case QAbstractSocket::HostLookupState:
        {
            setStatus(Connecting);
            break;
        }
        case QAbstractSocket::UnconnectedState:
        {
            setStatus(Closed);
            break;
        }
        case QAbstractSocket::ConnectedState:
        {
            setStatus(Open);
            break;
        }
        case QAbstractSocket::ClosingState:
        {
            setStatus(Closing);
            break;
        }
        default:
        {
            setStatus(Connecting);
            break;
        }
    }
}

void QQmlWebSocket::setStatus(QQmlWebSocket::Status status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    if (status != Error) {
        setErrorString();
    }
    Q_EMIT statusChanged(m_status);
}

void QQmlWebSocket::setActive(bool active)
{
    if (m_isActive == active) {
        return;
    }
    m_isActive = active;
    Q_EMIT activeChanged(m_isActive);
    if (!m_componentCompleted) {
        return;
    }
    if (m_isActive) {
        open();
    } else {
        close();
    }
}

bool QQmlWebSocket::isActive() const
{
    return m_isActive;
}

void QQmlWebSocket::open()
{
    if (m_componentCompleted && m_isActive && m_url.isValid() && Q_LIKELY(m_webSocket)) {
        m_webSocket->open(m_url);
    }
}

void QQmlWebSocket::close()
{
    if (m_componentCompleted && Q_LIKELY(m_webSocket)) {
        m_webSocket->close();
    }
}

void QQmlWebSocket::setErrorString(QString errorString)
{
    if (m_errorString == errorString) {
        return;
    }
    m_errorString = errorString;
    Q_EMIT errorStringChanged(m_errorString);
}

QT_END_NAMESPACE
