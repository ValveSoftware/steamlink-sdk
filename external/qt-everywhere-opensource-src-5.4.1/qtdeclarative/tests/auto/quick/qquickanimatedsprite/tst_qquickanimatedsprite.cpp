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
#include <private/qabstractanimation_p.h>
#include <private/qquickanimatedsprite_p.h>

class tst_qquickanimatedsprite : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickanimatedsprite(){}

private slots:
    void initTestCase();
    void test_properties();
    void test_runningChangedSignal();
    void test_frameChangedSignal();
};

void tst_qquickanimatedsprite::initTestCase()
{
    QQmlDataTest::initTestCase();
    QUnifiedTimer::instance()->setConsistentTiming(true);
}

void tst_qquickanimatedsprite::test_properties()
{
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("basic.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QVERIFY(window->rootObject());
    QQuickAnimatedSprite* sprite = window->rootObject()->findChild<QQuickAnimatedSprite*>("sprite");
    QVERIFY(sprite);

    QTRY_VERIFY(sprite->running());
    QVERIFY(!sprite->paused());
    QVERIFY(sprite->interpolate());
    QCOMPARE(sprite->loops(), 30);

    sprite->setRunning(false);
    QVERIFY(!sprite->running());
    sprite->setInterpolate(false);
    QVERIFY(!sprite->interpolate());

    delete window;
}

void tst_qquickanimatedsprite::test_runningChangedSignal()
{
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("runningChange.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QVERIFY(window->rootObject());
    QQuickAnimatedSprite* sprite = window->rootObject()->findChild<QQuickAnimatedSprite*>("sprite");
    QVERIFY(sprite);

    QVERIFY(!sprite->running());

    QSignalSpy runningChangedSpy(sprite, SIGNAL(runningChanged(bool)));
    sprite->setRunning(true);
    QTRY_COMPARE(runningChangedSpy.count(), 1);
    QTRY_VERIFY(!sprite->running());
    QTRY_COMPARE(runningChangedSpy.count(), 2);

    delete window;
}

void tst_qquickanimatedsprite::test_frameChangedSignal()
{
    QQuickView *window = new QQuickView(0);

    window->setSource(testFileUrl("frameChange.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QVERIFY(window->rootObject());
    QQuickAnimatedSprite* sprite = window->rootObject()->findChild<QQuickAnimatedSprite*>("sprite");
    QVERIFY(sprite);

    QVERIFY(!sprite->running());
    QVERIFY(!sprite->paused());
    QCOMPARE(sprite->loops(), 3);
    QCOMPARE(sprite->frameCount(), 6);

    QSignalSpy frameChangedSpy(sprite, SIGNAL(currentFrameChanged(int)));
    sprite->setRunning(true);
    QTRY_COMPARE(frameChangedSpy.count(), 3*6);
    QTRY_VERIFY(!sprite->running());

    delete window;
}

QTEST_MAIN(tst_qquickanimatedsprite)

#include "tst_qquickanimatedsprite.moc"
