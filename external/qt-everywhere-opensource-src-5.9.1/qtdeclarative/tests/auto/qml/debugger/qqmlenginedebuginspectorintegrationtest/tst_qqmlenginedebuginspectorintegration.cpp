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

#include "qqmlinspectorclient.h"
#include "qqmlenginedebugclient.h"
#include "../shared/debugutil_p.h"
#include "../../../shared/util.h"

#include <private/qqmldebugconnection_p.h>

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>
#include <QtNetwork/qhostaddress.h>
#include <QtCore/qtimer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>
#include <QtCore/qlibraryinfo.h>

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
        , m_recipient(0)
    {
    }


private:
    void init(bool restrictServices);
    QmlDebugObjectReference findRootObject();

    QQmlDebugProcess *m_process;
    QQmlInspectorClient *m_inspectorClient;
    QQmlEngineDebugClient *m_engineDebugClient;
    QQmlInspectorResultRecipient *m_recipient;

private slots:
    void cleanup();

    void connect_data();
    void connect();
    void objectLocationLookup();
    void select();
    void createObject();
    void moveObject();
    void destroyObject();
};

QmlDebugObjectReference tst_QQmlEngineDebugInspectorIntegration::findRootObject()
{
    bool success = false;
    m_engineDebugClient->queryAvailableEngines(&success);

    if (!QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result())))
        return QmlDebugObjectReference();

    m_engineDebugClient->queryRootContexts(m_engineDebugClient->engines()[0].debugId, &success);
    if (!QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result())))
        return QmlDebugObjectReference();

    int count = m_engineDebugClient->rootContext().contexts.count();
    m_engineDebugClient->queryObject(
                m_engineDebugClient->rootContext().contexts[count - 1].objects[0], &success);
    if (!QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result())))
        return QmlDebugObjectReference();
    return m_engineDebugClient->object();
}

void tst_QQmlEngineDebugInspectorIntegration::init(bool restrictServices)
{
    const QString argument = QString::fromLatin1("-qmljsdebugger=port:%1,%2,block%3")
            .arg(STR_PORT_FROM).arg(STR_PORT_TO)
            .arg(restrictServices ? QStringLiteral(",services:QmlDebugger,QmlInspector") :
                                    QString());

    m_process = new QQmlDebugProcess(QLibraryInfo::location(QLibraryInfo::BinariesPath) + "/qml",
                                     this);
    m_process->start(QStringList() << argument << testFile("qtquick2.qml"));
    QVERIFY2(m_process->waitForSessionStart(),
             "Could not launch application, or did not get 'Waiting for connection'.");

    QQmlDebugConnection *m_connection = new QQmlDebugConnection(this);
    m_inspectorClient = new QQmlInspectorClient(m_connection);
    m_engineDebugClient = new QQmlEngineDebugClient(m_connection);
    m_recipient = new QQmlInspectorResultRecipient(this);
    QObject::connect(m_inspectorClient, &QQmlInspectorClient::responseReceived,
                     m_recipient, &QQmlInspectorResultRecipient::recordResponse);

    QList<QQmlDebugClient *> others = QQmlDebugTest::createOtherClients(m_connection);

    m_connection->connectToHost(QLatin1String("127.0.0.1"), m_process->debugPort());
    QVERIFY(m_connection->waitForConnected());
    foreach (QQmlDebugClient *other, others)
        QCOMPARE(other->state(), restrictServices ? QQmlDebugClient::Unavailable :
                                                    QQmlDebugClient::Enabled);
    qDeleteAll(others);

    QTRY_COMPARE(m_inspectorClient->state(), QQmlDebugClient::Enabled);
    QTRY_COMPARE(m_engineDebugClient->state(), QQmlDebugClient::Enabled);
}

void tst_QQmlEngineDebugInspectorIntegration::cleanup()
{
    if (QTest::currentTestFailed()) {
        qDebug() << "Process State:" << m_process->state();
        qDebug() << "Application Output:" << m_process->output();
    }
    delete m_process;
    m_process = 0;
    delete m_engineDebugClient;
    m_engineDebugClient = 0;
    delete m_inspectorClient;
    m_inspectorClient = 0;
    delete m_recipient;
    m_recipient = 0;
}

