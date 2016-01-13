/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtEnginio module of the Qt Toolkit.
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

#include <Enginio/private/enginiobackendconnection_p.h>
#include <Enginio/enginioclient.h>
#include <Enginio/private/enginioclient_p.h>
#include <Enginio/enginioreply.h>

#include <QtCore/QTimerEvent>
#include <QtCore/qbytearray.h>
#include <QtCore/qcryptographichash.h>
#include <QtCore/QtEndian>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qstring.h>
#include <QtCore/quuid.h>
#include <QtNetwork/qtcpsocket.h>
#include <QtCore/qdebug.h>

#define CRLF QLatin1String("\r\n")

QT_BEGIN_NAMESPACE

const static int NIL = 0x00;
const static int FIN = 0x80;
const static int MSB = 0x80;
const static int MSK = 0x80;
const static int OPC = 0x0F;
const static int LEN = 0x7F;

const static int ThirtySeconds = 30000;
const static int TwoMinutes = 120000;
const static quint64 DefaultHeaderLength = 2;
const static quint64 LargePayloadHeaderLength = 8;
const static quint64 MaskingKeyLength = 4;
const static quint64 NormalPayloadMarker = 126;
const static quint64 LargePayloadMarker = 127;
const static quint64 NormalPayloadLengthLimit = 0xFFFF;

namespace {

const QString HttpResponseStatus(QStringLiteral("HTTP/1\\.1\\s([0-9]{3})\\s"));
const QString SecWebSocketAcceptHeader(QStringLiteral("Sec-WebSocket-Accept:\\s(.{28})\r\n"));
const QString UpgradeHeader(QStringLiteral("Upgrade:\\s(.+)\r\n"));
const QString ConnectionHeader(QStringLiteral("Connection:\\s(.+)\r\n"));

QString gBase64EncodedSha1VerificationKey;

void computeBase64EncodedSha1VerificationKey(const QByteArray &base64Key)
{
    // http://tools.ietf.org/html/rfc6455#section-4.2.2 §5./ 4.
    QByteArray webSocketMagicString(QByteArrayLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));
    webSocketMagicString.prepend(base64Key);
    gBase64EncodedSha1VerificationKey = QString::fromUtf8(QCryptographicHash::hash(webSocketMagicString, QCryptographicHash::Sha1).toBase64());
}

const QByteArray generateBase64EncodedUniqueKey()
{
    return QUuid::createUuid().toRfc4122().toBase64();
}

QByteArray generateMaskingKey()
{
    // The masking key is a 32-bit value chosen at random by the client.
    QByteArray uuid = QUuid::createUuid().toRfc4122();
    QByteArray key = uuid.left(MaskingKeyLength);
    for (int octet = MaskingKeyLength; octet < uuid.size(); ++octet) {
        int index = octet % key.size();
        key[index] = key[index] ^ uuid[octet];
    }
    return key;
}

void maskData(QByteArray &data, const QByteArray &maskingKey )
{
    // Client-to-Server Masking.
    // http://tools.ietf.org/html/rfc6455#section-5.3

    for (int octet = 0; octet < data.size(); ++octet)
        data[octet] = data[octet] ^ maskingKey[octet % maskingKey.size()];
}

int extractResponseStatus(QString responseString)
{
    const QRegularExpression re(HttpResponseStatus);
    QRegularExpressionMatch match = re.match(responseString);
    return match.captured(1).toInt();
}

const QString extractResponseHeader(QString pattern, QString responseString, bool ignoreCase = true)
{
    const QRegularExpression re(pattern);
    QRegularExpressionMatch match = re.match(responseString);

    if (ignoreCase)
        return match.captured(1).toLower();

    return match.captured(1);
}

