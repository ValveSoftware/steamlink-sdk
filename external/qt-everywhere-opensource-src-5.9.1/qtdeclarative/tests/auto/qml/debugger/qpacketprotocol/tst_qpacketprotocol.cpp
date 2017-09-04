/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
#include <qtest.h>
#include <QSignalSpy>
#include <QTimer>
#include <QTcpSocket>
#include <QTcpServer>
#include <QDebug>
#include <QBuffer>

#include <private/qpacketprotocol_p.h>
#include <private/qpacket_p.h>

#include "debugutil_p.h"

class tst_QPacketProtocol : public QObject
{
    Q_OBJECT

private:
    QTcpServer *m_server;
    QTcpSocket *m_client;
    QTcpSocket *m_serverConn;

private slots:
    void init();
    void cleanup();

    void send();
    void packetsAvailable();
    void packetsAvailable_data();
    void read();
};

void tst_QPacketProtocol::init()
{
    m_server = new QTcpServer(this);
    m_serverConn = 0;
    QVERIFY(m_server->listen(QHostAddress("127.0.0.1")));

    m_client = new QTcpSocket(this);

    QSignalSpy serverSpy(m_server, SIGNAL(newConnection()));
    QSignalSpy clientSpy(m_client, SIGNAL(connected()));

    m_client->connectToHost(m_server->serverAddress(), m_server->serverPort());

    QVERIFY(clientSpy.count() > 0 || clientSpy.wait());
    QVERIFY(serverSpy.count() > 0 || serverSpy.wait());

    m_serverConn = m_server->nextPendingConnection();
}

void tst_QPacketProtocol::cleanup()
{
    delete m_client;
    delete m_serverConn;
    delete m_server;
}

void tst_QPacketProtocol::send()
{
    QPacketProtocol in(m_client);
    QPacketProtocol out(m_serverConn);

    QByteArray ba;
    int num;

    QPacket packet(QDataStream::Qt_DefaultCompiledVersion);
    packet << "Hello world" << 123;
    out.send(packet.data());

    QVERIFY(QQmlDebugTest::waitForSignal(&in, SIGNAL(readyRead())));

    QPacket p(QDataStream::Qt_DefaultCompiledVersion, in.read());
    p >> ba >> num;
    QCOMPARE(ba, QByteArray("Hello world") + '\0');
    QCOMPARE(num, 123);
}

void tst_QPacketProtocol::packetsAvailable()
{
    QFETCH(int, packetCount);

    QPacketProtocol out(m_client);
    QPacketProtocol in(m_serverConn);

    QCOMPARE(out.packetsAvailable(), qint64(0));
    QCOMPARE(in.packetsAvailable(), qint64(0));

    for (int i=0; i<packetCount; i++) {
        QPacket packet(QDataStream::Qt_DefaultCompiledVersion);
        packet << "Hello";
        out.send(packet.data());
    }

    QVERIFY(QQmlDebugTest::waitForSignal(&in, SIGNAL(readyRead())));
    QCOMPARE(in.packetsAvailable(), qint64(packetCount));
}

void tst_QPacketProtocol::packetsAvailable_data()
{
    QTest::addColumn<int>("packetCount");

    QTest::newRow("1") << 1;
    QTest::newRow("2") << 2;
    QTest::newRow("10") << 10;
}

void tst_QPacketProtocol::read()
{
    QPacketProtocol in(m_client);
    QPacketProtocol out(m_serverConn);

    QVERIFY(in.read().isEmpty());

    QPacket packet(QDataStream::Qt_DefaultCompiledVersion);
    packet << 123;
    out.send(packet.data());

    QPacket packet2(QDataStream::Qt_DefaultCompiledVersion);
    packet2 << 456;
    out.send(packet2.data());
    QVERIFY(QQmlDebugTest::waitForSignal(&in, SIGNAL(readyRead())));

    int num;

    QPacket p1(QDataStream::Qt_DefaultCompiledVersion, in.read());
    QVERIFY(!p1.atEnd());
    p1 >> num;
    QCOMPARE(num, 123);

    QPacket p2(QDataStream::Qt_DefaultCompiledVersion, in.read());
    QVERIFY(!p2.atEnd());
    p2 >> num;
    QCOMPARE(num, 456);

    QVERIFY(in.read().isEmpty());
}

QTEST_MAIN(tst_QPacketProtocol)

#include "tst_qpacketprotocol.moc"
