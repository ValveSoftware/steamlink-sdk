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
#include "../../../shared/util.h"

#include <private/qqmldebugclient_p.h>
#include <private/qqmldebugconnection_p.h>
#include <private/qpacket_p.h>
#include <private/qqmlenginecontrolclient_p.h>

#include <QtTest/qtest.h>
#include <QtCore/qlibraryinfo.h>

#define STR_PORT_FROM "13773"
#define STR_PORT_TO "13783"

class QQmlEngineBlocker : public QObject
{
    Q_OBJECT
public:
    QQmlEngineBlocker(QQmlEngineControlClient *parent);

public slots:
    void blockEngine(int engineId, const QString &name);
};

QQmlEngineBlocker::QQmlEngineBlocker(QQmlEngineControlClient *parent): QObject(parent)
{
    connect(parent, &QQmlEngineControlClient::engineAboutToBeAdded,
            this, &QQmlEngineBlocker::blockEngine);
    connect(parent, &QQmlEngineControlClient::engineAboutToBeRemoved,
            this, &QQmlEngineBlocker::blockEngine);
}

void QQmlEngineBlocker::blockEngine(int engineId, const QString &name)
{
    Q_UNUSED(name);
    static_cast<QQmlEngineControlClient *>(parent())->blockEngine(engineId);
}

class tst_QQmlEngineControl : public QQmlDataTest
{
    Q_OBJECT

public:
    tst_QQmlEngineControl()
        : m_process(0)
        , m_connection(0)
        , m_client(0)
    {}

private:
    QQmlDebugProcess *m_process;
    QQmlDebugConnection *m_connection;
    QQmlEngineControlClient *m_client;

    void connect(const QString &testFile, bool restrictServices);
    void engine_data();

private slots:
    void cleanup();

    void startEngine_data();
    void startEngine();
    void stopEngine_data();
    void stopEngine();
};

void tst_QQmlEngineControl::connect(const QString &testFile, bool restrictServices)
{
    const QString executable = QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qmlscene";
    QStringList arguments;
    arguments << QString::fromLatin1("-qmljsdebugger=port:%1,%2,block%3")
                 .arg(STR_PORT_FROM).arg(STR_PORT_TO)
                 .arg(restrictServices ? QStringLiteral(",services:EngineControl") : QString());

    arguments << QQmlDataTest::instance()->testFile(testFile);

    m_process = new QQmlDebugProcess(executable, this);
    m_process->start(QStringList() << arguments);
    QVERIFY2(m_process->waitForSessionStart(), "Could not launch application, or did not get 'Waiting for connection'.");

    m_connection = new QQmlDebugConnection();
    m_client = new QQmlEngineControlClient(m_connection);
    new QQmlEngineBlocker(m_client);
    QList<QQmlDebugClient *> others = QQmlDebugTest::createOtherClients(m_connection);

    const int port = m_process->debugPort();
    m_connection->connectToHost(QLatin1String("127.0.0.1"), port);

    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);
    foreach (QQmlDebugClient *other, others)
        QCOMPARE(other->state(), restrictServices ? QQmlDebugClient::Unavailable :
                                                    QQmlDebugClient::Enabled);
    qDeleteAll(others);
}

void tst_QQmlEngineControl::cleanup()
{
    if (QTest::currentTestFailed()) {
        qDebug() << "Process State:" << (m_process ? m_process->state() : QLatin1String("null"));
        qDebug() << "Application Output:" << (m_process ? m_process->output() : QLatin1String("null"));
        qDebug() << "Connection State:" << QQmlDebugTest::connectionStateString(m_connection);
        qDebug() << "Client State:" << QQmlDebugTest::clientStateString(m_client);
    }
    delete m_process;
    m_process = 0;
    delete m_client;
    m_client = 0;
    delete m_connection;
    m_connection = 0;
}

void tst_QQmlEngineControl::engine_data()
{
    QTest::addColumn<bool>("restrictMode");
    QTest::newRow("unrestricted") << false;
    QTest::newRow("restricted") << true;
}

void tst_QQmlEngineControl::startEngine_data()
{
    engine_data();
}

void tst_QQmlEngineControl::startEngine()
{
    QFETCH(bool, restrictMode);

    connect("test.qml", restrictMode);

    QTRY_VERIFY(!m_client->blockedEngines().empty());
    m_client->releaseEngine(m_client->blockedEngines().last());

    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(engineAdded(int,QString))),
             "No engine start message received in time.");
}

void tst_QQmlEngineControl::stopEngine_data()
{
    engine_data();
}

void tst_QQmlEngineControl::stopEngine()
{
    QFETCH(bool, restrictMode);

    connect("exit.qml", restrictMode);

    QTRY_VERIFY(!m_client->blockedEngines().empty());
    m_client->releaseEngine(m_client->blockedEngines().last());

    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(engineAdded(int,QString))),
             "No engine start message received in time.");

    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(engineAboutToBeRemoved(int,QString))),
             "No engine about to stop message received in time.");
    m_client->releaseEngine(m_client->blockedEngines().last());
    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(engineRemoved(int,QString))),
             "No engine stop message received in time.");
}

QTEST_MAIN(tst_QQmlEngineControl)

#include "tst_qqmlenginecontrol.moc"