const QByteArray constructOpeningHandshake(const QUrl& url)
{
    // http://tools.ietf.org/html/rfc6455#section-4.1 §2./ 7.
    // The request must include a header field with the name
    // Sec-WebSocket-Key. The value of this header field must be a
    // nonce consisting of a randomly selected 16-byte value that has
    // been base64-encoded.
    // The nonce must be selected randomly for each connection.

    const QByteArray secWebSocketKeyBase64 = generateBase64EncodedUniqueKey();
    computeBase64EncodedSha1VerificationKey(secWebSocketKeyBase64);

    const QString request =  QLatin1String("GET ") % url.path(QUrl::FullyEncoded) % QChar::fromLatin1('?')
                                % url.query(QUrl::FullyEncoded) % QLatin1String(" HTTP/1.1") % CRLF %

                             QLatin1String("Host: ") % url.host(QUrl::FullyEncoded) % QChar::fromLatin1(':')
                                % QString::number(url.port(8080)) % CRLF %

                             QLatin1String("Upgrade: websocket") % CRLF %
                             QLatin1String("Connection: upgrade") % CRLF %
                             QLatin1String("Sec-WebSocket-Key: ") % QString::fromUtf8(secWebSocketKeyBase64) % CRLF %
                             QLatin1String("Sec-WebSocket-Version: 13") % CRLF % CRLF;

    return request.toUtf8();
}

const QByteArray constructFrameHeader(bool isFinalFragment
                                      , int opcode
                                      , quint64 payloadLength
                                      , const QByteArray &maskingKey
                                      )
{
    QByteArray frameHeader(DefaultHeaderLength, 0);

    // FIN, x, x, x, Opcode
    frameHeader[0] = frameHeader[0] | (isFinalFragment ? FIN : NIL) | opcode;

    // Masking bit
    frameHeader[1] = frameHeader[1] | MSK;

    // Payload length. Small payload.
    if (payloadLength < NormalPayloadMarker)
        frameHeader[1] = frameHeader[1] | payloadLength;
    else {
        if (payloadLength > NormalPayloadLengthLimit) {
            frameHeader[1] = frameHeader[1] | LargePayloadMarker;
            quint64 lengthBigEndian = qToBigEndian<quint64>(payloadLength);
            QByteArray lengthBytesBigEndian(reinterpret_cast<char*>(&lengthBigEndian), LargePayloadHeaderLength);

            if (lengthBytesBigEndian[0] & MSB) {
                qDebug() << "\t ERROR: Payload too large!" << payloadLength;
                return QByteArray();
            }

            frameHeader.append(lengthBytesBigEndian);
        } else {
            frameHeader[1] = frameHeader[1] | NormalPayloadMarker;
            quint16 lengthBigEndian = qToBigEndian<quint16>(payloadLength);
            frameHeader.append(reinterpret_cast<char*>(&lengthBigEndian), DefaultHeaderLength);
        }
    }

    // Masking-key
    frameHeader.append(maskingKey);

    return frameHeader;
}

} // namespace

/*!
    \brief Class to establish a stateful connection to a backend.
    The communication is based on the WebSocket protocol.
    \sa connectToBackend

    \internal
*/

EnginioBackendConnection::EnginioBackendConnection(QObject *parent)
    : QObject(parent)
    , _protocolOpcode(ContinuationFrameOp)
    , _protocolDecodeState(HandshakePending)
    , _sentCloseFrame(false)
    , _isFinalFragment(false)
    , _isPayloadMasked(false)
    , _payloadLength(0)
    , _tcpSocket(new QTcpSocket(this))
{
    _tcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    _tcpSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    QObject::connect(_tcpSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onSocketConnectionError(QAbstractSocket::SocketError)));
    QObject::connect(_tcpSocket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(onSocketStateChanged(QAbstractSocket::SocketState)));
    QObject::connect(_tcpSocket, SIGNAL(readyRead()), this, SLOT(onSocketReadyRead()));
}

void EnginioBackendConnection::onEnginioFinished(EnginioReply *reply)
{
    struct ReplyScope {
        EnginioReply *_reply;
        ReplyScope(EnginioReply *r) : _reply(r) { }
        ~ReplyScope() { _reply->deleteLater(); }
    } scope(reply);

    if (reply->isError()) {
        qDebug() << "\n\n### EnginioBackendConnection ERROR";
        qDebug() << reply->errorString();
        reply->dumpDebugInfo();
        qDebug() << "\n###\n";
        return;
    }

    QJsonValue urlValue = reply->data()[EnginioString::expiringUrl];

    if (!urlValue.isString()) {
        qDebug() << "## Retrieving connection url failed.";
        return;
    }

    qDebug() << "## Initiating WebSocket connection.";

    _socketUrl = QUrl(urlValue.toString());
    _tcpSocket->connectToHost(_socketUrl.host(), _socketUrl.port(8080));
}

