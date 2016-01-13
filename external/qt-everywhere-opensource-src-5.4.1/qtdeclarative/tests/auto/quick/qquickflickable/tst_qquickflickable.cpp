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
#include <qtest.h>
#include <QtTest/QSignalSpy>
#include <QtGui/QStyleHints>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/qquickview.h>
#include <private/qquickflickable_p.h>
#include <private/qquickflickable_p_p.h>
#include <private/qquicktransition_p.h>
#include <private/qqmlvaluetype_p.h>
#include <math.h>
#include "../../shared/util.h"
#include "../shared/viewtestutil.h"
#include "../shared/visualtestutil.h"

#include <qpa/qwindowsysteminterface.h>

using namespace QQuickViewTestUtil;
using namespace QQuickVisualTestUtil;

class tst_qquickflickable : public QQmlDataTest
{
    Q_OBJECT
public:

private slots:
    void create();
    void horizontalViewportSize();
    void verticalViewportSize();
    void visibleAreaRatiosUpdate();
    void properties();
    void boundsBehavior();
    void rebound();
    void maximumFlickVelocity();
    void flickDeceleration();
    void pressDelay();
    void nestedPressDelay();
    void nestedClickThenFlick();
    void flickableDirection();
    void resizeContent();
    void returnToBounds();
    void returnToBounds_data();
    void wheel();
    void movingAndFlicking();
    void movingAndFlicking_data();
    void movingAndDragging();
    void movingAndDragging_data();
    void flickOnRelease();
    void pressWhileFlicking();
    void disabled();
    void flickVelocity();
    void margins();
    void cancelOnMouseGrab();
    void clickAndDragWhenTransformed();
    void flickTwiceUsingTouches();
    void nestedStopAtBounds();
    void nestedStopAtBounds_data();
    void stopAtBounds();
    void stopAtBounds_data();
    void nestedMouseAreaUsingTouch();
    void pressDelayWithLoader();
    void cleanup();

private:
    void flickWithTouch(QQuickWindow *window, QTouchDevice *touchDevice, const QPoint &from, const QPoint &to);
};

void tst_qquickflickable::cleanup()
{
    QVERIFY(QGuiApplication::topLevelWindows().isEmpty());
}

void tst_qquickflickable::create()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("flickable01.qml"));
    QQuickFlickable *obj = qobject_cast<QQuickFlickable*>(c.create());

    QVERIFY(obj != 0);
    QCOMPARE(obj->isAtXBeginning(), true);
    QCOMPARE(obj->isAtXEnd(), false);
    QCOMPARE(obj->isAtYBeginning(), true);
    QCOMPARE(obj->isAtYEnd(), false);
    QCOMPARE(obj->contentX(), 0.);
    QCOMPARE(obj->contentY(), 0.);

    QCOMPARE(obj->horizontalVelocity(), 0.);
    QCOMPARE(obj->verticalVelocity(), 0.);

    QCOMPARE(obj->isInteractive(), true);
    QCOMPARE(obj->boundsBehavior(), QQuickFlickable::DragAndOvershootBounds);
    QCOMPARE(obj->pressDelay(), 0);
    QCOMPARE(obj->maximumFlickVelocity(), 2500.);

    delete obj;
}

void tst_qquickflickable::horizontalViewportSize()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("flickable02.qml"));
    QQuickFlickable *obj = qobject_cast<QQuickFlickable*>(c.create());

    QVERIFY(obj != 0);
    QCOMPARE(obj->contentWidth(), 800.);
    QCOMPARE(obj->contentHeight(), 300.);
    QCOMPARE(obj->isAtXBeginning(), true);
    QCOMPARE(obj->isAtXEnd(), false);
    QCOMPARE(obj->isAtYBeginning(), true);
    QCOMPARE(obj->isAtYEnd(), false);

    delete obj;
}

void tst_qquickflickable::verticalViewportSize()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("flickable03.qml"));
    QQuickFlickable *obj = qobject_cast<QQuickFlickable*>(c.create());

    QVERIFY(obj != 0);
    QCOMPARE(obj->contentWidth(), 200.);
    QCOMPARE(obj->contentHeight(), 6000.);
    QCOMPARE(obj->isAtXBeginning(), true);
    QCOMPARE(obj->isAtXEnd(), false);
    QCOMPARE(obj->isAtYBeginning(), true);
    QCOMPARE(obj->isAtYEnd(), false);

    delete obj;
}

void tst_qquickflickable::visibleAreaRatiosUpdate()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("ratios.qml"));
    QQuickItem *obj = qobject_cast<QQuickItem*>(c.create());

    QVERIFY(obj != 0);
    // check initial ratio values
    QCOMPARE(obj->property("heightRatioIs").toDouble(), obj->property("heightRatioShould").toDouble());
    QCOMPARE(obj->property("widthRatioIs").toDouble(), obj->property("widthRatioShould").toDouble());
    // change flickable geometry so that flicking is enabled (content size > flickable size)
    obj->setProperty("forceNoFlicking", false);
    QCOMPARE(obj->property("heightRatioIs").toDouble(), obj->property("heightRatioShould").toDouble());
    QCOMPARE(obj->property("widthRatioIs").toDouble(), obj->property("widthRatioShould").toDouble());
    // change flickable geometry so that flicking is disabled (content size == flickable size)
    obj->setProperty("forceNoFlicking", true);
    QCOMPARE(obj->property("heightRatioIs").toDouble(), obj->property("heightRatioShould").toDouble());
    QCOMPARE(obj->property("widthRatioIs").toDouble(), obj->property("widthRatioShould").toDouble());

    delete obj;
}

void tst_qquickflickable::properties()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("flickable04.qml"));
    QQuickFlickable *obj = qobject_cast<QQuickFlickable*>(c.create());

    QVERIFY(obj != 0);
    QCOMPARE(obj->isInteractive(), false);
    QCOMPARE(obj->boundsBehavior(), QQuickFlickable::StopAtBounds);
    QCOMPARE(obj->pressDelay(), 200);
    QCOMPARE(obj->maximumFlickVelocity(), 2000.);

    QVERIFY(obj->property("ok").toBool() == false);
    QMetaObject::invokeMethod(obj, "check");
    QVERIFY(obj->property("ok").toBool() == true);

    delete obj;
}

void tst_qquickflickable::boundsBehavior()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; Flickable { boundsBehavior: Flickable.StopAtBounds }", QUrl::fromLocalFile(""));
    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(component.create());
    QSignalSpy spy(flickable, SIGNAL(boundsBehaviorChanged()));

    QVERIFY(flickable);
    QVERIFY(flickable->boundsBehavior() == QQuickFlickable::StopAtBounds);

    flickable->setBoundsBehavior(QQuickFlickable::DragAndOvershootBounds);
    QVERIFY(flickable->boundsBehavior() == QQuickFlickable::DragAndOvershootBounds);
    QCOMPARE(spy.count(),1);
    flickable->setBoundsBehavior(QQuickFlickable::DragAndOvershootBounds);
    QCOMPARE(spy.count(),1);

    flickable->setBoundsBehavior(QQuickFlickable::DragOverBounds);
    QVERIFY(flickable->boundsBehavior() == QQuickFlickable::DragOverBounds);
    QCOMPARE(spy.count(),2);
    flickable->setBoundsBehavior(QQuickFlickable::DragOverBounds);
    QCOMPARE(spy.count(),2);

    flickable->setBoundsBehavior(QQuickFlickable::StopAtBounds);
    QVERIFY(flickable->boundsBehavior() == QQuickFlickable::StopAtBounds);
    QCOMPARE(spy.count(),3);
    flickable->setBoundsBehavior(QQuickFlickable::StopAtBounds);
    QCOMPARE(spy.count(),3);

    delete flickable;
}

