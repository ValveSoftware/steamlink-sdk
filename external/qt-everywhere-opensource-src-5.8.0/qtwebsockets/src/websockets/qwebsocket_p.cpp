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

#include "qwebsocket.h"
#include "qwebsocket_p.h"
#include "qwebsocketprotocol_p.h"
#include "qwebsockethandshakerequest_p.h"
#include "qwebsockethandshakeresponse_p.h"
#include "qdefaultmaskgenerator_p.h"

#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QTcpSocket>
#include <QtCore/QByteArray>
#include <QtCore/QtEndian>
#include <QtCore/QCryptographicHash>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtNetwork/QHostAddress>
#include <QtCore/QStringBuilder>   //for more efficient string concatenation
#ifndef QT_NONETWORKPROXY
#include <QtNetwork/QNetworkProxy>
#endif
#ifndef QT_NO_SSL
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslError>
#include <QtNetwork/QSslPreSharedKeyAuthenticator>
#endif

#include <QtCore/QDebug>

#include <limits>

QT_BEGIN_NAMESPACE

const quint64 FRAME_SIZE_IN_BYTES = 512 * 512 * 2;	//maximum size of a frame when sending a message

QWebSocketConfiguration::QWebSocketConfiguration() :
#ifndef QT_NO_SSL
    m_sslConfiguration(QSslConfiguration::defaultConfiguration()),
    m_ignoredSslErrors(),
    m_ignoreSslErrors(false),
#endif
#ifndef QT_NO_NETWORKPROXY
    m_proxy(QNetworkProxy::DefaultProxy),
#endif
    m_pSocket(Q_NULLPTR)
{
}

/*!
    \internal
*/
QWebSocketPrivate::QWebSocketPrivate(const QString &origin, QWebSocketProtocol::Version version,
                                     QWebSocket *pWebSocket) :
    QObjectPrivate(),
    q_ptr(pWebSocket),
    m_pSocket(Q_NULLPTR),
    m_errorString(),
    m_version(version),
    m_resourceName(),
    m_request(),
    m_origin(origin),
    m_protocol(),
    m_extension(),
    m_socketState(QAbstractSocket::UnconnectedState),
    m_pauseMode(QAbstractSocket::PauseNever),
    m_readBufferSize(0),
    m_key(),
    m_mustMask(true),
    m_isClosingHandshakeSent(false),
    m_isClosingHandshakeReceived(false),
    m_closeCode(QWebSocketProtocol::CloseCodeNormal),
    m_closeReason(),
    m_pingTimer(),
    m_dataProcessor(),
    m_configuration(),
    m_pMaskGenerator(&m_defaultMaskGenerator),
    m_defaultMaskGenerator(),
    m_handshakeState(NothingDoneState)
{
}

/*!
    \internal
*/
QWebSocketPrivate::QWebSocketPrivate(QTcpSocket *pTcpSocket, QWebSocketProtocol::Version version,
                                     QWebSocket *pWebSocket) :
    QObjectPrivate(),
    q_ptr(pWebSocket),
    m_pSocket(pTcpSocket),
    m_errorString(pTcpSocket->errorString()),
    m_version(version),
    m_resourceName(),
    m_request(),
    m_origin(),
    m_protocol(),
    m_extension(),
    m_socketState(pTcpSocket->state()),
    m_pauseMode(pTcpSocket->pauseMode()),
    m_readBufferSize(pTcpSocket->readBufferSize()),
    m_key(),
    m_mustMask(true),
    m_isClosingHandshakeSent(false),
    m_isClosingHandshakeReceived(false),
    m_closeCode(QWebSocketProtocol::CloseCodeNormal),
    m_closeReason(),
    m_pingTimer(),
    m_dataProcessor(),
    m_configuration(),
    m_pMaskGenerator(&m_defaultMaskGenerator),
    m_defaultMaskGenerator(),
    m_handshakeState(NothingDoneState)
{
}

/*!
    \internal
*/
void QWebSocketPrivate::init()
{
    Q_ASSERT(q_ptr);
    Q_ASSERT(m_pMaskGenerator);

    m_pMaskGenerator->seed();

    if (m_pSocket) {
        makeConnections(m_pSocket);
    }
}

/*!
    \internal
*/
QWebSocketPrivate::~QWebSocketPrivate()
{
}

/*!
    \internal
*/
void QWebSocketPrivate::closeGoingAway()
{
    if (!m_pSocket)
        return;
    if (state() == QAbstractSocket::ConnectedState)
        close(QWebSocketProtocol::CloseCodeGoingAway, QWebSocket::tr("Connection closed"));
    releaseConnections(m_pSocket);
}

/*!
    \internal
 */
void QWebSocketPrivate::abort()
{
    if (m_pSocket)
        m_pSocket->abort();
}

/*!
    \internal
 */
QAbstractSocket::SocketError QWebSocketPrivate::error() const
{
    QAbstractSocket::SocketError err = QAbstractSocket::UnknownSocketError;
    if (Q_LIKELY(m_pSocket))
        err = m_pSocket->error();
    return err;
}

/*!
    \internal
 */
QString QWebSocketPrivate::errorString() const
{
    QString errMsg;
    if (!m_errorString.isEmpty())
        errMsg = m_errorString;
    else if (m_pSocket)
        errMsg = m_pSocket->errorString();
    return errMsg;
}

/*!
    \internal
 */
bool QWebSocketPrivate::flush()
{
    bool result = true;
    if (Q_LIKELY(m_pSocket))
        result = m_pSocket->flush();
    return result;
}

/*!
    \internal
 */
qint64 QWebSocketPrivate::sendTextMessage(const QString &message)
{
    return doWriteFrames(message.toUtf8(), false);
}

