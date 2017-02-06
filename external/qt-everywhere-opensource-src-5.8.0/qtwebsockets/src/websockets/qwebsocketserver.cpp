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
    \class QWebSocketServer

    \inmodule QtWebSockets
    \since 5.3

    \brief Implements a WebSocket-based server.

    It is modeled after QTcpServer, and behaves the same. So, if you know how to use QTcpServer,
    you know how to use QWebSocketServer.
    This class makes it possible to accept incoming WebSocket connections.
    You can specify the port or have QWebSocketServer pick one automatically.
    You can listen on a specific address or on all the machine's addresses.
    Call listen() to have the server listen for incoming connections.

    The newConnection() signal is then emitted each time a client connects to the server.
    Call nextPendingConnection() to accept the pending connection as a connected QWebSocket.
    The function returns a pointer to a QWebSocket in QAbstractSocket::ConnectedState that you can
    use for communicating with the client.

    If an error occurs, serverError() returns the type of error, and errorString() can be called
    to get a human readable description of what happened.

    When listening for connections, the address and port on which the server is listening are
    available as serverAddress() and serverPort().

    Calling close() makes QWebSocketServer stop listening for incoming connections.

    QWebSocketServer currently does not support
    \l {WebSocket Extensions} and
    \l {WebSocket Subprotocols}.

    \note When working with self-signed certificates, \l{Firefox bug 594502} prevents \l{Firefox} to
    connect to a secure WebSocket server. To work around this problem, first browse to the
    secure WebSocket server using HTTPS. FireFox will indicate that the certificate is invalid.
    From here on, the certificate can be added to the exceptions. After this, the secure WebSockets
    connection should work.

    QWebSocketServer only supports version 13 of the WebSocket protocol, as outlined in \l{RFC 6455}.

    \sa {WebSocket Server Example}, QWebSocket
*/

/*!
  \page echoserver.html example
  \title WebSocket server example
  \brief A sample WebSocket server echoing back messages sent to it.

  \section1 Description
  The echoserver example implements a WebSocket server that echoes back everything that is sent
  to it.
  \section1 Code
  We start by creating a QWebSocketServer (`new QWebSocketServer()`). After the creation, we listen
  on all local network interfaces (`QHostAddress::Any`) on the specified \a port.
  \snippet echoserver/echoserver.cpp constructor
  If listening is successful, we connect the `newConnection()` signal to the slot
  `onNewConnection()`.
  The `newConnection()` signal will be thrown whenever a new WebSocket client is connected to our
  server.

  \snippet echoserver/echoserver.cpp onNewConnection
  When a new connection is received, the client QWebSocket is retrieved (`nextPendingConnection()`),
  and the signals we are interested in are connected to our slots
  (`textMessageReceived()`, `binaryMessageReceived()` and `disconnected()`).
  The client socket is remembered in a list, in case we would like to use it later
  (in this example, nothing is done with it).

  \snippet echoserver/echoserver.cpp processTextMessage
  Whenever `processTextMessage()` is triggered, we retrieve the sender, and if valid, send back the
  original message (`sendTextMessage()`).
  The same is done with binary messages.
  \snippet echoserver/echoserver.cpp processBinaryMessage
  The only difference is that the message now is a QByteArray instead of a QString.

  \snippet echoserver/echoserver.cpp socketDisconnected
  Whenever a socket is disconnected, we remove it from the clients list and delete the socket.
  Note: it is best to use `deleteLater()` to delete the socket.
*/

/*!
    \fn void QWebSocketServer::acceptError(QAbstractSocket::SocketError socketError)
    This signal is emitted when the acceptance of a new connection results in an error.
    The \a socketError parameter describes the type of error that occurred.

    \sa pauseAccepting(), resumeAccepting()
*/

/*!
    \fn void QWebSocketServer::serverError(QWebSocketProtocol::CloseCode closeCode)
    This signal is emitted when an error occurs during the setup of a WebSocket connection.
    The \a closeCode parameter describes the type of error that occurred

    \sa errorString()
*/

/*!
    \fn void QWebSocketServer::newConnection()
    This signal is emitted every time a new connection is available.

    \sa hasPendingConnections(), nextPendingConnection()
*/

/*!
    \fn void QWebSocketServer::closed()
    This signal is emitted when the server closed its connection.

    \sa close()
*/

