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
#include <qdeclarativedatatest.h>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/private/qdeclarativedebugclient_p.h>
#include <QtDeclarative/private/qdeclarativedebugservice_p.h>
#include <QtDeclarative/private/qjsdebuggeragent_p.h>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include "../shared/debugutil_p.h"

class QJSDebugClient : public QDeclarativeDebugClient
{
    Q_OBJECT
public:
    QJSDebugClient(QDeclarativeDebugConnection *connection) : QDeclarativeDebugClient(QLatin1String("JSDebugger"), connection) {}

    void initTestCase();
    void ping();
    void exec(const QByteArray &debuggerId, const QString &expr);
    void setBreakpoints(const QSet<JSAgentBreakpointData> &breakpoints);
    void setWatchExpressions(const QStringList &watchExpressions);
    void stepOver();
    void stepInto();
    void interrupt();
    void stepOut();
    void continueExecution();
    void expandObjectById(const QByteArray& objectName, quint64 objectId);
    void setProperty(const QByteArray& id, qint64 objectId, const QString &property, const QString &value);
    void activateFrame(int frameId);
    void startCoverageCompleted();
    void startCoverageRun();

    // info from last exec
    JSAgentWatchData exec_data;
    QByteArray exec_iname;

    QByteArray object_name;
    QList<JSAgentWatchData> object_children;

    int frame_id;

    // info from last stop
    QList<JSAgentStackData> break_stackFrames;
    QList<JSAgentWatchData> break_watches;
    QList<JSAgentWatchData> break_locals;
    bool break_becauseOfException;
    QString break_error;

signals:
    void statusHasChanged();

    void pong();
    void result();
    void stopped();
    void expanded();
    void watchTriggered();
    void coverageScriptLoaded();
    void coverageFuncEntered();
    void coverageFuncExited();
    void coveragePosChanged();
    void coverageCompleted();

protected:
    virtual void statusChanged(Status status);
    virtual void messageReceived(const QByteArray &data);

private:
    int m_ping;
};

class QJSDebugProcess : public QObject
{
    Q_OBJECT
public:
    QJSDebugProcess();
    ~QJSDebugProcess();

    void start(const QString &binary, const QStringList &arguments);
    bool waitForStarted();

private slots:
    void processAppOutput();

private:
    QProcess m_process;
    QTimer m_timer;
    QEventLoop m_eventLoop;
    bool m_started;
};

class tst_QDeclarativeDebugJS : public QDeclarativeDataTest
{
    Q_OBJECT

private slots:
    void initTestCase();
    void pingPong();
    void exec();
    void setBreakpoint();
    void stepOver();
    void stepInto();
    void interrupt();
    void stepOut();
    void continueExecution();
    void expandObject();
    void setProperty();
    void setProperty2();
    void watchExpression();
    void activateFrame();
    void setBreakpoint2();
    void stepOver2();
    void stepInto2();
    void interrupt2();
    void stepOut2();
    void continueExecution2();
    void expandObject2();
    void setProperty3();
    void setProperty4();
    void activateFrame2();
    void verifyQMLOptimizerDisabled();
    void testCoverageCompleted();
    void testCoverageRun();

private:
    QString m_binary;
};

void QJSDebugClient::ping()
{
    m_ping++;
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "PING";
    rs << cmd;
    rs << m_ping;
    sendMessage(reply);
}

void QJSDebugClient::exec(const QByteArray &debuggerId, const QString &expr)
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "EXEC";
    rs << cmd;
    rs << debuggerId;
    rs << expr;
    sendMessage(reply);
}

void QJSDebugClient::setBreakpoints(const QSet<JSAgentBreakpointData> &breakpoints)
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "BREAKPOINTS";
    rs << cmd
       << breakpoints;
    sendMessage(reply);
}

void QJSDebugClient::setWatchExpressions(const QStringList &watchExpressions)
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "WATCH_EXPRESSIONS";
    rs << cmd
       << watchExpressions;
    sendMessage(reply);
}

void QJSDebugClient::stepOver()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "STEPOVER";
    rs << cmd;
    sendMessage(reply);
}

void QJSDebugClient::stepInto()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "STEPINTO";
    rs << cmd;
    sendMessage(reply);
}

