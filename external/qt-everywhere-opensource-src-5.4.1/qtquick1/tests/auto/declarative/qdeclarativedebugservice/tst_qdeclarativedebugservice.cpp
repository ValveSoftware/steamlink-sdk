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
#include <private/qdeclarativedebugservice_p.h>

#include "../shared/debugutil_p.h"


class tst_QDeclarativeDebugService : public QObject
{
    Q_OBJECT
private:
    QDeclarativeDebugConnection *m_conn;

private slots:
    void initTestCase();

    void name();
    void status();
    void sendMessage();
    void idForObject();
    void objectForId();
    void objectToString();
};

void tst_QDeclarativeDebugService::initTestCase()
{
    QTest::ignoreMessage(QtDebugMsg, "QDeclarativeDebugServer: Waiting for connection on port 13769...");
    new QDeclarativeEngine(this);

    m_conn = new QDeclarativeDebugConnection(this);
    m_conn->connectToHost("127.0.0.1", 13769);

    QTest::ignoreMessage(QtDebugMsg, "QDeclarativeDebugServer: Connection established");
    bool ok = m_conn->waitForConnected();
    QVERIFY(ok);

    QTRY_VERIFY(QDeclarativeDebugService::hasDebuggingClient());
}

void tst_QDeclarativeDebugService::name()
{
    QString name = "tst_QDeclarativeDebugService::name()";

    QDeclarativeDebugService service(name);
    QCOMPARE(service.name(), name);
}

void tst_QDeclarativeDebugService::status()
{
    QDeclarativeDebugTestService service("tst_QDeclarativeDebugService::status()");
    QCOMPARE(service.status(), QDeclarativeDebugService::Unavailable);

    {
        QDeclarativeDebugTestClient client("tst_QDeclarativeDebugService::status()", m_conn);
        QTRY_COMPARE(client.status(), QDeclarativeDebugClient::Enabled);
        QTRY_COMPARE(service.status(), QDeclarativeDebugService::Enabled);
    }


    QTRY_COMPARE(service.status(), QDeclarativeDebugService::Unavailable);

    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeDebugService: Conflicting plugin name \"tst_QDeclarativeDebugService::status()\"");

    QDeclarativeDebugService duplicate("tst_QDeclarativeDebugService::status()");
    QCOMPARE(duplicate.status(), QDeclarativeDebugService::NotConnected);
}

void tst_QDeclarativeDebugService::sendMessage()
{
    QDeclarativeDebugTestService service("tst_QDeclarativeDebugService::sendMessage()");
    QDeclarativeDebugTestClient client("tst_QDeclarativeDebugService::sendMessage()", m_conn);

    QByteArray msg = "hello!";

    QTRY_COMPARE(client.status(), QDeclarativeDebugClient::Enabled);
    QTRY_COMPARE(service.status(), QDeclarativeDebugService::Enabled);

    client.sendMessage(msg);
    QByteArray resp = client.waitForResponse();
    QCOMPARE(resp, msg);

    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeDebugService: Conflicting plugin name \"tst_QDeclarativeDebugService::sendMessage()\"");
    QDeclarativeDebugService duplicate("tst_QDeclarativeDebugService::sendMessage()");
    duplicate.sendMessage("msg");
}

void tst_QDeclarativeDebugService::idForObject()
{
    QCOMPARE(QDeclarativeDebugService::idForObject(0), -1);

    QObject *objA = new QObject;

    int idA = QDeclarativeDebugService::idForObject(objA);
    QVERIFY(idA >= 0);
    QCOMPARE(QDeclarativeDebugService::objectForId(idA), objA);

    int idAA = QDeclarativeDebugService::idForObject(objA);
    QCOMPARE(idAA, idA);

    QObject *objB = new QObject;
    int idB = QDeclarativeDebugService::idForObject(objB);
    QVERIFY(idB != idA);
    QCOMPARE(QDeclarativeDebugService::objectForId(idB), objB);

    delete objA;
    delete objB;
}

void tst_QDeclarativeDebugService::objectForId()
{
    QCOMPARE(QDeclarativeDebugService::objectForId(-1), static_cast<QObject*>(0));
    QCOMPARE(QDeclarativeDebugService::objectForId(1), static_cast<QObject*>(0));

    QObject *obj = new QObject;
    int id = QDeclarativeDebugService::idForObject(obj);
    QCOMPARE(QDeclarativeDebugService::objectForId(id), obj);

    delete obj;
    QCOMPARE(QDeclarativeDebugService::objectForId(id), static_cast<QObject*>(0));
}

void tst_QDeclarativeDebugService::objectToString()
{
    QCOMPARE(QDeclarativeDebugService::objectToString(0), QString("NULL"));

    QObject *obj = new QObject;
    QCOMPARE(QDeclarativeDebugService::objectToString(obj), QString("QObject: <unnamed>"));

    obj->setObjectName("Hello");
    QCOMPARE(QDeclarativeDebugService::objectToString(obj), QString("QObject: Hello"));
    delete obj;
}


int main(int argc, char *argv[])
{
    int _argc = argc + 1;
    char **_argv = new char*[_argc];
    for (int i = 0; i < argc; ++i)
        _argv[i] = argv[i];
    _argv[_argc - 1] = const_cast<char *>("-qmljsdebugger=port:13769");

    QApplication app(_argc, _argv);
    tst_QDeclarativeDebugService tc;
    return QTest::qExec(&tc, _argc, _argv);
    delete _argv;
}

#include "tst_qdeclarativedebugservice.moc"