void EnginioBackendConnection::protocolError(const char* message, WebSocketCloseStatus status)
{
    qWarning() << QLatin1Literal(message) << QStringLiteral("Closing socket.");
    close(status);
    _tcpSocket->close();
}

void EnginioBackendConnection::timerEvent(QTimerEvent *event)
{

    if (event->timerId() == _keepAliveTimer.timerId()) {
        _pingTimeoutTimer.start(ThirtySeconds, this);
        ping();
        return;
    }

    if (event->timerId() == _pingTimeoutTimer.timerId()) {
        _pingTimeoutTimer.stop();
        close();
        emit timeOut();
        return;
    }

    QObject::timerEvent(event);
}

void EnginioBackendConnection::onSocketConnectionError(QAbstractSocket::SocketError error)
{
    protocolError("Socket connection error.");
    qWarning() << "\t\t->" << error;
}

void EnginioBackendConnection::onSocketStateChanged(QAbstractSocket::SocketState socketState)
{
    switch (socketState) {
    case QAbstractSocket::ConnectedState:
        qDebug() << "\t -> Starting WebSocket handshake.";
        _protocolDecodeState = HandshakePending;
        _sentCloseFrame = false;
        // The protocol handshake will appear to the HTTP server
        // to be a regular GET request with an Upgrade offer.
        _tcpSocket->write(constructOpeningHandshake(_socketUrl));
        break;
    case QAbstractSocket::ClosingState:
        _protocolDecodeState = HandshakePending;
        _applicationData.clear();
        _payloadLength = 0;
        break;
    case QAbstractSocket::UnconnectedState:
        emit stateChanged(DisconnectedState);
        break;
    default:
        break;
    }
}

