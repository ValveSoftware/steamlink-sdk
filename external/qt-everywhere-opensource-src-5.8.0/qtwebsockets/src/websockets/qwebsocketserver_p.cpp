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

#include "qwebsocketserver.h"
#include "qwebsocketserver_p.h"
#ifndef QT_NO_SSL
#include "qsslserver_p.h"
#endif
#include "qwebsocketprotocol.h"
#include "qwebsockethandshakerequest_p.h"
#include "qwebsockethandshakeresponse_p.h"
#include "qwebsocket.h"
#include "qwebsocket_p.h"
#include "qwebsocketcorsauthenticator.h"

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QNetworkProxy>

QT_BEGIN_NAMESPACE

//both constants are taken from the default settings of Apache
//see: http://httpd.apache.org/docs/2.2/mod/core.html#limitrequestfieldsize and
//http://httpd.apache.org/docs/2.2/mod/core.html#limitrequestfields
const int MAX_HEADERLINE_LENGTH = 8 * 1024; //maximum length of a http request header line
const int MAX_HEADERLINES = 100;            //maximum number of http request header lines

/*!
    \internal
 */
QWebSocketServerPrivate::QWebSocketServerPrivate(const QString &serverName,
                                                 QWebSocketServerPrivate::SslMode secureMode,
                                                 QWebSocketServer * const pWebSocketServer) :
    QObjectPrivate(),
    q_ptr(pWebSocketServer),
    m_pTcpServer(Q_NULLPTR),
    m_serverName(serverName),
    m_secureMode(secureMode),
    m_pendingConnections(),
    m_error(QWebSocketProtocol::CloseCodeNormal),
    m_errorString(),
    m_maxPendingConnections(30)
{
    Q_ASSERT(pWebSocketServer);
}

/*!
    \internal
 */
void QWebSocketServerPrivate::init()
{
    if (m_secureMode == NonSecureMode) {
        m_pTcpServer = new QTcpServer(q_ptr);
        if (Q_LIKELY(m_pTcpServer))
            QObjectPrivate::connect(m_pTcpServer, &QTcpServer::newConnection,
                                    this, &QWebSocketServerPrivate::onNewConnection);
        else
            qFatal("Could not allocate memory for tcp server.");
    } else {
#ifndef QT_NO_SSL
        QSslServer *pSslServer = new QSslServer(q_ptr);
        m_pTcpServer = pSslServer;
        if (Q_LIKELY(m_pTcpServer)) {
            QObjectPrivate::connect(pSslServer, &QSslServer::newEncryptedConnection,
                                    this, &QWebSocketServerPrivate::onNewConnection,
                                    Qt::QueuedConnection);
            QObject::connect(pSslServer, &QSslServer::peerVerifyError,
                             q_ptr, &QWebSocketServer::peerVerifyError);
            QObject::connect(pSslServer, &QSslServer::sslErrors,
                             q_ptr, &QWebSocketServer::sslErrors);
            QObject::connect(pSslServer, &QSslServer::preSharedKeyAuthenticationRequired,
                             q_ptr, &QWebSocketServer::preSharedKeyAuthenticationRequired);
        }
#else
        qFatal("SSL not supported on this platform.");
#endif
    }
    QObject::connect(m_pTcpServer, &QTcpServer::acceptError, q_ptr, &QWebSocketServer::acceptError);
}

/*!
    \internal
 */
QWebSocketServerPrivate::~QWebSocketServerPrivate()
{
}

/*!
    \internal
 */
void QWebSocketServerPrivate::close(bool aboutToDestroy)
{
    Q_Q(QWebSocketServer);
    m_pTcpServer->close();
    while (!m_pendingConnections.isEmpty()) {
        QWebSocket *pWebSocket = m_pendingConnections.dequeue();
        pWebSocket->close(QWebSocketProtocol::CloseCodeGoingAway,
                          QWebSocketServer::tr("Server closed."));
        pWebSocket->deleteLater();
    }
    if (!aboutToDestroy) {
        //emit signal via the event queue, so the server gets time
        //to process any hanging events, like flushing buffers aso
        QMetaObject::invokeMethod(q, "closed", Qt::QueuedConnection);
    }
}

/*!
    \internal
 */
QString QWebSocketServerPrivate::errorString() const
{
    if (m_errorString.isEmpty())
        return m_pTcpServer->errorString();
    else
        return m_errorString;
}

/*!
    \internal
 */
bool QWebSocketServerPrivate::hasPendingConnections() const
{
    return !m_pendingConnections.isEmpty();
}

/*!
    \internal
 */
bool QWebSocketServerPrivate::isListening() const
{
    return m_pTcpServer->isListening();
}

