/****************************************************************************
**
** Copyright (C) 2014 Kurt Pattyn <pattyn.kurt@gmail.com>.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
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
    request.readHandshake(input);

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