void tst_qquickflickable::rebound()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("rebound.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(flickable != 0);

    QQuickTransition *rebound = window->rootObject()->findChild<QQuickTransition*>("rebound");
    QVERIFY(rebound);
    QSignalSpy reboundSpy(rebound, SIGNAL(runningChanged()));

    QSignalSpy movementStartedSpy(flickable, SIGNAL(movementStarted()));
    QSignalSpy movementEndedSpy(flickable, SIGNAL(movementEnded()));
    QSignalSpy vMoveSpy(flickable, SIGNAL(movingVerticallyChanged()));
    QSignalSpy hMoveSpy(flickable, SIGNAL(movingHorizontallyChanged()));

    // flick and test the transition is run
    flick(window.data(), QPoint(20,20), QPoint(120,120), 200);

    QTRY_COMPARE(window->rootObject()->property("transitionsStarted").toInt(), 2);
    QCOMPARE(hMoveSpy.count(), 1);
    QCOMPARE(vMoveSpy.count(), 1);
    QCOMPARE(movementStartedSpy.count(), 1);
    QCOMPARE(movementEndedSpy.count(), 0);
    QVERIFY(rebound->running());

    QTRY_VERIFY(!flickable->isMoving());
    QCOMPARE(flickable->contentX(), 0.0);
    QCOMPARE(flickable->contentY(), 0.0);

    QCOMPARE(hMoveSpy.count(), 2);
    QCOMPARE(vMoveSpy.count(), 2);
    QCOMPARE(movementStartedSpy.count(), 1);
    QCOMPARE(movementEndedSpy.count(), 1);
    QCOMPARE(window->rootObject()->property("transitionsStarted").toInt(), 2);
    QVERIFY(!rebound->running());
    QCOMPARE(reboundSpy.count(), 2);

    hMoveSpy.clear();
    vMoveSpy.clear();
    movementStartedSpy.clear();
    movementEndedSpy.clear();
    window->rootObject()->setProperty("transitionsStarted", 0);
    window->rootObject()->setProperty("transitionsFinished", 0);

    // flick and trigger the transition multiple times
    // (moving signals are emitted as soon as the first transition starts)
    flick(window.data(), QPoint(20,20), QPoint(120,120), 200);     // both x and y will bounce back
    flick(window.data(), QPoint(20,120), QPoint(120,20), 200);     // only x will bounce back

    QVERIFY(flickable->isMoving());
    QVERIFY(window->rootObject()->property("transitionsStarted").toInt() >= 1);
    QCOMPARE(hMoveSpy.count(), 1);
    QCOMPARE(vMoveSpy.count(), 1);
    QCOMPARE(movementStartedSpy.count(), 1);

    QTRY_VERIFY(!flickable->isMoving());
    QCOMPARE(flickable->contentX(), 0.0);

    // moving started/stopped signals should only have been emitted once,
    // and when they are, all transitions should have finished
    QCOMPARE(hMoveSpy.count(), 2);
    QCOMPARE(vMoveSpy.count(), 2);
    QCOMPARE(movementStartedSpy.count(), 1);
    QCOMPARE(movementEndedSpy.count(), 1);

    hMoveSpy.clear();
    vMoveSpy.clear();
    movementStartedSpy.clear();
    movementEndedSpy.clear();
    window->rootObject()->setProperty("transitionsStarted", 0);
    window->rootObject()->setProperty("transitionsFinished", 0);

    // disable and the default transition should run
    // (i.e. moving but transition->running = false)
    window->rootObject()->setProperty("transitionEnabled", false);

    flick(window.data(), QPoint(20,20), QPoint(120,120), 200);
    QCOMPARE(window->rootObject()->property("transitionsStarted").toInt(), 0);
    QCOMPARE(hMoveSpy.count(), 1);
    QCOMPARE(vMoveSpy.count(), 1);
    QCOMPARE(movementStartedSpy.count(), 1);
    QCOMPARE(movementEndedSpy.count(), 0);

    QTRY_VERIFY(!flickable->isMoving());
    QCOMPARE(hMoveSpy.count(), 2);
    QCOMPARE(vMoveSpy.count(), 2);
    QCOMPARE(movementStartedSpy.count(), 1);
    QCOMPARE(movementEndedSpy.count(), 1);
    QCOMPARE(window->rootObject()->property("transitionsStarted").toInt(), 0);
}

void tst_qquickflickable::maximumFlickVelocity()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; Flickable { maximumFlickVelocity: 1.0; }", QUrl::fromLocalFile(""));
    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(component.create());
    QSignalSpy spy(flickable, SIGNAL(maximumFlickVelocityChanged()));

    QVERIFY(flickable);
    QCOMPARE(flickable->maximumFlickVelocity(), 1.0);

    flickable->setMaximumFlickVelocity(2.0);
    QCOMPARE(flickable->maximumFlickVelocity(), 2.0);
    QCOMPARE(spy.count(),1);
    flickable->setMaximumFlickVelocity(2.0);
    QCOMPARE(spy.count(),1);

    delete flickable;
}

void tst_qquickflickable::flickDeceleration()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; Flickable { flickDeceleration: 1.0; }", QUrl::fromLocalFile(""));
    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(component.create());
    QSignalSpy spy(flickable, SIGNAL(flickDecelerationChanged()));

    QVERIFY(flickable);
    QCOMPARE(flickable->flickDeceleration(), 1.0);

    flickable->setFlickDeceleration(2.0);
    QCOMPARE(flickable->flickDeceleration(), 2.0);
    QCOMPARE(spy.count(),1);
    flickable->setFlickDeceleration(2.0);
    QCOMPARE(spy.count(),1);

    delete flickable;
}

void tst_qquickflickable::pressDelay()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("pressDelay.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window->rootObject());
    QSignalSpy spy(flickable, SIGNAL(pressDelayChanged()));

    QVERIFY(flickable);
    QCOMPARE(flickable->pressDelay(), 100);

    flickable->setPressDelay(200);
    QCOMPARE(flickable->pressDelay(), 200);
    QCOMPARE(spy.count(),1);
    flickable->setPressDelay(200);
    QCOMPARE(spy.count(),1);

    QQuickItem *mouseArea = window->rootObject()->findChild<QQuickItem*>("mouseArea");
    QSignalSpy clickedSpy(mouseArea, SIGNAL(clicked(QQuickMouseEvent*)));

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(150, 150));

    // The press should not occur immediately
    QVERIFY(mouseArea->property("pressed").toBool() == false);

    // But, it should occur eventually
    QTRY_VERIFY(mouseArea->property("pressed").toBool() == true);

    QCOMPARE(clickedSpy.count(),0);

    // On release the clicked signal should be emitted
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(150, 150));
    QCOMPARE(clickedSpy.count(),1);

    // Press and release position should match
    QCOMPARE(flickable->property("pressX").toReal(), flickable->property("releaseX").toReal());
    QCOMPARE(flickable->property("pressY").toReal(), flickable->property("releaseY").toReal());


    // Test a quick tap within the pressDelay timeout
    clickedSpy.clear();
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(180, 180));

    // The press should not occur immediately
    QVERIFY(mouseArea->property("pressed").toBool() == false);

    QCOMPARE(clickedSpy.count(),0);

    // On release the press, release and clicked signal should be emitted
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(180, 180));
    QCOMPARE(clickedSpy.count(),1);

    // Press and release position should match
    QCOMPARE(flickable->property("pressX").toReal(), flickable->property("releaseX").toReal());
    QCOMPARE(flickable->property("pressY").toReal(), flickable->property("releaseY").toReal());


    // QTBUG-31168
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(150, 110));

    // The press should not occur immediately
    QVERIFY(mouseArea->property("pressed").toBool() == false);

    QTest::mouseMove(window.data(), QPoint(150, 190));

    // As we moved pass the drag threshold, we should never receive the press
    QVERIFY(mouseArea->property("pressed").toBool() == false);
    QTRY_VERIFY(mouseArea->property("pressed").toBool() == false);

    // On release the clicked signal should *not* be emitted
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(150, 190));
    QCOMPARE(clickedSpy.count(),1);
}