/*!
    \internal
 */
bool QWebSocketServerPrivate::listen(const QHostAddress &address, quint16 port)
{
    bool success = m_pTcpServer->listen(address, port);
    if (!success)
        setErrorFromSocketError(m_pTcpServer->serverError(), m_pTcpServer->errorString());
    return success;
}

/*!
    \internal
 */
int QWebSocketServerPrivate::maxPendingConnections() const
{
    return m_maxPendingConnections;
}

/*!
    \internal
 */
void QWebSocketServerPrivate::addPendingConnection(QWebSocket *pWebSocket)
{
    if (m_pendingConnections.size() < maxPendingConnections())
        m_pendingConnections.enqueue(pWebSocket);
}

/*!
    \internal
 */
void QWebSocketServerPrivate::setErrorFromSocketError(QAbstractSocket::SocketError error,
                                                      const QString &errorDescription)
{
    Q_UNUSED(error);
    setError(QWebSocketProtocol::CloseCodeAbnormalDisconnection, errorDescription);
}

/*!
    \internal
 */
QWebSocket *QWebSocketServerPrivate::nextPendingConnection()
{
    QWebSocket *pWebSocket = Q_NULLPTR;
    if (Q_LIKELY(!m_pendingConnections.isEmpty()))
        pWebSocket = m_pendingConnections.dequeue();
    return pWebSocket;
}

/*!
    \internal
 */
void QWebSocketServerPrivate::pauseAccepting()
{
    m_pTcpServer->pauseAccepting();
}

#ifndef QT_NO_NETWORKPROXY
/*!
    \internal
 */
QNetworkProxy QWebSocketServerPrivate::proxy() const
{
    return m_pTcpServer->proxy();
}

/*!
    \internal
 */
void QWebSocketServerPrivate::setProxy(const QNetworkProxy &networkProxy)
{
    m_pTcpServer->setProxy(networkProxy);
}
#endif
/*!
    \internal
 */
void QWebSocketServerPrivate::resumeAccepting()
{
    m_pTcpServer->resumeAccepting();
}

/*!
    \internal
 */
QHostAddress QWebSocketServerPrivate::serverAddress() const
{
    return m_pTcpServer->serverAddress();
}

/*!
    \internal
 */
QWebSocketProtocol::CloseCode QWebSocketServerPrivate::serverError() const
{
    return m_error;
}

/*!
    \internal
 */
quint16 QWebSocketServerPrivate::serverPort() const
{
    return m_pTcpServer->serverPort();
}

/*!
    \internal
 */
void QWebSocketServerPrivate::setMaxPendingConnections(int numConnections)
{
    if (m_pTcpServer->maxPendingConnections() <= numConnections)
        m_pTcpServer->setMaxPendingConnections(numConnections + 1);
    m_maxPendingConnections = numConnections;
}

/*!
    \internal
 */
bool QWebSocketServerPrivate::setSocketDescriptor(qintptr socketDescriptor)
{
    return m_pTcpServer->setSocketDescriptor(socketDescriptor);
}

/*!
    \internal
 */
qintptr QWebSocketServerPrivate::socketDescriptor() const
{
    return m_pTcpServer->socketDescriptor();
}

/*!
    \internal
 */
QList<QWebSocketProtocol::Version> QWebSocketServerPrivate::supportedVersions() const
{
    QList<QWebSocketProtocol::Version> supportedVersions;
    supportedVersions << QWebSocketProtocol::currentVersion();	//we only support V13
    return supportedVersions;
}

/*!
    \internal
 */
QStringList QWebSocketServerPrivate::supportedProtocols() const
{
    QStringList supportedProtocols;
    return supportedProtocols;	//no protocols are currently supported
}

/*!
    \internal
 */
QStringList QWebSocketServerPrivate::supportedExtensions() const
{
    QStringList supportedExtensions;
    return supportedExtensions;	//no extensions are currently supported
}

/*!
  \internal
 */
void QWebSocketServerPrivate::setServerName(const QString &serverName)
{
    if (m_serverName != serverName)
        m_serverName = serverName;
}

/*!
  \internal
 */
QString QWebSocketServerPrivate::serverName() const
{
    return m_serverName;
}

/*!
  \internal
 */
QWebSocketServerPrivate::SslMode QWebSocketServerPrivate::secureMode() const
{
    return m_secureMode;
}

#ifndef QT_NO_SSL
void QWebSocketServerPrivate::setSslConfiguration(const QSslConfiguration &sslConfiguration)
{
    if (m_secureMode == SecureMode)
        qobject_cast<QSslServer *>(m_pTcpServer)->setSslConfiguration(sslConfiguration);
}

