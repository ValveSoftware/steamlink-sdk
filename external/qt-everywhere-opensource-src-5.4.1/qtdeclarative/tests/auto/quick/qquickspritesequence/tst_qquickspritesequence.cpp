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
