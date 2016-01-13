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
#include <QtCore/QLibraryInfo>

#include "../shared/debugutil_p.h"
#include "../../../shared/util.h"
#include "qqmlinspectorclient.h"

#define STR_PORT_FROM "3772"
#define STR_PORT_TO "3782"



class tst_QQmlInspector : public QQmlDataTest
{
    Q_OBJECT

public:
    tst_QQmlInspector()
        : m_process(0)
        , m_connection(0)
        , m_client(0)
    {
    }

private:
    void startQmlsceneProcess(const char *qmlFile);

private:
    QQmlDebugProcess *m_process;
    QQmlDebugConnection *m_connection;
    QQmlInspectorClient *m_client;

private slots:
    void init();
    void cleanup();

    void connect();
    void showAppOnTop();
    void reloadQml();
    void reloadQmlWindow();
};

void tst_QQmlInspector::startQmlsceneProcess(const char * /* qmlFile */)
{
    const QString argument = "-qmljsdebugger=port:" STR_PORT_FROM "," STR_PORT_TO ",block";

    // ### This should be using qml instead of qmlscene, but can't because of QTBUG-33376 (same as the XFAIL testcase)
    m_process = new QQmlDebugProcess(QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qmlscene", this);
    m_process->start(QStringList() << argument << testFile("qtquick2.qml"));
    QVERIFY2(m_process->waitForSessionStart(),
             "Could not launch application, or did not get 'Waiting for connection'.");

    QQmlDebugConnection *m_connection = new QQmlDebugConnection();
    m_client = new QQmlInspectorClient(m_connection);

    const int port = m_process->debugPort();
    m_connection->connectToHost(QLatin1String("127.0.0.1"), port);
}

void tst_QQmlInspector::init()
{
}

void tst_QQmlInspector::cleanup()
{
    if (QTest::currentTestFailed()) {
        qDebug() << "Process State:" << m_process->state();
        qDebug() << "Application Output:" << m_process->output();
    }
    delete m_process;
    delete m_connection;
    delete m_client;
}

void tst_QQmlInspector::connect()
{
    startQmlsceneProcess("qtquick2.qml");
    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);
}

void tst_QQmlInspector::showAppOnTop()
{
    startQmlsceneProcess("qtquick2.qml");
    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    m_client->setShowAppOnTop(true);
    QVERIFY(QQmlDebugTest::waitForSignal(m_client, SIGNAL(responseReceived())));
    QCOMPARE(m_client->m_requestResult, true);

    m_client->setShowAppOnTop(false);
    QVERIFY(QQmlDebugTest::waitForSignal(m_client, SIGNAL(responseReceived())));
    QCOMPARE(m_client->m_requestResult, true);
}

void tst_QQmlInspector::reloadQml()
{
    startQmlsceneProcess("qtquick2.qml");
    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    QByteArray fileContents;

    QFile file(testFile("changes.txt"));
    if (file.open(QFile::ReadOnly))
        fileContents = file.readAll();
    file.close();

    QHash<QString, QByteArray> changesHash;
    changesHash.insert("qtquick2.qml", fileContents);

    m_client->reloadQml(changesHash);
    QVERIFY(QQmlDebugTest::waitForSignal(m_client, SIGNAL(responseReceived())));

    QTRY_COMPARE(m_process->output().contains(
                 QString("version 2.0")), true);

    QCOMPARE(m_client->m_requestResult, true);
    QCOMPARE(m_client->m_reloadRequestId, m_client->m_responseId);
}

void tst_QQmlInspector::reloadQmlWindow()
{
    startQmlsceneProcess("window.qml");
    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    QByteArray fileContents;

    QFile file(testFile("changes.txt"));
    if (file.open(QFile::ReadOnly))
        fileContents = file.readAll();
    file.close();

    QHash<QString, QByteArray> changesHash;
    changesHash.insert("window.qml", fileContents);

    m_client->reloadQml(changesHash);
    QVERIFY(QQmlDebugTest::waitForSignal(m_client, SIGNAL(responseReceived())));

    QEXPECT_FAIL("", "cannot debug with a QML file containing a top-level Window", Abort); // QTBUG-33376
    QTRY_COMPARE(m_process->output().contains(
                     QString("version 2.0")), true);

    QCOMPARE(m_client->m_requestResult, true);
    QCOMPARE(m_client->m_reloadRequestId, m_client->m_responseId);
}

QTEST_MAIN(tst_QQmlInspector)

#include "tst_qqmlinspector.moc"
