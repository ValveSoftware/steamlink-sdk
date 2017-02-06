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
#include "../shared/debugutil_p.h"
#include "../../../shared/util.h"

#include <private/qqmldebugconnection_p.h>

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>
#include <QtCore/qtimer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qthread.h>
#include <QtCore/qlibraryinfo.h>
#include <QtNetwork/qhostaddress.h>

#define STR_PORT_FROM "3772"
#define STR_PORT_TO "3782"


class tst_QQmlInspector : public QQmlDataTest
{
    Q_OBJECT

private:
    void startQmlProcess(const QString &qmlFile, bool restrictMode = true);
    void checkAnimationSpeed(int targetMillisPerDegree);

private:
    QScopedPointer<QQmlDebugProcess> m_process;
    QScopedPointer<QQmlDebugConnection> m_connection;
    QScopedPointer<QQmlInspectorClient> m_client;
    QScopedPointer<QQmlInspectorResultRecipient> m_recipient;

private slots:
    void cleanup();

    void connect_data();
    void connect();
    void setAnimationSpeed();
    void showAppOnTop();
};

void tst_QQmlInspector::startQmlProcess(const QString &qmlFile, bool restrictServices)
{
    const QString argument = QString::fromLatin1("-qmljsdebugger=port:%1,%2,block%3")
            .arg(STR_PORT_FROM).arg(STR_PORT_TO)
            .arg(restrictServices ? QStringLiteral(",services:QmlInspector") : QString());

    m_process.reset(new QQmlDebugProcess(QLibraryInfo::location(QLibraryInfo::BinariesPath) +
                                         "/qml"));
    // Make sure the animation timing is exact
    m_process->addEnvironment(QLatin1String("QSG_RENDER_LOOP=basic"));
    m_process->start(QStringList() << argument << testFile(qmlFile));
    QVERIFY2(m_process->waitForSessionStart(),
             "Could not launch application, or did not get 'Waiting for connection'.");

    m_client.reset();
    m_connection.reset(new QQmlDebugConnection);
    m_client.reset(new QQmlInspectorClient(m_connection.data()));

    m_recipient.reset(new QQmlInspectorResultRecipient);
    QObject::connect(m_client.data(), &QQmlInspectorClient::responseReceived,
                     m_recipient.data(), &QQmlInspectorResultRecipient::recordResponse);

    QList<QQmlDebugClient *> others = QQmlDebugTest::createOtherClients(m_connection.data());

    m_connection->connectToHost(QLatin1String("127.0.0.1"), m_process->debugPort());
    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    foreach (QQmlDebugClient *other, others)
        QCOMPARE(other->state(), restrictServices ? QQmlDebugClient::Unavailable :
                                                    QQmlDebugClient::Enabled);
    qDeleteAll(others);
}

void tst_QQmlInspector::checkAnimationSpeed(int targetMillisPerDegree)
{
    QString degreesString = QStringLiteral("degrees");
    QString millisecondsString = QStringLiteral("milliseconds");
    for (int i = 0; i < 2; ++i) { // skip one period; the change might have happened inside it
        int position = m_process->output().length();
        while (!m_process->output().mid(position).contains(degreesString) ||
               !m_process->output().mid(position).contains(millisecondsString)) {
            QVERIFY(QQmlDebugTest::waitForSignal(m_process.data(),
                                                 SIGNAL(readyReadStandardOutput())));
        }
    }

    QStringList words = m_process->output().split(QLatin1Char(' '));
    int degreesMarker = words.lastIndexOf(degreesString);
    QVERIFY(degreesMarker > 1);
    double degrees = words[degreesMarker - 1].toDouble();
    int millisecondsMarker = words.lastIndexOf(millisecondsString);
    QVERIFY(millisecondsMarker > 1);
    int milliseconds = words[millisecondsMarker - 1].toInt();

    double millisecondsPerDegree = milliseconds / degrees;
    QVERIFY(millisecondsPerDegree > targetMillisPerDegree - 3);
    QVERIFY(millisecondsPerDegree < targetMillisPerDegree + 3);

}

void tst_QQmlInspector::cleanup()
{
    if (QTest::currentTestFailed()) {
        qDebug() << "Process State:" << m_process->state();
        qDebug() << "Application Output:" << m_process->output();
    }
}

void tst_QQmlInspector::connect_data()
{
    QTest::addColumn<QString>("file");
    QTest::addColumn<bool>("restrictMode");
    QTest::newRow("rectangle/unrestricted") << "qtquick2.qml" << false;
    QTest::newRow("rectangle/restricted")   << "qtquick2.qml" << true;
    QTest::newRow("window/unrestricted")    << "window.qml"   << false;
    QTest::newRow("window/restricted")      << "window.qml"   << true;
}

void tst_QQmlInspector::connect()
{
    QFETCH(QString, file);
    QFETCH(bool, restrictMode);
    startQmlProcess(file, restrictMode);
    QVERIFY(m_client);
    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    int requestId = m_client->setInspectToolEnabled(true);
    QTRY_COMPARE(m_recipient->lastResponseId, requestId);
    QVERIFY(m_recipient->lastResult);

    requestId = m_client->setInspectToolEnabled(false);
    QTRY_COMPARE(m_recipient->lastResponseId, requestId);
    QVERIFY(m_recipient->lastResult);
}

void tst_QQmlInspector::showAppOnTop()
{
    startQmlProcess("qtquick2.qml");
    QVERIFY(m_client);
    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);

    int requestId = m_client->setShowAppOnTop(true);
    QTRY_COMPARE(m_recipient->lastResponseId, requestId);
    QVERIFY(m_recipient->lastResult);

    requestId = m_client->setShowAppOnTop(false);
    QTRY_COMPARE(m_recipient->lastResponseId, requestId);
    QVERIFY(m_recipient->lastResult);
}

void tst_QQmlInspector::setAnimationSpeed()
{
    startQmlProcess("qtquick2.qml");
    QVERIFY(m_client);
    QTRY_COMPARE(m_client->state(), QQmlDebugClient::Enabled);
    checkAnimationSpeed(10);

    int requestId = m_client->setAnimationSpeed(0.5);
    QTRY_COMPARE(m_recipient->lastResponseId, requestId);
    QVERIFY(m_recipient->lastResult);
    checkAnimationSpeed(5);

    requestId = m_client->setAnimationSpeed(2.0);
    QTRY_COMPARE(m_recipient->lastResponseId, requestId);
    QVERIFY(m_recipient->lastResult);
    checkAnimationSpeed(20);

    requestId = m_client->setAnimationSpeed(1.0);
    QTRY_COMPARE(m_recipient->lastResponseId, requestId);
    QVERIFY(m_recipient->lastResult);
    checkAnimationSpeed(10);
}

QTEST_MAIN(tst_QQmlInspector)

#include "tst_qqmlinspector.moc"