/*!
    \internal
 */
qint64 QWebSocketPrivate::sendBinaryMessage(const QByteArray &data)
{
    return doWriteFrames(data, true);
}

#ifndef QT_NO_SSL
/*!
    \internal
 */
void QWebSocketPrivate::setSslConfiguration(const QSslConfiguration &sslConfiguration)
{
    m_configuration.m_sslConfiguration = sslConfiguration;
}

/*!
    \internal
 */
QSslConfiguration QWebSocketPrivate::sslConfiguration() const
{
    return m_configuration.m_sslConfiguration;
}

/*!
    \internal
 */
void QWebSocketPrivate::ignoreSslErrors(const QList<QSslError> &errors)
{
    m_configuration.m_ignoredSslErrors = errors;
}

/*!
 * \internal
 */
void QWebSocketPrivate::ignoreSslErrors()
{
    m_configuration.m_ignoreSslErrors = true;
    if (Q_LIKELY(m_pSocket)) {
        QSslSocket *pSslSocket = qobject_cast<QSslSocket *>(m_pSocket);
        if (Q_LIKELY(pSslSocket))
            pSslSocket->ignoreSslErrors();
    }
}

/*!
* \internal
*/
void QWebSocketPrivate::_q_updateSslConfiguration()
{
    if (QSslSocket *sslSock = qobject_cast<QSslSocket *>(m_pSocket))
        m_configuration.m_sslConfiguration = sslSock->sslConfiguration();
}

#endif

/*!
  Called from QWebSocketServer
  \internal
 */
QWebSocket *QWebSocketPrivate::upgradeFrom(QTcpSocket *pTcpSocket,
                                           const QWebSocketHandshakeRequest &request,
                                           const QWebSocketHandshakeResponse &response,
                                           QObject *parent)
{
    QWebSocket *pWebSocket = new QWebSocket(pTcpSocket, response.acceptedVersion(), parent);
    if (Q_LIKELY(pWebSocket)) {
        QNetworkRequest netRequest(request.requestUrl());
        const auto headers = request.headers();
        for (auto it = headers.begin(), end = headers.end(); it != end; ++it)
            netRequest.setRawHeader(it.key().toLatin1(), it.value().toLatin1());
#ifndef QT_NO_SSL
        if (QSslSocket *sslSock = qobject_cast<QSslSocket *>(pTcpSocket))
            pWebSocket->setSslConfiguration(sslSock->sslConfiguration());
#endif
        pWebSocket->d_func()->setExtension(response.acceptedExtension());
        pWebSocket->d_func()->setOrigin(request.origin());
        pWebSocket->d_func()->setRequest(netRequest);
        pWebSocket->d_func()->setProtocol(response.acceptedProtocol());
        pWebSocket->d_func()->setResourceName(request.requestUrl().toString(QUrl::RemoveUserInfo));
        //a server should not send masked frames
        pWebSocket->d_func()->enableMasking(false);
    }

    return pWebSocket;
}

/*!
    \internal
 */
void QWebSocketPrivate::close(QWebSocketProtocol::CloseCode closeCode, QString reason)
{
    if (Q_UNLIKELY(!m_pSocket))
        return;
    if (!m_isClosingHandshakeSent) {
        Q_Q(QWebSocket);
        m_closeCode = closeCode;
        m_closeReason = reason;
        const quint16 code = qToBigEndian<quint16>(closeCode);
        QByteArray payload;
        payload.append(static_cast<const char *>(static_cast<const void *>(&code)), 2);
        if (!reason.isEmpty())
            payload.append(reason.toUtf8());
        quint32 maskingKey = 0;
        if (m_mustMask) {
            maskingKey = generateMaskingKey();
            QWebSocketProtocol::mask(payload.data(), payload.size(), maskingKey);
        }
        QByteArray frame = getFrameHeader(QWebSocketProtocol::OpCodeClose,
                                          payload.size(), maskingKey, true);
        frame.append(payload);
        m_pSocket->write(frame);
        m_pSocket->flush();

        m_isClosingHandshakeSent = true;

        Q_EMIT q->aboutToClose();
    }
    m_pSocket->close();
}

/*!
    \internal
 */
