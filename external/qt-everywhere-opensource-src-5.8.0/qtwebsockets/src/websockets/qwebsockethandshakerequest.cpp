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

#include "qwebsockethandshakerequest_p.h"
#include "qwebsocketprotocol.h"
#include "qwebsocketprotocol_p.h"

#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtCore/QTextStream>
#include <QtCore/QUrl>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <functional>   //for std::greater

QT_BEGIN_NAMESPACE

/*!
    \brief Constructs a new QWebSocketHandshakeRequest given \a port and \a isSecure
    \internal
 */
QWebSocketHandshakeRequest::QWebSocketHandshakeRequest(int port, bool isSecure) :
    m_port(port),
    m_isSecure(isSecure),
    m_isValid(false),
    m_headers(),
    m_versions(),
    m_key(),
    m_origin(),
    m_protocols(),
    m_extensions(),
    m_requestUrl()
{
}

/*!
    \internal
 */
QWebSocketHandshakeRequest::~QWebSocketHandshakeRequest()
{
}

/*!
    \brief Clears the request
    \internal
 */
void QWebSocketHandshakeRequest::clear()
{
    m_isValid = false;
    m_headers.clear();
    m_versions.clear();
    m_key.clear();
    m_origin.clear();
    m_protocols.clear();
    m_extensions.clear();
    m_requestUrl.clear();
}

/*!
    \internal
 */
int QWebSocketHandshakeRequest::port() const
{
    return m_requestUrl.port(m_port);
}

/*!
    \internal
 */
bool QWebSocketHandshakeRequest::isSecure() const
{
    return m_isSecure;
}

/*!
    \internal
 */
bool QWebSocketHandshakeRequest::isValid() const
{
    return m_isValid;
}

/*!
    \internal
 */
QMap<QString, QString> QWebSocketHandshakeRequest::headers() const
{
    return m_headers;
}

/*!
    \internal
 */
QList<QWebSocketProtocol::Version> QWebSocketHandshakeRequest::versions() const
{
    return m_versions;
}

/*!
    \internal
 */
QString QWebSocketHandshakeRequest::resourceName() const
{
    return m_requestUrl.path();
}

/*!
    \internal
 */
QString QWebSocketHandshakeRequest::key() const
{
    return m_key;
}

/*!
    \internal
 */
QString QWebSocketHandshakeRequest::host() const
{
    return m_requestUrl.host();
}

/*!
    \internal
 */
QString QWebSocketHandshakeRequest::origin() const
{
    return m_origin;
}

/*!
    \internal
 */
QList<QString> QWebSocketHandshakeRequest::protocols() const
{
    return m_protocols;
}

/*!
    \internal
 */
QList<QString> QWebSocketHandshakeRequest::extensions() const
{
    return m_extensions;
}

/*!
    \internal
 */
QUrl QWebSocketHandshakeRequest::requestUrl() const
{
    return m_requestUrl;
}

/*!
    Reads a line of text from the given textstream (terminated by CR/LF).
    If an empty line was detected, an empty string is returned.
    When an error occurs, a null string is returned.
    \internal
 */
static QString readLine(QTextStream &stream, int maxHeaderLineLength)
{
    QString line;
    char c;
    while (!stream.atEnd()) {
        stream >> c;
        if (stream.status() != QTextStream::Ok)
            return QString();
        if (c == char('\r')) {
            //eat the \n character
            stream >> c;
            line.append(QStringLiteral(""));
            break;
        } else {
            line.append(QChar::fromLatin1(c));
            if (line.length() > maxHeaderLineLength)
                return QString();
        }
    }
    return line;
}

/*!
    \internal
 */
