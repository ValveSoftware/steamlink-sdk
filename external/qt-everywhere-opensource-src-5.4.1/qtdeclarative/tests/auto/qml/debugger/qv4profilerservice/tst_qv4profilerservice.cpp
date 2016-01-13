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
#include <QLibraryInfo>

#include "debugutil_p.h"
#include "qqmldebugclient.h"
#include "../../../shared/util.h"

#define STR_PORT_FROM "13774"
#define STR_PORT_TO "13790"

struct QV4ProfilerData
{
    int messageType;
    QString filename;
    QString functionname;
    int lineNumber;
    double totalTime;
    double selfTime;
    int treeLevel;

    QByteArray toByteArray() const;
};

class QV4ProfilerClient : public QQmlDebugClient
{
    Q_OBJECT

public:
    enum MessageType {
        V4Entry,
        V4Complete,
        V4SnapshotChunk,
        V4SnapshotComplete,
        V4Started,

        V4MaximumMessage
    };

    enum ServiceState { NotRunning, Running } serviceState;

    QV4ProfilerClient(QQmlDebugConnection *connection)
        : QQmlDebugClient(QLatin1String("V8Profiler"), connection)
        , serviceState(NotRunning)
    {
    }

    void startProfiling(const QString &name) {
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        stream << QByteArray("V8PROFILER") << QByteArray("start") << name;
        sendMessage(message);
    }

    void stopProfiling(const QString &name) {
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        stream << QByteArray("V8PROFILER") << QByteArray("stop") << name;
        sendMessage(message);
    }

    void takeSnapshot() {
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        stream << QByteArray("V8SNAPSHOT") << QByteArray("full");
        sendMessage(message);
    }

    void deleteSnapshots() {
        QByteArray message;
        QDataStream stream(&message, QIODevice::WriteOnly);
        stream << QByteArray("V8SNAPSHOT") << QByteArray("delete");
        sendMessage(message);
    }

    QList<QV4ProfilerData> traceMessages;
    QList<QByteArray> snapshotMessages;

signals:
    void started();
    void complete();
    void snapshot();

protected:
    void messageReceived(const QByteArray &message);
};

class tst_QV4ProfilerService : public QQmlDataTest
{
    Q_OBJECT

public:
    tst_QV4ProfilerService()
        : m_process(0)
        , m_connection(0)
        , m_client(0)
    {
    }

private:
    QQmlDebugProcess *m_process;
    QQmlDebugConnection *m_connection;
    QV4ProfilerClient *m_client;

    bool connect(bool block, const QString &testFile, QString *error);

private slots:
    void cleanup();

    void blockingConnectWithTraceEnabled();
    void blockingConnectWithTraceDisabled();
    void nonBlockingConnect();
    void snapshot();
    void profileOnExit();
    void console();
};

void QV4ProfilerClient::messageReceived(const QByteArray &message)
{
    QByteArray msg = message;
    QDataStream stream(&msg, QIODevice::ReadOnly);

    int messageType;
    stream >> messageType;

    QVERIFY(messageType >= 0);
    QVERIFY(messageType < QV4ProfilerClient::V4MaximumMessage);

    switch (messageType) {
    case QV4ProfilerClient::V4Entry: {
        QCOMPARE(serviceState, Running);
        QV4ProfilerData entry;
        stream >> entry.filename >> entry.functionname >> entry.lineNumber >> entry.totalTime >> entry.selfTime >> entry.treeLevel;
        traceMessages.append(entry);
        break;
    }
    case QV4ProfilerClient::V4Complete:
        QCOMPARE(serviceState, Running);
        serviceState = NotRunning;
        emit complete();
        break;
    case QV4ProfilerClient::V4SnapshotChunk: {
        QByteArray json;
        stream >> json;
        snapshotMessages.append(json);
        break;
    }
    case QV4ProfilerClient::V4SnapshotComplete:
        emit snapshot();
        break;
    case QV4ProfilerClient::V4Started:
        QCOMPARE(serviceState, NotRunning);
        serviceState = Running;
        emit started();
        break;
    default:
        QString failMessage = QString("Unknown message type: %1").arg(messageType);
        QFAIL(qPrintable(failMessage));
    }

    QVERIFY(stream.atEnd());
}

bool tst_QV4ProfilerService::connect(bool block, const QString &testFile,
                                     QString *error)
{
    const QString executable = QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qml";
    QStringList arguments;

    if (block)
        arguments << QString("-qmljsdebugger=port:" STR_PORT_FROM "," STR_PORT_TO ",block");
    else
        arguments << QString("-qmljsdebugger=port:" STR_PORT_FROM "," STR_PORT_TO);

    arguments << QQmlDataTest::instance()->testFile(testFile);

    m_connection = new QQmlDebugConnection();
    m_client = new QV4ProfilerClient(m_connection);

    m_process = new QQmlDebugProcess(executable);
    m_process->start(QStringList() << arguments);
    if (!m_process->waitForSessionStart()) {
        *error = QLatin1String("Could not launch application, or did not get 'Waiting for connection'.");
        return false;
    }

    m_connection->connectToHost(QLatin1String("127.0.0.1"), m_process->debugPort());
    if (!m_connection->waitForConnected()) {
        *error = QLatin1String("Could not connect to debugger port.");
        return false;
    }
    return true;
}

void tst_QV4ProfilerService::cleanup()
{
    if (QTest::currentTestFailed()) {
        qDebug() << "Process State:" << m_process->state();
        qDebug() << "Application Output:" << m_process->output();
    }
    delete m_client;
    delete m_process;
    delete m_connection;
}

void tst_QV4ProfilerService::blockingConnectWithTraceEnabled()
{
    QString error;
    if (!connect(true, "test.qml", &error))
        QFAIL(qPrintable(error));

    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    m_client->startProfiling("");
    m_client->stopProfiling("");
    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(complete())),
             "No trace received in time.");
}

void tst_QV4ProfilerService::blockingConnectWithTraceDisabled()
{
    QString error;
    if (!connect(true, "test.qml", &error))
        QFAIL(qPrintable(error));

    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    m_client->stopProfiling("");
    QVERIFY2(!QQmlDebugTest::waitForSignal(m_client, SIGNAL(complete()), 1000),
             "Unexpected trace received.");
    m_client->startProfiling("");
    m_client->stopProfiling("");
    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(complete())),
             "No trace received in time.");
}

void tst_QV4ProfilerService::nonBlockingConnect()
{
    QString error;
    if (!connect(false, "test.qml", &error))
        QFAIL(qPrintable(error));

    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    m_client->startProfiling("");
    m_client->stopProfiling("");
    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(complete())),
             "No trace received in time.");
}

void tst_QV4ProfilerService::snapshot()
{
    QString error;
    if (!connect(false, "test.qml", &error))
        QFAIL(qPrintable(error));

    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    m_client->takeSnapshot();
    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(snapshot())),
             "No trace received in time.");
}

void tst_QV4ProfilerService::profileOnExit()
{
    QString error;
    if (!connect(true, "exit.qml", &error))
        QFAIL(qPrintable(error));

    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    m_client->startProfiling("");
    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(complete())),
             "No trace received in time.");
}

void tst_QV4ProfilerService::console()
{
    QString error;
    if (!connect(true, "console.qml", &error))
        QFAIL(qPrintable(error));

    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    m_client->stopProfiling("");

    QVERIFY2(QQmlDebugTest::waitForSignal(m_client, SIGNAL(complete())),
             "No trace received in time.");
    QVERIFY(!m_client->traceMessages.isEmpty());
}

QTEST_MAIN(tst_QV4ProfilerService)

#include "tst_qv4profilerservice.moc"