void QWebSocketPrivate::open(const QNetworkRequest &request, bool mask)
{
    //just delete the old socket for the moment;
    //later, we can add more 'intelligent' handling by looking at the URL

    Q_Q(QWebSocket);
    QUrl url = request.url();
    if (!url.isValid() || url.toString().contains(QStringLiteral("\r\n"))) {
        setErrorString(QWebSocket::tr("Invalid URL."));
        Q_EMIT q->error(QAbstractSocket::ConnectionRefusedError);
        return;
    }
    if (m_pSocket) {
        releaseConnections(m_pSocket);
        m_pSocket->deleteLater();
        m_pSocket = Q_NULLPTR;
    }
    //if (m_url != url)
    if (Q_LIKELY(!m_pSocket)) {
        m_dataProcessor.clear();
        m_isClosingHandshakeReceived = false;
        m_isClosingHandshakeSent = false;

        setRequest(request);
        QString resourceName = url.path(QUrl::FullyEncoded);
        // Check for encoded \r\n
        if (resourceName.contains(QStringLiteral("%0D%0A"))) {
            setRequest(QNetworkRequest());  //clear request
            setErrorString(QWebSocket::tr("Invalid resource name."));
            Q_EMIT q->error(QAbstractSocket::ConnectionRefusedError);
            return;
        }
        if (!url.query().isEmpty()) {
            if (!resourceName.endsWith(QChar::fromLatin1('?'))) {
                resourceName.append(QChar::fromLatin1('?'));
            }
            resourceName.append(url.query(QUrl::FullyEncoded));
        }
        if (resourceName.isEmpty())
            resourceName = QStringLiteral("/");
        setResourceName(resourceName);
        enableMasking(mask);

    #ifndef QT_NO_SSL
        if (url.scheme() == QStringLiteral("wss")) {
            if (!QSslSocket::supportsSsl()) {
                const QString message =
                        QWebSocket::tr("SSL Sockets are not supported on this platform.");
                setErrorString(message);
                Q_EMIT q->error(QAbstractSocket::UnsupportedSocketOperationError);
            } else {
                QSslSocket *sslSocket = new QSslSocket(q_ptr);
                m_pSocket = sslSocket;
                if (Q_LIKELY(m_pSocket)) {
                    m_pSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
                    m_pSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
                    m_pSocket->setReadBufferSize(m_readBufferSize);
                    m_pSocket->setPauseMode(m_pauseMode);

                    makeConnections(m_pSocket);
                    setSocketState(QAbstractSocket::ConnectingState);

                    sslSocket->setSslConfiguration(m_configuration.m_sslConfiguration);
                    if (Q_UNLIKELY(m_configuration.m_ignoreSslErrors))
                        sslSocket->ignoreSslErrors();
                    else
                        sslSocket->ignoreSslErrors(m_configuration.m_ignoredSslErrors);
    #ifndef QT_NO_NETWORKPROXY
                    sslSocket->setProxy(m_configuration.m_proxy);
    #endif
                    sslSocket->connectToHostEncrypted(url.host(), url.port(443));
                } else {
                    const QString message = QWebSocket::tr("Out of memory.");
                    setErrorString(message);
                    Q_EMIT q->error(QAbstractSocket::SocketResourceError);
                }
            }
        } else
    #endif
        if (url.scheme() == QStringLiteral("ws")) {
            m_pSocket = new QTcpSocket(q_ptr);
            if (Q_LIKELY(m_pSocket)) {
                m_pSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
                m_pSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
                m_pSocket->setReadBufferSize(m_readBufferSize);
                m_pSocket->setPauseMode(m_pauseMode);

                makeConnections(m_pSocket);
                setSocketState(QAbstractSocket::ConnectingState);
    #ifndef QT_NO_NETWORKPROXY
                m_pSocket->setProxy(m_configuration.m_proxy);
    #endif
                m_pSocket->connectToHost(url.host(), url.port(80));
            } else {
                const QString message = QWebSocket::tr("Out of memory.");
                setErrorString(message);
                Q_EMIT q->error(QAbstractSocket::SocketResourceError);
            }
        } else {
            const QString message =
                    QWebSocket::tr("Unsupported WebSocket scheme: %1").arg(url.scheme());
            setErrorString(message);
            Q_EMIT q->error(QAbstractSocket::UnsupportedSocketOperationError);
        }
    }
}

/*!
    \internal
 */
void QWebSocketPrivate::ping(const QByteArray &payload)
{
    QByteArray payloadTruncated = payload.left(125);
    m_pingTimer.restart();
    quint32 maskingKey = 0;
    if (m_mustMask)
        maskingKey = generateMaskingKey();
    QByteArray pingFrame = getFrameHeader(QWebSocketProtocol::OpCodePing, payloadTruncated.size(),
                                          maskingKey, true);
    if (m_mustMask)
        QWebSocketProtocol::mask(&payloadTruncated, maskingKey);
    pingFrame.append(payloadTruncated);
    qint64 ret = writeFrame(pingFrame);
    Q_UNUSED(ret);
}

/*!
  \internal
    Sets the version to use for the WebSocket protocol;
    this must be set before the socket is opened.
*/
void QWebSocketPrivate::setVersion(QWebSocketProtocol::Version version)
{
    if (m_version != version)
        m_version = version;
}

/*!
    \internal
    Sets the resource name of the connection; must be set before the socket is openend
*/
void QWebSocketPrivate::setResourceName(const QString &resourceName)
{
    if (m_resourceName != resourceName)
        m_resourceName = resourceName;
}

/*!
  \internal
 */
void QWebSocketPrivate::setRequest(const QNetworkRequest &request)
{
    if (m_request != request)
        m_request = request;
}

/*!
  \internal
 */
void QWebSocketPrivate::setOrigin(const QString &origin)
{
    if (m_origin != origin)
        m_origin = origin;
}

/*!
  \internal
 */
void QWebSocketPrivate::setProtocol(const QString &protocol)
{
    if (m_protocol != protocol)
        m_protocol = protocol;
}

/*!
  \internal
 */
void QWebSocketPrivate::setExtension(const QString &extension)
{
    if (m_extension != extension)
        m_extension = extension;
}

/*!
  \internal
 */
void QWebSocketPrivate::enableMasking(bool enable)
{
    if (m_mustMask != enable)
        m_mustMask = enable;
}

/*!
 * \internal
 */