void tst_QQmlEngineDebugInspectorIntegration::connect_data()
{
    QTest::addColumn<bool>("restrictMode");
    QTest::newRow("unrestricted") << false;
    QTest::newRow("restricted") << true;
}

void tst_QQmlEngineDebugInspectorIntegration::connect()
{
    QFETCH(bool, restrictMode);
    init(restrictMode);
}

void tst_QQmlEngineDebugInspectorIntegration::objectLocationLookup()
{
    init(true);

    bool success = false;
    QmlDebugObjectReference rootObject = findRootObject();
    QVERIFY(rootObject.debugId != -1);
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
}

void tst_QQmlEngineDebugInspectorIntegration::select()
{
    init(true);
    QmlDebugObjectReference rootObject = findRootObject();
    QList<int> childIds;
    int requestId = 0;
    foreach (const QmlDebugObjectReference &child, rootObject.children) {
        requestId = m_inspectorClient->select(QList<int>() << child.debugId);
        QTRY_COMPARE(m_recipient->lastResponseId, requestId);
        QVERIFY(m_recipient->lastResult);
        childIds << child.debugId;
    }
    requestId = m_inspectorClient->select(childIds);
    QTRY_COMPARE(m_recipient->lastResponseId, requestId);
    QVERIFY(m_recipient->lastResult);
}

void tst_QQmlEngineDebugInspectorIntegration::createObject()
{
    init(true);

    QString qml = QLatin1String("Rectangle {\n"
                                "  id: xxxyxxx\n"
                                "  width: 10\n"
                                "  height: 10\n"
                                "  color: \"blue\"\n"
                                "}");

    QmlDebugObjectReference rootObject = findRootObject();
    QVERIFY(rootObject.debugId != -1);
    QCOMPARE(rootObject.children.length(), 2);

    int requestId = m_inspectorClient->createObject(
                qml, rootObject.debugId, QStringList() << QLatin1String("import QtQuick 2.0"),
                QLatin1String("testcreate.qml"));
    QTRY_COMPARE(m_recipient->lastResponseId, requestId);
    QVERIFY(m_recipient->lastResult);

    rootObject = findRootObject();
    QVERIFY(rootObject.debugId != -1);
    QCOMPARE(rootObject.children.length(), 3);
    QCOMPARE(rootObject.children[2].idString, QLatin1String("xxxyxxx"));
}

void tst_QQmlEngineDebugInspectorIntegration::moveObject()
{
    init(true);
    QmlDebugObjectReference rootObject = findRootObject();
    QVERIFY(rootObject.debugId != -1);
    QCOMPARE(rootObject.children.length(), 2);

    int childId = rootObject.children[0].debugId;
    int requestId = m_inspectorClient->moveObject(childId, rootObject.children[1].debugId);
    QTRY_COMPARE(m_recipient->lastResponseId, requestId);
    QVERIFY(m_recipient->lastResult);

    rootObject = findRootObject();
    QVERIFY(rootObject.debugId != -1);
    QCOMPARE(rootObject.children.length(), 1);
    bool success = false;
    m_engineDebugClient->queryObject(rootObject.children[0], &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result())));
    QCOMPARE(m_engineDebugClient->object().children.length(), 1);
    QCOMPARE(m_engineDebugClient->object().children[0].debugId, childId);
}

void tst_QQmlEngineDebugInspectorIntegration::destroyObject()
{
    init(true);
    QmlDebugObjectReference rootObject = findRootObject();
    QVERIFY(rootObject.debugId != -1);
    QCOMPARE(rootObject.children.length(), 2);

    int requestId = m_inspectorClient->destroyObject(rootObject.children[0].debugId);
    QTRY_COMPARE(m_recipient->lastResponseId, requestId);
    QVERIFY(m_recipient->lastResult);

    rootObject = findRootObject();
    QVERIFY(rootObject.debugId != -1);
    QCOMPARE(rootObject.children.length(), 1);
    bool success = false;
    m_engineDebugClient->queryObject(rootObject.children[0], &success);
    QVERIFY(success);
    QVERIFY(QQmlDebugTest::waitForSignal(m_engineDebugClient, SIGNAL(result())));
    QCOMPARE(m_engineDebugClient->object().children.length(), 0);
}

QTEST_MAIN(tst_QQmlEngineDebugInspectorIntegration)

#include "tst_qqmlenginedebuginspectorintegration.moc"
