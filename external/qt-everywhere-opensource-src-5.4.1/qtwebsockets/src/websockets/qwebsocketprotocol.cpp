/****************************************************************************
**
** Copyright (C) 2014 Kurt Pattyn <pattyn.kurt@gmail.com>.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebSockets module of the Qt Toolkit.
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

#include "qwebsocketprotocol_p.h"
#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtCore/QtEndian>

QT_BEGIN_NAMESPACE

/*!
  \namespace QWebSocketProtocol
  \inmodule QtWebSockets
  \brief Contains constants related to the WebSocket standard.
  \since 5.3
*/

/*!
    \enum QWebSocketProtocol::CloseCode

    \inmodule QtWebSockets

    The close codes supported by WebSockets V13

    \value CloseCodeNormal                  Normal closure
    \value CloseCodeGoingAway               Going away
    \value CloseCodeProtocolError           Protocol error
    \value CloseCodeDatatypeNotSupported    Unsupported data
    \value CloseCodeReserved1004            Reserved
    \value CloseCodeMissingStatusCode       No status received
    \value CloseCodeAbnormalDisconnection   Abnormal closure
    \value CloseCodeWrongDatatype           Invalid frame payload data
    \value CloseCodePolicyViolated          Policy violation
    \value CloseCodeTooMuchData             Message too big
    \value CloseCodeMissingExtension        Mandatory extension missing
    \value CloseCodeBadOperation            Internal server error
    \value CloseCodeTlsHandshakeFailed      TLS handshake failed

    \sa QWebSocket::close()
*/
/*!
    \enum QWebSocketProtocol::Version

    \inmodule QtWebSockets

    \brief The different defined versions of the WebSocket protocol.

    For an overview of the differences between the different protocols, see
    <http://code.google.com/p/pywebsocket/wiki/WebSocketProtocolSpec>

    \value VersionUnknown   Unknown or unspecified version.
    \value Version0         hixie76:
                            http://tools.ietf.org/html/draft-hixie-thewebsocketprotocol-76 &
                            hybi-00:
                            http://tools.ietf.org/html/draft-ietf-hybi-thewebsocketprotocol-00.
                            Works with key1, key2 and a key in the payload.
                            Attribute: Sec-WebSocket-Draft value 0.
                            Not supported by QtWebSockets.
    \value Version4         hybi-04:
                            http://tools.ietf.org/id/draft-ietf-hybi-thewebsocketprotocol-04.txt.
                            Changed handshake: key1, key2, key3
                            ==> Sec-WebSocket-Key, Sec-WebSocket-Nonce, Sec-WebSocket-Accept
                            Sec-WebSocket-Draft renamed to Sec-WebSocket-Version
                            Sec-WebSocket-Version = 4.
                            Not supported by QtWebSockets.
    \value Version5         hybi-05:
                            http://tools.ietf.org/id/draft-ietf-hybi-thewebsocketprotocol-05.txt.
                            Sec-WebSocket-Version = 5
                            Removed Sec-WebSocket-Nonce
                            Added Sec-WebSocket-Accept.
                            Not supported by QtWebSockets.
    \value Version6         Sec-WebSocket-Version = 6.
                            Not supported by QtWebSockets.
    \value Version7         hybi-07:
                            http://tools.ietf.org/html/draft-ietf-hybi-thewebsocketprotocol-07.
                            Sec-WebSocket-Version = 7.
                            Not supported by QtWebSockets.
    \value Version8         hybi-8, hybi-9, hybi-10, hybi-11 and hybi-12.
                            Status codes 1005 and 1006 are added and all codes are now unsigned
                            Internal error results in 1006.
                            Not supported by QtWebSockets.
    \value Version13        hybi-13, hybi14, hybi-15, hybi-16, hybi-17 and RFC 6455.
                            Sec-WebSocket-Version = 13
                            Status code 1004 is now reserved
                            Added 1008, 1009 and 1010
                            Must support TLS
                            Clarify multiple version support.
                            Supported by QtWebSockets.
    \value VersionLatest    Refers to the latest known version to QtWebSockets.
*/

/*!
    \enum QWebSocketProtocol::OpCode

    \inmodule QtWebSockets

    The frame opcodes as defined by the WebSockets standard

    \value OpCodeContinue     Continuation frame
    \value OpCodeText         Text frame
    \value OpCodeBinary       Binary frame
    \value OpCodeReserved3    Reserved
    \value OpCodeReserved4    Reserved
    \value OpCodeReserved5    Reserved
    \value OpCodeReserved6    Reserved
    \value OpCodeReserved7    Reserved
    \value OpCodeClose        Close frame
    \value OpCodePing         Ping frame
    \value OpCodePong         Pong frame
    \value OpCodeReservedB    Reserved
    \value OpCodeReservedC    Reserved
    \value OpCodeReservedD    Reserved
    \value OpCodeReservedE    Reserved
    \value OpCodeReservedF    Reserved

    \internal
*/

/*!
  \fn QWebSocketProtocol::isOpCodeReserved(OpCode code)
  Checks if \a code is a valid OpCode

  \internal
*/

/*!
  \fn QWebSocketProtocol::isCloseCodeValid(int closeCode)
  Checks if \a closeCode is a valid WebSocket close code

  \internal
*/

/*!
  \fn QWebSocketProtocol::currentVersion()
  Returns the latest version that WebSocket is supporting

  \internal
*/

/*!
    Parses the \a versionString and converts it to a Version value

    \internal
*/
QWebSocketProtocol::Version QWebSocketProtocol::versionFromString(const QString &versionString)
{
    bool ok = false;
    Version version = VersionUnknown;
    const int ver = versionString.toInt(&ok);
    QSet<Version> supportedVersions;
    supportedVersions << Version0 << Version4 << Version5 << Version6 << Version7 << Version8
                      << Version13;
    if (Q_LIKELY(ok) && (supportedVersions.contains(static_cast<Version>(ver))))
        version = static_cast<Version>(ver);
    return version;
}

/*!
    Mask the \a payload with the given \a maskingKey and stores the result back in \a payload.

    \internal
*/
void QWebSocketProtocol::mask(QByteArray *payload, quint32 maskingKey)
{
    Q_ASSERT(payload);
    mask(payload->data(), payload->size(), maskingKey);
}

/*!
    Masks the \a payload of length \a size with the given \a maskingKey and
    stores the result back in \a payload.

    \internal
*/
void QWebSocketProtocol::mask(char *payload, quint64 size, quint32 maskingKey)
{
    Q_ASSERT(payload);
    const quint8 mask[] = { quint8((maskingKey & 0xFF000000u) >> 24),
                            quint8((maskingKey & 0x00FF0000u) >> 16),
                            quint8((maskingKey & 0x0000FF00u) >> 8),
                            quint8((maskingKey & 0x000000FFu))
                          };
    int i = 0;
    while (size-- > 0)
        *payload++ ^= mask[i++ % 4];
}

QT_END_NAMESPACE