// QTBUG-17361
void tst_qquickflickable::nestedPressDelay()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("nestedPressDelay.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *outer = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(outer != 0);

    QQuickFlickable *inner = window->rootObject()->findChild<QQuickFlickable*>("innerFlickable");
    QVERIFY(inner != 0);

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(150, 150));
    // the MouseArea is not pressed immediately
    QVERIFY(outer->property("pressed").toBool() == false);
    QVERIFY(inner->property("pressed").toBool() == false);

    // The inner pressDelay will prevail (50ms, vs. 10sec)
    // QTRY_VERIFY() has 5sec timeout, so will timeout well within 10sec.
    QTRY_VERIFY(outer->property("pressed").toBool() == true);

    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(150, 150));

    // Dragging inner Flickable should work
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(80, 150));
    // the MouseArea is not pressed immediately
    QVERIFY(outer->property("pressed").toBool() == false);
    QVERIFY(inner->property("pressed").toBool() == false);

    QTest::mouseMove(window.data(), QPoint(60, 150));
    QTest::mouseMove(window.data(), QPoint(40, 150));
    QTest::mouseMove(window.data(), QPoint(20, 150));

    QVERIFY(inner->property("moving").toBool() == true);
    QVERIFY(outer->property("moving").toBool() == false);

    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(20, 150));

    // Dragging the MouseArea in the inner Flickable should move the inner Flickable
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(150, 150));
    // the MouseArea is not pressed immediately
    QVERIFY(outer->property("pressed").toBool() == false);

    QTest::mouseMove(window.data(), QPoint(130, 150));
    QTest::mouseMove(window.data(), QPoint(110, 150));
    QTest::mouseMove(window.data(), QPoint(90, 150));


    QVERIFY(outer->property("moving").toBool() == false);
    QVERIFY(inner->property("moving").toBool() == true);

    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(90, 150));
}

// QTBUG-37316
void tst_qquickflickable::nestedClickThenFlick()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("nestedClickThenFlick.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *outer = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(outer != 0);

    QQuickFlickable *inner = window->rootObject()->findChild<QQuickFlickable*>("innerFlickable");
    QVERIFY(inner != 0);

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(150, 150));

    // the MouseArea is not pressed immediately
    QVERIFY(outer->property("pressed").toBool() == false);
    QTRY_VERIFY(outer->property("pressed").toBool() == true);

    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(150, 150));

    QVERIFY(outer->property("pressed").toBool() == false);

    // Dragging inner Flickable should work
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(80, 150));
    // the MouseArea is not pressed immediately

    QVERIFY(outer->property("pressed").toBool() == false);

    QTest::mouseMove(window.data(), QPoint(80, 148));
    QTest::mouseMove(window.data(), QPoint(80, 140));
    QTest::mouseMove(window.data(), QPoint(80, 120));
    QTest::mouseMove(window.data(), QPoint(80, 100));

    QVERIFY(outer->property("moving").toBool() == false);
    QVERIFY(inner->property("moving").toBool() == true);

    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(80, 100));
}

void tst_qquickflickable::flickableDirection()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; Flickable { flickableDirection: Flickable.VerticalFlick; }", QUrl::fromLocalFile(""));
    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(component.create());
    QSignalSpy spy(flickable, SIGNAL(flickableDirectionChanged()));

    QVERIFY(flickable);
    QCOMPARE(flickable->flickableDirection(), QQuickFlickable::VerticalFlick);

    flickable->setFlickableDirection(QQuickFlickable::HorizontalAndVerticalFlick);
    QCOMPARE(flickable->flickableDirection(), QQuickFlickable::HorizontalAndVerticalFlick);
    QCOMPARE(spy.count(),1);

    flickable->setFlickableDirection(QQuickFlickable::AutoFlickDirection);
    QCOMPARE(flickable->flickableDirection(), QQuickFlickable::AutoFlickDirection);
    QCOMPARE(spy.count(),2);

    flickable->setFlickableDirection(QQuickFlickable::HorizontalFlick);
    QCOMPARE(flickable->flickableDirection(), QQuickFlickable::HorizontalFlick);
    QCOMPARE(spy.count(),3);

    flickable->setFlickableDirection(QQuickFlickable::HorizontalFlick);
    QCOMPARE(flickable->flickableDirection(), QQuickFlickable::HorizontalFlick);
    QCOMPARE(spy.count(),3);

    delete flickable;
}

// QtQuick 1.1
void tst_qquickflickable::resizeContent()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("setRebound", QVariant::fromValue(false));
    QQmlComponent c(&engine, testFileUrl("resize.qml"));
    QQuickItem *root = qobject_cast<QQuickItem*>(c.create());
    QQuickFlickable *obj = findItem<QQuickFlickable>(root, "flick");

    QVERIFY(obj != 0);
    QCOMPARE(obj->contentX(), 0.);
    QCOMPARE(obj->contentY(), 0.);
    QCOMPARE(obj->contentWidth(), 300.);
    QCOMPARE(obj->contentHeight(), 300.);

    QMetaObject::invokeMethod(root, "resizeContent");

    QCOMPARE(obj->contentX(), 100.);
    QCOMPARE(obj->contentY(), 100.);
    QCOMPARE(obj->contentWidth(), 600.);
    QCOMPARE(obj->contentHeight(), 600.);

    delete root;
}

void tst_qquickflickable::returnToBounds()
{
    QFETCH(bool, setRebound);

    QScopedPointer<QQuickView> window(new QQuickView);

    window->rootContext()->setContextProperty("setRebound", setRebound);
    window->setSource(testFileUrl("resize.qml"));
    QVERIFY(window->rootObject() != 0);
    QQuickFlickable *obj = findItem<QQuickFlickable>(window->rootObject(), "flick");

    QQuickTransition *rebound = window->rootObject()->findChild<QQuickTransition*>("rebound");
    QVERIFY(rebound);
    QSignalSpy reboundSpy(rebound, SIGNAL(runningChanged()));

    QVERIFY(obj != 0);
    QCOMPARE(obj->contentX(), 0.);
    QCOMPARE(obj->contentY(), 0.);
    QCOMPARE(obj->contentWidth(), 300.);
    QCOMPARE(obj->contentHeight(), 300.);

    obj->setContentX(100);
    obj->setContentY(400);
    QTRY_COMPARE(obj->contentX(), 100.);
    QTRY_COMPARE(obj->contentY(), 400.);

    QMetaObject::invokeMethod(window->rootObject(), "returnToBounds");

    if (setRebound)
        QTRY_VERIFY(rebound->running());

    QTRY_COMPARE(obj->contentX(), 0.);
    QTRY_COMPARE(obj->contentY(), 0.);

    QVERIFY(!rebound->running());
    QCOMPARE(reboundSpy.count(), setRebound ? 2 : 0);
}

void tst_qquickflickable::returnToBounds_data()
{
    QTest::addColumn<bool>("setRebound");

    QTest::newRow("with bounds transition") << true;
    QTest::newRow("with bounds transition") << false;
}

void tst_qquickflickable::wheel()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("wheel.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flick = window->rootObject()->findChild<QQuickFlickable*>("flick");
    QVERIFY(flick != 0);

    {
        QPoint pos(200, 200);
        QWheelEvent event(pos, window->mapToGlobal(pos), QPoint(), QPoint(0,-120), -120, Qt::Vertical, Qt::NoButton, Qt::NoModifier);
        event.setAccepted(false);
        QGuiApplication::sendEvent(window.data(), &event);
    }

    QTRY_VERIFY(flick->contentY() > 0);
    QVERIFY(flick->contentX() == 0);

    flick->setContentY(0);
    QVERIFY(flick->contentY() == 0);

    {
        QPoint pos(200, 200);
        QWheelEvent event(pos, window->mapToGlobal(pos), QPoint(), QPoint(-120,0), -120, Qt::Horizontal, Qt::NoButton, Qt::NoModifier);

        event.setAccepted(false);
        QGuiApplication::sendEvent(window.data(), &event);
    }

    QTRY_VERIFY(flick->contentX() > 0);
    QVERIFY(flick->contentY() == 0);
}