void EnginioBackendConnection::onSocketReadyRead()
{
    //     WebSocket Protocol (RFC6455)
    //     Base Framing Protocol
    //     http://tools.ietf.org/html/rfc6455#section-5.2
    //
    //      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //     +-+-+-+-+-------+-+-------------+-------------------------------+
    //     |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
    //     |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
    //     |N|V|V|V|       |S|             |   (if payload len==126/127)   |
    //     | |1|2|3|       |K|             |                               |
    //     +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
    //     |     Extended payload length continued, if payload len == 127  |
    //     + - - - - - - - - - - - - - - - +-------------------------------+
    //     |                               |Masking-key, if MASK set to 1  |
    //     +-------------------------------+-------------------------------+
    //     | Masking-key (continued)       |          Payload Data         |
    //     +-------------------------------- - - - - - - - - - - - - - - - +
    //     :                     Payload Data continued ...                :
    //     + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
    //     |                     Payload Data continued ...                |
    //     +---------------------------------------------------------------+

    while (_tcpSocket->bytesAvailable()) {
        switch (_protocolDecodeState) {
        case HandshakePending: {
            // The response is closed by a CRLF line on its own (e.g. ends with two newlines).
            while (_handshakeReply.isEmpty()
                   || (!_handshakeReply.endsWith(QString(CRLF % CRLF).toUtf8())
                   // According to documentation QIODevice::readLine replaces newline characters on
                   // Windows with '\n', so just to be on the safe side:
                   && !_handshakeReply.endsWith(QByteArrayLiteral("\n\n")))) {

                if (!_tcpSocket->bytesAvailable())
                    return;

                _handshakeReply.append(_tcpSocket->readLine());
            }

            QString response = QString::fromUtf8(_handshakeReply);
            _handshakeReply.clear();

            int statusCode = extractResponseStatus(response);
            QString secWebSocketAccept = extractResponseHeader(SecWebSocketAcceptHeader, response, /* ignoreCase */ false);
            bool hasValidKey = secWebSocketAccept == gBase64EncodedSha1VerificationKey;

            if (statusCode != 101 || !hasValidKey
                    || extractResponseHeader(UpgradeHeader, response) != QStringLiteral("websocket")
                    || extractResponseHeader(ConnectionHeader, response) != QStringLiteral("upgrade")
                    )
                return protocolError("Handshake failed!");

            _keepAliveTimer.start(TwoMinutes, this);
            _protocolDecodeState = FrameHeaderPending;
            emit stateChanged(ConnectedState);
        } // Fall-through.

        case FrameHeaderPending: {
            if (quint64(_tcpSocket->bytesAvailable()) < DefaultHeaderLength)
                return;

            // Large payload.
            if (_payloadLength == LargePayloadMarker) {
                if (quint64(_tcpSocket->bytesAvailable()) < LargePayloadHeaderLength)
                    return;

                char data[LargePayloadHeaderLength];
                if (quint64(_tcpSocket->read(data, LargePayloadHeaderLength)) != LargePayloadHeaderLength)
                    return protocolError("Reading large payload length failed!");

                if (data[0] & MSB)
                    return protocolError("The most significant bit of a large payload length must be 0!", MessageTooBigCloseStatus);

                // 8 bytes interpreted as a 64-bit unsigned integer
                _payloadLength = qFromBigEndian<quint64>(reinterpret_cast<uchar*>(data));
                _protocolDecodeState = PayloadDataPending;

                break;
            }

            char data[DefaultHeaderLength];
            if (quint64(_tcpSocket->read(data, DefaultHeaderLength)) != DefaultHeaderLength)
                return protocolError("Reading header failed!");

            if (!_payloadLength) {
                // This is the initial frame header data.
                _isFinalFragment = (data[0] & FIN);
                _protocolOpcode = static_cast<WebSocketOpcode>(data[0] & OPC);
                _isPayloadMasked = (data[1] & MSK);
                _payloadLength = (data[1] & LEN);

                if (_isPayloadMasked)
                    return protocolError("Invalid masked frame received from server.");

                // For data length 0-125 LEN is the payload length.
                if (_payloadLength < NormalPayloadMarker)
                    _protocolDecodeState = PayloadDataPending;

            } else {
                Q_ASSERT(_payloadLength == NormalPayloadMarker);
                // Normal sized payload: 2 bytes interpreted as the payload
                // length expressed in network byte order (e.g. big endian).
                _payloadLength = qFromBigEndian<quint16>(reinterpret_cast<uchar*>(data));
                _protocolDecodeState = PayloadDataPending;
            }

            break;
        }

        case PayloadDataPending: {
            if (static_cast<quint64>(_tcpSocket->bytesAvailable()) < _payloadLength)
                return;

            if (_protocolOpcode == ConnectionCloseOp) {
                WebSocketCloseStatus closeStatus = UnknownCloseStatus;
                if (_payloadLength >= DefaultHeaderLength) {
                    char data[DefaultHeaderLength];
                    if (quint64(_tcpSocket->read(data, DefaultHeaderLength)) != DefaultHeaderLength)
                        return protocolError("Reading connection close status failed!");

                     closeStatus = static_cast<WebSocketCloseStatus>(qFromBigEndian<quint16>(reinterpret_cast<uchar*>(data)));

                     // The body may contain UTF-8-encoded data with value /reason/,
                     // the interpretation of this data is however not defined by the
                     // specification. Further more the data is not guaranteed to be
                     // human readable, thus it is safe for us to just discard the rest
                     // of the message at this point.
                }

                qDebug() << "Connection closed by the server with status:" << closeStatus;

                QJsonObject data;
                data[EnginioString::messageType] = QStringLiteral("close");
                data[EnginioString::status] = closeStatus;
                emit dataReceived(data);

                close(closeStatus);

                _tcpSocket->close();
                return;
            }

            // We received data from the server so restart the timer.
            _keepAliveTimer.start(TwoMinutes, this);

            _applicationData.append(_tcpSocket->read(_payloadLength));
            _protocolDecodeState = FrameHeaderPending;
            _payloadLength = 0;

            if (!_isFinalFragment)
                break;

            switch (_protocolOpcode) {
            case TextFrameOp: {
                QJsonObject data = QJsonDocument::fromJson(_applicationData).object();
                data[EnginioString::messageType] = QStringLiteral("data");
                emit dataReceived(data);
                break;
            }
            case PingOp:{
                // We must send back identical application data as found in the message.
                QByteArray payload = _applicationData;
                QByteArray maskingKey = generateMaskingKey();
                QByteArray message = constructFrameHeader(/*isFinalFragment*/ true, PongOp, payload.size(), maskingKey);
                Q_ASSERT(!message.isEmpty());
                maskData(payload, maskingKey);
                message.append(payload);
                _tcpSocket->write(message);
                break;
            }
            case PongOp:
                _pingTimeoutTimer.stop();
                emit pong();
                break;
            default:
                protocolError("WebSocketOpcode not yet supported.", UnsupportedDataTypeCloseStatus);
                qWarning() << "\t\t->" << _protocolOpcode;
            }

            _applicationData.clear();

            break;
        }
        }
    }
}