void QWebSocketPrivate::makeConnections(const QTcpSocket *pTcpSocket)
{
    Q_ASSERT(pTcpSocket);
    Q_Q(QWebSocket);

    if (Q_LIKELY(pTcpSocket)) {
        //pass through signals
        typedef void (QAbstractSocket:: *ASErrorSignal)(QAbstractSocket::SocketError);
        typedef void (QWebSocket:: *WSErrorSignal)(QAbstractSocket::SocketError);
        QObject::connect(pTcpSocket,
                         static_cast<ASErrorSignal>(&QAbstractSocket::error),
                         q, static_cast<WSErrorSignal>(&QWebSocket::error));
#ifndef QT_NO_NETWORKPROXY
        QObject::connect(pTcpSocket, &QAbstractSocket::proxyAuthenticationRequired, q,
                         &QWebSocket::proxyAuthenticationRequired);
#endif // QT_NO_NETWORKPROXY
        QObject::connect(pTcpSocket, &QAbstractSocket::readChannelFinished, q,
                         &QWebSocket::readChannelFinished);
        QObject::connect(pTcpSocket, &QAbstractSocket::aboutToClose, q, &QWebSocket::aboutToClose);
        QObject::connect(pTcpSocket, &QAbstractSocket::bytesWritten, q, &QWebSocket::bytesWritten);


        QObjectPrivate::connect(pTcpSocket, &QObject::destroyed,
                                this, &QWebSocketPrivate::socketDestroyed);

        //catch signals
        QObjectPrivate::connect(pTcpSocket, &QAbstractSocket::stateChanged, this,
                                &QWebSocketPrivate::processStateChanged);
        //!!!important to use a QueuedConnection here;
        //with QTcpSocket there is no problem, but with QSslSocket the processing hangs
        QObjectPrivate::connect(pTcpSocket, &QAbstractSocket::readyRead, this,
                                &QWebSocketPrivate::processData, Qt::QueuedConnection);
#ifndef QT_NO_SSL
        const QSslSocket * const sslSocket = qobject_cast<const QSslSocket *>(pTcpSocket);
        if (sslSocket) {
            QObject::connect(sslSocket, &QSslSocket::preSharedKeyAuthenticationRequired, q,
                             &QWebSocket::preSharedKeyAuthenticationRequired);
            QObject::connect(sslSocket, &QSslSocket::encryptedBytesWritten, q,
                             &QWebSocket::bytesWritten);
            typedef void (QSslSocket:: *sslErrorSignalType)(const QList<QSslError> &);
            QObject::connect(sslSocket,
                             static_cast<sslErrorSignalType>(&QSslSocket::sslErrors),
                             q, &QWebSocket::sslErrors);
            QObjectPrivate::connect(sslSocket, &QSslSocket::encrypted,
                                    this, &QWebSocketPrivate::_q_updateSslConfiguration);
        } else
#endif // QT_NO_SSL
        {
            QObject::connect(pTcpSocket, &QAbstractSocket::bytesWritten, q,
                             &QWebSocket::bytesWritten);
        }
    }

    QObject::connect(&m_dataProcessor, &QWebSocketDataProcessor::textFrameReceived, q,
                     &QWebSocket::textFrameReceived);
    QObject::connect(&m_dataProcessor, &QWebSocketDataProcessor::binaryFrameReceived, q,
                     &QWebSocket::binaryFrameReceived);
    QObject::connect(&m_dataProcessor, &QWebSocketDataProcessor::binaryMessageReceived, q,
                     &QWebSocket::binaryMessageReceived);
    QObject::connect(&m_dataProcessor, &QWebSocketDataProcessor::textMessageReceived, q,
                     &QWebSocket::textMessageReceived);
    QObjectPrivate::connect(&m_dataProcessor, &QWebSocketDataProcessor::errorEncountered, this,
                            &QWebSocketPrivate::close);
    QObjectPrivate::connect(&m_dataProcessor, &QWebSocketDataProcessor::pingReceived, this,
                            &QWebSocketPrivate::processPing);
    QObjectPrivate::connect(&m_dataProcessor, &QWebSocketDataProcessor::pongReceived, this,
                            &QWebSocketPrivate::processPong);
    QObjectPrivate::connect(&m_dataProcessor, &QWebSocketDataProcessor::closeReceived, this,
                            &QWebSocketPrivate::processClose);
}

/*!
 * \internal
 */
void QWebSocketPrivate::releaseConnections(const QTcpSocket *pTcpSocket)
{
    if (Q_LIKELY(pTcpSocket))
        pTcpSocket->disconnect(pTcpSocket);
    m_dataProcessor.disconnect();
}

/*!
    \internal
 */
QWebSocketProtocol::Version QWebSocketPrivate::version() const
{
    return m_version;
}

/*!
    \internal
 */
QString QWebSocketPrivate::resourceName() const
{
    return m_resourceName;
}

/*!
    \internal
 */
QNetworkRequest QWebSocketPrivate::request() const
{
    return m_request;
}

/*!
    \internal
 */
QString QWebSocketPrivate::origin() const
{
    return m_origin;
}

/*!
    \internal
 */
QString QWebSocketPrivate::protocol() const
{
    return m_protocol;
}

/*!
    \internal
 */
QString QWebSocketPrivate::extension() const
{
    return m_extension;
}

/*!
 * \internal
 */
QWebSocketProtocol::CloseCode QWebSocketPrivate::closeCode() const
{
    return m_closeCode;
}

/*!
 * \internal
 */
QString QWebSocketPrivate::closeReason() const
{
    return m_closeReason;
}

/*!
 * \internal
 */
