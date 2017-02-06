/****************************************************************************
**
** Copyright (C) 2016 Research In Motion.
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

#include "../../shared/util.h"
#include <QQmlApplicationEngine>
#include <QSignalSpy>
#include <QProcess>
#include <QDebug>

class tst_qqmlapplicationengine : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlapplicationengine() {}


private slots:
    void initTestCase();
    void basicLoading();
    void application();
    void applicationProperties();
private:
    QString buildDir;
    QString srcDir;
};

void tst_qqmlapplicationengine::initTestCase()
{
    qputenv("QT_MESSAGE_PATTERN", ""); // don't let it modify the debug output from testapp
    buildDir = QDir::currentPath();
    QQmlDataTest::initTestCase(); //Changes current path to src dir
    srcDir = QDir::currentPath();
}

void tst_qqmlapplicationengine::basicLoading()
{
    int size = 0;

    QQmlApplicationEngine *test = new QQmlApplicationEngine(testFileUrl("basicTest.qml"));
    QCOMPARE(test->rootObjects().size(), ++size);
    QVERIFY(test->rootObjects()[size -1]);
    QVERIFY(test->rootObjects()[size -1]->property("success").toBool());

    QSignalSpy objectCreated(test, SIGNAL(objectCreated(QObject*,QUrl)));
    test->load(testFileUrl("basicTest.qml"));
    QCOMPARE(objectCreated.count(), size);//one less than rootObjects().size() because we missed the first one
    QCOMPARE(test->rootObjects().size(), ++size);
    QVERIFY(test->rootObjects()[size -1]);
    QVERIFY(test->rootObjects()[size -1]->property("success").toBool());

    QByteArray testQml("import QtQml 2.0; QtObject{property bool success: true; property TestItem t: TestItem{}}");
    test->loadData(testQml, testFileUrl("dynamicTest.qml"));
    QCOMPARE(objectCreated.count(), size);
    QCOMPARE(test->rootObjects().size(), ++size);
    QVERIFY(test->rootObjects()[size -1]);
    QVERIFY(test->rootObjects()[size -1]->property("success").toBool());

    delete test;
}

void tst_qqmlapplicationengine::application()
{
    /* This test batches together some tests about running an external application
       written with QQmlApplicationEngine. The application tests the following functionality
       which is easier to do by watching a separate process:
       -Loads relative paths from the working directory
       -quits when quit is called
       -emits aboutToQuit after quit is called
       -has access to application command line arguments

       Note that checking the output means that on builds with extra debugging, this might fail with a false positive.
       Also the testapp is automatically built and installed in shadow builds, so it does NOT use testData
   */
#ifndef QT_NO_PROCESS
    QDir::setCurrent(buildDir);
    QProcess *testProcess = new QProcess(this);
    QStringList args;
    args << QLatin1String("testData");
    testProcess->start(QLatin1String("testapp/testapp"), args);
    QVERIFY(testProcess->waitForFinished(5000));
    QCOMPARE(testProcess->exitCode(), 0);
    QByteArray test_stdout = testProcess->readAllStandardOutput();
    QByteArray test_stderr = testProcess->readAllStandardError();
    QByteArray test_stderr_target("qml: Start: testData\nqml: End\n");
#ifdef Q_OS_WIN
    test_stderr_target.replace('\n', QByteArray("\r\n"));
#endif
    QCOMPARE(test_stdout, QByteArray(""));
    QVERIFY(QString(test_stderr).endsWith(QString(test_stderr_target)));
    delete testProcess;
    QDir::setCurrent(srcDir);
#else // !QT_NO_PROCESS
    QSKIP("No process support");
#endif // QT_NO_PROCESS
}

void tst_qqmlapplicationengine::applicationProperties()
{
    const QString originalName = QCoreApplication::applicationName();
    const QString originalVersion = QCoreApplication::applicationVersion();
    const QString originalOrganization = QCoreApplication::organizationName();
    const QString originalDomain = QCoreApplication::organizationDomain();
    QString firstName = QLatin1String("Test A");
    QString firstVersion = QLatin1String("0.0A");
    QString firstOrganization = QLatin1String("Org A");
    QString firstDomain = QLatin1String("a.org");
    QString secondName = QLatin1String("Test B");
    QString secondVersion = QLatin1String("0.0B");
    QString secondOrganization = QLatin1String("Org B");
    QString secondDomain = QLatin1String("b.org");

    QCoreApplication::setApplicationName(firstName);
    QCoreApplication::setApplicationVersion(firstVersion);
    QCoreApplication::setOrganizationName(firstOrganization);
    QCoreApplication::setOrganizationDomain(firstDomain);

    QQmlApplicationEngine *test = new QQmlApplicationEngine(testFileUrl("applicationTest.qml"));
    QObject* root = test->rootObjects().at(0);
    QVERIFY(root);
    QCOMPARE(root->property("originalName").toString(), firstName);
    QCOMPARE(root->property("originalVersion").toString(), firstVersion);
    QCOMPARE(root->property("originalOrganization").toString(), firstOrganization);
    QCOMPARE(root->property("originalDomain").toString(), firstDomain);
    QCOMPARE(root->property("currentName").toString(), secondName);
    QCOMPARE(root->property("currentVersion").toString(), secondVersion);
    QCOMPARE(root->property("currentOrganization").toString(), secondOrganization);
    QCOMPARE(root->property("currentDomain").toString(), secondDomain);
    QCOMPARE(QCoreApplication::applicationName(), secondName);
    QCOMPARE(QCoreApplication::applicationVersion(), secondVersion);
    QCOMPARE(QCoreApplication::organizationName(), secondOrganization);
    QCOMPARE(QCoreApplication::organizationDomain(), secondDomain);

    QObject* application = root->property("applicationInstance").value<QObject*>();
    QVERIFY(application);
    QSignalSpy nameChanged(application, SIGNAL(nameChanged()));
    QSignalSpy versionChanged(application, SIGNAL(versionChanged()));
    QSignalSpy organizationChanged(application, SIGNAL(organizationChanged()));
    QSignalSpy domainChanged(application, SIGNAL(domainChanged()));

    QCoreApplication::setApplicationName(originalName);
    QCoreApplication::setApplicationVersion(originalVersion);
    QCoreApplication::setOrganizationName(originalOrganization);
    QCoreApplication::setOrganizationDomain(originalDomain);

    QCOMPARE(nameChanged.count(), 1);
    QCOMPARE(versionChanged.count(), 1);
    QCOMPARE(organizationChanged.count(), 1);
    QCOMPARE(domainChanged.count(), 1);

    delete test;
}

QTEST_MAIN(tst_qqmlapplicationengine)

#include "tst_qqmlapplicationengine.moc"
