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

#include "qqmldebugtestservice.h"
#include "debugutil_p.h"

#include <private/qqmldebugconnector_p.h>
#include <private/qqmldebugconnection_p.h>

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>
#include <QtNetwork/qhostaddress.h>
#include <QtCore/qtimer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>

#include <ctime>

QString fileName;

class tst_QQmlDebugLocal : public QObject
{
    Q_OBJECT

private:
    QQmlDebugConnection *m_conn;
    QQmlDebugTestService *m_service;

    bool connect();

signals:
    void waiting();
    void parallel();

private slots:
    void initTestCase();

    void name();
    void state();
    void sendMessage();
};

void tst_QQmlDebugLocal::initTestCase()
{
    fileName = QString::fromLatin1("tst_QQmlDebugLocal%1").arg(std::time(0));
    QQmlDebugConnector::setPluginKey("QQmlDebugServer");
    QTest::ignoreMessage(QtWarningMsg,
                         "QML debugger: Cannot set plugin key after loading the plugin.");
    m_service = new QQmlDebugTestService("tst_QQmlDebugLocal::handshake()");

    const QString waitingMsg = QString("QML Debugger: Connecting to socket %1...").arg(fileName);
    QTest::ignoreMessage(QtDebugMsg, waitingMsg.toLatin1().constData());

    m_conn = new QQmlDebugConnection(this);
    m_conn->startLocalServer(fileName);

    QQmlDebuggingEnabler::connectToLocalDebugger(fileName);

    new QQmlEngine(this);

    QQmlDebugTestClient client("tst_QQmlDebugLocal::handshake()", m_conn);

    for (int i = 0; i < 50; ++i) {
        // try for 5 seconds ...
        if (m_conn->waitForConnected(100))
            break;
    }

    QVERIFY(m_conn->isConnected());

    QTRY_COMPARE(client.state(), QQmlDebugClient::Enabled);
}

void tst_QQmlDebugLocal::name()
{
    QString name = "tst_QQmlDebugLocal::name()";

    QQmlDebugClient client(name, m_conn);
    QCOMPARE(client.name(), name);
}

void tst_QQmlDebugLocal::state()
{
    {
        QQmlDebugConnection dummyConn;
        QQmlDebugClient client("tst_QQmlDebugLocal::state()", &dummyConn);
        QCOMPARE(client.state(), QQmlDebugClient::NotConnected);
        QCOMPARE(client.serviceVersion(), -1.0f);
    }

    QQmlDebugTestClient client("tst_QQmlDebugLocal::state()", m_conn);
    QCOMPARE(client.state(), QQmlDebugClient::Unavailable);

    // duplicate plugin name
    QTest::ignoreMessage(QtWarningMsg, "QQmlDebugClient: Conflicting plugin name \"tst_QQmlDebugLocal::state()\"");
    QQmlDebugClient client2("tst_QQmlDebugLocal::state()", m_conn);
    QCOMPARE(client2.state(), QQmlDebugClient::NotConnected);

    QQmlDebugClient client3("tst_QQmlDebugLocal::state3()", 0);
    QCOMPARE(client3.state(), QQmlDebugClient::NotConnected);
}

void tst_QQmlDebugLocal::sendMessage()
{
    QQmlDebugTestClient client("tst_QQmlDebugLocal::handshake()", m_conn);

    QByteArray msg = "hello!";

    QTRY_COMPARE(client.state(), QQmlDebugClient::Enabled);

    client.sendMessage(msg);
    QByteArray resp = client.waitForResponse();
    QCOMPARE(resp, msg);
}

QTEST_MAIN(tst_QQmlDebugLocal)

#include "tst_qqmldebuglocal.moc"