void QJSDebugClient::interrupt()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "INTERRUPT";
    rs << cmd;
    sendMessage(reply);
}

void QJSDebugClient::stepOut()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "STEPOUT";
    rs << cmd;
    sendMessage(reply);
}

void QJSDebugClient::continueExecution()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "CONTINUE";
    rs << cmd;
    sendMessage(reply);
}

void QJSDebugClient::expandObjectById(const QByteArray& objectName, quint64 objectId)
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "EXPAND";
    rs << cmd
       << objectName
       << objectId;
    sendMessage(reply);
}

void QJSDebugClient::setProperty(const QByteArray& id, qint64 objectId, const QString &property, const QString &value)
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "SET_PROPERTY";
    rs << cmd
       << id
       << objectId
       << property
       << value;
    sendMessage(reply);
}

void QJSDebugClient::activateFrame(int frameId)
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "ACTIVATE_FRAME";
    rs << cmd
       << frameId;
    sendMessage(reply);
}

void QJSDebugClient::startCoverageRun()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "COVERAGE";
    bool enabled = true;
    rs << cmd
       << enabled;
    sendMessage(reply);
}

void QJSDebugClient::startCoverageCompleted()
{
    QByteArray reply;
    QDataStream rs(&reply, QIODevice::WriteOnly);
    QByteArray cmd = "COVERAGE";
    bool enabled = false;
    rs << cmd
       << enabled;
    sendMessage(reply);
}

void QJSDebugClient::statusChanged(Status /*status*/)
{
    emit statusHasChanged();
}

void QJSDebugClient::messageReceived(const QByteArray &data)
{
    QByteArray rwData = data;
    QDataStream stream(&rwData, QIODevice::ReadOnly);

    QByteArray command;
    stream >> command;

    if (command == "STOPPED") {
        stream >> break_stackFrames >> break_watches >> break_locals >> break_becauseOfException >> break_error;
        if (!break_becauseOfException) {
            break_error.clear();
        }
        emit stopped();
    } else if (command == "RESULT") {
        stream >> exec_iname;
        stream >> exec_data;
        emit result();
    } else if (command == "EXPANDED") {
        stream >> object_name >> object_children;
        emit expanded();
    } else if (command == "LOCALS") {
        stream >> frame_id >> break_locals;
        if (!stream.atEnd()) { // compatibility with jsdebuggeragent from 2.1, 2.2
            stream >> break_watches;
        }
        emit watchTriggered();
    } else if (command == "PONG") {
        int ping;
        stream >> ping;
        QCOMPARE(ping, m_ping);
        emit pong();
    } else if (command == "COVERAGE") {
        qint64 time;
        int messageType;
        qint64 scriptId;
        QString program;
        QString fileName;
        int baseLineNumber;
        int lineNumber;
        int columnNumber;
        QString returnValue;

        stream >> time >> messageType >> scriptId >> program >> fileName >> baseLineNumber
        >> lineNumber >> columnNumber >> returnValue;
        if (messageType == CoverageComplete) {
            emit coverageCompleted();
        } else if (messageType == CoverageScriptLoad) {
            emit coverageScriptLoaded();
        } else if (messageType == CoveragePosChange) {
            emit coveragePosChanged();
        } else if (messageType == CoverageFuncEntry) {
            emit coverageFuncEntered();
        } else if (messageType == CoverageFuncExit) {
            emit coverageFuncExited();
        }
    } else {
        QFAIL("Unknown message :" + command);
    }
    QVERIFY(stream.atEnd());
}

QJSDebugProcess::QJSDebugProcess()
    : m_started(false)
{
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, SIGNAL(readyReadStandardOutput()), this, SLOT(processAppOutput()));
    connect(&m_timer, SIGNAL(timeout()), &m_eventLoop, SLOT(quit()));
    m_timer.setSingleShot(true);
    m_timer.setInterval(5000);
    QStringList environment = QProcess::systemEnvironment();
    environment.append("QML_DISABLE_OPTIMIZER=1");
    m_process.setEnvironment(environment);
}

QJSDebugProcess::~QJSDebugProcess()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(5000);
    }
}

