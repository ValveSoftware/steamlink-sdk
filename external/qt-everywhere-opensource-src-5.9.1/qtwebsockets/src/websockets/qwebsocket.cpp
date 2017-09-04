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
    \class QWebSocket

    \inmodule QtWebSockets
    \since 5.3
    \brief Implements a TCP socket that talks the WebSocket protocol.

    WebSockets is a web technology providing full-duplex communications channels over
    a single TCP connection.
    The WebSocket protocol was standardized by the IETF as
    \l {RFC 6455} in 2011.
    QWebSocket can both be used in a client application and server application.

    This class was modeled after QAbstractSocket.

    QWebSocket currently does not support
    \l {WebSocket Extensions} and
    \l {WebSocket Subprotocols}.

    QWebSocket only supports version 13 of the WebSocket protocol, as outlined in
    \l {RFC 6455}.

    \note Some proxies do not understand certain HTTP headers used during a WebSocket handshake.
    In that case, non-secure WebSocket connections fail. The best way to mitigate against
    this problem is to use WebSocket over a secure connection.

    \warning To generate masks, this implementation of WebSockets uses the cryptographically
    insecure qrand() function.
    For more information about the importance of good masking,
    see \l {"Talking to Yourself for Fun and Profit" by Lin-Shung Huang et al}.
    The best measure against attacks mentioned in the document above,
    is to use QWebSocket over a secure connection (\e wss://).
    In general, always be careful to not have 3rd party script access to
    a QWebSocket in your application.

    \sa QAbstractSocket, QTcpSocket

    \sa {QWebSocket client example}
*/

/*!
    \page echoclient.html example
    \title QWebSocket client example
    \brief A sample WebSocket client that sends a message and displays the message that
    it receives back.

    \section1 Description
    The EchoClient example implements a WebSocket client that sends a message to a WebSocket server
    and dumps the answer that it gets back.
    This example should ideally be used with the EchoServer example.
    \section1 Code
    We start by connecting to the `connected()` signal.
    \snippet echoclient/echoclient.cpp constructor
    After the connection, we open the socket to the given \a url.

    \snippet echoclient/echoclient.cpp onConnected
    When the client is connected successfully, we connect to the `onTextMessageReceived()` signal,
    and send out "Hello, world!".
    If connected with the EchoServer, we will receive the same message back.

    \snippet echoclient/echoclient.cpp onTextMessageReceived
    Whenever a message is received, we write it out.
*/

/*!
  \fn void QWebSocket::connected()
  \brief Emitted when a connection is successfully established.
  A connection is successfully established when the socket is connected
  and the handshake was successful.
  \sa open(), disconnected()
*/
/*!
  \fn void QWebSocket::disconnected()
  \brief Emitted when the socket is disconnected.
  \sa close(), connected()
*/
/*!
    \fn void QWebSocket::aboutToClose()

    This signal is emitted when the socket is about to close.
    Connect this signal if you have operations that need to be performed before the socket closes
    (e.g., if you have data in a separate buffer that needs to be written to the device).

    \sa close()
 */
/*!
\fn void QWebSocket::proxyAuthenticationRequired(const QNetworkProxy &proxy, QAuthenticator *authenticator)

This signal can be emitted when a \a proxy that requires
authentication is used. The \a authenticator object can then be
filled in with the required details to allow authentication and
continue the connection.

\note It is not possible to use a QueuedConnection to connect to
this signal, as the connection will fail if the authenticator has
not been filled in with new information when the signal returns.

\sa QAuthenticator, QNetworkProxy
*/
/*!
    \fn void QWebSocket::stateChanged(QAbstractSocket::SocketState state);

    This signal is emitted whenever QWebSocket's state changes.
    The \a state parameter is the new state.

    \note QAbstractSocket::ConnectedState is emitted after the handshake
    with the server has succeeded.

    QAbstractSocket::SocketState is not a registered metatype, so for queued
    connections, you will have to register it with Q_REGISTER_METATYPE() and
    qRegisterMetaType().

    \sa state()
*/
/*!
    \fn void QWebSocket::readChannelFinished()

    This signal is emitted when the input (reading) stream is closed in this device.
    It is emitted as soon as the closing is detected.

    \sa close()
*/
/*!
    \fn void QWebSocket::bytesWritten(qint64 bytes)

    This signal is emitted every time a payload of data has been written to the socket.
    The \a bytes argument is set to the number of bytes that were written in this payload.

    \note This signal has the same meaning both for secure and non-secure WebSockets.
    As opposed to QSslSocket, bytesWritten() is only emitted when encrypted data is effectively
    written (see QSslSocket::encryptedBytesWritten()).
    \sa close()
*/

/*!
    \fn void QWebSocket::textFrameReceived(const QString &frame, bool isLastFrame);

    This signal is emitted whenever a text frame is received. The \a frame contains the data and
    \a isLastFrame indicates whether this is the last frame of the complete message.

    This signal can be used to process large messages frame by frame, instead of waiting for the
    complete message to arrive.

    \sa binaryFrameReceived()
*/
/*!
    \fn void QWebSocket::binaryFrameReceived(const QByteArray &frame, bool isLastFrame);

    This signal is emitted whenever a binary frame is received. The \a frame contains the data and
    \a isLastFrame indicates whether this is the last frame of the complete message.

    This signal can be used to process large messages frame by frame, instead of waiting for the
    complete message to arrive.

    \sa textFrameReceived()
*/
/*!
    \fn void QWebSocket::textMessageReceived(const QString &message);

    This signal is emitted whenever a text message is received. The \a message contains the
    received text.

    \sa binaryMessageReceived()
*/
/*!
    \fn void QWebSocket::binaryMessageReceived(const QByteArray &message);

    This signal is emitted whenever a binary message is received. The \a message contains the
    received bytes.

    \sa textMessageReceived()
*/
/*!
    \fn void QWebSocket::error(QAbstractSocket::SocketError error);

    This signal is emitted after an error occurred. The \a error
    parameter describes the type of error that occurred.

    QAbstractSocket::SocketError is not a registered metatype, so for queued
    connections, you will have to register it with Q_DECLARE_METATYPE() and
    qRegisterMetaType().

    \sa error(), errorString()
*/
/*!
    \fn void QWebSocket::sslErrors(const QList<QSslError> &errors)
    QWebSocket emits this signal after the SSL handshake to indicate that one or more errors have
    occurred while establishing the identity of the peer.
    The errors are usually an indication that QWebSocket is unable to securely identify the peer.
    Unless any action is taken, the connection will be dropped after this signal has been emitted.
    If you want to continue connecting despite the errors that have occurred, you must call
    QWebSocket::ignoreSslErrors() from inside a slot connected to this signal.
    If you need to access the error list at a later point, you can call sslErrors()
    (without arguments).

    \a errors contains one or more errors that prevent QWebSocket from verifying the identity of
    the peer.

    \note You cannot use Qt::QueuedConnection when connecting to this signal, or calling
    QWebSocket::ignoreSslErrors() will have no effect.
*/
/*!
    \fn void QWebSocket::preSharedKeyAuthenticationRequired(QSslPreSharedKeyAuthenticator *authenticator)
    \since 5.8

    This signal is emitted if the SSL/TLS handshake negotiates a PSK
    ciphersuite, and therefore a PSK authentication is then required.

    When using PSK, the client must send to the server a valid identity and a
    valid pre shared key, in order for the SSL handshake to continue.
    Applications can provide this information in a slot connected to this
    signal, by filling in the passed \a authenticator object according to their
    needs.

    \note Ignoring this signal, or failing to provide the required credentials,
    will cause the handshake to fail, and therefore the connection to be aborted.

    \note The \a authenticator object is owned by the websocket and must not be
    deleted by the application.

    \sa QSslPreSharedKeyAuthenticator
    \sa QSslSocket::preSharedKeyAuthenticationRequired()
*/
/*!
    \fn void QWebSocket::pong(quint64 elapsedTime, const QByteArray &payload)

    Emitted when a pong message is received in reply to a previous ping.
    \a elapsedTime contains the roundtrip time in milliseconds and \a payload contains an optional
    payload that was sent with the ping.

    \sa ping()
  */
#include "qwebsocket.h"
#include "qwebsocket_p.h"

#include <QtCore/QUrl>
#include <QtNetwork/QTcpSocket>
#include <QtCore/QByteArray>
#include <QtNetwork/QHostAddress>

#include <QtCore/QDebug>

#include <limits>

QT_BEGIN_NAMESPACE

/*!
 * \brief Creates a new QWebSocket with the given \a origin,
 * the \a version of the protocol to use and \a parent.
 *
 * The \a origin of the client is as specified in \l {RFC 6454}.
 * (The \a origin is not required for non-web browser clients
 * (see \l {RFC 6455})).
 * The \a origin may not contain new line characters, otherwise the connection will be
 * aborted immediately during the handshake phase.
 * \note Currently only V13 (\l {RFC 6455}) is supported
 */
QWebSocket::QWebSocket(const QString &origin,
                       QWebSocketProtocol::Version version,
                       QObject *parent) :
    QObject(*(new QWebSocketPrivate(origin, version, this)), parent)
{
    Q_D(QWebSocket);
    d->init();
}

/*!
 * \brief Destroys the QWebSocket. Closes the socket if it is still open,
 * and releases any used resources.
 */
QWebSocket::~QWebSocket()
{
    Q_D(QWebSocket);
    d->closeGoingAway();
}

/*!
 * \brief Aborts the current socket and resets the socket.
 * Unlike close(), this function immediately closes the socket,
 * discarding any pending data in the write buffer.
 */
void QWebSocket::abort()
{
    Q_D(QWebSocket);
    d->abort();
}

/*!
 * Returns the type of error that last occurred
 * \sa errorString()
 */
QAbstractSocket::SocketError QWebSocket::error() const
{
    Q_D(const QWebSocket);
    return d->error();
}

//only called by QWebSocketPrivate::upgradeFrom
/*!
  \internal
 */
QWebSocket::QWebSocket(QTcpSocket *pTcpSocket,
                       QWebSocketProtocol::Version version, QObject *parent) :
    QObject(*(new QWebSocketPrivate(pTcpSocket, version, this)), parent)
{
    Q_D(QWebSocket);
    d->init();
}

/*!
 * Returns a human-readable description of the last error that occurred
 *
 * \sa error()
 */
QString QWebSocket::errorString() const
{
    Q_D(const QWebSocket);
    return d->errorString();
}

/*!
    This function writes as much as possible from the internal write buffer
    to the underlying network socket, without blocking.
    If any data was written, this function returns true; otherwise false is returned.
    Call this function if you need QWebSocket to start sending buffered data immediately.
    The number of bytes successfully written depends on the operating system.
    In most cases, you do not need to call this function,
    because QWebSocket will start sending data automatically
    once control goes back to the event loop.
*/
bool QWebSocket::flush()
{
    Q_D(QWebSocket);
    return d->flush();
}

/*!
    \brief Sends the given \a message over the socket as a text message and
    returns the number of bytes actually sent.

    \sa sendBinaryMessage()
 */
qint64 QWebSocket::sendTextMessage(const QString &message)
{
    Q_D(QWebSocket);
    return d->sendTextMessage(message);
}

/*!
    \brief Sends the given \a data over the socket as a binary message and
    returns the number of bytes actually sent.

    \sa sendTextMessage()
 */
qint64 QWebSocket::sendBinaryMessage(const QByteArray &data)
{
    Q_D(QWebSocket);
    return d->sendBinaryMessage(data);
}

/*!
    \brief Gracefully closes the socket with the given \a closeCode and \a reason.

    Any data in the write buffer is flushed before the socket is closed.
    The \a closeCode is a QWebSocketProtocol::CloseCode indicating the reason to close, and
    \a reason describes the reason of the closure more in detail
 */
void QWebSocket::close(QWebSocketProtocol::CloseCode closeCode, const QString &reason)
{
    Q_D(QWebSocket);
    d->close(closeCode, reason);
}

/*!
    \brief Opens a WebSocket connection using the given \a url.

    If the url contains newline characters (\\r\\n), then the error signal will be emitted
    with QAbstractSocket::ConnectionRefusedError as error type.
 */
void QWebSocket::open(const QUrl &url)
{
    Q_D(QWebSocket);
    QNetworkRequest request(url);
    d->open(request, true);
}

/*!
    \brief Opens a WebSocket connection using the given \a request.
    \since 5.6

    The \a request url will be used to open the WebSocket connection.
    Headers present in the request will be sent to the server in the upgrade request,
    together with the ones needed for the websocket handshake.
 */
void QWebSocket::open(const QNetworkRequest &request)
{
    Q_D(QWebSocket);
    d->open(request, true);
}

/*!
    \brief Pings the server to indicate that the connection is still alive.
    Additional \a payload can be sent along the ping message.

    The size of the \a payload cannot be bigger than 125.
    If it is larger, the \a payload is clipped to 125 bytes.

    \sa pong()
 */
void QWebSocket::ping(const QByteArray &payload)
{
    Q_D(QWebSocket);
    d->ping(payload);
}

#ifndef QT_NO_SSL
/*!
    This slot tells QWebSocket to ignore errors during QWebSocket's
    handshake phase and continue connecting. If you want to continue
    with the connection even if errors occur during the handshake
    phase, then you must call this slot, either from a slot connected
    to sslErrors(), or before the handshake phase. If you don't call
    this slot, either in response to errors or before the handshake,
    the connection will be dropped after the sslErrors() signal has
    been emitted.

    \warning Be sure to always let the user inspect the errors
    reported by the sslErrors() signal, and only call this method
    upon confirmation from the user that proceeding is ok.
    If there are unexpected errors, the connection should be aborted.
    Calling this method without inspecting the actual errors will
    most likely pose a security risk for your application. Use it
    with great care!

    \sa sslErrors(), QSslSocket::ignoreSslErrors(), QNetworkReply::ignoreSslErrors()
*/
void QWebSocket::ignoreSslErrors()
{
    Q_D(QWebSocket);
    d->ignoreSslErrors();
}

/*!
    \overload

    This method tells QWebSocket to ignore the errors given in \a errors.

    Note that you can set the expected certificate in the SSL error:
    If, for instance, you want to connect to a server that uses
    a self-signed certificate, consider the following snippet:

    \snippet src_websockets_ssl_qwebsocket.cpp 6

    Multiple calls to this function will replace the list of errors that
    were passed in previous calls.
    You can clear the list of errors you want to ignore by calling this
    function with an empty list.

    \sa sslErrors()
*/
void QWebSocket::ignoreSslErrors(const QList<QSslError> &errors)
{
    Q_D(QWebSocket);
    d->ignoreSslErrors(errors);
}

/*!
    Sets the socket's SSL configuration to be the contents of \a sslConfiguration.

    This function sets the local certificate, the ciphers, the private key and
    the CA certificates to those stored in \a sslConfiguration.
    It is not possible to set the SSL-state related fields.
    \sa sslConfiguration()
 */
void QWebSocket::setSslConfiguration(const QSslConfiguration &sslConfiguration)
{
    Q_D(QWebSocket);
    d->setSslConfiguration(sslConfiguration);
}

/*!
    Returns the socket's SSL configuration state.
    The default SSL configuration of a socket is to use the default ciphers,
    default CA certificates, no local private key or certificate.
    The SSL configuration also contains fields that can change with time without notice.

    \sa setSslConfiguration()
 */
QSslConfiguration QWebSocket::sslConfiguration() const
{
    Q_D(const QWebSocket);
    return d->sslConfiguration();
}

#endif  //not QT_NO_SSL

/*!
    \brief Returns the version the socket is currently using.
 */
QWebSocketProtocol::Version QWebSocket::version() const
{
    Q_D(const QWebSocket);
    return d->version();
}

/*!
    \brief Returns the name of the resource currently accessed.
 */
QString QWebSocket::resourceName() const
{
    Q_D(const QWebSocket);
    return d->resourceName();
}

/*!
    \brief Returns the url the socket is connected to or will connect to.
 */
QUrl QWebSocket::requestUrl() const
{
    Q_D(const QWebSocket);
    return d->request().url();
}

/*!
    \brief Returns the request that was or will be used to open this socket.
    \since 5.6
 */
QNetworkRequest QWebSocket::request() const
{
    Q_D(const QWebSocket);
    return d->request();
}

/*!
    \brief Returns the current origin.
 */
QString QWebSocket::origin() const
{
    Q_D(const QWebSocket);
    return d->origin();
}

/*!
    \brief Returns the code indicating why the socket was closed.
    \sa QWebSocketProtocol::CloseCode, closeReason()
 */
QWebSocketProtocol::CloseCode QWebSocket::closeCode() const
{
    Q_D(const QWebSocket);
    return d->closeCode();
}

/*!
    \brief Returns the reason why the socket was closed.
    \sa closeCode()
 */
QString QWebSocket::closeReason() const
{
    Q_D(const QWebSocket);
    return d->closeReason();
}

/*!
    \brief Returns the current state of the socket.
 */
QAbstractSocket::SocketState QWebSocket::state() const
{
    Q_D(const QWebSocket);
    return d->state();
}

/*!
    Returns the local address
 */
QHostAddress QWebSocket::localAddress() const
{
    Q_D(const QWebSocket);
    return d->localAddress();
}

/*!
    Returns the local port
 */
quint16 QWebSocket::localPort() const
{
    Q_D(const QWebSocket);
    return d->localPort();
}

/*!
    Returns the pause mode of this socket
 */
QAbstractSocket::PauseModes QWebSocket::pauseMode() const
{
    Q_D(const QWebSocket);
    return d->pauseMode();
}

/*!
    Returns the peer address
 */
QHostAddress QWebSocket::peerAddress() const
{
    Q_D(const QWebSocket);
    return d->peerAddress();
}

/*!
    Returns the peerName
 */
QString QWebSocket::peerName() const
{
    Q_D(const QWebSocket);
    return d->peerName();
}

/*!
    Returns the peerport
 */
quint16 QWebSocket::peerPort() const
{
    Q_D(const QWebSocket);
    return d->peerPort();
}

#ifndef QT_NO_NETWORKPROXY
/*!
    Returns the currently configured proxy
 */
QNetworkProxy QWebSocket::proxy() const
{
    Q_D(const QWebSocket);
    return d->proxy();
}

/*!
    Sets the proxy to \a networkProxy
 */
void QWebSocket::setProxy(const QNetworkProxy &networkProxy)
{
    Q_D(QWebSocket);
    d->setProxy(networkProxy);
}
#endif

/*!
    Sets the generator to use for creating masks to \a maskGenerator.
    The default QWebSocket generator can be reset by supplying a \e Q_NULLPTR.
    The mask generator can be changed at any time, even while the connection is open.
 */
void QWebSocket::setMaskGenerator(const QMaskGenerator *maskGenerator)
{
    Q_D(QWebSocket);
    d->setMaskGenerator(maskGenerator);
}

/*!
    Returns the mask generator that is currently used by this QWebSocket.
 */
const QMaskGenerator *QWebSocket::maskGenerator() const
{
    Q_D(const QWebSocket);
    return d->maskGenerator();
}

/*!
    Returns the size in bytes of the readbuffer that is used by the socket.
 */
qint64 QWebSocket::readBufferSize() const
{
    Q_D(const QWebSocket);
    return d->readBufferSize();
}

/*!
    Continues data transfer on the socket. This method should only be used after the socket
    has been set to pause upon notifications and a notification has been received.
    The only notification currently supported is sslErrors().
    Calling this method if the socket is not paused results in undefined behavior.

    \sa pauseMode(), setPauseMode()
 */
void QWebSocket::resume()
{
    Q_D(QWebSocket);
    d->resume();
}

/*!
    Controls whether to pause upon receiving a notification. The \a pauseMode parameter specifies
    the conditions in which the socket should be paused.

    The only notification currently supported is sslErrors().
    If set to PauseOnSslErrors, data transfer on the socket will be paused
    and needs to be enabled explicitly again by calling resume().
    By default, this option is set to PauseNever. This option must be called
    before connecting to the server, otherwise it will result in undefined behavior.

    \sa pauseMode(), resume()
 */
void QWebSocket::setPauseMode(QAbstractSocket::PauseModes pauseMode)
{
    Q_D(QWebSocket);
    d->setPauseMode(pauseMode);
}

/*!
    Sets the size of QWebSocket's internal read buffer to be \a size bytes.

    If the buffer size is limited to a certain size, QWebSocket won't buffer more than
    this size of data.
    Exceptionally, a buffer size of 0 means that the read buffer is unlimited and
    all incoming data is buffered. This is the default.
    This option is useful if you only read the data at certain points in time
    (for example, in a real-time streaming application) or if you want to protect your socket against
    receiving too much data, which may eventually cause your application to run out of memory.

    \sa readBufferSize()
*/
void QWebSocket::setReadBufferSize(qint64 size)
{
    Q_D(QWebSocket);
    d->setReadBufferSize(size);
}

/*!
    Returns \c true if the socket is ready for reading and writing; otherwise
    returns \c false.
 */
bool QWebSocket::isValid() const
{
    Q_D(const QWebSocket);
    return d->isValid();
}

QT_END_NAMESPACE