void QWebSocketHandshakeRequest::readHandshake(QTextStream &textStream, int maxHeaderLineLength,
                                               int maxHeaders)
{
    clear();
    if (Q_UNLIKELY(textStream.status() != QTextStream::Ok))
        return;
    const QString requestLine = readLine(textStream, maxHeaderLineLength);
    if (requestLine.isNull()) {
        clear();
        return;
    }
    const QStringList tokens = requestLine.split(' ', QString::SkipEmptyParts);
    if (Q_UNLIKELY(tokens.length() < 3)) {
        clear();
        return;
    }
    const QString verb(tokens.at(0));
    const QString resourceName(tokens.at(1));
    const QString httpProtocol(tokens.at(2));
    bool conversionOk = false;
    const float httpVersion = httpProtocol.midRef(5).toFloat(&conversionOk);

    if (Q_UNLIKELY(!conversionOk)) {
        clear();
        return;
    }
    QString headerLine = readLine(textStream, maxHeaderLineLength);
    if (headerLine.isNull()) {
        clear();
        return;
    }
    m_headers.clear();
    while (!headerLine.isEmpty()) {
        const QStringList headerField = headerLine.split(QStringLiteral(": "),
                                                         QString::SkipEmptyParts);
        if (Q_UNLIKELY(headerField.length() < 2)) {
            clear();
            return;
        }
        m_headers.insertMulti(headerField.at(0).toLower(), headerField.at(1));
        if (m_headers.size() > maxHeaders) {
            clear();
            return;
        }
        headerLine = readLine(textStream, maxHeaderLineLength);
        if (headerLine.isNull()) {
            clear();
            return;
        }
    }

    m_requestUrl = QUrl::fromEncoded(resourceName.toLatin1());
    QString host = m_headers.value(QStringLiteral("host"), QString());
    if (m_requestUrl.isRelative()) {
        // see http://tools.ietf.org/html/rfc6455#page-17
        // No. 4 item in "The requirements for this handshake"
        int idx = host.indexOf(QStringLiteral(":"));
        bool ok = false;
        int port = 0;
        if (idx != -1) {
            port = host.rightRef(host.length() - idx - 1).toInt(&ok);
            host.truncate(idx);
        }
        m_requestUrl.setHost(host);
        if (ok)
            m_requestUrl.setPort(port);
    }
    if (m_requestUrl.scheme().isEmpty()) {
        const QString scheme =  isSecure() ? QStringLiteral("wss") : QStringLiteral("ws");
        m_requestUrl.setScheme(scheme);
    }

    const QStringList versionLines = m_headers.values(QStringLiteral("sec-websocket-version"));
    for (QStringList::const_iterator v = versionLines.begin(); v != versionLines.end(); ++v) {
        const QStringList versions = (*v).split(QStringLiteral(","), QString::SkipEmptyParts);
        for (QStringList::const_iterator i = versions.begin(); i != versions.end(); ++i) {
            bool ok = false;
            (void)(*i).toUInt(&ok);
            if (!ok) {
                clear();
                return;
            }
            const QWebSocketProtocol::Version ver =
                    QWebSocketProtocol::versionFromString((*i).trimmed());
            m_versions << ver;
        }
    }
    //sort in descending order
    std::sort(m_versions.begin(), m_versions.end(), std::greater<QWebSocketProtocol::Version>());
    m_key = m_headers.value(QStringLiteral("sec-websocket-key"), QString());
    //must contain "Upgrade", case-insensitive
    const QString upgrade = m_headers.value(QStringLiteral("upgrade"), QString());
    //must be equal to "websocket", case-insensitive
    const QString connection = m_headers.value(QStringLiteral("connection"), QString());
    const QStringList connectionLine = connection.split(QStringLiteral(","),
                                                        QString::SkipEmptyParts);
    QStringList connectionValues;
    for (QStringList::const_iterator c = connectionLine.begin(); c != connectionLine.end(); ++c)
        connectionValues << (*c).trimmed();

    //optional headers
    m_origin = m_headers.value(QStringLiteral("origin"), QString());
    const QStringList protocolLines = m_headers.values(QStringLiteral("sec-websocket-protocol"));
    for (QStringList::const_iterator pl = protocolLines.begin(); pl != protocolLines.end(); ++pl) {
        QStringList protocols = (*pl).split(QStringLiteral(","), QString::SkipEmptyParts);
        for (QStringList::const_iterator p = protocols.begin(); p != protocols.end(); ++p)
            m_protocols << (*p).trimmed();
    }
    const QStringList extensionLines = m_headers.values(QStringLiteral("sec-websocket-extensions"));
    for (QStringList::const_iterator el = extensionLines.begin();
         el != extensionLines.end(); ++el) {
        QStringList extensions = (*el).split(QStringLiteral(","), QString::SkipEmptyParts);
        for (QStringList::const_iterator e = extensions.begin(); e != extensions.end(); ++e)
            m_extensions << (*e).trimmed();
    }

    //TODO: authentication field

    m_isValid = !(host.isEmpty() ||
                  resourceName.isEmpty() ||
                  m_versions.isEmpty() ||
                  m_key.isEmpty() ||
                  (verb != QStringLiteral("GET")) ||
                  (!conversionOk || (httpVersion < 1.1f)) ||
                  (upgrade.toLower() != QStringLiteral("websocket")) ||
                  (!connectionValues.contains(QStringLiteral("upgrade"), Qt::CaseInsensitive)));
    if (Q_UNLIKELY(!m_isValid))
        clear();
}

QT_END_NAMESPACE