void QJSDebugProcess::start(const QString &binary, const QStringList &arguments)
{
    m_process.start(binary, arguments);
    QVERIFY2(m_process.waitForStarted(),
             qPrintable(QString::fromLatin1("Unable to launch %1: %2").arg(binary, m_process.errorString())));
    m_timer.start();
}

bool QJSDebugProcess::waitForStarted()
{
    m_eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

    return m_started;
}

void QJSDebugProcess::processAppOutput()
{
    const QString appOutput = m_process.readAll();
    static QRegExp newline("[\n\r]{1,2}");
    QStringList lines = appOutput.split(newline);
    foreach (const QString &line, lines) {
        if (line.isEmpty())
            continue;
        if (line.startsWith("Qml debugging is enabled")) // ignore
            continue;
        if (line.startsWith("QDeclarativeDebugServer:")) {
            if (line.contains("Waiting for connection ")) {
                m_started = true;
                m_eventLoop.quit();
                continue;
            }
            if (line.contains("Connection established")) {
                continue;
            }
        }
        qDebug() << line;
    }
}

void tst_QDeclarativeDebugJS::initTestCase()
{
    QDeclarativeDataTest::initTestCase();
    const QString appFolder = QFINDTESTDATA("app");
    QVERIFY2(!appFolder.isEmpty(), qPrintable(QString::fromLatin1("Unable to locate app folder from %1").arg(QDir::currentPath())));
    m_binary = appFolder + QStringLiteral("/app");
#ifdef Q_OS_WIN
    m_binary += QStringLiteral(".exe");
#endif // Q_OS_WIN
    const QFileInfo fi(m_binary);
    QVERIFY2(fi.isExecutable(), qPrintable(QString::fromLatin1("%1 is not executable.").arg(m_binary)));
}

void tst_QDeclarativeDebugJS::pingPong()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771"
                  << QDeclarativeDataTest::instance()->testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QJSDebugClient client(&connection);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    client.ping();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(pong())));
}

void tst_QDeclarativeDebugJS::exec()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771"
                  << QDeclarativeDataTest::instance()->testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QJSDebugClient client(&connection);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    // Evaluate script without context
    client.exec("queryid", "1+1");
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(result())));
    QCOMPARE(client.exec_iname, QByteArray("queryid"));
    QCOMPARE(client.exec_data.exp, QByteArray("1+1"));
    QCOMPARE(client.exec_data.name, QByteArray("1+1"));
    QCOMPARE(client.exec_data.hasChildren, false);
    QCOMPARE(client.exec_data.type, QByteArray("Number"));
    QCOMPARE(client.exec_data.value, QByteArray("2"));

    // TODO: Test access to context after breakpoint is hit
}


void tst_QDeclarativeDebugJS::setBreakpoint()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << QDeclarativeDataTest::instance()->testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 11;

    //TEST LINE
    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    JSAgentStackData data = client.break_stackFrames.at(0);
    QCOMPARE(data.fileUrl, bp1.fileUrl);
    QCOMPARE(data.lineNumber, bp1.lineNumber);

}

void tst_QDeclarativeDebugJS::stepOver()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << QDeclarativeDataTest::instance()->testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = QDeclarativeDataTest::instance()->testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 11;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.stepOver();

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    JSAgentStackData data = client.break_stackFrames.at(0);
    QCOMPARE(data.fileUrl, bp1.fileUrl);
    QCOMPARE(data.lineNumber, bp1.lineNumber +1);
}

void tst_QDeclarativeDebugJS::stepInto()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << QDeclarativeDataTest::instance()->testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 12;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.stepInto();

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    QByteArray functionName("functionInScript");
    JSAgentStackData data = client.break_stackFrames.at(0);
    QCOMPARE(data.functionName, functionName);
    QCOMPARE(data.fileUrl, QByteArray(testFileUrl("backtrace1.js").toEncoded()));
}

void tst_QDeclarativeDebugJS::interrupt()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 12;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.interrupt();

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    QByteArray functionName("functionInScript");
    JSAgentStackData data = client.break_stackFrames.at(0);
    QCOMPARE(data.functionName, functionName);
    QCOMPARE(data.fileUrl, QByteArray(testFileUrl("backtrace1.js").toEncoded()));
}