/*!
    \fn void QWebSocketServer::originAuthenticationRequired(QWebSocketCorsAuthenticator *authenticator)
    This signal is emitted when a new connection is requested.
    The slot connected to this signal should indicate whether the origin
    (which can be determined by the origin() call) is allowed in the \a authenticator object
    (by issuing \l{QWebSocketCorsAuthenticator::}{setAllowed()}).

    If no slot is connected to this signal, all origins will be accepted by default.

    \note It is not possible to use a QueuedConnection to connect to
    this signal, as the connection will always succeed.
*/

/*!
    \fn void QWebSocketServer::peerVerifyError(const QSslError &error)

    QWebSocketServer can emit this signal several times during the SSL handshake,
    before encryption has been established, to indicate that an error has
    occurred while establishing the identity of the peer. The \a error is
    usually an indication that QWebSocketServer is unable to securely identify the
    peer.

    This signal provides you with an early indication when something is wrong.
    By connecting to this signal, you can manually choose to tear down the
    connection from inside the connected slot before the handshake has
    completed. If no action is taken, QWebSocketServer will proceed to emitting
    QWebSocketServer::sslErrors().

    \sa sslErrors()
*/

/*!
    \fn void QWebSocketServer::sslErrors(const QList<QSslError> &errors)

    QWebSocketServer emits this signal after the SSL handshake to indicate that one
    or more errors have occurred while establishing the identity of the
    peer. The errors are usually an indication that QWebSocketServer is unable to
    securely identify the peer. Unless any action is taken, the connection
    will be dropped after this signal has been emitted.

    \a errors contains one or more errors that prevent QSslSocket from
    verifying the identity of the peer.

    \sa peerVerifyError()
*/

/*!
    \fn void QWebSocketServer::preSharedKeyAuthenticationRequired(QSslPreSharedKeyAuthenticator *authenticator)
    \since 5.8

    QWebSocketServer emits this signal when it negotiates a PSK ciphersuite, and
    therefore a PSK authentication is then required.

    When using PSK, the client must send to the server a valid identity and a
    valid pre shared key, in order for the SSL handshake to continue.
    Applications can provide this information in a slot connected to this
    signal, by filling in the passed \a authenticator object according to their
    needs.

    \note Ignoring this signal, or failing to provide the required credentials,
    will cause the handshake to fail, and therefore the connection to be aborted.

    \note The \a authenticator object is owned by the socket and must not be
    deleted by the application.

    \sa QSslPreSharedKeyAuthenticator
    \sa QSslSocket::preSharedKeyAuthenticationRequired()
*/

/*!
  \enum QWebSocketServer::SslMode
  Indicates whether the server operates over wss (SecureMode) or ws (NonSecureMode)

  \value SecureMode The server operates in secure mode (over wss)
  \value NonSecureMode The server operates in non-secure mode (over ws)
  */

#include "qwebsocketprotocol.h"
#include "qwebsocket.h"
#include "qwebsocketserver.h"
#include "qwebsocketserver_p.h"

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QNetworkProxy>

#ifndef QT_NO_SSL
#include <QtNetwork/QSslConfiguration>
#endif

QT_BEGIN_NAMESPACE

/*!
    Constructs a new QWebSocketServer with the given \a serverName.
    The \a serverName will be used in the HTTP handshake phase to identify the server.
    It can be empty, in which case an empty server name will be sent to the client.
    The \a secureMode parameter indicates whether the server operates over wss (\l{SecureMode})
    or over ws (\l{NonSecureMode}).

    \a parent is passed to the QObject constructor.
 */
QWebSocketServer::QWebSocketServer(const QString &serverName, SslMode secureMode,
                                   QObject *parent) :
    QObject(*(new QWebSocketServerPrivate(serverName,
                                      #ifndef QT_NO_SSL
                                      (secureMode == SecureMode) ?
                                          QWebSocketServerPrivate::SecureMode :
                                          QWebSocketServerPrivate::NonSecureMode,
                                      #else
                                      QWebSocketServerPrivate::NonSecureMode,
                                      #endif
                                      this)), parent)
{
#ifdef QT_NO_SSL
    Q_UNUSED(secureMode)
#endif
    Q_D(QWebSocketServer);
    d->init();
}

/*!
    Destroys the QWebSocketServer object. If the server is listening for connections,
    the socket is automatically closed.
    Any client \l{QWebSocket}s that are still queued are closed and deleted.

    \sa close()
 */
QWebSocketServer::~QWebSocketServer()
{
    Q_D(QWebSocketServer);
    d->close(true);
}

/*!
  Closes the server. The server will no longer listen for incoming connections.
 */
void QWebSocketServer::close()
{
    Q_D(QWebSocketServer);
    d->close();
}