void tst_qquickflickable::movingAndFlicking_data()
{
    QTest::addColumn<bool>("verticalEnabled");
    QTest::addColumn<bool>("horizontalEnabled");
    QTest::addColumn<QPoint>("flickToWithoutSnapBack");
    QTest::addColumn<QPoint>("flickToWithSnapBack");

    QTest::newRow("vertical")
            << true << false
            << QPoint(50, 100)
            << QPoint(50, 300);

    QTest::newRow("horizontal")
            << false << true
            << QPoint(-50, 200)
            << QPoint(150, 200);

    QTest::newRow("both")
            << true << true
            << QPoint(-50, 100)
            << QPoint(150, 300);
}

void tst_qquickflickable::movingAndFlicking()
{
    QFETCH(bool, verticalEnabled);
    QFETCH(bool, horizontalEnabled);
    QFETCH(QPoint, flickToWithoutSnapBack);
    QFETCH(QPoint, flickToWithSnapBack);

    const QPoint flickFrom(50, 200);   // centre

    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("flickable03.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(flickable != 0);

    QSignalSpy vMoveSpy(flickable, SIGNAL(movingVerticallyChanged()));
    QSignalSpy hMoveSpy(flickable, SIGNAL(movingHorizontallyChanged()));
    QSignalSpy moveSpy(flickable, SIGNAL(movingChanged()));
    QSignalSpy vFlickSpy(flickable, SIGNAL(flickingVerticallyChanged()));
    QSignalSpy hFlickSpy(flickable, SIGNAL(flickingHorizontallyChanged()));
    QSignalSpy flickSpy(flickable, SIGNAL(flickingChanged()));

    QSignalSpy moveStartSpy(flickable, SIGNAL(movementStarted()));
    QSignalSpy moveEndSpy(flickable, SIGNAL(movementEnded()));
    QSignalSpy flickStartSpy(flickable, SIGNAL(flickStarted()));
    QSignalSpy flickEndSpy(flickable, SIGNAL(flickEnded()));

    // do a flick that keeps the view within the bounds
    flick(window.data(), flickFrom, flickToWithoutSnapBack, 200);

    QTRY_VERIFY(flickable->isMoving());
    QCOMPARE(flickable->isMovingHorizontally(), horizontalEnabled);
    QCOMPARE(flickable->isMovingVertically(), verticalEnabled);
    QVERIFY(flickable->isFlicking());
    QCOMPARE(flickable->isFlickingHorizontally(), horizontalEnabled);
    QCOMPARE(flickable->isFlickingVertically(), verticalEnabled);
    // contentX/contentY are either unchanged, or moving is true when the value changed.
    QCOMPARE(flickable->property("movingInContentX").value<bool>(), true);
    QCOMPARE(flickable->property("movingInContentY").value<bool>(), true);

    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(vMoveSpy.count(), verticalEnabled ? 1 : 0);
    QCOMPARE(hMoveSpy.count(), horizontalEnabled ? 1 : 0);
    QCOMPARE(flickSpy.count(), 1);
    QCOMPARE(vFlickSpy.count(), verticalEnabled ? 1 : 0);
    QCOMPARE(hFlickSpy.count(), horizontalEnabled ? 1 : 0);

    QCOMPARE(moveStartSpy.count(), 1);
    QCOMPARE(flickStartSpy.count(), 1);

    // wait for any motion to end
    QTRY_VERIFY(!flickable->isMoving());

    QVERIFY(!flickable->isMovingHorizontally());
    QVERIFY(!flickable->isMovingVertically());
    QVERIFY(!flickable->isFlicking());
    QVERIFY(!flickable->isFlickingHorizontally());
    QVERIFY(!flickable->isFlickingVertically());

    QCOMPARE(moveSpy.count(), 2);
    QCOMPARE(vMoveSpy.count(), verticalEnabled ? 2 : 0);
    QCOMPARE(hMoveSpy.count(), horizontalEnabled ? 2 : 0);
    QCOMPARE(flickSpy.count(), 2);
    QCOMPARE(vFlickSpy.count(), verticalEnabled ? 2 : 0);
    QCOMPARE(hFlickSpy.count(), horizontalEnabled ? 2 : 0);

    QCOMPARE(moveStartSpy.count(), 1);
    QCOMPARE(moveEndSpy.count(), 1);
    QCOMPARE(flickStartSpy.count(), 1);
    QCOMPARE(flickEndSpy.count(), 1);

    // Stop on a full pixel after user interaction
    if (verticalEnabled)
        QCOMPARE(flickable->contentY(), (qreal)qRound(flickable->contentY()));
    if (horizontalEnabled)
        QCOMPARE(flickable->contentX(), (qreal)qRound(flickable->contentX()));

    // clear for next flick
    vMoveSpy.clear(); hMoveSpy.clear(); moveSpy.clear();
    vFlickSpy.clear(); hFlickSpy.clear(); flickSpy.clear();
    moveStartSpy.clear(); moveEndSpy.clear();
    flickStartSpy.clear(); flickEndSpy.clear();

    // do a flick that flicks the view out of bounds
    flickable->setContentX(0);
    flickable->setContentY(0);
    QTRY_VERIFY(!flickable->isMoving());
    flick(window.data(), flickFrom, flickToWithSnapBack, 10);

    QTRY_VERIFY(flickable->isMoving());
    QCOMPARE(flickable->isMovingHorizontally(), horizontalEnabled);
    QCOMPARE(flickable->isMovingVertically(), verticalEnabled);
    QVERIFY(flickable->isFlicking());
    QCOMPARE(flickable->isFlickingHorizontally(), horizontalEnabled);
    QCOMPARE(flickable->isFlickingVertically(), verticalEnabled);

    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(vMoveSpy.count(), verticalEnabled ? 1 : 0);
    QCOMPARE(hMoveSpy.count(), horizontalEnabled ? 1 : 0);
    QCOMPARE(flickSpy.count(), 1);
    QCOMPARE(vFlickSpy.count(), verticalEnabled ? 1 : 0);
    QCOMPARE(hFlickSpy.count(), horizontalEnabled ? 1 : 0);

    QCOMPARE(moveStartSpy.count(), 1);
    QCOMPARE(moveEndSpy.count(), 0);
    QCOMPARE(flickStartSpy.count(), 1);
    QCOMPARE(flickEndSpy.count(), 0);

    // wait for any motion to end
    QTRY_VERIFY(!flickable->isMoving());

    QVERIFY(!flickable->isMovingHorizontally());
    QVERIFY(!flickable->isMovingVertically());
    QVERIFY(!flickable->isFlicking());
    QVERIFY(!flickable->isFlickingHorizontally());
    QVERIFY(!flickable->isFlickingVertically());

    QCOMPARE(moveSpy.count(), 2);
    QCOMPARE(vMoveSpy.count(), verticalEnabled ? 2 : 0);
    QCOMPARE(hMoveSpy.count(), horizontalEnabled ? 2 : 0);
    QCOMPARE(flickSpy.count(), 2);
    QCOMPARE(vFlickSpy.count(), verticalEnabled ? 2 : 0);
    QCOMPARE(hFlickSpy.count(), horizontalEnabled ? 2 : 0);

    QCOMPARE(moveStartSpy.count(), 1);
    QCOMPARE(moveEndSpy.count(), 1);
    QCOMPARE(flickStartSpy.count(), 1);
    QCOMPARE(flickEndSpy.count(), 1);

    QCOMPARE(flickable->contentX(), 0.0);
    QCOMPARE(flickable->contentY(), 0.0);
}


void tst_qquickflickable::movingAndDragging_data()
{
    QTest::addColumn<bool>("verticalEnabled");
    QTest::addColumn<bool>("horizontalEnabled");
    QTest::addColumn<QPoint>("moveByWithoutSnapBack");
    QTest::addColumn<QPoint>("moveByWithSnapBack");

    QTest::newRow("vertical")
            << true << false
            << QPoint(0, -10)
            << QPoint(0, 20);

    QTest::newRow("horizontal")
            << false << true
            << QPoint(-10, 0)
            << QPoint(20, 0);

    QTest::newRow("both")
            << true << true
            << QPoint(-10, -10)
            << QPoint(20, 20);
}

void tst_qquickflickable::movingAndDragging()
{
    QFETCH(bool, verticalEnabled);
    QFETCH(bool, horizontalEnabled);
    QFETCH(QPoint, moveByWithoutSnapBack);
    QFETCH(QPoint, moveByWithSnapBack);

    const QPoint moveFrom(50, 200);   // centre

    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("flickable03.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(flickable != 0);

    QSignalSpy vDragSpy(flickable, SIGNAL(draggingVerticallyChanged()));
    QSignalSpy hDragSpy(flickable, SIGNAL(draggingHorizontallyChanged()));
    QSignalSpy dragSpy(flickable, SIGNAL(draggingChanged()));
    QSignalSpy vMoveSpy(flickable, SIGNAL(movingVerticallyChanged()));
    QSignalSpy hMoveSpy(flickable, SIGNAL(movingHorizontallyChanged()));
    QSignalSpy moveSpy(flickable, SIGNAL(movingChanged()));

    QSignalSpy dragStartSpy(flickable, SIGNAL(dragStarted()));
    QSignalSpy dragEndSpy(flickable, SIGNAL(dragEnded()));
    QSignalSpy moveStartSpy(flickable, SIGNAL(movementStarted()));
    QSignalSpy moveEndSpy(flickable, SIGNAL(movementEnded()));

    // start the drag
    QTest::mousePress(window.data(), Qt::LeftButton, 0, moveFrom);
    QTest::mouseMove(window.data(), moveFrom + moveByWithoutSnapBack);
    QTest::mouseMove(window.data(), moveFrom + moveByWithoutSnapBack*2);
    QTest::mouseMove(window.data(), moveFrom + moveByWithoutSnapBack*3);

    QTRY_VERIFY(flickable->isMoving());
    QCOMPARE(flickable->isMovingHorizontally(), horizontalEnabled);
    QCOMPARE(flickable->isMovingVertically(), verticalEnabled);
    QVERIFY(flickable->isDragging());
    QCOMPARE(flickable->isDraggingHorizontally(), horizontalEnabled);
    QCOMPARE(flickable->isDraggingVertically(), verticalEnabled);
    // contentX/contentY are either unchanged, or moving and dragging are true when the value changes.
    QCOMPARE(flickable->property("movingInContentX").value<bool>(), true);
    QCOMPARE(flickable->property("movingInContentY").value<bool>(), true);
    QCOMPARE(flickable->property("draggingInContentX").value<bool>(), true);
    QCOMPARE(flickable->property("draggingInContentY").value<bool>(), true);

    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(vMoveSpy.count(), verticalEnabled ? 1 : 0);
    QCOMPARE(hMoveSpy.count(), horizontalEnabled ? 1 : 0);
    QCOMPARE(dragSpy.count(), 1);
    QCOMPARE(vDragSpy.count(), verticalEnabled ? 1 : 0);
    QCOMPARE(hDragSpy.count(), horizontalEnabled ? 1 : 0);

    QCOMPARE(moveStartSpy.count(), 1);
    QCOMPARE(dragStartSpy.count(), 1);

    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, moveFrom + moveByWithoutSnapBack*3);

    QVERIFY(!flickable->isDragging());
    QVERIFY(!flickable->isDraggingHorizontally());
    QVERIFY(!flickable->isDraggingVertically());
    QCOMPARE(dragSpy.count(), 2);
    QCOMPARE(vDragSpy.count(), verticalEnabled ? 2 : 0);
    QCOMPARE(hDragSpy.count(), horizontalEnabled ? 2 : 0);
    QCOMPARE(dragStartSpy.count(), 1);
    QCOMPARE(dragEndSpy.count(), 1);
    // Don't test whether moving finished because a flick could occur

    // wait for any motion to end
    QTRY_VERIFY(flickable->isMoving() == false);

    QVERIFY(!flickable->isMovingHorizontally());
    QVERIFY(!flickable->isMovingVertically());
    QVERIFY(!flickable->isDragging());
    QVERIFY(!flickable->isDraggingHorizontally());
    QVERIFY(!flickable->isDraggingVertically());

    QCOMPARE(dragSpy.count(), 2);
    QCOMPARE(vDragSpy.count(), verticalEnabled ? 2 : 0);
    QCOMPARE(hDragSpy.count(), horizontalEnabled ? 2 : 0);
    QCOMPARE(moveSpy.count(), 2);
    QCOMPARE(vMoveSpy.count(), verticalEnabled ? 2 : 0);
    QCOMPARE(hMoveSpy.count(), horizontalEnabled ? 2 : 0);

    QCOMPARE(dragStartSpy.count(), 1);
    QCOMPARE(dragEndSpy.count(), 1);
    QCOMPARE(moveStartSpy.count(), 1);
    QCOMPARE(moveEndSpy.count(), 1);

    // Stop on a full pixel after user interaction
    if (verticalEnabled)
        QCOMPARE(flickable->contentY(), (qreal)qRound(flickable->contentY()));
    if (horizontalEnabled)
        QCOMPARE(flickable->contentX(), (qreal)qRound(flickable->contentX()));

    // clear for next drag
     vMoveSpy.clear(); hMoveSpy.clear(); moveSpy.clear();
     vDragSpy.clear(); hDragSpy.clear(); dragSpy.clear();
     moveStartSpy.clear(); moveEndSpy.clear();
     dragStartSpy.clear(); dragEndSpy.clear();

     // do a drag that drags the view out of bounds
     flickable->setContentX(0);
     flickable->setContentY(0);
     QTRY_VERIFY(!flickable->isMoving());
     QTest::mousePress(window.data(), Qt::LeftButton, 0, moveFrom);
     QTest::mouseMove(window.data(), moveFrom + moveByWithSnapBack);
     QTest::mouseMove(window.data(), moveFrom + moveByWithSnapBack*2);
     QTest::mouseMove(window.data(), moveFrom + moveByWithSnapBack*3);

     QVERIFY(flickable->isMoving());
     QCOMPARE(flickable->isMovingHorizontally(), horizontalEnabled);
     QCOMPARE(flickable->isMovingVertically(), verticalEnabled);
     QVERIFY(flickable->isDragging());
     QCOMPARE(flickable->isDraggingHorizontally(), horizontalEnabled);
     QCOMPARE(flickable->isDraggingVertically(), verticalEnabled);

     QCOMPARE(moveSpy.count(), 1);
     QCOMPARE(vMoveSpy.count(), verticalEnabled ? 1 : 0);
     QCOMPARE(hMoveSpy.count(), horizontalEnabled ? 1 : 0);
     QCOMPARE(dragSpy.count(), 1);
     QCOMPARE(vDragSpy.count(), verticalEnabled ? 1 : 0);
     QCOMPARE(hDragSpy.count(), horizontalEnabled ? 1 : 0);

     QCOMPARE(moveStartSpy.count(), 1);
     QCOMPARE(moveEndSpy.count(), 0);
     QCOMPARE(dragStartSpy.count(), 1);
     QCOMPARE(dragEndSpy.count(), 0);

     QTest::mouseRelease(window.data(), Qt::LeftButton, 0, moveFrom + moveByWithSnapBack*3);

     // should now start snapping back to bounds (moving but not dragging)
     QVERIFY(flickable->isMoving());
     QCOMPARE(flickable->isMovingHorizontally(), horizontalEnabled);
     QCOMPARE(flickable->isMovingVertically(), verticalEnabled);
     QVERIFY(!flickable->isDragging());
     QVERIFY(!flickable->isDraggingHorizontally());
     QVERIFY(!flickable->isDraggingVertically());

     QCOMPARE(moveSpy.count(), 1);
     QCOMPARE(vMoveSpy.count(), verticalEnabled ? 1 : 0);
     QCOMPARE(hMoveSpy.count(), horizontalEnabled ? 1 : 0);
     QCOMPARE(dragSpy.count(), 2);
     QCOMPARE(vDragSpy.count(), verticalEnabled ? 2 : 0);
     QCOMPARE(hDragSpy.count(), horizontalEnabled ? 2 : 0);

     QCOMPARE(moveStartSpy.count(), 1);
     QCOMPARE(moveEndSpy.count(), 0);

     // wait for any motion to end
     QTRY_VERIFY(!flickable->isMoving());

     QVERIFY(!flickable->isMovingHorizontally());
     QVERIFY(!flickable->isMovingVertically());
     QVERIFY(!flickable->isDragging());
     QVERIFY(!flickable->isDraggingHorizontally());
     QVERIFY(!flickable->isDraggingVertically());

     QCOMPARE(moveSpy.count(), 2);
     QCOMPARE(vMoveSpy.count(), verticalEnabled ? 2 : 0);
     QCOMPARE(hMoveSpy.count(), horizontalEnabled ? 2 : 0);
     QCOMPARE(dragSpy.count(), 2);
     QCOMPARE(vDragSpy.count(), verticalEnabled ? 2 : 0);
     QCOMPARE(hDragSpy.count(), horizontalEnabled ? 2 : 0);

     QCOMPARE(moveStartSpy.count(), 1);
     QCOMPARE(moveEndSpy.count(), 1);
     QCOMPARE(dragStartSpy.count(), 1);
     QCOMPARE(dragEndSpy.count(), 1);

     QCOMPARE(flickable->contentX(), 0.0);
     QCOMPARE(flickable->contentY(), 0.0);
}

void tst_qquickflickable::flickOnRelease()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("flickable03.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(flickable != 0);

    // Vertical with a quick press-move-release: should cause a flick in release.
    QSignalSpy vFlickSpy(flickable, SIGNAL(flickingVerticallyChanged()));
    // Use something that generates a huge velocity just to make it testable.
    // In practice this feature matters on touchscreen devices where the
    // underlying drivers will hopefully provide a pre-calculated velocity
    // (based on more data than what the UI gets), thus making this use case
    // working even with small movements.
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(50, 300));
    QTest::mouseMove(window.data(), QPoint(50, 10), 10);
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(50, 10), 10);

    QCOMPARE(vFlickSpy.count(), 1);

    // wait for any motion to end
    QTRY_VERIFY(flickable->isMoving() == false);