QByteArray QWebSocketPrivate::getFrameHeader(QWebSocketProtocol::OpCode opCode,
                                             quint64 payloadLength, quint32 maskingKey,
                                             bool lastFrame)
{
    QByteArray header;
    bool ok = payloadLength <= 0x7FFFFFFFFFFFFFFFULL;

    if (Q_LIKELY(ok)) {
        //FIN, RSV1-3, opcode (RSV-1, RSV-2 and RSV-3 are zero)
        quint8 byte = static_cast<quint8>((opCode & 0x0F) | (lastFrame ? 0x80 : 0x00));
        header.append(static_cast<char>(byte));

        byte = 0x00;
        if (maskingKey != 0)
            byte |= 0x80;
        if (payloadLength <= 125) {
            byte |= static_cast<quint8>(payloadLength);
            header.append(static_cast<char>(byte));
        } else if (payloadLength <= 0xFFFFU) {
            byte |= 126;
            header.append(static_cast<char>(byte));
            quint16 swapped = qToBigEndian<quint16>(static_cast<quint16>(payloadLength));
            header.append(static_cast<const char *>(static_cast<const void *>(&swapped)), 2);
        } else if (payloadLength <= 0x7FFFFFFFFFFFFFFFULL) {
            byte |= 127;
            header.append(static_cast<char>(byte));
            quint64 swapped = qToBigEndian<quint64>(payloadLength);
            header.append(static_cast<const char *>(static_cast<const void *>(&swapped)), 8);
        }

        if (maskingKey != 0) {
            const quint32 mask = qToBigEndian<quint32>(maskingKey);
            header.append(static_cast<const char *>(static_cast<const void *>(&mask)),
                          sizeof(quint32));
        }
    } else {
        setErrorString(QStringLiteral("WebSocket::getHeader: payload too big!"));
        Q_EMIT q_ptr->error(QAbstractSocket::DatagramTooLargeError);
    }

    return header;
}

/*!
 * \internal
 */
qint64 QWebSocketPrivate::doWriteFrames(const QByteArray &data, bool isBinary)
{
    qint64 payloadWritten = 0;
    if (Q_UNLIKELY(!m_pSocket) || (state() != QAbstractSocket::ConnectedState))
        return payloadWritten;

    Q_Q(QWebSocket);
    const QWebSocketProtocol::OpCode firstOpCode = isBinary ?
                QWebSocketProtocol::OpCodeBinary : QWebSocketProtocol::OpCodeText;

    int numFrames = data.size() / FRAME_SIZE_IN_BYTES;
    QByteArray tmpData(data);
    tmpData.detach();
    char *payload = tmpData.data();
    quint64 sizeLeft = quint64(data.size()) % FRAME_SIZE_IN_BYTES;
    if (Q_LIKELY(sizeLeft))
        ++numFrames;

    //catch the case where the payload is zero bytes;
    //in this case, we still need to send a frame
    if (Q_UNLIKELY(numFrames == 0))
        numFrames = 1;
    quint64 currentPosition = 0;
    quint64 bytesLeft = data.size();

    for (int i = 0; i < numFrames; ++i) {
        quint32 maskingKey = 0;
        if (m_mustMask)
            maskingKey = generateMaskingKey();

        const bool isLastFrame = (i == (numFrames - 1));
        const bool isFirstFrame = (i == 0);

        const quint64 size = qMin(bytesLeft, FRAME_SIZE_IN_BYTES);
        const QWebSocketProtocol::OpCode opcode = isFirstFrame ? firstOpCode
                                                               : QWebSocketProtocol::OpCodeContinue;

        //write header
        m_pSocket->write(getFrameHeader(opcode, size, maskingKey, isLastFrame));

        //write payload
        if (Q_LIKELY(size > 0)) {
            char *currentData = payload + currentPosition;
            if (m_mustMask)
                QWebSocketProtocol::mask(currentData, size, maskingKey);
            qint64 written = m_pSocket->write(currentData, static_cast<qint64>(size));
            if (Q_LIKELY(written > 0)) {
                payloadWritten += written;
            } else {
                m_pSocket->flush();
                setErrorString(QWebSocket::tr("Error writing bytes to socket: %1.")
                               .arg(m_pSocket->errorString()));
                Q_EMIT q->error(QAbstractSocket::NetworkError);
                break;
            }
        }
        currentPosition += size;
        bytesLeft -= size;
    }
    if (Q_UNLIKELY(payloadWritten != data.size())) {
        setErrorString(QWebSocket::tr("Bytes written %1 != %2.")
                       .arg(payloadWritten).arg(data.size()));
        Q_EMIT q->error(QAbstractSocket::NetworkError);
    }
    return payloadWritten;
}

/*!
    \internal
 */
quint32 QWebSocketPrivate::generateMaskingKey() const
{
    return m_pMaskGenerator->nextMask();
}

/*!
    \internal
 */
QByteArray QWebSocketPrivate::generateKey() const
{
    QByteArray key;

    for (int i = 0; i < 4; ++i) {
        const quint32 tmp = m_pMaskGenerator->nextMask();
        key.append(static_cast<const char *>(static_cast<const void *>(&tmp)), sizeof(quint32));
    }

    return key.toBase64();
}


/*!
    \internal
 */
QString QWebSocketPrivate::calculateAcceptKey(const QByteArray &key) const
{
    const QByteArray tmpKey = key + QByteArrayLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    const QByteArray hash = QCryptographicHash::hash(tmpKey, QCryptographicHash::Sha1).toBase64();
    return QString::fromLatin1(hash);
}

/*!
    \internal
 */
qint64 QWebSocketPrivate::writeFrames(const QList<QByteArray> &frames)
{
    qint64 written = 0;
    if (Q_LIKELY(m_pSocket)) {
        QList<QByteArray>::const_iterator it;
        for (it = frames.cbegin(); it < frames.cend(); ++it)
            written += writeFrame(*it);
    }
    return written;
}

/*!
    \internal
 */
qint64 QWebSocketPrivate::writeFrame(const QByteArray &frame)
{
    qint64 written = 0;
    if (Q_LIKELY(m_pSocket))
        written = m_pSocket->write(frame);
    return written;
}

/*!
    \internal
 */
static QString readLine(QTcpSocket *pSocket)
{
    Q_ASSERT(pSocket);
    QString line;
    char c;
    while (pSocket->getChar(&c)) {
        if (c == char('\r')) {
            pSocket->getChar(&c);
            break;
        } else {
            line.append(QChar::fromLatin1(c));
        }
    }
    return line;
}

