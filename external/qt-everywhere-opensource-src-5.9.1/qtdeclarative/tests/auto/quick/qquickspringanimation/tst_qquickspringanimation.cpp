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
#include <qtest.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/qquickview.h>
#include <private/qquickspringanimation_p.h>
#include <private/qqmlvaluetype_p.h>
#include "../../shared/util.h"

class tst_qquickspringanimation : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickspringanimation();

private slots:
    void defaultValues();
    void values();
    void disabled();
    void inTransition();

private:
    QQmlEngine engine;
};

tst_qquickspringanimation::tst_qquickspringanimation()
{
}

void tst_qquickspringanimation::defaultValues()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("springanimation1.qml"));
    QQuickSpringAnimation *obj = qobject_cast<QQuickSpringAnimation*>(c.create());

    QVERIFY(obj != 0);

    QCOMPARE(obj->to(), 0.);
    QCOMPARE(obj->velocity(), 0.);
    QCOMPARE(obj->spring(), 0.);
    QCOMPARE(obj->damping(), 0.);
    QCOMPARE(obj->epsilon(), 0.01);
    QCOMPARE(obj->modulus(), 0.);
    QCOMPARE(obj->mass(), 1.);
    QCOMPARE(obj->isRunning(), false);

    delete obj;
}

void tst_qquickspringanimation::values()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("springanimation2.qml"));
    QObject *root = c.create();

    QQuickSpringAnimation *obj = root->findChild<QQuickSpringAnimation*>();

    QVERIFY(obj != 0);

    QCOMPARE(obj->to(), 1.44);
    QCOMPARE(obj->velocity(), 0.9);
    QCOMPARE(obj->spring(), 1.0);
    QCOMPARE(obj->damping(), 0.5);
    QCOMPARE(obj->epsilon(), 0.25);
    QCOMPARE(obj->modulus(), 360.0);
    QCOMPARE(obj->mass(), 2.0);
    QCOMPARE(obj->isRunning(), true);

    QTRY_COMPARE(obj->isRunning(), false);

    delete obj;
}

void tst_qquickspringanimation::disabled()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("springanimation3.qml"));
    QQuickSpringAnimation *obj = qobject_cast<QQuickSpringAnimation*>(c.create());

    QVERIFY(obj != 0);

    QCOMPARE(obj->to(), 1.44);
    QCOMPARE(obj->velocity(), 0.9);
    QCOMPARE(obj->spring(), 1.0);
    QCOMPARE(obj->damping(), 0.5);
    QCOMPARE(obj->epsilon(), 0.25);
    QCOMPARE(obj->modulus(), 360.0);
    QCOMPARE(obj->mass(), 2.0);
    QCOMPARE(obj->isRunning(), false);

    delete obj;
}

void tst_qquickspringanimation::inTransition()
{
    QQuickView view(testFileUrl("inTransition.qml"));
    view.show();
    // this used to crash after ~1 sec, once the spring animation was done
    QTest::qWait(2000);
}

QTEST_MAIN(tst_qquickspringanimation)

#include "tst_qquickspringanimation.moc"
