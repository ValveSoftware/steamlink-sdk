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
#include <QtTest/QtTest>
#include "../../shared/util.h"
#include <QtQuick/qquickview.h>
#include <private/qquickspritesequence_p.h>

class tst_qquickspritesequence : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickspritesequence(){}

private slots:
    void test_properties();
    void test_framerateAdvance();//Separate codepath for QQuickSpriteEngine
    void test_huge();//Separate codepath for QQuickSpriteEngine
    void test_jumpToCrash();
    void test_spriteBeforeGoal();
    void test_spriteAfterGoal();
};

void tst_qquickspritesequence::test_properties()
{
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("basic.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QVERIFY(window->rootObject());
    QQuickSpriteSequence* sprite = window->rootObject()->findChild<QQuickSpriteSequence*>("sprite");
    QVERIFY(sprite);

    QVERIFY(sprite->running());
    QVERIFY(sprite->interpolate());

    sprite->setRunning(false);
    QVERIFY(!sprite->running());
    sprite->setInterpolate(false);
    QVERIFY(!sprite->interpolate());

    delete window;
}

void tst_qquickspritesequence::test_huge()
{
    /* Merely tests that it doesn't crash, as waiting for it to complete
       (or even having something to watch) would bloat CI.
       The large allocations of memory involved and separate codepath does make
       a doesn't crash test worthwhile.
    */
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("huge.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QVERIFY(window->rootObject());
    QQuickSpriteSequence* sprite = window->rootObject()->findChild<QQuickSpriteSequence*>("sprite");
    QVERIFY(sprite);

    delete window;
}

void tst_qquickspritesequence::test_framerateAdvance()
{
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("advance.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QVERIFY(window->rootObject());
    QQuickSpriteSequence* sprite = window->rootObject()->findChild<QQuickSpriteSequence*>("sprite");
    QVERIFY(sprite);

    QTRY_COMPARE(sprite->currentSprite(), QLatin1String("secondState"));
    delete window;
}

void tst_qquickspritesequence::test_jumpToCrash()
{
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("crashonstart.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    //verify: Don't crash

    delete window;
}

void tst_qquickspritesequence::test_spriteBeforeGoal()
{
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("spritebeforegoal.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    //verify: Don't crash

    delete window;
}

void tst_qquickspritesequence::test_spriteAfterGoal()
{
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("spriteaftergoal.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    //verify: Don't crash

    delete window;
}

QTEST_MAIN(tst_qquickspritesequence)

#include "tst_qquickspritesequence.moc"