QSslConfiguration QWebSocketServerPrivate::sslConfiguration() const
{
    if (m_secureMode == SecureMode)
        return qobject_cast<QSslServer *>(m_pTcpServer)->sslConfiguration();
    else
        return QSslConfiguration::defaultConfiguration();
}
#endif

void QWebSocketServerPrivate::setError(QWebSocketProtocol::CloseCode code, const QString &errorString)
{
    if ((m_error != code) || (m_errorString != errorString)) {
        Q_Q(QWebSocketServer);
        m_error = code;
        m_errorString = errorString;
        Q_EMIT q->serverError(code);
    }
}

/*!
    \internal
 */
void QWebSocketServerPrivate::onNewConnection()
{
    while (m_pTcpServer->hasPendingConnections()) {
        QTcpSocket *pTcpSocket = m_pTcpServer->nextPendingConnection();
        //use a queued connection because a QSslSocket
        //needs the event loop to process incoming data
        //if not queued, data is incomplete when handshakeReceived is called
        QObjectPrivate::connect(pTcpSocket, &QTcpSocket::readyRead,
                                this, &QWebSocketServerPrivate::handshakeReceived,
                                Qt::QueuedConnection);
    }
}

/*!
    \internal
 */
void QWebSocketServerPrivate::onCloseConnection()
{
    if (Q_LIKELY(currentSender)) {
        QTcpSocket *pTcpSocket = qobject_cast<QTcpSocket*>(currentSender->sender);
        if (Q_LIKELY(pTcpSocket))
            pTcpSocket->close();
    }
}

/*!
    \internal
 */
void QWebSocketServerPrivate::handshakeReceived()
{
    if (Q_UNLIKELY(!currentSender)) {
        return;
    }
    QTcpSocket *pTcpSocket = qobject_cast<QTcpSocket*>(currentSender->sender);
    if (Q_UNLIKELY(!pTcpSocket)) {
        return;
    }
    //When using Google Chrome the handshake in received in two parts.
    //Therefore, the readyRead signal is emitted twice.
    //This is a guard against the BEAST attack.
    //See: https://www.imperialviolet.org/2012/01/15/beastfollowup.html
    //For Safari, the handshake is delivered at once
    //FIXME: For FireFox, the readyRead signal is never emitted
    //This is a bug in FireFox (see https://bugzilla.mozilla.org/show_bug.cgi?id=594502)
    if (!pTcpSocket->canReadLine()) {
        return;
    }
    disconnect(pTcpSocket, &QTcpSocket::readyRead,
               this, &QWebSocketServerPrivate::handshakeReceived);
    Q_Q(QWebSocketServer);
    bool success = false;
    bool isSecure = false;

    if (m_pendingConnections.length() >= maxPendingConnections()) {
        pTcpSocket->close();
        pTcpSocket->deleteLater();
        setError(QWebSocketProtocol::CloseCodeAbnormalDisconnection,
                 QWebSocketServer::tr("Too many pending connections."));
        return;
    }

    QWebSocketHandshakeRequest request(pTcpSocket->peerPort(), isSecure);
    QTextStream textStream(pTcpSocket);
    request.readHandshake(textStream, MAX_HEADERLINE_LENGTH, MAX_HEADERLINES);

    if (request.isValid()) {
        QWebSocketCorsAuthenticator corsAuthenticator(request.origin());
        Q_EMIT q->originAuthenticationRequired(&corsAuthenticator);

        QWebSocketHandshakeResponse response(request,
                                             m_serverName,
                                             corsAuthenticator.allowed(),
                                             supportedVersions(),
                                             supportedProtocols(),
                                             supportedExtensions());

        if (response.isValid()) {
            QTextStream httpStream(pTcpSocket);
            httpStream << response;
            httpStream.flush();

            if (response.canUpgrade()) {
                QWebSocket *pWebSocket = QWebSocketPrivate::upgradeFrom(pTcpSocket,
                                                                        request,
                                                                        response);
                if (pWebSocket) {
                    addPendingConnection(pWebSocket);
                    Q_EMIT q->newConnection();
                    success = true;
                } else {
                    setError(QWebSocketProtocol::CloseCodeAbnormalDisconnection,
                             QWebSocketServer::tr("Upgrade to WebSocket failed."));
                }
            }
            else {
                setError(response.error(), response.errorString());
            }
        } else {
            setError(QWebSocketProtocol::CloseCodeProtocolError,
                     QWebSocketServer::tr("Invalid response received."));
        }
    }
    if (!success) {
        pTcpSocket->close();
    }
}

QT_END_NAMESPACE
