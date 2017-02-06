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
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/qquickview.h>
#include <QtQml/private/qanimationgroupjob_p.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQuick/private/qquickitemanimation_p.h>
#include <QtQuick/private/qquickitemanimation_p_p.h>
#include <QtQuick/private/qquicktransition_p.h>
#include <QtQuick/private/qquickanimation_p.h>
#include <QtQuick/private/qquickpathinterpolator_p.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QEasingCurve>

#include <limits.h>
#include <math.h>

#include "../../shared/util.h"

class tst_qquickanimations : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickanimations() {}

private slots:
    void initTestCase()
    {
        QQmlEngine engine;  // ensure types are registered
        QQmlDataTest::initTestCase();
    }

    void simpleProperty();
    void simpleNumber();
    void simpleColor();
    void simpleRotation();
    void simplePath();
    void simpleAnchor();
    void reparent();
    void pathInterpolator();
    void pathInterpolatorBackwardJump();
    void pathWithNoStart();
    void alwaysRunToEnd();
    void complete();
    void resume();
    void dotProperty();
    void badTypes();
    void badProperties();
    void mixedTypes();
    void properties();
    void propertiesTransition();
    void pathTransition();
    void disabledTransition();
    void invalidDuration();
    void attached();
    void propertyValueSourceDefaultStart();
    void dontStart();
    void easingProperties();
    void rotation();
    void startStopSignals();
    void runningTrueBug();
    void nonTransitionBug();
    void registrationBug();
    void doubleRegistrationBug();
    void alwaysRunToEndRestartBug();
    void transitionAssignmentBug();
    void pauseBindingBug();
    void pauseBug();
    void loopingBug();
    void anchorBug();
    void pathAnimationInOutBackBug();
    void scriptActionBug();
    void groupAnimationNullChildBug();
    void scriptActionCrash();
    void animatorInvalidTargetCrash();
    void defaultPropertyWarning();
};

#define QTIMED_COMPARE(lhs, rhs) do { \
    for (int ii = 0; ii < 5; ++ii) { \
        if (lhs == rhs)  \
            break; \
        QTest::qWait(50); \
    } \
    QCOMPARE(lhs, rhs); \
} while (false)

void tst_qquickanimations::simpleProperty()
{
    QQuickRectangle rect;
    QQuickPropertyAnimation animation;
    animation.setTargetObject(&rect);
    animation.setProperty("x");
    animation.setTo(200);
    QCOMPARE(animation.target(), &rect);
    QCOMPARE(animation.property(), QLatin1String("x"));
    QCOMPARE(animation.to().toReal(), 200.0);
    animation.start();
    QVERIFY(animation.isRunning());
    QTest::qWait(animation.duration());
    QTIMED_COMPARE(rect.x(), 200.0);

    rect.setPosition(QPointF(0,0));
    animation.start();
    QVERIFY(animation.isRunning());
    animation.pause();
    QVERIFY(animation.isPaused());
    animation.setCurrentTime(125);
    QCOMPARE(animation.currentTime(), 125);
    QCOMPARE(rect.x(),100.0);
}

void tst_qquickanimations::simpleNumber()
{
    QQuickRectangle rect;
    QQuickNumberAnimation animation;
    animation.setTargetObject(&rect);
    animation.setProperty("x");
    animation.setTo(200);
    QCOMPARE(animation.target(), &rect);
    QCOMPARE(animation.property(), QLatin1String("x"));
    QCOMPARE(animation.to(), qreal(200));
    animation.start();
    QVERIFY(animation.isRunning());
    QTest::qWait(animation.duration());
    QTIMED_COMPARE(rect.x(), qreal(200));

    rect.setX(0);
    animation.start();
    animation.pause();
    QVERIFY(animation.isRunning());
    QVERIFY(animation.isPaused());
    animation.setCurrentTime(125);
    QCOMPARE(animation.currentTime(), 125);
    QCOMPARE(rect.x(), qreal(100));
}

void tst_qquickanimations::simpleColor()
{
    QQuickRectangle rect;
    QQuickColorAnimation animation;
    animation.setTargetObject(&rect);
    animation.setProperty("color");
    animation.setTo(QColor("red"));
    QCOMPARE(animation.target(), &rect);
    QCOMPARE(animation.property(), QLatin1String("color"));
    QCOMPARE(animation.to(), QColor("red"));
    animation.start();
    QVERIFY(animation.isRunning());
    QTest::qWait(animation.duration());
    QTIMED_COMPARE(rect.color(), QColor("red"));

    rect.setColor(QColor("blue"));
    animation.start();
    animation.pause();
    QVERIFY(animation.isRunning());
    QVERIFY(animation.isPaused());
    animation.setCurrentTime(125);
    QCOMPARE(animation.currentTime(), 125);
    QCOMPARE(rect.color(), QColor::fromRgbF(0.498039, 0, 0.498039, 1));

    rect.setColor(QColor("green"));
    animation.setFrom(QColor("blue"));
    QCOMPARE(animation.from(), QColor("blue"));
    animation.restart();
    QCOMPARE(rect.color(), QColor("blue"));
    QVERIFY(animation.isRunning());
    animation.setCurrentTime(125);
    QCOMPARE(rect.color(), QColor::fromRgbF(0.498039, 0, 0.498039, 1));
}

void tst_qquickanimations::simpleRotation()
{
    QQuickRectangle rect;
    QQuickRotationAnimation animation;
    animation.setTargetObject(&rect);
    animation.setProperty("rotation");
    animation.setTo(270);
    QCOMPARE(animation.target(), &rect);
    QCOMPARE(animation.property(), QLatin1String("rotation"));
    QCOMPARE(animation.to(), qreal(270));
    QCOMPARE(animation.direction(), QQuickRotationAnimation::Numerical);
    animation.start();
    QVERIFY(animation.isRunning());
    QTest::qWait(animation.duration());
    QTIMED_COMPARE(rect.rotation(), qreal(270));

    rect.setRotation(0);
    animation.start();
    animation.pause();
    QVERIFY(animation.isRunning());
    QVERIFY(animation.isPaused());
    animation.setCurrentTime(125);
    QCOMPARE(animation.currentTime(), 125);
    QCOMPARE(rect.rotation(), qreal(135));
}

