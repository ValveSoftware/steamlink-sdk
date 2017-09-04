/****************************************************************************
**
** Copyright (C) 2016 Research In Motion.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the manual tests of the Qt Toolkit.
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

#include "testdriver.h"
#include <QCoreApplication>
const int interval = 0;

QObject* singleFactory(QQmlEngine *, QJSEngine*)
{
    QObject* ret = new QObject; //NOTE: No parent - but also shouldn't need to be created
    return ret;
}

TestDriver::TestDriver(const QUrl &componentUrl, int maxIter)
    : QObject(0), e(0), c(0), i(maxIter)
{
    testFile = componentUrl;
    QTimer::singleShot(interval, this, SLOT(setUp()));
}

void TestDriver::setUp()
{
    qmlRegisterType<TestType>("Test", 2, 0, "TestTypeCpp");
    for (int j=2; j<1000; j++) {
        qmlRegisterType<TestType>("Test", j, 0, "TestType");
        qmlRegisterType<TestType>("Test", j, 1, "TestType");
        qmlRegisterType<TestType>("Test", j, 2, "TestType");
        qmlRegisterType<TestType>("Test", j, 3, "TestType");
        qmlRegisterType<TestType>("Test", j, 4, "TestType");
        qmlRegisterType<TestType>("Test", j, 5, "TestType");
        qmlRegisterType<TestType>("Test", j, 6, "TestType");
        qmlRegisterType<TestType>("Test", j, 7, "TestType");
        qmlRegisterType<TestType>("Test", j, 8, "TestType");
        qmlRegisterType<TestType>("Test", j, 9, "TestType");
    }
    qmlRegisterType<TestType>("Test2", 1, 0, "TestType");
    qmlRegisterType<TestType>("Test2", 3, 0, "TestType");
    qmlRegisterType<TestType>("Test3", 9, 0, "TestType");
    qmlRegisterType(QUrl::fromLocalFile(QDir::currentPath() + QLatin1String("TestType.qml")), "Test", 2, 0, "TestType2");
    qmlRegisterUncreatableType<TestType2>("Test", 2, 0, "CannotDoThis", QLatin1String("Just don't"));
    qmlRegisterType<TestType3>();
    qmlRegisterSingletonType<QObject>("Test", 2, 0, "Singlet", singleFactory);

    e = new QQmlEngine(this);
    c = new QQmlComponent(e, testFile);
    QObject* unused = c->create();
    if (!unused) {
        qDebug() << "Creation failure" << c->errorString();
        exit(1);
    }
    unused->setParent(c); //Not testing leaky objects in the tree
    QTimer::singleShot(interval, this, SLOT(tearDown()));
}

void TestDriver::tearDown()
{
    delete c;
    e->collectGarbage();
    delete e;
    qmlClearTypeRegistrations();
    //Note that i < 0 will effectively never terminate. This is deliberate as a prime use is to run indefinitely and watch the memory *not* grow
    i--;
    if (!i)
        qApp->quit();
    else
        QTimer::singleShot(interval, this, SLOT(setUp()));
}