#ifdef Q_OS_MAC
    QEXPECT_FAIL("", "QTBUG-26094 stopping on a full pixel doesn't work on OS X", Continue);
#endif
    // Stop on a full pixel after user interaction
    QCOMPARE(flickable->contentY(), (qreal)qRound(flickable->contentY()));
}

void tst_qquickflickable::pressWhileFlicking()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("flickable03.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(flickable != 0);

    QSignalSpy vMoveSpy(flickable, SIGNAL(movingVerticallyChanged()));
    QSignalSpy hMoveSpy(flickable, SIGNAL(movingHorizontallyChanged()));
    QSignalSpy moveSpy(flickable, SIGNAL(movingChanged()));
    QSignalSpy hFlickSpy(flickable, SIGNAL(flickingHorizontallyChanged()));
    QSignalSpy vFlickSpy(flickable, SIGNAL(flickingVerticallyChanged()));
    QSignalSpy flickSpy(flickable, SIGNAL(flickingChanged()));

    // flick then press while it is still moving
    // flicking == false, moving == true;
    flick(window.data(), QPoint(20,190), QPoint(20, 50), 200);
    QVERIFY(flickable->verticalVelocity() > 0.0);
    QTRY_VERIFY(flickable->isFlicking());
    QVERIFY(flickable->isFlickingVertically());
    QVERIFY(!flickable->isFlickingHorizontally());
    QVERIFY(flickable->isMoving());
    QVERIFY(flickable->isMovingVertically());
    QVERIFY(!flickable->isMovingHorizontally());
    QCOMPARE(vMoveSpy.count(), 1);
    QCOMPARE(hMoveSpy.count(), 0);
    QCOMPARE(moveSpy.count(), 1);
    QCOMPARE(vFlickSpy.count(), 1);
    QCOMPARE(hFlickSpy.count(), 0);
    QCOMPARE(flickSpy.count(), 1);

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(20, 50));
    QTRY_VERIFY(!flickable->isFlicking());
    QVERIFY(!flickable->isFlickingVertically());
    QVERIFY(flickable->isMoving());
    QVERIFY(flickable->isMovingVertically());

    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(20,50));
    QVERIFY(!flickable->isFlicking());
    QVERIFY(!flickable->isFlickingVertically());
    QTRY_VERIFY(!flickable->isMoving());
    QVERIFY(!flickable->isMovingVertically());
    // Stop on a full pixel after user interaction
    QCOMPARE(flickable->contentX(), (qreal)qRound(flickable->contentX()));
}