void tst_qquickanimations::simplePath()
{
    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("pathAnimation.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *redRect = rect->findChild<QQuickRectangle*>();
        QVERIFY(redRect);
        QQuickPathAnimation *pathAnim = rect->findChild<QQuickPathAnimation*>();
        QVERIFY(pathAnim);

        QCOMPARE(pathAnim->duration(), 100);
        QCOMPARE(pathAnim->target(), redRect);

        pathAnim->start();
        pathAnim->pause();

        pathAnim->setCurrentTime(30);
        QCOMPARE(redRect->x(), qreal(167));
        QCOMPARE(redRect->y(), qreal(104));

        pathAnim->setCurrentTime(100);
        QCOMPARE(redRect->x(), qreal(300));
        QCOMPARE(redRect->y(), qreal(300));

        //verify animation runs to end
        pathAnim->start();
        QCOMPARE(redRect->x(), qreal(50));
        QCOMPARE(redRect->y(), qreal(50));
        QTRY_COMPARE(redRect->x(), qreal(300));
        QCOMPARE(redRect->y(), qreal(300));

        pathAnim->setOrientation(QQuickPathAnimation::RightFirst);
        QCOMPARE(pathAnim->orientation(), QQuickPathAnimation::RightFirst);
        pathAnim->start();
        QTRY_VERIFY(redRect->rotation() != 0);
        pathAnim->stop();

        delete rect;
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("pathAnimation2.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *redRect = rect->findChild<QQuickRectangle*>();
        QVERIFY(redRect);
        QQuickPathAnimation *pathAnim = rect->findChild<QQuickPathAnimation*>();
        QVERIFY(pathAnim);

        QCOMPARE(pathAnim->orientation(), QQuickPathAnimation::RightFirst);
        QCOMPARE(pathAnim->endRotation(), qreal(0));
        QCOMPARE(pathAnim->orientationEntryDuration(), 10);
        QCOMPARE(pathAnim->orientationExitDuration(), 10);

        pathAnim->start();
        pathAnim->pause();
        QCOMPARE(redRect->x(), qreal(50));
        QCOMPARE(redRect->y(), qreal(50));
        QCOMPARE(redRect->rotation(), qreal(-360));

        pathAnim->setCurrentTime(50);
        QCOMPARE(redRect->x(), qreal(175));
        QCOMPARE(redRect->y(), qreal(175));
        QCOMPARE(redRect->rotation(), qreal(-315));

        pathAnim->setCurrentTime(100);
        QCOMPARE(redRect->x(), qreal(300));
        QCOMPARE(redRect->y(), qreal(300));
        QCOMPARE(redRect->rotation(), qreal(0));

        delete rect;
    }
}

void tst_qquickanimations::simpleAnchor()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("reanchor.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);

    QQuickRectangle *greenRect = rect->findChild<QQuickRectangle*>();
    QVERIFY(greenRect);

    QCOMPARE(rect->state(), QLatin1String("reanchored"));
    QCOMPARE(greenRect->x(), qreal(10));
    QCOMPARE(greenRect->y(), qreal(0));
    QCOMPARE(greenRect->width(), qreal(190));
    QCOMPARE(greenRect->height(), qreal(150));

    rect->setState("");

    //verify animation in progress
    QTRY_VERIFY(greenRect->x() < 10 && greenRect->x() > 0);
    QVERIFY(greenRect->y() > 0 && greenRect->y() < 10);
    QVERIFY(greenRect->width() < 190 && greenRect->width() > 150);
    QVERIFY(greenRect->height() > 150 && greenRect->height() < 190);

    //verify end state ("")
    QTRY_COMPARE(greenRect->x(), qreal(0));
    QCOMPARE(greenRect->y(), qreal(10));
    QCOMPARE(greenRect->width(), qreal(150));
    QCOMPARE(greenRect->height(), qreal(190));

    rect->setState("reanchored2");

    //verify animation in progress
    QTRY_VERIFY(greenRect->y() > 10 && greenRect->y() < 50);
    QVERIFY(greenRect->height() > 125 && greenRect->height() < 190);
    //NOTE: setting left/right anchors to undefined removes the anchors, but does not resize.
    QCOMPARE(greenRect->x(), qreal(0));
    QCOMPARE(greenRect->width(), qreal(150));

    //verify end state ("reanchored2")
    QTRY_COMPARE(greenRect->y(), qreal(50));
    QCOMPARE(greenRect->height(), qreal(125));
    QCOMPARE(greenRect->x(), qreal(0));
    QCOMPARE(greenRect->width(), qreal(150));

    rect->setState("reanchored");

    //verify animation in progress
    QTRY_VERIFY(greenRect->x() < 10 && greenRect->x() > 0);
    QVERIFY(greenRect->y() > 0 && greenRect->y() < 50);
    QVERIFY(greenRect->width() < 190 && greenRect->width() > 150);
    QVERIFY(greenRect->height() > 125 && greenRect->height() < 150);

    //verify end state ("reanchored")
    QTRY_COMPARE(greenRect->x(), qreal(10));
    QCOMPARE(greenRect->y(), qreal(0));
    QCOMPARE(greenRect->width(), qreal(190));
    QCOMPARE(greenRect->height(), qreal(150));

    rect->setState("reanchored2");

    //verify animation in progress
    QTRY_VERIFY(greenRect->x() < 10 && greenRect->x() > 0);
    QVERIFY(greenRect->y() > 0 && greenRect->y() < 50);
    QVERIFY(greenRect->width() < 190 && greenRect->width() > 150);
    QVERIFY(greenRect->height() > 125 && greenRect->height() < 150);

    //verify end state ("reanchored2")
    QTRY_COMPARE(greenRect->x(), qreal(0));
    QCOMPARE(greenRect->y(), qreal(50));
    QCOMPARE(greenRect->width(), qreal(150));
    QCOMPARE(greenRect->height(), qreal(125));

    delete rect;
}

void tst_qquickanimations::reparent()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("reparent.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);

    QQuickRectangle *target = rect->findChild<QQuickRectangle*>("target");
    QVERIFY(target);

    QCOMPARE(target->parentItem(), rect);
    QCOMPARE(target->x(), qreal(0));
    QCOMPARE(target->y(), qreal(0));
    QCOMPARE(target->width(), qreal(50));
    QCOMPARE(target->height(), qreal(50));
    QCOMPARE(target->rotation(), qreal(0));
    QCOMPARE(target->scale(), qreal(1));

    rect->setState("state1");

    QQuickRectangle *viaParent = rect->findChild<QQuickRectangle*>("viaParent");
    QVERIFY(viaParent);

    QQuickRectangle *newParent = rect->findChild<QQuickRectangle*>("newParent");
    QVERIFY(newParent);

    QTest::qWait(100);

    //animation in progress
    QTRY_COMPARE(target->parentItem(), viaParent);
    QVERIFY(target->x() > -100 && target->x() < 50);
    QVERIFY(target->y() > -100 && target->y() < 50);
    QVERIFY(target->width() > 50 && target->width() < 100);
    QCOMPARE(target->height(), qreal(50));
    QCOMPARE(target->rotation(), qreal(-45));
    QCOMPARE(target->scale(), qreal(.5));

    //end state
    QTRY_COMPARE(target->parentItem(), newParent);
    QCOMPARE(target->x(), qreal(50));
    QCOMPARE(target->y(), qreal(50));
    QCOMPARE(target->width(), qreal(100));
    QCOMPARE(target->height(), qreal(50));
    QCOMPARE(target->rotation(), qreal(0));
    QCOMPARE(target->scale(), qreal(1));

    delete rect;
}

void tst_qquickanimations::pathInterpolator()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("pathInterpolator.qml"));
    QQuickPathInterpolator *interpolator = qobject_cast<QQuickPathInterpolator*>(c.create());
    QVERIFY(interpolator);

    QCOMPARE(interpolator->progress(), qreal(0));
    QCOMPARE(interpolator->x(), qreal(50));
    QCOMPARE(interpolator->y(), qreal(50));
    QCOMPARE(interpolator->angle(), qreal(0));

    interpolator->setProgress(.5);
    QCOMPARE(interpolator->progress(), qreal(.5));
    QCOMPARE(interpolator->x(), qreal(175));
    QCOMPARE(interpolator->y(), qreal(175));
    QCOMPARE(interpolator->angle(), qreal(90));

    interpolator->setProgress(1);
    QCOMPARE(interpolator->progress(), qreal(1));
    QCOMPARE(interpolator->x(), qreal(300));
    QCOMPARE(interpolator->y(), qreal(300));
    QCOMPARE(interpolator->angle(), qreal(0));

    //for path interpulator the progress value must be [0,1] range.
    interpolator->setProgress(1.1);
    QCOMPARE(interpolator->progress(), qreal(1));

    interpolator->setProgress(-0.000123);
    QCOMPARE(interpolator->progress(), qreal(0));
}

