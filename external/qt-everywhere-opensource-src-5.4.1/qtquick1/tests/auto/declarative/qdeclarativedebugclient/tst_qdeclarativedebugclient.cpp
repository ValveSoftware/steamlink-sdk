/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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
#include <qtest.h>
#include <QSignalSpy>
#include <QTimer>
#include <QHostAddress>
#include <QDebug>
#include <QThread>

#include <QtDeclarative/qdeclarativeengine.h>

#include <private/qdeclarativedebugclient_p.h>

#include "../shared/debugutil_p.h"

class tst_QDeclarativeDebugClient : public QObject
{
    Q_OBJECT

private:
    QDeclarativeDebugConnection *m_conn;

private slots:
    void initTestCase();

    void name();
    void status();
    void sendMessage();
};

void tst_QDeclarativeDebugClient::initTestCase()
{
    QTest::ignoreMessage(QtDebugMsg, "QDeclarativeDebugServer: Waiting for connection on port 13770...");
    new QDeclarativeEngine(this);

    m_conn = new QDeclarativeDebugConnection(this);

    QDeclarativeDebugTestClient client("tst_QDeclarativeDebugClient::handshake()", m_conn);
    QDeclarativeDebugTestService service("tst_QDeclarativeDebugClient::handshake()");

    m_conn->connectToHost("127.0.0.1", 13770);

    QTest::ignoreMessage(QtDebugMsg, "QDeclarativeDebugServer: Connection established");
    bool ok = m_conn->waitForConnected();
    QVERIFY(ok);

    QTRY_VERIFY(QDeclarativeDebugService::hasDebuggingClient());
    QTRY_COMPARE(client.status(), QDeclarativeDebugClient::Enabled);
}

void tst_QDeclarativeDebugClient::name()
{
    QString name = "tst_QDeclarativeDebugClient::name()";

    QDeclarativeDebugClient client(name, m_conn);
    QCOMPARE(client.name(), name);
}

void tst_QDeclarativeDebugClient::status()
{
    {
        QDeclarativeDebugConnection dummyConn;
        QDeclarativeDebugClient client("tst_QDeclarativeDebugClient::status()", &dummyConn);
        QCOMPARE(client.status(), QDeclarativeDebugClient::NotConnected);
    }

    QDeclarativeDebugTestClient client("tst_QDeclarativeDebugClient::status()", m_conn);
    QCOMPARE(client.status(), QDeclarativeDebugClient::Unavailable);

    {
        QDeclarativeDebugTestService service("tst_QDeclarativeDebugClient::status()");
        QTRY_COMPARE(client.status(), QDeclarativeDebugClient::Enabled);
    }

    QTRY_COMPARE(client.status(), QDeclarativeDebugClient::Unavailable);

    // duplicate plugin name
    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeDebugClient: Conflicting plugin name \"tst_QDeclarativeDebugClient::status()\"");
    QDeclarativeDebugClient client2("tst_QDeclarativeDebugClient::status()", m_conn);
    QCOMPARE(client2.status(), QDeclarativeDebugClient::NotConnected);

    QDeclarativeDebugClient client3("tst_QDeclarativeDebugClient::status3()", 0);
    QCOMPARE(client3.status(), QDeclarativeDebugClient::NotConnected);
}

void tst_QDeclarativeDebugClient::sendMessage()
{
    QDeclarativeDebugTestService service("tst_QDeclarativeDebugClient::sendMessage()");
    QDeclarativeDebugTestClient client("tst_QDeclarativeDebugClient::sendMessage()", m_conn);

    QByteArray msg = "hello!";

    QTRY_COMPARE(client.status(), QDeclarativeDebugClient::Enabled);

    client.sendMessage(msg);
    QByteArray resp = client.waitForResponse();
    QCOMPARE(resp, msg);
}

int main(int argc, char *argv[])
{
    int _argc = argc + 1;
    char **_argv = new char*[_argc];
    for (int i = 0; i < argc; ++i)
        _argv[i] = argv[i];
    _argv[_argc - 1] = const_cast<char *>("-qmljsdebugger=port:13770");

    QApplication app(_argc, _argv);
    tst_QDeclarativeDebugClient tc;
    return QTest::qExec(&tc, _argc, _argv);
    delete _argv;
}

#include "tst_qdeclarativedebugclient.moc"