void tst_qquickflickable::disabled()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("disabled.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flick = window->rootObject()->findChild<QQuickFlickable*>("flickable");
    QVERIFY(flick != 0);

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(50, 90));

    QTest::mouseMove(window.data(), QPoint(50, 80));
    QTest::mouseMove(window.data(), QPoint(50, 70));
    QTest::mouseMove(window.data(), QPoint(50, 60));

    QVERIFY(flick->isMoving() == false);

    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(50, 60));

    // verify that mouse clicks on other elements still work (QTBUG-20584)
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(50, 10));
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(50, 10));

    QTRY_VERIFY(window->rootObject()->property("clicked").toBool() == true);
}

void tst_qquickflickable::flickVelocity()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("flickable03.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(flickable != 0);

    // flick up
    flick(window.data(), QPoint(20,190), QPoint(20, 50), 200);
    QVERIFY(flickable->verticalVelocity() > 0.0);
    QTRY_VERIFY(flickable->verticalVelocity() == 0.0);

    // flick down
    flick(window.data(), QPoint(20,10), QPoint(20, 140), 200);
    QTRY_VERIFY(flickable->verticalVelocity() < 0.0);
    QTRY_VERIFY(flickable->verticalVelocity() == 0.0);

#ifdef Q_OS_MAC
    QSKIP("boost doesn't work on OS X");
    return;
#endif

    // Flick multiple times and verify that flick acceleration is applied.
    QQuickFlickablePrivate *fp = QQuickFlickablePrivate::get(flickable);
    bool boosted = false;
    for (int i = 0; i < 6; ++i) {
        flick(window.data(), QPoint(20,390), QPoint(20, 50), 100);
        boosted |= fp->flickBoost > 1.0;
    }
    QVERIFY(boosted);

    // Flick in opposite direction -> boost cancelled.
    flick(window.data(), QPoint(20,10), QPoint(20, 340), 200);
    QTRY_VERIFY(flickable->verticalVelocity() < 0.0);
    QVERIFY(fp->flickBoost == 1.0);
}