void tst_qquickanimations::pathInterpolatorBackwardJump()
{
#ifdef Q_CC_MINGW
    QSKIP("QTBUG-36290 - MinGW Animation tests are flaky.");
#endif
    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("pathInterpolatorBack.qml"));
        QQuickPathInterpolator *interpolator = qobject_cast<QQuickPathInterpolator*>(c.create());
        QVERIFY(interpolator);

        QCOMPARE(interpolator->progress(), qreal(0));
        QCOMPARE(interpolator->x(), qreal(50));
        QCOMPARE(interpolator->y(), qreal(50));
        QCOMPARE(interpolator->angle(), qreal(90));

        interpolator->setProgress(.5);
        QCOMPARE(interpolator->progress(), qreal(.5));
        QCOMPARE(interpolator->x(), qreal(100));
        QCOMPARE(interpolator->y(), qreal(75));
        QCOMPARE(interpolator->angle(), qreal(270));

        interpolator->setProgress(1);
        QCOMPARE(interpolator->progress(), qreal(1));
        QCOMPARE(interpolator->x(), qreal(200));
        QCOMPARE(interpolator->y(), qreal(50));
        QCOMPARE(interpolator->angle(), qreal(0));

        //make sure we don't get caught in infinite loop here
        interpolator->setProgress(0);
        QCOMPARE(interpolator->progress(), qreal(0));
        QCOMPARE(interpolator->x(), qreal(50));
        QCOMPARE(interpolator->y(), qreal(50));
        QCOMPARE(interpolator->angle(), qreal(90));
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("pathInterpolatorBack2.qml"));
        QQuickPathInterpolator *interpolator = qobject_cast<QQuickPathInterpolator*>(c.create());
        QVERIFY(interpolator);

        QCOMPARE(interpolator->progress(), qreal(0));
        QCOMPARE(interpolator->x(), qreal(200));
        QCOMPARE(interpolator->y(), qreal(280));
        QCOMPARE(interpolator->angle(), qreal(180));

        interpolator->setProgress(1);
        QCOMPARE(interpolator->progress(), qreal(1));
        QCOMPARE(interpolator->x(), qreal(0));
        QCOMPARE(interpolator->y(), qreal(80));
        QCOMPARE(interpolator->angle(), qreal(180));

        //make sure we don't get caught in infinite loop here
        interpolator->setProgress(0);
        QCOMPARE(interpolator->progress(), qreal(0));
        QCOMPARE(interpolator->x(), qreal(200));
        QCOMPARE(interpolator->y(), qreal(280));
        QCOMPARE(interpolator->angle(), qreal(180));
    }
}

