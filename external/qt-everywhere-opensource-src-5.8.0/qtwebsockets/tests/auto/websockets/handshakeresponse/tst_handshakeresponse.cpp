/****************************************************************************
**
** Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include <QtTest/QtTest>
#include <QtTest/qtestcase.h>
#include <QtCore/QDebug>
#include <QtCore/QByteArray>
#include <QtCore/QtEndian>

#include "private/qwebsockethandshakerequest_p.h"
#include "private/qwebsockethandshakeresponse_p.h"
#include "private/qwebsocketprotocol_p.h"
#include "QtWebSockets/qwebsocketprotocol.h"

QT_USE_NAMESPACE

Q_DECLARE_METATYPE(QWebSocketProtocol::CloseCode)
Q_DECLARE_METATYPE(QWebSocketProtocol::OpCode)

class tst_HandshakeResponse : public QObject
{
    Q_OBJECT

public:
    tst_HandshakeResponse();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void tst_date_response();
};

tst_HandshakeResponse::tst_HandshakeResponse()
{}

void tst_HandshakeResponse::initTestCase()
{
}

void tst_HandshakeResponse::cleanupTestCase()
{}

void tst_HandshakeResponse::init()
{
    qRegisterMetaType<QWebSocketProtocol::OpCode>("QWebSocketProtocol::OpCode");
    qRegisterMetaType<QWebSocketProtocol::CloseCode>("QWebSocketProtocol::CloseCode");
}

void tst_HandshakeResponse::cleanup()
{
}

void tst_HandshakeResponse::tst_date_response()
{
    QWebSocketHandshakeRequest request(80, false);
    QString buffer;
    QTextStream input(&buffer);
    input << QStringLiteral("GET / HTTP/1.1\r\nHost: example.com\r\nSec-WebSocket-Version: 13\r\n") +
             QStringLiteral("Sec-WebSocket-Key: AVDFBDDFF\r\n") +
             QStringLiteral("Upgrade: websocket\r\n") +
             QStringLiteral("Connection: Upgrade\r\n\r\n");
    request.readHandshake(input, 8 * 1024, 100);

    QWebSocketHandshakeResponse response(request, "example.com", true,
                                         QList<QWebSocketProtocol::Version>() << QWebSocketProtocol::Version13,
                                         QList<QString>(),
                                         QList<QString>());
    QString data;
    QTextStream output(&data);
    output << response;

    QStringList list = data.split("\r\n");
    int index = list.indexOf(QRegExp("Date:.*"));
    QVERIFY(index > -1);
    QVERIFY(QLocale::c().toDateTime(list[index], "'Date:' ddd, dd MMM yyyy hh:mm:ss 'GMT'").isValid());
}

QTEST_MAIN(tst_HandshakeResponse)

#include "tst_handshakeresponse.moc"