/*!
    \brief Establish a stateful connection to the backend specified by EnginioClient
    \a client. Note that the client already has to be set up (e.g. backendId has to be valid).
    Optionally, to let the server only send specific messages of interest,
    a \a messageFilter can be provided with the following json scheme:

    {
        "data": {
            "objectType": "objects.todos"
        },
        "event": "create"
    }

    The "event" property can be one of "create", "update" or "delete".

    \internal
*/

void EnginioBackendConnection::connectToBackend(EnginioClientConnectionPrivate* client, const QJsonObject &messageFilter)
{
    Q_ASSERT(client);
    Q_ASSERT(!client->_backendId.isEmpty());

    QUrl url(client->_serviceUrl);
    url.setPath(QStringLiteral("/v1/stream_url"));

    QByteArray filter = QJsonDocument(messageFilter).toJson(QJsonDocument::Compact);
    filter.prepend("filter=");
    url.setQuery(QString::fromUtf8(filter));

    QJsonObject headers;
    headers[ QStringLiteral("Accept") ] = QStringLiteral("application/json");
    QJsonObject data;
    data[EnginioString::headers] = headers;

    emit stateChanged(ConnectingState);
    QNetworkReply *nreply = client->customRequest(url, EnginioString::Get, data);
    EnginioReply *reply = new EnginioReply(client, nreply);
    QObject::connect(reply, SIGNAL(finished(EnginioReply*)), this, SLOT(onEnginioFinished(EnginioReply*)));
}

void EnginioBackendConnection::connectToBackend(EnginioClient *client, const QJsonObject &messageFilter)
{
    connectToBackend(EnginioClientConnectionPrivate::get(client), messageFilter);
}

void EnginioBackendConnection::close(WebSocketCloseStatus closeStatus)
{
    if (_sentCloseFrame)
        return;

    _sentCloseFrame = true;
    _keepAliveTimer.stop();

    QByteArray payload;
    quint16 closeStatusBigEndian = qToBigEndian<quint16>(closeStatus);
    payload.append(reinterpret_cast<char*>(&closeStatusBigEndian), DefaultHeaderLength);

    QByteArray maskingKey = generateMaskingKey();
    QByteArray message = constructFrameHeader(/*isFinalFragment*/ true, ConnectionCloseOp, payload.size(), maskingKey);
    Q_ASSERT(!message.isEmpty());

    maskData(payload, maskingKey);
    message.append(payload);
    _tcpSocket->write(message);
}

void EnginioBackendConnection::ping()
{
    if (_sentCloseFrame)
        return;

    // The WebSocket server should accept ping frames without payload according to
    // the specification, but ours does not, so let's add a dummy payload.
    QByteArray dummy;
    dummy.append(QStringLiteral("Ping.").toUtf8());
    QByteArray maskingKey = generateMaskingKey();
    QByteArray message = constructFrameHeader(/*isFinalFragment*/ true, PingOp, dummy.size(), maskingKey);
    Q_ASSERT(!message.isEmpty());

    maskData(dummy, maskingKey);
    message.append(dummy);
    _tcpSocket->write(message);
}

QT_END_NAMESPACE
