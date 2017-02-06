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

//QQmlDebugTest
#include "debugutil_p.h"
#include "../../../shared/util.h"

#include <private/qqmldebugclient_p.h>
#include <private/qqmldebugconnection_p.h>
#include <private/qpacket_p.h>

#include <QtCore/qstring.h>
#include <QtCore/qlibraryinfo.h>
#include <QtTest/qtest.h>

const char *NORMALMODE = "-qmljsdebugger=port:3777,3787,block";
const char *QMLFILE = "test.qml";

class QQmlDebugMsgClient;
class tst_QDebugMessageService : public QQmlDataTest
{
    Q_OBJECT

public:
    tst_QDebugMessageService();

    void init();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void cleanup();

    void retrieveDebugOutput();

private:
    QQmlDebugProcess *m_process;
    QQmlDebugMsgClient *m_client;
    QQmlDebugConnection *m_connection;
};

struct LogEntry {
    LogEntry(QtMsgType _type, QString _message)
        : type(_type), message(_message) {}

    QtMsgType type;
    QString message;
    int line;
    QString file;
    QString function;
    QString category;

    QString toString() const
    {
        return QString::number(type) + ": " + message + " (" + category + ")";
    }
};

bool operator==(const LogEntry &t1, const LogEntry &t2)
{
    return t1.type == t2.type && t1.message == t2.message
            && t1.line == t2.line && t1.file == t2.file
            && t1.function == t2.function && t1.category == t2.category;
}

class QQmlDebugMsgClient : public QQmlDebugClient
{
    Q_OBJECT
public:
    QQmlDebugMsgClient(QQmlDebugConnection *connection)
        : QQmlDebugClient(QLatin1String("DebugMessages"), connection)
    {
    }

    QList<LogEntry> logBuffer;

protected:
    //inherited from QQmlDebugClient
    void stateChanged(State state);
    void messageReceived(const QByteArray &data);

signals:
    void enabled();
    void debugOutput();
};

void QQmlDebugMsgClient::stateChanged(State state)
{
    if (state == Enabled) {
        emit enabled();
    }
}

void QQmlDebugMsgClient::messageReceived(const QByteArray &data)
{
    QPacket ds(connection()->currentDataStreamVersion(), data);
    QByteArray command;
    ds >> command;

    if (command == "MESSAGE") {
        int type;
        QByteArray message;
        QByteArray file;
        QByteArray function;
        QByteArray category;
        qint64 timestamp;
        int line;
        ds >> type >> message >> file >> line >> function >> category >> timestamp;
        QVERIFY(ds.atEnd());

        QVERIFY(type >= QtDebugMsg);
        QVERIFY(type <= QtFatalMsg);
        QVERIFY(timestamp > 0);

        LogEntry entry((QtMsgType)type, QString::fromUtf8(message));
        entry.line = line;
        entry.file = QString::fromUtf8(file);
        entry.function = QString::fromUtf8(function);
        entry.category = QString::fromUtf8(category);
        logBuffer << entry;
        emit debugOutput();
    } else {
        QFAIL("Unknown message");
    }
}

tst_QDebugMessageService::tst_QDebugMessageService()
{
}

void tst_QDebugMessageService::initTestCase()
{
    QQmlDataTest::initTestCase();
    m_process = 0;
    m_client = 0;
    m_connection = 0;
}

void tst_QDebugMessageService::cleanupTestCase()
{
    if (m_process)
        delete m_process;

    if (m_client)
        delete m_client;

    if (m_connection)
        delete m_connection;
}

void tst_QDebugMessageService::init()
{
    m_connection = new QQmlDebugConnection();
    m_process = new QQmlDebugProcess(QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qml", this);
    m_client = new QQmlDebugMsgClient(m_connection);

    m_process->start(QStringList() << QLatin1String(NORMALMODE) << QQmlDataTest::instance()->testFile(QMLFILE));
    QVERIFY2(m_process->waitForSessionStart(),
             "Could not launch application, or did not get 'Waiting for connection'.");

    const int port = m_process->debugPort();
    m_connection->connectToHost("127.0.0.1", port);
    QVERIFY(m_connection->waitForConnected());

    if (m_client->state() != QQmlDebugClient::Enabled)
        QQmlDebugTest::waitForSignal(m_client, SIGNAL(enabled()));

    QCOMPARE(m_client->state(), QQmlDebugClient::Enabled);
}

void tst_QDebugMessageService::cleanup()
{
    if (QTest::currentTestFailed()) {
        qDebug() << "Process State:" << m_process->state();
        qDebug() << "Application Output:" << m_process->output();
    }
    if (m_process)
        delete m_process;

    if (m_client)
        delete m_client;

    if (m_connection)
        delete m_connection;

    m_process = 0;
    m_client = 0;
    m_connection = 0;
}

void tst_QDebugMessageService::retrieveDebugOutput()
{
    init();

    QTRY_VERIFY(m_client->logBuffer.size() >= 2);

    const QString path =
            QUrl::fromLocalFile(QQmlDataTest::instance()->testFile(QMLFILE)).toString();
    LogEntry entry1(QtDebugMsg, QLatin1String("console.log"));
    entry1.line = 35;
    entry1.file = path;
    entry1.function = QLatin1String("onCompleted");
    entry1.category = QLatin1String("qml");
    LogEntry entry2(QtDebugMsg, QLatin1String("console.count: 1"));
    entry2.line = 36;
    entry2.file = path;
    entry2.function = QLatin1String("onCompleted");
    entry2.category = QLatin1String("default");

    QVERIFY(m_client->logBuffer.contains(entry1));
    QVERIFY(m_client->logBuffer.contains(entry2));
}

QTEST_MAIN(tst_QDebugMessageService)

#include "tst_qdebugmessageservice.moc"