void tst_qquickanimations::pathWithNoStart()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("pathAnimationNoStart.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);

    QQuickRectangle *redRect = rect->findChild<QQuickRectangle*>();
    QVERIFY(redRect);
    QQuickPathAnimation *pathAnim = rect->findChild<QQuickPathAnimation*>();
    QVERIFY(pathAnim);

    pathAnim->start();
    pathAnim->pause();
    QCOMPARE(redRect->x(), qreal(50));
    QCOMPARE(redRect->y(), qreal(50));

    pathAnim->setCurrentTime(50);
    QCOMPARE(redRect->x(), qreal(175));
    QCOMPARE(redRect->y(), qreal(175));

    pathAnim->setCurrentTime(100);
    QCOMPARE(redRect->x(), qreal(300));
    QCOMPARE(redRect->y(), qreal(300));

    redRect->setX(100);
    redRect->setY(100);
    pathAnim->start();
    QCOMPARE(redRect->x(), qreal(100));
    QCOMPARE(redRect->y(), qreal(100));
    QTRY_COMPARE(redRect->x(), qreal(300));
    QCOMPARE(redRect->y(), qreal(300));
}

void tst_qquickanimations::alwaysRunToEnd()
{
    QQuickRectangle rect;
    QQuickPropertyAnimation animation;
    animation.setTargetObject(&rect);
    animation.setProperty("x");
    animation.setTo(200);
    animation.setDuration(1000);
    animation.setLoops(-1);
    animation.setAlwaysRunToEnd(true);
    QCOMPARE(animation.loops(), -1);
    QVERIFY(animation.alwaysRunToEnd());

    QElapsedTimer timer;
    timer.start();
    animation.start();

    // Make sure the animation has started but is not finished, yet.
    QTRY_VERIFY(rect.x() > qreal(0) && rect.x() != qreal(200));

    animation.stop();

    // Make sure it didn't just jump to the end and also didn't revert to the start.
    QVERIFY(rect.x() > qreal(0) && rect.x() != qreal(200));

    // Make sure it eventually reaches the end.
    QTRY_COMPARE(rect.x(), qreal(200));

    // This should have taken at least 1s but less than 2s
    // (otherwise it has run the animation twice).
    qint64 elapsed = timer.elapsed();
    QVERIFY(elapsed >= 1000 && elapsed < 2000);
}

void tst_qquickanimations::complete()
{
    QQuickRectangle rect;
    QQuickPropertyAnimation animation;
    animation.setTargetObject(&rect);
    animation.setProperty("x");
    animation.setFrom(1);
    animation.setTo(200);
    animation.setDuration(500);
    QCOMPARE(animation.from().toInt(), 1);
    animation.start();
    QTest::qWait(50);
    animation.stop();
    QVERIFY(rect.x() != qreal(200));
    animation.start();
    QTRY_VERIFY(animation.isRunning());
    animation.complete();
    QCOMPARE(rect.x(), qreal(200));
}

void tst_qquickanimations::resume()
{
    QQuickRectangle rect;
    QQuickPropertyAnimation animation;
    animation.setTargetObject(&rect);
    animation.setProperty("x");
    animation.setFrom(10);
    animation.setTo(200);
    animation.setDuration(1000);
    QCOMPARE(animation.from().toInt(), 10);

    animation.start();
    QTest::qWait(400);
    animation.pause();
    qreal x = rect.x();
    QVERIFY(x != qreal(200) && x != qreal(10));
    QVERIFY(animation.isRunning());
    QVERIFY(animation.isPaused());

    animation.resume();
    QVERIFY(animation.isRunning());
    QVERIFY(!animation.isPaused());
    QTest::qWait(400);
    animation.stop();
    QVERIFY(rect.x() > x);

    animation.start();
    QVERIFY(animation.isRunning());
    animation.pause();
    QVERIFY(animation.isPaused());
    animation.resume();
    QVERIFY(!animation.isPaused());

    QSignalSpy spy(&animation, SIGNAL(pausedChanged(bool)));
    animation.pause();
    QCOMPARE(spy.count(), 1);
    QVERIFY(animation.isPaused());
    animation.stop();
    QVERIFY(!animation.isPaused());
    QCOMPARE(spy.count(), 2);

    // Load QtQuick to ensure that QQuickPropertyAnimation is registered as PropertyAnimation
    {
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0\nQtObject {}\n", QUrl());
    }

    QByteArray message = "<Unknown File>: QML PropertyAnimation: setPaused() cannot be used when animation isn't running.";
    QTest::ignoreMessage(QtWarningMsg, message);
    animation.pause();
    QCOMPARE(spy.count(), 2);
    QVERIFY(!animation.isPaused());
    animation.resume();
    QVERIFY(!animation.isPaused());
    QVERIFY(!animation.isRunning());
    QCOMPARE(spy.count(), 2);
}

void tst_qquickanimations::dotProperty()
{
    QQuickRectangle rect;
    QQuickNumberAnimation animation;
    animation.setTargetObject(&rect);
    animation.setProperty("border.width");
    animation.setTo(10);
    animation.start();
    QTest::qWait(animation.duration()+50);
    QTIMED_COMPARE(rect.border()->width(), 10.0);

    rect.border()->setWidth(0);
    animation.start();
    animation.pause();
    animation.setCurrentTime(125);
    QCOMPARE(animation.currentTime(), 125);
    QCOMPARE(rect.border()->width(), 5.0);
}

void tst_qquickanimations::badTypes()
{
    //don't crash
    {
        QQuickView *view = new QQuickView;
        view->setSource(testFileUrl("badtype1.qml"));

        qApp->processEvents();

        delete view;
    }

    //make sure we get a compiler error
    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("badtype2.qml"));
        QTest::ignoreMessage(QtWarningMsg, "QQmlComponent: Component is not ready");
        c.create();

        QCOMPARE(c.errors().count(), 1);
        QCOMPARE(c.errors().at(0).description(), QLatin1String("Invalid property assignment: number expected"));
    }

    //make sure we get a compiler error
    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("badtype3.qml"));
        QTest::ignoreMessage(QtWarningMsg, "QQmlComponent: Component is not ready");
        c.create();

        QCOMPARE(c.errors().count(), 1);
        QCOMPARE(c.errors().at(0).description(), QLatin1String("Invalid property assignment: color expected"));
    }

    //don't crash
    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("badtype4.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickItemPrivate::get(rect)->setState("state1");

        QQuickRectangle *myRect = 0;
        QTRY_VERIFY(myRect = rect->findChild<QQuickRectangle*>("MyRect"));
        QTRY_COMPARE(myRect->x(),qreal(200));
    }
}