// this function is a copy of QHttpNetworkReplyPrivate::parseStatus
static bool parseStatusLine(const QByteArray &status, int *majorVersion, int *minorVersion,
                            int *statusCode, QString *reasonPhrase)
{
    // from RFC 2616:
    //        Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
    //        HTTP-Version   = "HTTP" "/" 1*DIGIT "." 1*DIGIT
    // that makes: 'HTTP/n.n xxx Message'
    // byte count:  0123456789012

    static const int minLength = 11;
    static const int dotPos = 6;
    static const int spacePos = 8;
    static const char httpMagic[] = "HTTP/";

    if (status.length() < minLength
        || !status.startsWith(httpMagic)
        || status.at(dotPos) != '.'
        || status.at(spacePos) != ' ') {
        // I don't know how to parse this status line
        return false;
    }

    // optimize for the valid case: defer checking until the end
    *majorVersion = status.at(dotPos - 1) - '0';
    *minorVersion = status.at(dotPos + 1) - '0';

    int i = spacePos;
    int j = status.indexOf(' ', i + 1); // j == -1 || at(j) == ' ' so j+1 == 0 && j+1 <= length()
    const QByteArray code = status.mid(i + 1, j - i - 1);

    bool ok;
    *statusCode = code.toInt(&ok);
    *reasonPhrase = QString::fromLatin1(status.constData() + j + 1);

    return ok && uint(*majorVersion) <= 9 && uint(* minorVersion) <= 9;
}


//called on the client for a server handshake response
/*!
    \internal
 */
void QWebSocketPrivate::processHandshake(QTcpSocket *pSocket)
{
    Q_Q(QWebSocket);
    if (Q_UNLIKELY(!pSocket))
        return;
    // Reset handshake on a new connection.
    if (m_handshakeState == AllDoneState)
        m_handshakeState = NothingDoneState;

    QString errorDescription;

    switch (m_handshakeState) {
    case NothingDoneState:
        m_headers.clear();
        m_handshakeState = ReadingStatusState;
        // no break
    case ReadingStatusState:
        if (!pSocket->canReadLine())
            return;
        m_statusLine = pSocket->readLine();
        if (Q_UNLIKELY(!parseStatusLine(m_statusLine, &m_httpMajorVersion, &m_httpMinorVersion, &m_httpStatusCode, &m_httpStatusMessage))) {
            errorDescription = QWebSocket::tr("Invalid statusline in response: %1.").arg(QString::fromLatin1(m_statusLine));
            break;
        }
        m_handshakeState = ReadingHeaderState;
        // no break
    case ReadingHeaderState:
        while (pSocket->canReadLine()) {
            QString headerLine = readLine(pSocket);
            const QStringList headerField = headerLine.split(QStringLiteral(": "),
                                                             QString::SkipEmptyParts);
            if (headerField.size() == 2) {
                m_headers.insertMulti(headerField[0].toLower(), headerField[1]);
            }
            if (headerField.isEmpty()) {
                m_handshakeState = ParsingHeaderState;
                break;
            }
        }

        if (m_handshakeState != ParsingHeaderState) {
            if (pSocket->state() != QAbstractSocket::ConnectedState) {
                errorDescription = QWebSocket::tr("QWebSocketPrivate::processHandshake: Connection closed while reading header.");
                break;
            }
            return;
        }
        // no break
    case ParsingHeaderState: {
        const QString acceptKey = m_headers.value(QStringLiteral("sec-websocket-accept"), QString());
        const QString upgrade = m_headers.value(QStringLiteral("upgrade"), QString());
        const QString connection = m_headers.value(QStringLiteral("connection"), QString());
//        unused for the moment
//        const QString extensions = m_headers.value(QStringLiteral("sec-websocket-extensions"),
//                                                 QString());
//        const QString protocol = m_headers.value(QStringLiteral("sec-websocket-protocol"),
//                                               QString());
        const QString version = m_headers.value(QStringLiteral("sec-websocket-version"), QString());

        bool ok = false;
        if (Q_LIKELY(m_httpStatusCode == 101)) {
            //HTTP/x.y 101 Switching Protocols
            //TODO: do not check the httpStatusText right now
            ok = !(acceptKey.isEmpty() ||
                   (m_httpMajorVersion < 1 || m_httpMinorVersion < 1) ||
                   (upgrade.toLower() != QStringLiteral("websocket")) ||
                   (connection.toLower() != QStringLiteral("upgrade")));
            if (ok) {
                const QString accept = calculateAcceptKey(m_key);
                ok = (accept == acceptKey);
                if (!ok)
                    errorDescription =
                      QWebSocket::tr("Accept-Key received from server %1 does not match the client key %2.")
                            .arg(acceptKey).arg(accept);
            } else {
                errorDescription =
                    QWebSocket::tr("QWebSocketPrivate::processHandshake: Invalid statusline in response: %1.")
                        .arg(QString::fromLatin1(m_statusLine));
            }
        } else if (m_httpStatusCode == 400) {
            //HTTP/1.1 400 Bad Request
            if (!version.isEmpty()) {
                const QStringList versions = version.split(QStringLiteral(", "),
                                                           QString::SkipEmptyParts);
                if (!versions.contains(QString::number(QWebSocketProtocol::currentVersion()))) {
                    //if needed to switch protocol version, then we are finished here
                    //because we cannot handle other protocols than the RFC one (v13)
                    errorDescription =
                            QWebSocket::tr("Handshake: Server requests a version that we don't support: %1.")
                            .arg(versions.join(QStringLiteral(", ")));
                } else {
                    //we tried v13, but something different went wrong
                    errorDescription =
                        QWebSocket::tr("QWebSocketPrivate::processHandshake: Unknown error condition encountered. Aborting connection.");
                }
            } else {
                    errorDescription =
                        QWebSocket::tr("QWebSocketPrivate::processHandshake: Unknown error condition encountered. Aborting connection.");
            }
        } else {
            errorDescription =
                    QWebSocket::tr("QWebSocketPrivate::processHandshake: Unhandled http status code: %1 (%2).")
                        .arg(m_httpStatusCode).arg(m_httpStatusMessage);
        }
        if (ok)
            m_handshakeState = AllDoneState;
        break;
    }
    case AllDoneState:
        Q_UNREACHABLE();
        break;
    }

    if (m_handshakeState == AllDoneState) {
        // handshake succeeded
        setSocketState(QAbstractSocket::ConnectedState);
        Q_EMIT q->connected();
    } else {
        // handshake failed
        m_handshakeState = AllDoneState;
        setErrorString(errorDescription);
        Q_EMIT q->error(QAbstractSocket::ConnectionRefusedError);
    }
}