void tst_QDeclarativeDebugJS::stepOut()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1,bp2;
    bp1.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 12;

    bp2.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp2.lineNumber = 13;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1 << bp2);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    client.stepInto();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.stepOut();

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));
    JSAgentStackData data = client.break_stackFrames.at(0);

    QCOMPARE(data.fileUrl, bp2.fileUrl);
    QCOMPARE(data.lineNumber, bp2.lineNumber);

}

void tst_QDeclarativeDebugJS::continueExecution()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1, bp2;
    bp1.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 11;

    bp2.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp2.lineNumber = 13;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1 << bp2);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.continueExecution();

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    JSAgentStackData data = client.break_stackFrames.at(0);
    QCOMPARE(data.fileUrl, bp2.fileUrl);
    QCOMPARE(data.lineNumber, bp2.lineNumber);
}

void tst_QDeclarativeDebugJS::expandObject()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp1.lineNumber = 17;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    foreach ( JSAgentWatchData data, client.break_locals)
    {
        if( data.name == "details")
        {
            //TEST LINE
            client.expandObjectById(data.name,data.objectId);
        }
    }

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(expanded())));
    QCOMPARE(client.object_name,QByteArray("details"));
    QCOMPARE(client.object_children.at(0).name,QByteArray("category"));
    QCOMPARE(client.object_children.at(0).type,QByteArray("String"));
    QCOMPARE(client.object_children.at(1).name,QByteArray("names"));
    QCOMPARE(client.object_children.at(1).type,QByteArray("Array"));
    QCOMPARE(client.object_children.at(2).name,QByteArray("aliases"));
    QCOMPARE(client.object_children.at(2).type,QByteArray("Array"));

}

void tst_QDeclarativeDebugJS::setProperty()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1, bp2;
    bp1.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp1.lineNumber = 17;
    bp2.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp2.lineNumber = 18;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1 << bp2);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    quint64 objectId;
    QByteArray objectName;

    foreach ( JSAgentWatchData data, client.break_locals)
    {
        if( data.name == "details")
        {
            objectId = data.objectId;
            objectName = data.name;
            //TEST LINE
            client.setProperty(data.name, data.objectId, "total", "names.length");
        }
    }

    client.continueExecution();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));
    client.expandObjectById(objectName,objectId);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(expanded())));

    QCOMPARE(client.object_name,QByteArray("details"));
    QCOMPARE(client.object_children.at(0).name,QByteArray("category"));
    QCOMPARE(client.object_children.at(0).type,QByteArray("String"));
    QCOMPARE(client.object_children.at(0).value,QByteArray("Superheroes"));
    QCOMPARE(client.object_children.at(1).name,QByteArray("names"));
    QCOMPARE(client.object_children.at(1).type,QByteArray("Array"));
    QCOMPARE(client.object_children.at(2).name,QByteArray("aliases"));
    QCOMPARE(client.object_children.at(2).type,QByteArray("Array"));
    QCOMPARE(client.object_children.at(3).name,QByteArray("total"));
    QCOMPARE(client.object_children.at(3).type,QByteArray("Number"));
    //    foreach ( JSAgentWatchData data, client.object_children)
    //    {
    //        qDebug() << data.name << data.type << data.value;
    //    }
}

void tst_QDeclarativeDebugJS::setProperty2()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1, bp2;
    bp1.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp1.lineNumber = 17;
    bp2.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp2.lineNumber = 18;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1 << bp2);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    quint64 objectId;
    QByteArray objectName;

    foreach ( JSAgentWatchData data, client.break_locals)
    {
        if( data.name == "details")
        {
            objectId = data.objectId;
            objectName = data.name;
            //TEST LINE
            client.setProperty(data.name, data.objectId, "category", data.name + ".category = 'comic characters'");
        }
    }

    client.continueExecution();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));
    client.expandObjectById(objectName,objectId);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(expanded())));

    QCOMPARE(client.object_name,QByteArray("details"));
    QCOMPARE(client.object_children.at(0).name,QByteArray("category"));
    QCOMPARE(client.object_children.at(0).type,QByteArray("String"));
    QCOMPARE(client.object_children.at(0).value,QByteArray("comic characters"));
    QCOMPARE(client.object_children.at(1).name,QByteArray("names"));
    QCOMPARE(client.object_children.at(1).type,QByteArray("Array"));
    QCOMPARE(client.object_children.at(2).name,QByteArray("aliases"));
    QCOMPARE(client.object_children.at(2).type,QByteArray("Array"));

    //    foreach ( JSAgentWatchData data, client.object_children)
    //    {
    //        qDebug() << data.name << data.type << data.value;
    //    }
}

