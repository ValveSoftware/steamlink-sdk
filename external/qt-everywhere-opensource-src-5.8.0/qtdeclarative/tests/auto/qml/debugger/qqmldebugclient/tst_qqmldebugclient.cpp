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

#include "debugutil_p.h"
#include "qqmldebugtestservice.h"

#include <private/qqmldebugconnector_p.h>
#include <private/qqmldebugconnection_p.h>

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>
#include <QtCore/qtimer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>
#include <QtNetwork/qhostaddress.h>
#include <QtQml/qqmlengine.h>

#define PORT 13770
#define STR_PORT "13770"

class tst_QQmlDebugClient : public QObject
{
    Q_OBJECT

private:
    QQmlDebugConnection *m_conn;
    QQmlDebugTestService *m_service;

private slots:
    void initTestCase();

    void name();
    void state();
    void sendMessage();
    void parallelConnect();
    void sequentialConnect();
};

void tst_QQmlDebugClient::initTestCase()
{
    QQmlDebugConnector::setPluginKey(QLatin1String("QQmlDebugServer"));
    QQmlDebugConnector::setServices(QStringList()
                                    << QStringLiteral("tst_QQmlDebugClient::handshake()"));
    QTest::ignoreMessage(QtWarningMsg,
                         "QML debugger: Cannot set plugin key after loading the plugin.");

    m_service = new QQmlDebugTestService("tst_QQmlDebugClient::handshake()");

    foreach (const QString &service, QQmlDebuggingEnabler::debuggerServices())
        QCOMPARE(QQmlDebugConnector::instance()->service(service), (QQmlDebugService *)0);
    foreach (const QString &service, QQmlDebuggingEnabler::inspectorServices())
        QCOMPARE(QQmlDebugConnector::instance()->service(service), (QQmlDebugService *)0);
    foreach (const QString &service, QQmlDebuggingEnabler::profilerServices())
        QCOMPARE(QQmlDebugConnector::instance()->service(service), (QQmlDebugService *)0);

    const QString waitingMsg = QString("QML Debugger: Waiting for connection on port %1...").arg(PORT);
    QTest::ignoreMessage(QtDebugMsg, waitingMsg.toLatin1().constData());
    QQmlDebuggingEnabler::startTcpDebugServer(PORT);

    new QQmlEngine(this);

    m_conn = new QQmlDebugConnection(this);

    QQmlDebugTestClient client("tst_QQmlDebugClient::handshake()", m_conn);


    for (int i = 0; i < 50; ++i) {
        // try for 5 seconds ...
        m_conn->connectToHost("127.0.0.1", PORT);
        if (m_conn->waitForConnected(100))
            break;
    }

    QVERIFY(m_conn->isConnected());

    QTRY_COMPARE(client.state(), QQmlDebugClient::Enabled);
}

void tst_QQmlDebugClient::name()
{
    QString name = "tst_QQmlDebugClient::name()";

    QQmlDebugClient client(name, m_conn);
    QCOMPARE(client.name(), name);
}

void tst_QQmlDebugClient::state()
{
    {
        QQmlDebugConnection dummyConn;
        QQmlDebugClient client("tst_QQmlDebugClient::state()", &dummyConn);
        QCOMPARE(client.state(), QQmlDebugClient::NotConnected);
        QCOMPARE(client.serviceVersion(), -1.0f);
    }

    QQmlDebugTestClient client("tst_QQmlDebugClient::state()", m_conn);
    QCOMPARE(client.state(), QQmlDebugClient::Unavailable);

    // duplicate plugin name
    QTest::ignoreMessage(QtWarningMsg, "QQmlDebugClient: Conflicting plugin name \"tst_QQmlDebugClient::state()\"");
    QQmlDebugClient client2("tst_QQmlDebugClient::state()", m_conn);
    QCOMPARE(client2.state(), QQmlDebugClient::NotConnected);

    QQmlDebugClient client3("tst_QQmlDebugClient::state3()", 0);
    QCOMPARE(client3.state(), QQmlDebugClient::NotConnected);
}

void tst_QQmlDebugClient::sendMessage()
{
    QQmlDebugTestClient client("tst_QQmlDebugClient::handshake()", m_conn);

    QByteArray msg = "hello!";

    QTRY_COMPARE(client.state(), QQmlDebugClient::Enabled);

    client.sendMessage(msg);
    QByteArray resp = client.waitForResponse();
    QCOMPARE(resp, msg);
}

void tst_QQmlDebugClient::parallelConnect()
{
    QQmlDebugConnection connection2;

    QTest::ignoreMessage(QtWarningMsg, "QML Debugger: Another client is already connected.");
    // will connect & immediately disconnect
    connection2.connectToHost("127.0.0.1", PORT);
    QTest::ignoreMessage(QtWarningMsg, "QQmlDebugConnection: Did not get handshake answer in time");
    QVERIFY(!connection2.waitForConnected(1000));
    QVERIFY(!connection2.isConnected());
    QVERIFY(m_conn->isConnected());
}

void tst_QQmlDebugClient::sequentialConnect()
{
    QQmlDebugConnection connection2;
    QQmlDebugTestClient client2("tst_QQmlDebugClient::handshake()", &connection2);

    m_conn->close();
    QVERIFY(!m_conn->isConnected());

    // Make sure that the disconnect is actually delivered to the server
    QTest::qWait(100);

    connection2.connectToHost("127.0.0.1", PORT);
    QVERIFY(connection2.waitForConnected());
    QVERIFY(connection2.isConnected());
    QTRY_COMPARE(client2.state(), QQmlDebugClient::Enabled);
}

QTEST_MAIN(tst_QQmlDebugClient)

#include "tst_qqmldebugclient.moc"