/*!
    \internal
 */
void QWebSocketPrivate::processStateChanged(QAbstractSocket::SocketState socketState)
{
    Q_ASSERT(m_pSocket);
    Q_Q(QWebSocket);
    QAbstractSocket::SocketState webSocketState = this->state();
    int port = 80;
    if (m_request.url().scheme() == QStringLiteral("wss"))
        port = 443;

    switch (socketState) {
    case QAbstractSocket::ConnectedState:
#ifndef QT_NO_SSL
        if (QSslSocket *sslSock = qobject_cast<QSslSocket *>(m_pSocket))
            m_configuration.m_sslConfiguration = sslSock->sslConfiguration();
#endif
        if (webSocketState == QAbstractSocket::ConnectingState) {
            m_key = generateKey();

            QList<QPair<QString, QString> > headers;
            const auto keys = m_request.rawHeaderList();
            for (const QByteArray &key : keys)
                headers << qMakePair(QString::fromLatin1(key),
                                     QString::fromLatin1(m_request.rawHeader(key)));

            const QString handshake =
                    createHandShakeRequest(m_resourceName,
                                           m_request.url().host()
                                                % QStringLiteral(":")
                                                % QString::number(m_request.url().port(port)),
                                           origin(),
                                           QString(),
                                           QString(),
                                           m_key,
                                           headers);
            if (handshake.isEmpty()) {
                m_pSocket->abort();
                Q_EMIT q->error(QAbstractSocket::ConnectionRefusedError);
                return;
            }
            m_pSocket->write(handshake.toLatin1());
        }
        break;

    case QAbstractSocket::ClosingState:
        if (webSocketState == QAbstractSocket::ConnectedState)
            setSocketState(QAbstractSocket::ClosingState);
        break;

    case QAbstractSocket::UnconnectedState:
        if (webSocketState != QAbstractSocket::UnconnectedState) {
            setSocketState(QAbstractSocket::UnconnectedState);
            Q_EMIT q->disconnected();
        }
        break;

    case QAbstractSocket::HostLookupState:
    case QAbstractSocket::ConnectingState:
    case QAbstractSocket::BoundState:
    case QAbstractSocket::ListeningState:
        //do nothing
        //to make C++ compiler happy;
        break;
    default:
        break;
    }
}

void QWebSocketPrivate::socketDestroyed(QObject *socket)
{
    Q_ASSERT(m_pSocket);
    if (m_pSocket == socket)
        m_pSocket = Q_NULLPTR;
}

/*!
 \internal
 */
void QWebSocketPrivate::processData()
{
    Q_ASSERT(m_pSocket);
    while (m_pSocket->bytesAvailable()) {
        if (state() == QAbstractSocket::ConnectingState) {
            if (!m_pSocket->canReadLine())
                break;
            processHandshake(m_pSocket);
        } else {
            m_dataProcessor.process(m_pSocket);
        }
    }
}

/*!
 \internal
 */
void QWebSocketPrivate::processPing(const QByteArray &data)
{
    Q_ASSERT(m_pSocket);
    quint32 maskingKey = 0;
    if (m_mustMask)
        maskingKey = generateMaskingKey();
    m_pSocket->write(getFrameHeader(QWebSocketProtocol::OpCodePong, data.size(), maskingKey, true));
    if (data.size() > 0) {
        QByteArray maskedData = data;
        if (m_mustMask)
            QWebSocketProtocol::mask(&maskedData, maskingKey);
        m_pSocket->write(maskedData);
    }
}

/*!
 \internal
 */
void QWebSocketPrivate::processPong(const QByteArray &data)
{
    Q_Q(QWebSocket);
    Q_EMIT q->pong(static_cast<quint64>(m_pingTimer.elapsed()), data);
}

/*!
 \internal
 */
void QWebSocketPrivate::processClose(QWebSocketProtocol::CloseCode closeCode, QString closeReason)
{
    m_isClosingHandshakeReceived = true;
    close(closeCode, closeReason);
}

/*!
    \internal
 */