void tst_qquickflickable::margins()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("margins.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->setTitle(QTest::currentTestFunction());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QQuickItem *root = window->rootObject();
    QVERIFY(root);
    QQuickFlickable *obj = qobject_cast<QQuickFlickable*>(root);
    QVERIFY(obj != 0);

    // starting state
    QCOMPARE(obj->contentX(), -40.);
    QCOMPARE(obj->contentY(), -20.);
    QCOMPARE(obj->contentWidth(), 1600.);
    QCOMPARE(obj->contentHeight(), 600.);
    QCOMPARE(obj->originX(), 0.);
    QCOMPARE(obj->originY(), 0.);

    // Reduce left margin
    obj->setLeftMargin(30);
    QTRY_COMPARE(obj->contentX(), -30.);

    // Reduce top margin
    obj->setTopMargin(20);
    QTRY_COMPARE(obj->contentY(), -20.);

    // position to the far right, including margin
    obj->setContentX(1600 + 50 - obj->width());
    obj->returnToBounds();
    QTRY_COMPARE(obj->contentX(), 1600. + 50. - obj->width());

    // position beyond the far right, including margin
    obj->setContentX(1600 + 50 - obj->width() + 1.);
    obj->returnToBounds();
    QTRY_COMPARE(obj->contentX(), 1600. + 50. - obj->width());

    // Reduce right margin
    obj->setRightMargin(40);
    QTRY_COMPARE(obj->contentX(), 1600. + 40. - obj->width());
    QCOMPARE(obj->contentWidth(), 1600.);

    // position to the far bottom, including margin
    obj->setContentY(600 + 30 - obj->height());
    obj->returnToBounds();
    QTRY_COMPARE(obj->contentY(), 600. + 30. - obj->height());

    // position beyond the far bottom, including margin
    obj->setContentY(600 + 30 - obj->height() + 1.);
    obj->returnToBounds();
    QTRY_COMPARE(obj->contentY(), 600. + 30. - obj->height());

    // Reduce bottom margin
    obj->setBottomMargin(20);
    QTRY_COMPARE(obj->contentY(), 600. + 20. - obj->height());
    QCOMPARE(obj->contentHeight(), 600.);

    delete root;
}

void tst_qquickflickable::cancelOnMouseGrab()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("cancel.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(flickable != 0);

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(10, 10));
    // drag out of bounds
    QTest::mouseMove(window.data(), QPoint(50, 50));
    QTest::mouseMove(window.data(), QPoint(100, 100));
    QTest::mouseMove(window.data(), QPoint(150, 150));

    QVERIFY(flickable->contentX() != 0);
    QVERIFY(flickable->contentY() != 0);
    QVERIFY(flickable->isMoving());
    QVERIFY(flickable->isDragging());

    // grabbing mouse will cancel flickable interaction.
    QQuickItem *item = window->rootObject()->findChild<QQuickItem*>("row");
    item->grabMouse();

    QTRY_COMPARE(flickable->contentX(), 0.);
    QTRY_COMPARE(flickable->contentY(), 0.);
    QTRY_VERIFY(!flickable->isMoving());
    QTRY_VERIFY(!flickable->isDragging());

    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(50, 10));

}

void tst_qquickflickable::clickAndDragWhenTransformed()
{
    QScopedPointer<QQuickView> view(new QQuickView);
    view->setSource(testFileUrl("transformedFlickable.qml"));
    QTRY_COMPARE(view->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(view.data());
    QQuickViewTestUtil::moveMouseAway(view.data());
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view.data()));
    QVERIFY(view->rootObject() != 0);

    QQuickFlickable *flickable = view->rootObject()->findChild<QQuickFlickable*>("flickable");
    QVERIFY(flickable != 0);

    // click outside child rect
    QTest::mousePress(view.data(), Qt::LeftButton, 0, QPoint(190, 190));
    QTRY_COMPARE(flickable->property("itemPressed").toBool(), false);
    QTest::mouseRelease(view.data(), Qt::LeftButton, 0, QPoint(190, 190));

    // click inside child rect
    QTest::mousePress(view.data(), Qt::LeftButton, 0, QPoint(200, 200));
    QTRY_COMPARE(flickable->property("itemPressed").toBool(), true);
    QTest::mouseRelease(view.data(), Qt::LeftButton, 0, QPoint(200, 200));

    const int threshold = qApp->styleHints()->startDragDistance();

    // drag outside bounds
    QTest::mousePress(view.data(), Qt::LeftButton, 0, QPoint(160, 160));
    QTest::qWait(10);
    QTest::mouseMove(view.data(), QPoint(160 + threshold * 2, 160));
    QTest::mouseMove(view.data(), QPoint(160 + threshold * 3, 160));
    QCOMPARE(flickable->isDragging(), false);
    QCOMPARE(flickable->property("itemPressed").toBool(), false);
    QTest::mouseRelease(view.data(), Qt::LeftButton, 0, QPoint(180, 160));

    // drag inside bounds
    QTest::mousePress(view.data(), Qt::LeftButton, 0, QPoint(200, 140));
    QTest::qWait(10);
    QTest::mouseMove(view.data(), QPoint(200 + threshold * 2, 140));
    QTest::mouseMove(view.data(), QPoint(200 + threshold * 3, 140));
    QCOMPARE(flickable->isDragging(), true);
    QCOMPARE(flickable->property("itemPressed").toBool(), false);
    QTest::mouseRelease(view.data(), Qt::LeftButton, 0, QPoint(220, 140));
}