void tst_qquickanimations::badProperties()
{
    //make sure we get a runtime error
    {
        QQmlEngine engine;

        QQmlComponent c1(&engine, testFileUrl("badproperty1.qml"));
        QByteArray message = testFileUrl("badproperty1.qml").toString().toUtf8() + ":18:9: QML ColorAnimation: Cannot animate non-existent property \"border.colr\"";
        QTest::ignoreMessage(QtWarningMsg, message);
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c1.create());
        QVERIFY(rect);

        QQmlComponent c2(&engine, testFileUrl("badproperty2.qml"));
        message = testFileUrl("badproperty2.qml").toString().toUtf8() + ":18:9: QML ColorAnimation: Cannot animate read-only property \"border\"";
        QTest::ignoreMessage(QtWarningMsg, message);
        rect = qobject_cast<QQuickRectangle*>(c2.create());
        QVERIFY(rect);

        //### should we warn here are well?
        //rect->setState("state1");
    }
}

//test animating mixed types with property animation in a transition
//for example, int + real; color + real; etc
void tst_qquickanimations::mixedTypes()
{
    //assumes border.width stays a real -- not real robust
    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("mixedtype1.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickItemPrivate::get(rect)->setState("state1");
        QTest::qWait(500);
        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("MyRect");
        QVERIFY(myRect);

        // We cannot get that more exact than that without dependable real-time behavior.
        QVERIFY(myRect->x() > 100 && myRect->x() < 200);
        QVERIFY(myRect->border()->width() > 1 && myRect->border()->width() < 10);
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("mixedtype2.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickItemPrivate::get(rect)->setState("state1");
        QTest::qWait(500);
        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("MyRect");
        QVERIFY(myRect);

        // We cannot get that more exact than that without dependable real-time behavior.
        QVERIFY(myRect->x() > 100 && myRect->x() < 200);
        QVERIFY(myRect->color() != QColor("red") && myRect->color() != QColor("blue"));
    }
}

void tst_qquickanimations::properties()
{
    const int waitDuration = 300;
    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("properties.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QTest::qWait(waitDuration);
        QTIMED_COMPARE(myRect->x(),qreal(200));
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("properties2.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QTest::qWait(waitDuration);
        QTIMED_COMPARE(myRect->x(),qreal(200));
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("properties3.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QTest::qWait(waitDuration);
        QTIMED_COMPARE(myRect->x(),qreal(300));
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("properties4.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QTest::qWait(waitDuration);
        QTIMED_COMPARE(myRect->y(),qreal(200));
        QTIMED_COMPARE(myRect->x(),qreal(100));
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("properties5.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QTest::qWait(waitDuration);
        QTIMED_COMPARE(myRect->x(),qreal(100));
        QTIMED_COMPARE(myRect->y(),qreal(200));
    }
}

void tst_qquickanimations::propertiesTransition()
{
    const int waitDuration = 300;
    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("propertiesTransition.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickItemPrivate::get(rect)->setState("moved");
        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QTest::qWait(waitDuration);
        QTIMED_COMPARE(myRect->x(),qreal(200));
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("propertiesTransition2.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QQuickItemPrivate::get(rect)->setState("moved");
        QCOMPARE(myRect->x(),qreal(200));
        QCOMPARE(myRect->y(),qreal(100));
        QTest::qWait(waitDuration);
        QTIMED_COMPARE(myRect->y(),qreal(200));
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("propertiesTransition3.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QQuickItemPrivate::get(rect)->setState("moved");
        QCOMPARE(myRect->x(),qreal(200));
        QCOMPARE(myRect->y(),qreal(100));
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("propertiesTransition4.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QQuickItemPrivate::get(rect)->setState("moved");
        QCOMPARE(myRect->x(),qreal(100));
        QTest::qWait(waitDuration);
        QTIMED_COMPARE(myRect->x(),qreal(200));
    }

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("propertiesTransition5.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QQuickItemPrivate::get(rect)->setState("moved");
        QCOMPARE(myRect->x(),qreal(100));
        QTest::qWait(waitDuration);
        QTIMED_COMPARE(myRect->x(),qreal(200));
    }

    /*{
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("propertiesTransition6.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QQuickItemPrivate::get(rect)->setState("moved");
        QCOMPARE(myRect->x(),qreal(100));
        QTest::qWait(waitDuration);
        QTIMED_COMPARE(myRect->x(),qreal(100));
    }*/

    {
        QQmlEngine engine;
        QQmlComponent c(&engine, testFileUrl("propertiesTransition7.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickItemPrivate::get(rect)->setState("moved");
        QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
        QVERIFY(myRect);
        QTest::qWait(waitDuration);
        QTIMED_COMPARE(myRect->x(),qreal(200));
    }

}

void tst_qquickanimations::pathTransition()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("pathTransition.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);

    QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("redRect");
    QVERIFY(myRect);

    QQuickItemPrivate::get(rect)->setState("moved");
    QTRY_VERIFY(myRect->x() < 500 && myRect->x() > 100 && myRect->y() > 50 && myRect->y() < 700 );  //animation started
    QTRY_VERIFY(qFuzzyCompare(myRect->x(), qreal(100)) && qFuzzyCompare(myRect->y(), qreal(700)));
    QTest::qWait(100);

    QQuickItemPrivate::get(rect)->setState("");
    QTRY_VERIFY(myRect->x() < 500 && myRect->x() > 100 && myRect->y() > 50 && myRect->y() < 700 );  //animation started
    QTRY_VERIFY(qFuzzyCompare(myRect->x(), qreal(500)) && qFuzzyCompare(myRect->y(), qreal(50)));
}

void tst_qquickanimations::disabledTransition()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("disabledTransition.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);

    QQuickRectangle *myRect = rect->findChild<QQuickRectangle*>("TheRect");
    QVERIFY(myRect);

    QQuickTransition *trans = rect->findChild<QQuickTransition*>();
    QVERIFY(trans);

    QCOMPARE(trans->enabled(), false);

    QQuickItemPrivate::get(rect)->setState("moved");
    QCOMPARE(myRect->x(),qreal(200));

    trans->setEnabled(true);
    QSignalSpy runningSpy(trans, SIGNAL(runningChanged()));
    QQuickItemPrivate::get(rect)->setState("");
    QCOMPARE(myRect->x(),qreal(200));
    QCOMPARE(runningSpy.count(), 1); //stopped -> running
    QVERIFY(trans->running());
    QTest::qWait(300);
    QTIMED_COMPARE(myRect->x(),qreal(100));
    QVERIFY(!trans->running());
    QCOMPARE(runningSpy.count(), 2); //running -> stopped
}

void tst_qquickanimations::invalidDuration()
{
    QQuickPropertyAnimation *animation = new QQuickPropertyAnimation;
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML PropertyAnimation: Cannot set a duration of < 0");
    animation->setDuration(-1);
    QCOMPARE(animation->duration(), 250);

    QQuickPauseAnimation *pauseAnimation = new QQuickPauseAnimation;
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML PauseAnimation: Cannot set a duration of < 0");
    pauseAnimation->setDuration(-1);
    QCOMPARE(pauseAnimation->duration(), 250);
}

void tst_qquickanimations::attached()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("attached.qml"));
    QTest::ignoreMessage(QtDebugMsg, "off");
    QTest::ignoreMessage(QtDebugMsg, "on");
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);
}

void tst_qquickanimations::propertyValueSourceDefaultStart()
{
    {
        QQmlEngine engine;

        QQmlComponent c(&engine, testFileUrl("valuesource.qml"));

        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickAbstractAnimation *myAnim = rect->findChild<QQuickAbstractAnimation*>("MyAnim");
        QVERIFY(myAnim);
        QVERIFY(myAnim->isRunning());
    }

    {
        QQmlEngine engine;

        QQmlComponent c(&engine, testFileUrl("valuesource2.qml"));

        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickAbstractAnimation *myAnim = rect->findChild<QQuickAbstractAnimation*>("MyAnim");
        QVERIFY(myAnim);
        QVERIFY(!myAnim->isRunning());
    }

    {
        QQmlEngine engine;

        QQmlComponent c(&engine, testFileUrl("dontAutoStart.qml"));

        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickAbstractAnimation *myAnim = rect->findChild<QQuickAbstractAnimation*>("MyAnim");
        QVERIFY(myAnim && !myAnim->qtAnimation());
        //QCOMPARE(myAnim->qtAnimation()->state(), QAbstractAnimationJob::Stopped);
    }
}


void tst_qquickanimations::dontStart()
{
    {
        QQmlEngine engine;

        QQmlComponent c(&engine, testFileUrl("dontStart.qml"));

        QString warning = c.url().toString() + ":14:13: QML NumberAnimation: setRunning() cannot be used on non-root animation nodes.";
        QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickAbstractAnimation *myAnim = rect->findChild<QQuickAbstractAnimation*>("MyAnim");
        QVERIFY(myAnim && !myAnim->qtAnimation());
        //QCOMPARE(myAnim->qtAnimation()->state(), QAbstractAnimationJob::Stopped);
    }

    {
        QQmlEngine engine;

        QQmlComponent c(&engine, testFileUrl("dontStart2.qml"));

        QString warning = c.url().toString() + ":15:17: QML NumberAnimation: setRunning() cannot be used on non-root animation nodes.";
        QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
        QVERIFY(rect);

        QQuickAbstractAnimation *myAnim = rect->findChild<QQuickAbstractAnimation*>("MyAnim");
        QVERIFY(myAnim && !myAnim->qtAnimation());
        //QCOMPARE(myAnim->qtAnimation()->state(), QAbstractAnimationJob::Stopped);
    }
}

void tst_qquickanimations::easingProperties()
{
    {
        QQmlEngine engine;
        QString componentStr = "import QtQuick 2.0\nNumberAnimation { easing.type: \"InOutQuad\" }";
        QQmlComponent animationComponent(&engine);
        animationComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickPropertyAnimation *animObject = qobject_cast<QQuickPropertyAnimation*>(animationComponent.create());

        QVERIFY(animObject != 0);
        QCOMPARE(animObject->easing().type(), QEasingCurve::InOutQuad);
    }

    {
        QQmlEngine engine;
        QString componentStr = "import QtQuick 2.0\nPropertyAnimation { easing.type: \"OutBounce\"; easing.amplitude: 5.0 }";
        QQmlComponent animationComponent(&engine);
        animationComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickPropertyAnimation *animObject = qobject_cast<QQuickPropertyAnimation*>(animationComponent.create());

        QVERIFY(animObject != 0);
        QCOMPARE(animObject->easing().type(), QEasingCurve::OutBounce);
        QCOMPARE(animObject->easing().amplitude(), 5.0);
    }

    {
        QQmlEngine engine;
        QString componentStr = "import QtQuick 2.0\nPropertyAnimation { easing.type: \"OutElastic\"; easing.amplitude: 5.0; easing.period: 3.0}";
        QQmlComponent animationComponent(&engine);
        animationComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickPropertyAnimation *animObject = qobject_cast<QQuickPropertyAnimation*>(animationComponent.create());

        QVERIFY(animObject != 0);
        QCOMPARE(animObject->easing().type(), QEasingCurve::OutElastic);
        QCOMPARE(animObject->easing().amplitude(), 5.0);
        QCOMPARE(animObject->easing().period(), 3.0);
    }

    {
        QQmlEngine engine;
        QString componentStr = "import QtQuick 2.0\nPropertyAnimation { easing.type: \"InOutBack\"; easing.overshoot: 2 }";
        QQmlComponent animationComponent(&engine);
        animationComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickPropertyAnimation *animObject = qobject_cast<QQuickPropertyAnimation*>(animationComponent.create());

        QVERIFY(animObject != 0);
        QCOMPARE(animObject->easing().type(), QEasingCurve::InOutBack);
        QCOMPARE(animObject->easing().overshoot(), 2.0);
    }

    {
        QQmlEngine engine;
        QString componentStr = "import QtQuick 2.0\nPropertyAnimation { easing.type: \"Bezier\"; easing.bezierCurve: [0.5, 0.2, 0.13, 0.65, 1.0, 1.0] }";
        QQmlComponent animationComponent(&engine);
        animationComponent.setData(componentStr.toLatin1(), QUrl::fromLocalFile(""));
        QQuickPropertyAnimation *animObject = qobject_cast<QQuickPropertyAnimation*>(animationComponent.create());

        QVERIFY(animObject != 0);
        QCOMPARE(animObject->easing().type(), QEasingCurve::BezierSpline);
        QList<QPointF> points = animObject->easing().cubicBezierSpline();
        QCOMPARE(points.count(), 3);
        QCOMPARE(points.at(0), QPointF(0.5, 0.2));
        QCOMPARE(points.at(1), QPointF(0.13, 0.65));
        QCOMPARE(points.at(2), QPointF(1.0, 1.0));
    }
}

void tst_qquickanimations::rotation()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("rotation.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);

    QQuickRectangle *rr = rect->findChild<QQuickRectangle*>("rr");
    QQuickRectangle *rr2 = rect->findChild<QQuickRectangle*>("rr2");
    QQuickRectangle *rr3 = rect->findChild<QQuickRectangle*>("rr3");
    QQuickRectangle *rr4 = rect->findChild<QQuickRectangle*>("rr4");

    QQuickItemPrivate::get(rect)->setState("state1");
    QTest::qWait(800);
    qreal r1 = rr->rotation();
    qreal r2 = rr2->rotation();
    qreal r3 = rr3->rotation();
    qreal r4 = rr4->rotation();

    QVERIFY(r1 > qreal(0) && r1 < qreal(370));
    QVERIFY(r2 > qreal(0) && r2 < qreal(370));
    QVERIFY(r3 < qreal(0) && r3 > qreal(-350));
    QVERIFY(r4 > qreal(0) && r4 < qreal(10));
    QCOMPARE(r1,r2);
    QVERIFY(r4 < r2);

    QTest::qWait(800);
    QTIMED_COMPARE(rr->rotation() + rr2->rotation() + rr3->rotation() + rr4->rotation(), qreal(370*4));
}

void tst_qquickanimations::startStopSignals()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("signals.qml"));
    QQuickItem *root = qobject_cast<QQuickItem*>(c.create());
    QVERIFY(root);

    QCOMPARE(root->property("startedCount").toInt(), 1);    //autostart
    QCOMPARE(root->property("stoppedCount").toInt(), 0);

    QMetaObject::invokeMethod(root, "stop");

    QCOMPARE(root->property("startedCount").toInt(), 1);
    QCOMPARE(root->property("stoppedCount").toInt(), 1);

    QElapsedTimer timer;
    timer.start();
    QMetaObject::invokeMethod(root, "start");

    QCOMPARE(root->property("startedCount").toInt(), 2);
    QCOMPARE(root->property("stoppedCount").toInt(), 1);

    QTRY_COMPARE(root->property("stoppedCount").toInt(), 2);
    QCOMPARE(root->property("startedCount").toInt(), 2);
    QVERIFY(timer.elapsed() >= 200);

    root->setProperty("alwaysRunToEnd", true);

    timer.restart();
    QMetaObject::invokeMethod(root, "start");

    QCOMPARE(root->property("startedCount").toInt(), 3);
    QCOMPARE(root->property("stoppedCount").toInt(), 2);

    QMetaObject::invokeMethod(root, "stop");

    QCOMPARE(root->property("startedCount").toInt(), 3);
    QCOMPARE(root->property("stoppedCount").toInt(), 2);

    QTRY_COMPARE(root->property("stoppedCount").toInt(), 3);
    QCOMPARE(root->property("startedCount").toInt(), 3);
    QVERIFY(timer.elapsed() >= 200);
}

void tst_qquickanimations::runningTrueBug()
{
    //ensure we start correctly when "running: true" is explicitly set
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("runningTrueBug.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect);

    QQuickRectangle *cloud = rect->findChild<QQuickRectangle*>("cloud");
    QVERIFY(cloud);
    QTest::qWait(1000);
    QVERIFY(cloud->x() > qreal(0));
}

//QTBUG-24308
void tst_qquickanimations::pathAnimationInOutBackBug()
{
    //ensure we don't pass bad progress value (out of [0,1]) to  QQuickPath::backwardsPointAt()
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("pathAnimationInOutBackCrash.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(c.create());
    QVERIFY(item);

    QQuickRectangle *rect = item->findChild<QQuickRectangle *>("rect");
    QVERIFY(rect);
    QTest::qWait(1000);
    QCOMPARE(rect->x(), qreal(0));
    QCOMPARE(rect->y(), qreal(0));
}

//QTBUG-12805
void tst_qquickanimations::nonTransitionBug()
{
    //tests that the animation values from the previous transition are properly cleared
    //in the case where an animation in the transition doesn't match anything (but previously did)
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("nonTransitionBug.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    QQuickRectangle *mover = rect->findChild<QQuickRectangle*>("mover");

    mover->setX(100);
    QCOMPARE(mover->x(), qreal(100));

    rectPrivate->setState("left");
    QTRY_COMPARE(mover->x(), qreal(0));

    mover->setX(100);
    QCOMPARE(mover->x(), qreal(100));

    //make sure we don't try to animate back to 0
    rectPrivate->setState("free");
    QTest::qWait(300);
    QCOMPARE(mover->x(), qreal(100));
}

//QTBUG-14042
void tst_qquickanimations::registrationBug()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("registrationBug.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);
    QTRY_COMPARE(rect->property("value"), QVariant(int(100)));
}

void tst_qquickanimations::doubleRegistrationBug()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("doubleRegistrationBug.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QQuickAbstractAnimation *anim = rect->findChild<QQuickAbstractAnimation*>("animation");
    QVERIFY(anim != 0);
    QTRY_COMPARE(anim->qtAnimation()->state(), QAbstractAnimationJob::Stopped);
}

//QTBUG-16736
void tst_qquickanimations::alwaysRunToEndRestartBug()
{
    QQuickRectangle rect;
    QQuickPropertyAnimation animation;
    animation.setTargetObject(&rect);
    animation.setProperty("x");
    animation.setTo(200);
    animation.setDuration(1000);
    animation.setLoops(-1);
    animation.setAlwaysRunToEnd(true);
    QCOMPARE(animation.loops(), -1);
    QVERIFY(animation.alwaysRunToEnd());
    animation.start();
    animation.stop();
    animation.start();
    animation.stop();

    // Waiting for a fixed time here would be dangerous as the starting and stopping itself takes
    // time and clocks are unreliable. The only thing we do know is that the animation should
    // eventually start and eventually stop. As its duration is 1000ms we can be pretty sure to hit
    // an in between state with the 50ms iterations QTRY_VERIFY does.
    QTRY_VERIFY(rect.x() != qreal(200));
    QTRY_COMPARE(rect.x(), qreal(200));
    QCOMPARE(static_cast<QQuickAbstractAnimation*>(&animation)->qtAnimation()->state(), QAbstractAnimationJob::Stopped);
}

//QTBUG-20227
void tst_qquickanimations::transitionAssignmentBug()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("transitionAssignmentBug.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QCOMPARE(rect->property("nullObject").toBool(), false);
}

//QTBUG-19080
void tst_qquickanimations::pauseBindingBug()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("pauseBindingBug.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);
    QQuickAbstractAnimation *anim = rect->findChild<QQuickAbstractAnimation*>("animation");
    QCOMPARE(anim->qtAnimation()->state(), QAbstractAnimationJob::Paused);

    delete rect;
}

//QTBUG-13598
void tst_qquickanimations::pauseBug()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("pauseBug.qml"));
    QQuickAbstractAnimation *anim = qobject_cast<QQuickAbstractAnimation*>(c.create());
    QVERIFY(anim != 0);
    QCOMPARE(anim->qtAnimation()->state(), QAbstractAnimationJob::Paused);
    QCOMPARE(anim->isPaused(), true);
    QCOMPARE(anim->isRunning(), true);

    delete anim;
}

//QTBUG-23092
void tst_qquickanimations::loopingBug()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("looping.qml"));
    QObject *obj = c.create();

    QQuickAbstractAnimation *anim = obj->findChild<QQuickAbstractAnimation*>();
    QVERIFY(anim != 0);
    QCOMPARE(anim->qtAnimation()->totalDuration(), 300);
    QCOMPARE(anim->isRunning(), true);
    QTRY_COMPARE(static_cast<QAnimationGroupJob*>(anim->qtAnimation())->firstChild()->currentLoop(), 2);
    QTRY_COMPARE(anim->isRunning(), false);

    QQuickRectangle *rect = obj->findChild<QQuickRectangle*>();
    QVERIFY(rect != 0);
    QCOMPARE(rect->rotation(), qreal(90));

    delete obj;
}

//QTBUG-24532
void tst_qquickanimations::anchorBug()
{
    QQuickAnchorAnimation animation;
    animation.setDuration(5000);
    animation.setEasing(QEasingCurve(QEasingCurve::InOutBack));
    animation.start();
    animation.pause();

    QCOMPARE(animation.qtAnimation()->duration(), 5000);
    QCOMPARE(static_cast<QQuickBulkValueAnimator*>(animation.qtAnimation())->easingCurve(), QEasingCurve(QEasingCurve::InOutBack));
}

//ScriptAction should not match a StateChangeScript if no scriptName has been specified
void tst_qquickanimations::scriptActionBug()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("scriptActionBug.qml"));
    QObject *obj = c.create();

    //Both the ScriptAction and StateChangeScript should be triggered
    QCOMPARE(obj->property("actionTriggered").toBool(), true);
    QCOMPARE(obj->property("actionTriggered").toBool(), true);
}

//QTBUG-34851
void tst_qquickanimations::groupAnimationNullChildBug()
{
    {
        QQmlEngine engine;

        QQmlComponent c(&engine, testFileUrl("sequentialAnimationNullChildBug.qml"));
        QQuickItem *root = qobject_cast<QQuickItem*>(c.create());
        QVERIFY(root);

        delete root;
    }

    {
        QQmlEngine engine;

        QQmlComponent c(&engine, testFileUrl("parallelAnimationNullChildBug.qml"));
        QQuickItem *root = qobject_cast<QQuickItem*>(c.create());
        QVERIFY(root);

        delete root;
    }
}

//ScriptAction should not crash if changing a state in a transition
void tst_qquickanimations::scriptActionCrash()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("scriptActionCrash.qml"));
    QObject *obj = c.create();

    //just testing that we don't crash
    QTest::qWait(1000); //5x transition duration

    delete obj;
}

// QTBUG-49364
// Test that we don't crash when the target of an Animator becomes
// invalid between the time the animator is started and the time the
// animator job is actually started
void tst_qquickanimations::animatorInvalidTargetCrash()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("animatorInvalidTargetCrash.qml"));
    QObject *obj = c.create();

    //just testing that we don't crash
    QTest::qWait(5000); //animator duration

    delete obj;
}

Q_DECLARE_METATYPE(QList<QQmlError>)

// QTBUG-22141
void tst_qquickanimations::defaultPropertyWarning()
{
    QQmlEngine engine;

    qRegisterMetaType<QList<QQmlError> >();

    QSignalSpy warnings(&engine, SIGNAL(warnings(QList<QQmlError>)));
    QVERIFY(warnings.isValid());

    QQmlComponent component(&engine, testFileUrl("defaultRotationAnimation.qml"));
    QScopedPointer<QQuickItem> root(qobject_cast<QQuickItem*>(component.create()));
    QVERIFY(root);

    QVERIFY(warnings.isEmpty());
}

QTEST_MAIN(tst_qquickanimations)

#include "tst_qquickanimations.moc"
