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
#include <QtCore/QObject>
#include <QtQml/qqml.h>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>

//Separate test, because if engine cleanup attempts fail they can easily break unrelated tests
class tst_qqmlenginecleanup : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlenginecleanup() {}

private slots:
    void test_qmlClearTypeRegistrations();
    void test_valueTypeProviderModule(); // QTBUG-43004
};

void tst_qqmlenginecleanup::test_qmlClearTypeRegistrations()
{
    //Test for preventing memory leaks is in tests/manual/qmltypememory
    QQmlEngine* engine;
    QQmlComponent* component;
    QUrl testFile = testFileUrl("types.qml");

    qmlRegisterType<QObject>("Test", 2, 0, "TestTypeCpp");
    engine = new QQmlEngine;
    component = new QQmlComponent(engine, testFile);
    QVERIFY(component->isReady());

    delete engine;
    delete component;
    qmlClearTypeRegistrations();

    //2nd run verifies that types can reload after a qmlClearTypeRegistrations
    qmlRegisterType<QObject>("Test", 2, 0, "TestTypeCpp");
    engine = new QQmlEngine;
    component = new QQmlComponent(engine, testFile);
    QVERIFY(component->isReady());

    delete engine;
    delete component;
    qmlClearTypeRegistrations();

    //3nd run verifies that TestTypeCpp is no longer registered
    engine = new QQmlEngine;
    component = new QQmlComponent(engine, testFile);
    QVERIFY(component->isError());
    QCOMPARE(component->errorString(),
            testFile.toString() +":33 module \"Test\" is not installed\n");

    delete engine;
    delete component;
}

static void cleanState(QQmlEngine **e)
{
    delete *e;
    qmlClearTypeRegistrations();
    *e = new QQmlEngine;
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

void tst_qqmlenginecleanup::test_valueTypeProviderModule()
{
    // this test ensures that a module which installs a value type
    // provider can be reinitialized after multiple calls to
    // qmlClearTypeRegistrations() without causing cycles in the
    // value type provider list.
    QQmlEngine *e = 0;
    QUrl testFile1 = testFileUrl("testFile1.qml");
    QUrl testFile2 = testFileUrl("testFile2.qml");
    bool noCycles = false;
    for (int i = 0; i < 20; ++i) {
        cleanState(&e);
        QQmlComponent c(e, this);
        c.loadUrl(i % 2 == 0 ? testFile1 : testFile2); // this will hang if cycles exist.
    }
    delete e;
    e = 0;
    noCycles = true;
    QVERIFY(noCycles);

    // this test ensures that no crashes occur due to using
    // a dangling QQmlType pointer in the type compiler
    // which results from qmlClearTypeRegistrations()
    QUrl testFile3 = testFileUrl("testFile3.qml");
    bool noDangling = false;
    for (int i = 0; i < 20; ++i) {
        cleanState(&e);
        QQmlComponent c(e, this);
        c.loadUrl(i % 2 == 0 ? testFile1 : testFile3); // this will crash if dangling ptr exists.
    }
    delete e;
    noDangling = true;
    QVERIFY(noDangling);
}

QTEST_MAIN(tst_qqmlenginecleanup)

#include "tst_qqmlenginecleanup.moc"
