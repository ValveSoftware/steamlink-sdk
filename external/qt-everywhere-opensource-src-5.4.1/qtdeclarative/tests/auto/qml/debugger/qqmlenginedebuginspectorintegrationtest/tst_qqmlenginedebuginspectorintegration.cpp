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
#include "qqmlenginedebugclient.h"

#define STR_PORT_FROM "3776"
#define STR_PORT_TO "3786"

class tst_QQmlEngineDebugInspectorIntegration : public QQmlDataTest
{
    Q_OBJECT

public:
    tst_QQmlEngineDebugInspectorIntegration()
        : m_process(0)
        , m_inspectorClient(0)
        , m_engineDebugClient(0)
    {
    }


private:
    QmlDebugObjectReference findRootObject();

    QQmlDebugProcess *m_process;
    QQmlInspectorClient *m_inspectorClient;
    QQmlEngineDebugClient *m_engineDebugClient;

private slots:
    void init();
    void cleanup();

    void connect();
    void clearObjectReferenceHashonReloadQml();
};


QmlDebugObjectReference tst_QQmlEngineDebugInspectorIntegration::findRootObject()
{
    bool success = false;
    m_engineDebugClient->queryAvailableEngines(&success);

    QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result()));

    m_engineDebugClient->queryRootContexts(m_engineDebugClient->engines()[0].debugId, &success);
    QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result()));
    int count = m_engineDebugClient->rootContext().contexts.count();
    m_engineDebugClient->queryObject(
                m_engineDebugClient->rootContext().contexts[count - 1].objects[0], &success);
    QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result()));
    return m_engineDebugClient->object();
}


void tst_QQmlEngineDebugInspectorIntegration::init()
{
    const QString argument = "-qmljsdebugger=port:" STR_PORT_FROM "," STR_PORT_TO ",block";

    // ### Still using qmlscene because of QTBUG-33376
    m_process = new QQmlDebugProcess(QLibraryInfo::location(QLibraryInfo::BinariesPath)
                                     + "/qmlscene", this);
    m_process->start(QStringList() << argument << testFile("qtquick2.qml"));
    QVERIFY2(m_process->waitForSessionStart(),
             "Could not launch application, or did not get 'Waiting for connection'.");

    QQmlDebugConnection *m_connection = new QQmlDebugConnection(this);
    m_inspectorClient = new QQmlInspectorClient(m_connection);
    m_engineDebugClient = new QQmlEngineDebugClient(m_connection);

    const int port = m_process->debugPort();
    m_connection->connectToHost(QLatin1String("127.0.0.1"), port);
    bool ok = m_connection->waitForConnected();
    QVERIFY(ok);
}

void tst_QQmlEngineDebugInspectorIntegration::cleanup()
{
    if (QTest::currentTestFailed()) {
        qDebug() << "Process State:" << m_process->state();
        qDebug() << "Application Output:" << m_process->output();
    }
    delete m_process;
    delete m_engineDebugClient;
    delete m_inspectorClient;
}

void tst_QQmlEngineDebugInspectorIntegration::connect()
{
    QTRY_COMPARE(m_inspectorClient->state(), QQmlDebugClient::Enabled);
    QTRY_COMPARE(m_engineDebugClient->state(), QQmlDebugClient::Enabled);
}

void tst_QQmlEngineDebugInspectorIntegration::clearObjectReferenceHashonReloadQml()
{
    QTRY_COMPARE(m_engineDebugClient->state(), QQmlDebugClient::Enabled);
    bool success = false;
    QmlDebugObjectReference rootObject = findRootObject();
    const QString fileName = QFileInfo(rootObject.source.url.toString()).fileName();
    int lineNumber = rootObject.source.lineNumber;
    int columnNumber = rootObject.source.columnNumber;
    m_engineDebugClient->queryObjectsForLocation(fileName, lineNumber,
                                        columnNumber, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result())));

    foreach (QmlDebugObjectReference child, rootObject.children) {
        success = false;
        lineNumber = child.source.lineNumber;
        columnNumber = child.source.columnNumber;
        m_engineDebugClient->queryObjectsForLocation(fileName, lineNumber,
                                       columnNumber, &success);
        QVERIFY(success);
        QVERIFY(QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result())));
    }

    QTRY_COMPARE(m_inspectorClient->state(), QQmlDebugClient::Enabled);

    QByteArray contents;
    contents.append("import QtQuick 2.0\n"
               "Text {"
               "y: 10\n"
               "text: \"test\"\n"
               "}");

    QHash<QString, QByteArray> changesHash;
    changesHash.insert("test.qml", contents);
    m_inspectorClient->reloadQml(changesHash);
    QVERIFY(QQmlDebugTest::waitForSignal(m_inspectorClient, SIGNAL(responseReceived())));

    lineNumber = rootObject.source.lineNumber;
    columnNumber = rootObject.source.columnNumber;
    success = false;
    m_engineDebugClient->queryObjectsForLocation(fileName, lineNumber,
                                   columnNumber, &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result())));

    foreach (QmlDebugObjectReference child, rootObject.children) {
        success = false;
        lineNumber = child.source.lineNumber;
        columnNumber = child.source.columnNumber;
        m_engineDebugClient->queryObjectsForLocation(fileName, lineNumber,
                                       columnNumber, &success);
        QVERIFY(success);
        QVERIFY(QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result())));
    }
}

QTEST_MAIN(tst_QQmlEngineDebugInspectorIntegration)

#include "tst_qqmlenginedebuginspectorintegration.moc"
