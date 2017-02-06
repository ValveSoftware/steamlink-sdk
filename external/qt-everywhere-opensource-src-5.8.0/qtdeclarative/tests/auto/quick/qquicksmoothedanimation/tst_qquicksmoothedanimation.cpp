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
#include <QtQuick/private/qquicksmoothedanimation_p.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <private/qqmlvaluetype_p.h>
#include "../../shared/util.h"

class tst_qquicksmoothedanimation : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquicksmoothedanimation();

private slots:
    void defaultValues();
    void values();
    void disabled();
    void simpleAnimation();
    void valueSource();
    void behavior();
    void deleteOnUpdate();
    void zeroDuration();
    void noStart();

private:
    QQmlEngine engine;
};

tst_qquicksmoothedanimation::tst_qquicksmoothedanimation()
{
}

void tst_qquicksmoothedanimation::defaultValues()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("smoothedanimation1.qml"));
    QQuickSmoothedAnimation *obj = qobject_cast<QQuickSmoothedAnimation*>(c.create());

    QVERIFY(obj != 0);

    QCOMPARE(obj->to(), 0.);
    QCOMPARE(obj->velocity(), 200.);
    QCOMPARE(obj->duration(), -1);
    QCOMPARE(obj->maximumEasingTime(), -1);
    QCOMPARE(obj->reversingMode(), QQuickSmoothedAnimation::Eased);

    delete obj;
}

void tst_qquicksmoothedanimation::values()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("smoothedanimation2.qml"));
    QQuickSmoothedAnimation *obj = qobject_cast<QQuickSmoothedAnimation*>(c.create());

    QVERIFY(obj != 0);

    QCOMPARE(obj->to(), 10.);
    QCOMPARE(obj->velocity(), 200.);
    QCOMPARE(obj->duration(), 300);
    QCOMPARE(obj->maximumEasingTime(), -1);
    QCOMPARE(obj->reversingMode(), QQuickSmoothedAnimation::Immediate);

    delete obj;
}

void tst_qquicksmoothedanimation::disabled()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("smoothedanimation3.qml"));
    QQuickSmoothedAnimation *obj = qobject_cast<QQuickSmoothedAnimation*>(c.create());

    QVERIFY(obj != 0);

    QCOMPARE(obj->to(), 10.);
    QCOMPARE(obj->velocity(), 250.);
    QCOMPARE(obj->maximumEasingTime(), 150);
    QCOMPARE(obj->reversingMode(), QQuickSmoothedAnimation::Sync);

    delete obj;
}

void tst_qquicksmoothedanimation::simpleAnimation()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("simpleanimation.qml"));
    QObject *obj = c.create();
    QVERIFY(obj);

    QQuickRectangle *rect = obj->findChild<QQuickRectangle*>("rect");
    QVERIFY(rect);

    QQuickSmoothedAnimation *animation = obj->findChild<QQuickSmoothedAnimation*>("anim");
    QVERIFY(animation);

    animation->setTargetObject(rect);
    animation->setProperty("x");
    animation->setTo(200);
    animation->setDuration(250);
    QCOMPARE(animation->target(), rect);
    QCOMPARE(animation->property(), QLatin1String("x"));
    QCOMPARE(animation->to(), qreal(200));
    animation->start();
    QVERIFY(animation->isRunning());
    QTest::qWait(animation->duration());
    QTRY_COMPARE(rect->x(), qreal(200));
    QTest::qWait(100);  //smoothed animation doesn't report stopped until delayed timer fires

    QVERIFY(!animation->isRunning());
    rect->setX(0);
    animation->start();
    QVERIFY(animation->isRunning());
    animation->pause();
    QVERIFY(animation->isRunning());
    QVERIFY(animation->isPaused());
    animation->setCurrentTime(125);
    QCOMPARE(animation->currentTime(), 125);
    QCOMPARE(rect->x(), qreal(100));
}

void tst_qquicksmoothedanimation::valueSource()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("smoothedanimationValueSource.qml"));

    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);

    QQuickRectangle *theRect = rect->findChild<QQuickRectangle*>("theRect");
    QVERIFY(theRect);

    QQuickSmoothedAnimation *easeX = rect->findChild<QQuickSmoothedAnimation*>("easeX");
    QVERIFY(easeX);
    QVERIFY(easeX->isRunning());

    QQuickSmoothedAnimation *easeY = rect->findChild<QQuickSmoothedAnimation*>("easeY");
    QVERIFY(easeY);
    QVERIFY(easeY->isRunning());

    // XXX get the proper duration
    QTest::qWait(100);

    QTRY_VERIFY(!easeX->isRunning());
    QTRY_VERIFY(!easeY->isRunning());

    QTRY_COMPARE(theRect->x(), qreal(200));
    QTRY_COMPARE(theRect->y(), qreal(200));

    delete rect;
}

void tst_qquicksmoothedanimation::behavior()
{
#ifdef Q_CC_MINGW
    QSKIP("QTBUG-36290 - MinGW Animation tests are flaky.");
#endif
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("smoothedanimationBehavior.qml"));

    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);

    QQuickRectangle *theRect = rect->findChild<QQuickRectangle*>("theRect");
    QVERIFY(theRect);

    QQuickSmoothedAnimation *easeX = rect->findChild<QQuickSmoothedAnimation*>("easeX");
    QVERIFY(easeX);

    QQuickSmoothedAnimation *easeY = rect->findChild<QQuickSmoothedAnimation*>("easeY");
    QVERIFY(easeY);

    // XXX get the proper duration
    QTest::qWait(400);

    QTRY_VERIFY(!easeX->isRunning());
    QTRY_VERIFY(!easeY->isRunning());

    QTRY_COMPARE(theRect->x(), qreal(200));
    QTRY_COMPARE(theRect->y(), qreal(200));

    delete rect;
}

void tst_qquicksmoothedanimation::deleteOnUpdate()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("deleteOnUpdate.qml"));

    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);

    QQuickSmoothedAnimation *anim = rect->findChild<QQuickSmoothedAnimation*>("anim");
    QVERIFY(anim);

    //don't crash
    QTest::qWait(500);

    delete rect;
}

void tst_qquicksmoothedanimation::zeroDuration()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("smoothedanimationZeroDuration.qml"));

    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);

    QQuickRectangle *theRect = rect->findChild<QQuickRectangle*>("theRect");
    QVERIFY(theRect);

    QQuickSmoothedAnimation *easeX = rect->findChild<QQuickSmoothedAnimation*>("easeX");
    QVERIFY(easeX);
    QVERIFY(easeX->isRunning());

    QTRY_VERIFY(!easeX->isRunning());
    QTRY_COMPARE(theRect->x(), qreal(200));

    delete rect;
}

//verify that an empty SmoothedAnimation does not fire up the animation system
//and keep it running forever
void tst_qquicksmoothedanimation::noStart()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("smoothedanimation1.qml"));
    QQuickSmoothedAnimation *obj = qobject_cast<QQuickSmoothedAnimation*>(c.create());

    QVERIFY(obj != 0);

    obj->start();
    QCOMPARE(obj->isRunning(), false);
    QTest::qWait(100);
    QCOMPARE(obj->isRunning(), false);
    //this could fail if the test is being run in parallel with other tests
    //or if an earlier test failed and didn't clean up (left an animation running)
    //QCOMPARE(QUnifiedTimer::instance()->runningAnimationCount(), 0);

    delete obj;
}

QTEST_MAIN(tst_qquicksmoothedanimation)

#include "tst_qquicksmoothedanimation.moc"