void tst_QDeclarativeDebugJS::watchExpression()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    QByteArray watchExpression("root.result = 20");
    QStringList watchList;
    watchList.append(QString(watchExpression));

    //TEST LINE
    client.setWatchExpressions(watchList);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 11;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    JSAgentWatchData data = client.break_watches.at(0);
    QCOMPARE(data.exp, watchExpression);
    QCOMPARE(data.value.toInt(), 20);
    QCOMPARE(data.type, QByteArray("Number"));
}

void tst_QDeclarativeDebugJS::activateFrame()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1, bp2;
    bp1.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp1.lineNumber = 3;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    client.stepInto();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.activateFrame(2);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(watchTriggered())));

    QCOMPARE(client.break_locals.at(1).name,QByteArray("names"));
    QCOMPARE(client.break_locals.at(1).type,QByteArray("Array"));
    QCOMPARE(client.break_locals.at(2).name,QByteArray("a"));
    QCOMPARE(client.break_locals.at(2).type,QByteArray("Number"));
    QCOMPARE(client.break_locals.at(2).value,QByteArray("4"));
    QCOMPARE(client.break_locals.at(3).name,QByteArray("b"));
    QCOMPARE(client.break_locals.at(3).type,QByteArray("Number"));
    QCOMPARE(client.break_locals.at(3).value,QByteArray("5"));

    //    foreach ( JSAgentWatchData data, client.break_locals)
    //    {
    //        qDebug() << data.name << data.type << data.value;
    //    }

}

void tst_QDeclarativeDebugJS::setBreakpoint2()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 40;

    //TEST LINE
    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(!QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));
}

void tst_QDeclarativeDebugJS::stepOver2()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 11;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.stepOver();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    client.stepOver();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    JSAgentStackData data = client.break_stackFrames.at(0);
    QCOMPARE(data.fileUrl, bp1.fileUrl);
    QCOMPARE(data.lineNumber, bp1.lineNumber +2);
}

void tst_QDeclarativeDebugJS::stepInto2()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp1.lineNumber = 17;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.stepInto();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    client.stepInto();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    QByteArray functionName("logger");
    JSAgentStackData data = client.break_stackFrames.at(0);
    QCOMPARE(data.functionName, functionName);
    QCOMPARE(data.fileUrl, QByteArray(testFileUrl("backtrace1.js").toEncoded()));
}

void tst_QDeclarativeDebugJS::interrupt2()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp1.lineNumber = 17;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.interrupt();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    client.interrupt();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    QByteArray functionName("logger");
    JSAgentStackData data = client.break_stackFrames.at(0);
    QCOMPARE(data.functionName, functionName);
    QCOMPARE(data.fileUrl, QByteArray(testFileUrl("backtrace1.js").toEncoded()));
}

void tst_QDeclarativeDebugJS::stepOut2()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1,bp2;
    bp1.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 12;

    bp2.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp2.lineNumber = 13;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1 << bp2);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    client.stepInto();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    client.stepInto();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.stepOut();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    client.stepOut();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    JSAgentStackData data = client.break_stackFrames.at(0);

    QCOMPARE(data.fileUrl, bp2.fileUrl);
    QCOMPARE(data.lineNumber, bp2.lineNumber);

}

void tst_QDeclarativeDebugJS::continueExecution2()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1, bp2, bp3;
    bp1.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 11;

    bp2.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp2.lineNumber = 12;

    bp3.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp3.lineNumber = 13;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1 << bp2 << bp3);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.continueExecution();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    client.continueExecution();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    JSAgentStackData data = client.break_stackFrames.at(0);
    QCOMPARE(data.fileUrl, bp3.fileUrl);
    QCOMPARE(data.lineNumber, bp3.lineNumber);
}

void tst_QDeclarativeDebugJS::expandObject2()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp1.lineNumber = 17;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.expandObjectById(QByteArray("details"),123);

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(expanded())));
    QCOMPARE(client.object_name,QByteArray("details"));

    QEXPECT_FAIL("", "", Continue);
            QCOMPARE(client.object_children.length(),0);

}