/*!
    Returns a human readable description of the last error that occurred.
    If no error occurred, an empty string is returned.

    \sa serverError()
*/
QString QWebSocketServer::errorString() const
{
    Q_D(const QWebSocketServer);
    return d->errorString();
}

/*!
    Returns true if the server has pending connections; otherwise returns false.

    \sa nextPendingConnection(), setMaxPendingConnections()
 */
bool QWebSocketServer::hasPendingConnections() const
{
    Q_D(const QWebSocketServer);
    return d->hasPendingConnections();
}

/*!
    Returns true if the server is currently listening for incoming connections;
    otherwise returns false. If listening fails, error() will return the reason.

    \sa listen(), error()
 */
bool QWebSocketServer::isListening() const
{
    Q_D(const QWebSocketServer);
    return d->isListening();
}

/*!
    Tells the server to listen for incoming connections on address \a address and port \a port.
    If \a port is 0, a port is chosen automatically.
    If \a address is QHostAddress::Any, the server will listen on all network interfaces.

    Returns true on success; otherwise returns false.

    \sa isListening()
 */
bool QWebSocketServer::listen(const QHostAddress &address, quint16 port)
{
    Q_D(QWebSocketServer);
    return d->listen(address, port);
}

/*!
    Returns the maximum number of pending accepted connections. The default is 30.

    \sa setMaxPendingConnections(), hasPendingConnections()
 */
int QWebSocketServer::maxPendingConnections() const
{
    Q_D(const QWebSocketServer);
    return d->maxPendingConnections();
}

/*!
    Returns the next pending connection as a connected QWebSocket object.
    QWebSocketServer does not take ownership of the returned QWebSocket object.
    It is up to the caller to delete the object explicitly when it will no longer be used,
    otherwise a memory leak will occur.
    Q_NULLPTR is returned if this function is called when there are no pending connections.

    Note: The returned QWebSocket object cannot be used from another thread.

    \sa hasPendingConnections()
*/
QWebSocket *QWebSocketServer::nextPendingConnection()
{
    Q_D(QWebSocketServer);
    return d->nextPendingConnection();
}

/*!
    Pauses incoming new connections. Queued connections will remain in queue.
    \sa resumeAccepting()
 */
void QWebSocketServer::pauseAccepting()
{
    Q_D(QWebSocketServer);
    d->pauseAccepting();
}

#ifndef QT_NO_NETWORKPROXY
/*!
    Returns the network proxy for this server. By default QNetworkProxy::DefaultProxy is used.

    \sa setProxy()
*/
QNetworkProxy QWebSocketServer::proxy() const
{
    Q_D(const QWebSocketServer);
    return d->proxy();
}

/*!
    Sets the explicit network proxy for this server to \a networkProxy.

    To disable the use of a proxy, use the QNetworkProxy::NoProxy proxy type:

    \code
        server->setProxy(QNetworkProxy::NoProxy);
    \endcode

    \sa proxy()
*/
void QWebSocketServer::setProxy(const QNetworkProxy &networkProxy)
{
    Q_D(QWebSocketServer);
    d->setProxy(networkProxy);
}
#endif

#ifndef QT_NO_SSL
/*!
    Sets the SSL configuration for the QWebSocketServer to \a sslConfiguration.
    This method has no effect if QWebSocketServer runs in non-secure mode
    (QWebSocketServer::NonSecureMode).

    \sa sslConfiguration(), SslMode
 */
void QWebSocketServer::setSslConfiguration(const QSslConfiguration &sslConfiguration)
{
    Q_D(QWebSocketServer);
    d->setSslConfiguration(sslConfiguration);
}

/*!
    Returns the SSL configuration used by the QWebSocketServer.
    If the server is not running in secure mode (QWebSocketServer::SecureMode),
    this method returns QSslConfiguration::defaultConfiguration().

    \sa setSslConfiguration(), SslMode, QSslConfiguration::defaultConfiguration()
 */
QSslConfiguration QWebSocketServer::sslConfiguration() const
{
    Q_D(const QWebSocketServer);
    return d->sslConfiguration();
}
#endif

/*!
    Resumes accepting new connections.
    \sa pauseAccepting()
 */
void QWebSocketServer::resumeAccepting()
{
    Q_D(QWebSocketServer);
    d->resumeAccepting();
}

/*!
    Sets the server name that will be used during the HTTP handshake phase to the given
    \a serverName.
    The \a serverName can be empty, in which case an empty server name will be sent to the client.
    Existing connected clients will not be notified of this change, only newly connecting clients
    will see this new name.
 */