void tst_qquickflickable::flickTwiceUsingTouches()
{
    QTouchDevice *touchDevice = new QTouchDevice;
    touchDevice->setName("Fake Touchscreen");
    touchDevice->setType(QTouchDevice::TouchScreen);
    touchDevice->setCapabilities(QTouchDevice::Position);
    QWindowSystemInterface::registerTouchDevice(touchDevice);

    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("longList.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(flickable != 0);

    QCOMPARE(flickable->contentY(), 0.0f);
    flickWithTouch(window.data(), touchDevice, QPoint(100, 400), QPoint(100, 240));

    qreal contentYAfterFirstFlick = flickable->contentY();
    qDebug() << "contentYAfterFirstFlick " << contentYAfterFirstFlick;
    QVERIFY(contentYAfterFirstFlick > 50.0f);
    // Wait until view stops moving
    QTRY_VERIFY(!flickable->isMoving());

    flickWithTouch(window.data(), touchDevice, QPoint(100, 400), QPoint(100, 240));

    // In the original bug, that second flick would cause Flickable to halt immediately
    qreal contentYAfterSecondFlick = flickable->contentY();
    qDebug() << "contentYAfterSecondFlick " << contentYAfterSecondFlick;
    QTRY_VERIFY(contentYAfterSecondFlick > (contentYAfterFirstFlick + 80.0f));
}

void tst_qquickflickable::flickWithTouch(QQuickWindow *window, QTouchDevice *touchDevice, const QPoint &from, const QPoint &to)
{
    QTest::touchEvent(window, touchDevice).press(0, from, window);
    QQuickTouchUtils::flush(window);

    QPoint diff = to - from;
    for (int i = 1; i <= 8; ++i) {
        QTest::touchEvent(window, touchDevice).move(0, from + i*diff/8, window);
        QQuickTouchUtils::flush(window);
    }
    QTest::touchEvent(window, touchDevice).release(0, to, window);
    QQuickTouchUtils::flush(window);
}

void tst_qquickflickable::nestedStopAtBounds_data()
{
    QTest::addColumn<bool>("transpose");
    QTest::addColumn<bool>("invert");

    QTest::newRow("left") << false << false;
    QTest::newRow("right") << false << true;
    QTest::newRow("top") << true << false;
    QTest::newRow("bottom") << true << true;
}

void tst_qquickflickable::nestedStopAtBounds()
{
    QFETCH(bool, transpose);
    QFETCH(bool, invert);

    QQuickView view;
    view.setSource(testFileUrl("nestedStopAtBounds.qml"));
    QTRY_COMPARE(view.status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(&view);
    QQuickViewTestUtil::moveMouseAway(&view);
    view.show();
    view.requestActivate();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.rootObject());

    QQuickFlickable *outer = qobject_cast<QQuickFlickable*>(view.rootObject());
    QVERIFY(outer);

    QQuickFlickable *inner = outer->findChild<QQuickFlickable*>("innerFlickable");
    QVERIFY(inner);
    inner->setFlickableDirection(transpose ? QQuickFlickable::VerticalFlick : QQuickFlickable::HorizontalFlick);
    inner->setContentX(invert ? 0 : 100);
    inner->setContentY(invert ? 0 : 100);

    const int threshold = qApp->styleHints()->startDragDistance();

    QPoint position(200, 200);
    int &axis = transpose ? position.ry() : position.rx();

    // drag toward the aligned boundary.  Outer flickable dragged.
    QTest::mousePress(&view, Qt::LeftButton, 0, position);
    QTest::qWait(10);
    axis += invert ? threshold * 2 : -threshold * 2;
    QTest::mouseMove(&view, position);
    axis += invert ? threshold : -threshold;
    QTest::mouseMove(&view, position);
    QCOMPARE(outer->isDragging(), true);
    QCOMPARE(inner->isDragging(), false);
    QTest::mouseRelease(&view, Qt::LeftButton, 0, position);

    QVERIFY(!outer->isDragging());
    QTRY_VERIFY(!outer->isMoving());

    axis = 200;
    outer->setContentX(50);
    outer->setContentY(50);

    // drag away from the aligned boundary.  Inner flickable dragged.
    QTest::mousePress(&view, Qt::LeftButton, 0, position);
    QTest::qWait(10);
    axis += invert ? -threshold * 2 : threshold * 2;
    QTest::mouseMove(&view, position);
    axis += invert ? -threshold : threshold;
    QTest::mouseMove(&view, position);
    QCOMPARE(outer->isDragging(), false);
    QCOMPARE(inner->isDragging(), true);
    QTest::mouseRelease(&view, Qt::LeftButton, 0, position);

    QTRY_VERIFY(!outer->isMoving());
}

void tst_qquickflickable::stopAtBounds_data()
{
    QTest::addColumn<bool>("transpose");
    QTest::addColumn<bool>("invert");
    QTest::addColumn<bool>("pixelAligned");

    QTest::newRow("left") << false << false << false;
    QTest::newRow("right") << false << true << false;
    QTest::newRow("top") << true << false << false;
    QTest::newRow("bottom") << true << true << false;
    QTest::newRow("left,pixelAligned") << false << false << true;
    QTest::newRow("right,pixelAligned") << false << true << true;
    QTest::newRow("top,pixelAligned") << true << false << true;
    QTest::newRow("bottom,pixelAligned") << true << true << true;
}

void tst_qquickflickable::stopAtBounds()
{
    QFETCH(bool, transpose);
    QFETCH(bool, invert);
    QFETCH(bool, pixelAligned);

    QQuickView view;
    view.setSource(testFileUrl("stopAtBounds.qml"));
    QTRY_COMPARE(view.status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(&view);
    QQuickViewTestUtil::moveMouseAway(&view);
    view.show();
    view.requestActivate();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.rootObject());

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(view.rootObject());
    QVERIFY(flickable);

    if (transpose)
        flickable->setContentY(invert ? 100 : 0);
    else
        flickable->setContentX(invert ? 100 : 0);
    flickable->setPixelAligned(pixelAligned);

    const int threshold = qApp->styleHints()->startDragDistance();

    QPoint position(200, 200);
    int &axis = transpose ? position.ry() : position.rx();

    // drag away from the aligned boundary. View should not move
    QTest::mousePress(&view, Qt::LeftButton, 0, position);
    QTest::qWait(10);
    for (int i = 0; i < 3; ++i) {
        axis += invert ? -threshold : threshold;
        QTest::mouseMove(&view, position);
    }
    QCOMPARE(flickable->isDragging(), false);
    if (invert)
        QCOMPARE(transpose ? flickable->isAtYEnd() : flickable->isAtXEnd(), true);
    else
        QCOMPARE(transpose ? flickable->isAtYBeginning() : flickable->isAtXBeginning(), true);

    // drag back towards boundary
    for (int i = 0; i < 24; ++i) {
        axis += invert ? threshold / 3 : -threshold / 3;
        QTest::mouseMove(&view, position);
    }
    QTRY_COMPARE(flickable->isDragging(), true);
    if (invert)
        QCOMPARE(transpose ? flickable->isAtYEnd() : flickable->isAtXEnd(), false);
    else
        QCOMPARE(transpose ? flickable->isAtYBeginning() : flickable->isAtXBeginning(), false);

    // Drag away from the aligned boundary again.
    // None of the mouse movements will position the view at the boundary exactly,
    // but the view should end up aligned on the boundary
    for (int i = 0; i < 5; ++i) {
        axis += invert ? -threshold * 2 : threshold * 2;
        QTest::mouseMove(&view, position);
    }
    QCOMPARE(flickable->isDragging(), true);

    // we should have hit the boundary and stopped
    if (invert) {
        QCOMPARE(transpose ? flickable->isAtYEnd() : flickable->isAtXEnd(), true);
        QCOMPARE(transpose ? flickable->contentY() : flickable->contentX(), 100.0);
    } else {
        QCOMPARE(transpose ? flickable->isAtYBeginning() : flickable->isAtXBeginning(), true);
        QCOMPARE(transpose ? flickable->contentY() : flickable->contentX(), 0.0);
    }

    QTest::mouseRelease(&view, Qt::LeftButton, 0, position);

    if (transpose) {
        flickable->setContentY(invert ? 100 : 0);
    } else {
        flickable->setContentX(invert ? 100 : 0);
    }
    if (invert)
        flick(&view, QPoint(20,20), QPoint(120,120), 100);
    else
        flick(&view, QPoint(120,120), QPoint(20,20), 100);

    QVERIFY(flickable->isFlicking());
    if (transpose) {
        if (invert)
            QTRY_COMPARE(flickable->isAtYBeginning(), true);
        else
            QTRY_COMPARE(flickable->isAtYEnd(), true);
    } else {
        if (invert)
            QTRY_COMPARE(flickable->isAtXBeginning(), true);
        else
            QTRY_COMPARE(flickable->isAtXEnd(), true);
    }
}

void tst_qquickflickable::nestedMouseAreaUsingTouch()
{
    QTouchDevice *touchDevice = new QTouchDevice;
    touchDevice->setName("Fake Touchscreen");
    touchDevice->setType(QTouchDevice::TouchScreen);
    touchDevice->setCapabilities(QTouchDevice::Position);
    QWindowSystemInterface::registerTouchDevice(touchDevice);

    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("nestedmousearea.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(flickable != 0);

    QCOMPARE(flickable->contentY(), 50.0f);
    flickWithTouch(window.data(), touchDevice, QPoint(100, 300), QPoint(100, 200));

    // flickable should not have moved
    QCOMPARE(flickable->contentY(), 50.0);

    // draggable item should have moved up
    QQuickItem *nested = window->rootObject()->findChild<QQuickItem*>("nested");
    QVERIFY(nested->y() < 100.0);
}

// QTBUG-31328
void tst_qquickflickable::pressDelayWithLoader()
{
    QScopedPointer<QQuickView> window(new QQuickView);
    window->setSource(testFileUrl("pressDelayWithLoader.qml"));
    QTRY_COMPARE(window->status(), QQuickView::Ready);
    QQuickViewTestUtil::centerOnScreen(window.data());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    // do not crash
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(150, 150));
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(150, 150));
}

QTEST_MAIN(tst_qquickflickable)

#include "tst_qquickflickable.moc"