void tst_QDeclarativeDebugJS::setProperty3()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1, bp2;
    bp1.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp1.lineNumber = 17;
    bp2.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp2.lineNumber = 18;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1 << bp2);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    quint64 objectId;
    QByteArray objectName;

    foreach ( JSAgentWatchData data, client.break_locals)
    {
        if( data.name == "details")
        {
            objectId = data.objectId;
            objectName = data.name;
            //TEST LINE
            client.setProperty(data.name, 123, "total", "names.length");
        }
    }

    client.continueExecution();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));
    client.expandObjectById(objectName,objectId);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(expanded())));

    QCOMPARE(client.object_name,QByteArray("details"));
    QCOMPARE(client.object_children.length(),3);

    //    foreach ( JSAgentWatchData data, client.object_children)
    //    {
    //        qDebug() << data.name << data.type << data.value;
    //    }
}

void tst_QDeclarativeDebugJS::setProperty4()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1, bp2;
    bp1.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp1.lineNumber = 17;
    bp2.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp2.lineNumber = 18;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1 << bp2);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    quint64 objectId;
    QByteArray objectName;

    foreach ( JSAgentWatchData data, client.break_locals)
    {
        if( data.name == "details")
        {
            objectId = data.objectId;
            objectName = data.name;
            //TEST LINE
            client.setProperty(data.name, 123, "category", data.name + ".category = 'comic characters'");
        }
    }

    client.continueExecution();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));
    client.expandObjectById(objectName,objectId);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(expanded())));

    QCOMPARE(client.object_name,QByteArray("details"));
    QCOMPARE(client.object_children.at(0).name,QByteArray("category"));
    QCOMPARE(client.object_children.at(0).type,QByteArray("String"));
    QCOMPARE(client.object_children.at(0).value,QByteArray("Superheroes"));
    QCOMPARE(client.object_children.at(1).name,QByteArray("names"));
    QCOMPARE(client.object_children.at(1).type,QByteArray("Array"));
    QCOMPARE(client.object_children.at(2).name,QByteArray("aliases"));
    QCOMPARE(client.object_children.at(2).type,QByteArray("Array"));

    //    foreach ( JSAgentWatchData data, client.object_children)
    //    {
    //        qDebug() << data.name << data.type << data.value;
    //    }
}

void tst_QDeclarativeDebugJS::activateFrame2()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1, bp2;
    bp1.fileUrl = testFileUrl("backtrace1.js").toEncoded();
    bp1.lineNumber = 4;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    client.stepInto();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

    //TEST LINE
    client.activateFrame(5);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(watchTriggered())));

    QCOMPARE(client.break_locals.length(),0);

    //    foreach ( JSAgentWatchData data, client.break_locals)
    //    {
    //        qDebug() << data.name << data.type << data.value;
    //    }

}

void tst_QDeclarativeDebugJS::verifyQMLOptimizerDisabled()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    QJSDebugClient client(&connection);

    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    JSAgentBreakpointData bp1;
    bp1.fileUrl = testFileUrl("backtrace1.qml").toEncoded();
    bp1.lineNumber = 21;

    client.setBreakpoints(QSet<JSAgentBreakpointData>() << bp1);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(stopped())));

}

void tst_QDeclarativeDebugJS::testCoverageCompleted()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QJSDebugClient client(&connection);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    client.startCoverageCompleted();
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(coverageCompleted())));
}

void tst_QDeclarativeDebugJS::testCoverageRun()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771,block"
                  << testFile("backtrace1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QJSDebugClient client(&connection);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    if (client.status() == QJSDebugClient::Unavailable)
        QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QJSDebugClient::Enabled);

    client.startCoverageRun();
    client.startCoverageCompleted();

    // The app might get "COVERAGE false" before anything is actually executed
    //QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(coverageScriptLoaded())));
    //QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(coveragePosChanged())));
    //QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(coverageFuncEntered())));
    //QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(coverageFuncExited())));
}

QTEST_MAIN(tst_QDeclarativeDebugJS)

#include "tst_qdeclarativedebugjs.moc"