void QWebSocketServer::setServerName(const QString &serverName)
{
    Q_D(QWebSocketServer);
    d->setServerName(serverName);
}

/*!
    Returns the server name that is used during the http handshake phase.
 */
QString QWebSocketServer::serverName() const
{
    Q_D(const QWebSocketServer);
    return d->serverName();
}

/*!
    Returns the server's address if the server is listening for connections; otherwise returns
    QHostAddress::Null.

    \sa serverPort(), listen()
 */
QHostAddress QWebSocketServer::serverAddress() const
{
    Q_D(const QWebSocketServer);
    return d->serverAddress();
}

/*!
    Returns the secure mode the server is running in.

    \sa QWebSocketServer(), SslMode
 */
QWebSocketServer::SslMode QWebSocketServer::secureMode() const
{
#ifndef QT_NO_SSL
    Q_D(const QWebSocketServer);
    return (d->secureMode() == QWebSocketServerPrivate::SecureMode) ?
                QWebSocketServer::SecureMode : QWebSocketServer::NonSecureMode;
#else
    return QWebSocketServer::NonSecureMode;
#endif
}

/*!
    Returns an error code for the last error that occurred.
    If no error occurred, QWebSocketProtocol::CloseCodeNormal is returned.

    \sa errorString()
 */
QWebSocketProtocol::CloseCode QWebSocketServer::error() const
{
    Q_D(const QWebSocketServer);
    return d->serverError();
}

/*!
    Returns the server's port if the server is listening for connections; otherwise returns 0.

    \sa serverAddress(), listen()
 */
quint16 QWebSocketServer::serverPort() const
{
    Q_D(const QWebSocketServer);
    return d->serverPort();
}

/*!
    Returns a URL clients can use to connect to this server if the server is listening for connections.
    Otherwise an invalid URL is returned.

    \sa serverPort(), serverAddress(), listen()
 */
QUrl QWebSocketServer::serverUrl() const
{
    QUrl url;

    if (!isListening()) {
        return url;
    }

    switch (secureMode()) {
    case NonSecureMode:
        url.setScheme(QStringLiteral("ws"));
        break;
    #ifndef QT_NO_SSL
    case SecureMode:
        url.setScheme(QStringLiteral("wss"));
        break;
    #endif
    }

    url.setPort(serverPort());

    if (serverAddress() == QHostAddress(QHostAddress::Any)) {
        // NOTE: On Windows at least, clients cannot connect to QHostAddress::Any
        // so in that case we always return LocalHost instead.
        url.setHost(QHostAddress(QHostAddress::LocalHost).toString());
    } else {
        url.setHost(serverAddress().toString());
    }

    return url;
}

/*!
    Sets the maximum number of pending accepted connections to \a numConnections.
    WebSocketServer will accept no more than \a numConnections incoming connections before
    nextPendingConnection() is called.
    By default, the limit is 30 pending connections.

    QWebSocketServer will emit the error() signal with
    the QWebSocketProtocol::CloseCodeAbnormalDisconnection close code
    when the maximum of connections has been reached.
    The WebSocket handshake will fail and the socket will be closed.

    \sa maxPendingConnections(), hasPendingConnections()
 */
void QWebSocketServer::setMaxPendingConnections(int numConnections)
{
    Q_D(QWebSocketServer);
    d->setMaxPendingConnections(numConnections);
}

/*!
    Sets the socket descriptor this server should use when listening for incoming connections to
    \a socketDescriptor.

    Returns true if the socket is set successfully; otherwise returns false.
    The socket is assumed to be in listening state.

    \sa socketDescriptor(), isListening()
 */
bool QWebSocketServer::setSocketDescriptor(int socketDescriptor)
{
    Q_D(QWebSocketServer);
    return d->setSocketDescriptor(socketDescriptor);
}

/*!
    Returns the native socket descriptor the server uses to listen for incoming instructions,
    or -1 if the server is not listening.
    If the server is using QNetworkProxy, the returned descriptor may not be usable with
    native socket functions.

    \sa setSocketDescriptor(), isListening()
 */
int QWebSocketServer::socketDescriptor() const
{
    Q_D(const QWebSocketServer);
    return d->socketDescriptor();
}

/*!
  Returns a list of WebSocket versions that this server is supporting.
 */
QList<QWebSocketProtocol::Version> QWebSocketServer::supportedVersions() const
{
    Q_D(const QWebSocketServer);
    return d->supportedVersions();
}

QT_END_NAMESPACE
