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
#include <QtDeclarative/private/qdeclarativedebugclient_p.h>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <QtCore/QProcess>
#include "../shared/debugutil_p.h"

class QDeclarativeObserverModeClient : public QDeclarativeDebugClient
{
    Q_OBJECT
public:
    enum Message {
        AnimationSpeedChanged  = 0,
        AnimationPausedChanged = 19, // highest value
        ChangeTool             = 1,
        ClearComponentCache    = 2,
        ColorChanged           = 3,
        CreateObject           = 5,
        CurrentObjectsChanged  = 6,
        DestroyObject          = 7,
        MoveObject             = 8,
        ObjectIdList           = 9,
        Reload                 = 10,
        Reloaded               = 11,
        SetAnimationSpeed      = 12,
        SetAnimationPaused     = 18,
        SetCurrentObjects      = 14,
        SetDesignMode          = 15,
        ShowAppOnTop           = 16,
        ToolChanged            = 17
    };

    QDeclarativeObserverModeClient(QDeclarativeDebugConnection *connection) : QDeclarativeDebugClient(QLatin1String("QDeclarativeObserverMode"), connection) {}

    void setDesignMode(bool set)
    {
        QByteArray message;
        QDataStream ds(&message, QIODevice::WriteOnly);
        ds << SetDesignMode << set;

        sendMessage(message);
    }

    Message msg;

signals:
    void statusHasChanged();
    void responseReceived();

protected:
    virtual void statusChanged(Status status);
    virtual void messageReceived(const QByteArray &data);
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

class tst_QDeclarativeDebugObserverMode : public QDeclarativeDataTest
{
    Q_OBJECT

private slots:
    void initTestCase();
    void setDesignMode();

private:
    QString m_binary;
};

void QDeclarativeObserverModeClient::statusChanged(Status /*status*/)
{
    emit statusHasChanged();
}

void QDeclarativeObserverModeClient::messageReceived(const QByteArray &data)
{
    QByteArray rwData = data;
    QDataStream stream(&rwData, QIODevice::ReadOnly);

    int type;
    stream >> type;
    msg = (Message)type;
    if (msg != ToolChanged)
        emit responseReceived();
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

void tst_QDeclarativeDebugObserverMode::initTestCase()
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

void tst_QDeclarativeDebugObserverMode::setDesignMode()
{
    QJSDebugProcess process;
    process.start(m_binary, QStringList() << "-qmljsdebugger=port:3771"
                  << QDeclarativeDataTest::instance()->testFile("qtquick1.qml"));
    QVERIFY(process.waitForStarted());

    QDeclarativeDebugConnection connection;
    connection.connectToHost("127.0.0.1", 3771);
    QVERIFY(connection.waitForConnected());

    QDeclarativeObserverModeClient client(&connection);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(statusHasChanged())));
    QCOMPARE(client.status(), QDeclarativeObserverModeClient::Enabled);

    client.setDesignMode(true);
    QVERIFY(QDeclarativeDebugTest::waitForSignal(&client, SIGNAL(responseReceived())));
    QCOMPARE(client.msg, QDeclarativeObserverModeClient::SetDesignMode);
}

QTEST_MAIN(tst_QDeclarativeDebugObserverMode)

#include "tst_qdeclarativedebugobservermode.moc"