QString QWebSocketPrivate::createHandShakeRequest(QString resourceName,
                                                  QString host,
                                                  QString origin,
                                                  QString extensions,
                                                  QString protocols,
                                                  QByteArray key,
                                                  const QList<QPair<QString, QString> > &headers)
{
    QStringList handshakeRequest;
    if (resourceName.contains(QStringLiteral("\r\n"))) {
        setErrorString(QWebSocket::tr("The resource name contains newlines. " \
                                      "Possible attack detected."));
        return QString();
    }
    if (host.contains(QStringLiteral("\r\n"))) {
        setErrorString(QWebSocket::tr("The hostname contains newlines. " \
                                      "Possible attack detected."));
        return QString();
    }
    if (origin.contains(QStringLiteral("\r\n"))) {
        setErrorString(QWebSocket::tr("The origin contains newlines. " \
                                      "Possible attack detected."));
        return QString();
    }
    if (extensions.contains(QStringLiteral("\r\n"))) {
        setErrorString(QWebSocket::tr("The extensions attribute contains newlines. " \
                                      "Possible attack detected."));
        return QString();
    }
    if (protocols.contains(QStringLiteral("\r\n"))) {
        setErrorString(QWebSocket::tr("The protocols attribute contains newlines. " \
                                      "Possible attack detected."));
        return QString();
    }

    handshakeRequest << QStringLiteral("GET ") % resourceName % QStringLiteral(" HTTP/1.1") <<
                        QStringLiteral("Host: ") % host <<
                        QStringLiteral("Upgrade: websocket") <<
                        QStringLiteral("Connection: Upgrade") <<
                        QStringLiteral("Sec-WebSocket-Key: ") % QString::fromLatin1(key);
    if (!origin.isEmpty())
        handshakeRequest << QStringLiteral("Origin: ") % origin;
    handshakeRequest << QStringLiteral("Sec-WebSocket-Version: ")
                            % QString::number(QWebSocketProtocol::currentVersion());
    if (extensions.length() > 0)
        handshakeRequest << QStringLiteral("Sec-WebSocket-Extensions: ") % extensions;
    if (protocols.length() > 0)
        handshakeRequest << QStringLiteral("Sec-WebSocket-Protocol: ") % protocols;

    for (const auto &header : headers)
        handshakeRequest << header.first % QStringLiteral(": ") % header.second;

    handshakeRequest << QStringLiteral("\r\n");

    return handshakeRequest.join(QStringLiteral("\r\n"));
}

/*!
    \internal
 */
QAbstractSocket::SocketState QWebSocketPrivate::state() const
{
    return m_socketState;
}

/*!
    \internal
 */
void QWebSocketPrivate::setSocketState(QAbstractSocket::SocketState state)
{
    Q_Q(QWebSocket);
    if (m_socketState != state) {
        m_socketState = state;
        Q_EMIT q->stateChanged(m_socketState);
    }
}

/*!
    \internal
 */
void QWebSocketPrivate::setErrorString(const QString &errorString)
{
    if (m_errorString != errorString)
        m_errorString = errorString;
}

/*!
    \internal
 */
QHostAddress QWebSocketPrivate::localAddress() const
{
    QHostAddress address;
    if (Q_LIKELY(m_pSocket))
        address = m_pSocket->localAddress();
    return address;
}

/*!
    \internal
 */
quint16 QWebSocketPrivate::localPort() const
{
    quint16 port = 0;
    if (Q_LIKELY(m_pSocket))
        port = m_pSocket->localPort();
    return port;
}

/*!
    \internal
 */
QAbstractSocket::PauseModes QWebSocketPrivate::pauseMode() const
{
    return m_pauseMode;
}

/*!
    \internal
 */
QHostAddress QWebSocketPrivate::peerAddress() const
{
    QHostAddress address;
    if (Q_LIKELY(m_pSocket))
        address = m_pSocket->peerAddress();
    return address;
}

/*!
    \internal
 */
QString QWebSocketPrivate::peerName() const
{
    QString name;
    if (Q_LIKELY(m_pSocket))
        name = m_pSocket->peerName();
    return name;
}

/*!
    \internal
 */
quint16 QWebSocketPrivate::peerPort() const
{
    quint16 port = 0;
    if (Q_LIKELY(m_pSocket))
        port = m_pSocket->peerPort();
    return port;
}

#ifndef QT_NO_NETWORKPROXY
/*!
    \internal
 */
QNetworkProxy QWebSocketPrivate::proxy() const
{
    return m_configuration.m_proxy;
}

/*!
    \internal
 */
void QWebSocketPrivate::setProxy(const QNetworkProxy &networkProxy)
{
    if (m_configuration.m_proxy != networkProxy)
        m_configuration.m_proxy = networkProxy;
}
#endif  //QT_NO_NETWORKPROXY

/*!
    \internal
 */
void QWebSocketPrivate::setMaskGenerator(const QMaskGenerator *maskGenerator)
{
    if (!maskGenerator)
        m_pMaskGenerator = &m_defaultMaskGenerator;
    else if (maskGenerator != m_pMaskGenerator)
        m_pMaskGenerator = const_cast<QMaskGenerator *>(maskGenerator);
}

/*!
    \internal
 */
const QMaskGenerator *QWebSocketPrivate::maskGenerator() const
{
    Q_ASSERT(m_pMaskGenerator);
    return m_pMaskGenerator;
}

/*!
    \internal
 */
qint64 QWebSocketPrivate::readBufferSize() const
{
    return m_readBufferSize;
}

/*!
    \internal
 */
void QWebSocketPrivate::resume()
{
    if (Q_LIKELY(m_pSocket))
        m_pSocket->resume();
}

/*!
  \internal
 */
void QWebSocketPrivate::setPauseMode(QAbstractSocket::PauseModes pauseMode)
{
    m_pauseMode = pauseMode;
    if (Q_LIKELY(m_pSocket))
        m_pSocket->setPauseMode(m_pauseMode);
}

/*!
    \internal
 */
void QWebSocketPrivate::setReadBufferSize(qint64 size)
{
    m_readBufferSize = size;
    if (Q_LIKELY(m_pSocket))
        m_pSocket->setReadBufferSize(m_readBufferSize);
}

/*!
    \internal
 */
bool QWebSocketPrivate::isValid() const
{
    return (m_pSocket && m_pSocket->isValid() &&
            (m_socketState == QAbstractSocket::ConnectedState));
}

QT_END_NAMESPACE
