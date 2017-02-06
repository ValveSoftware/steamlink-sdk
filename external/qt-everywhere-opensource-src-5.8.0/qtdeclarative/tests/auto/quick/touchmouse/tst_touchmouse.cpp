/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include <QtGui/qstylehints.h>

#include <QtQuick/qquickview.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/private/qquickevents_p_p.h>
#include <QtQuick/private/qquickmousearea_p.h>
#include <QtQuick/private/qquickmultipointtoucharea_p.h>
#include <QtQuick/private/qquickpincharea_p.h>
#include <QtQuick/private/qquickflickable_p.h>
#include <QtQuick/private/qquickwindow_p.h>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlproperty.h>

#include "../../shared/util.h"
#include "../shared/viewtestutil.h"

struct Event
{
    Event(QEvent::Type t, QPoint mouse, QPoint global)
        :type(t), mousePos(mouse), mousePosGlobal(global)
    {}

    Event(QEvent::Type t, QList<QTouchEvent::TouchPoint> touch)
        :type(t), points(touch)
    {}

    QEvent::Type type;
    QPoint mousePos;
    QPoint mousePosGlobal;
    QList<QTouchEvent::TouchPoint> points;
};

class EventItem : public QQuickItem
{
    Q_OBJECT

Q_SIGNALS:
    void onTouchEvent(QQuickItem *receiver);

public:
    EventItem(QQuickItem *parent = 0)
        : QQuickItem(parent), acceptMouse(false), acceptTouch(false), filterTouch(false)
    {
        setAcceptedMouseButtons(Qt::LeftButton);
    }

    void touchEvent(QTouchEvent *event)
    {
        eventList.append(Event(event->type(), event->touchPoints()));
        event->setAccepted(acceptTouch);
        emit onTouchEvent(this);
    }
    void mousePressEvent(QMouseEvent *event)
    {
        eventList.append(Event(event->type(), event->pos(), event->globalPos()));
        event->setAccepted(acceptMouse);
    }
    void mouseMoveEvent(QMouseEvent *event)
    {
        eventList.append(Event(event->type(), event->pos(), event->globalPos()));
        event->setAccepted(acceptMouse);
    }
    void mouseReleaseEvent(QMouseEvent *event)
    {
        eventList.append(Event(event->type(), event->pos(), event->globalPos()));
        event->setAccepted(acceptMouse);
    }
    void mouseDoubleClickEvent(QMouseEvent *event)
    {
        eventList.append(Event(event->type(), event->pos(), event->globalPos()));
        event->setAccepted(acceptMouse);
    }

    void mouseUngrabEvent()
    {
        eventList.append(Event(QEvent::UngrabMouse, QPoint(0,0), QPoint(0,0)));
    }

    bool event(QEvent *event) {
        return QQuickItem::event(event);
    }

    QList<Event> eventList;
    bool acceptMouse;
    bool acceptTouch;
    bool filterTouch; // when used as event filter

    bool eventFilter(QObject *, QEvent *event)
    {
        if (event->type() == QEvent::TouchBegin ||
                event->type() == QEvent::TouchUpdate ||
                event->type() == QEvent::TouchCancel ||
                event->type() == QEvent::TouchEnd) {
            QTouchEvent *touch = static_cast<QTouchEvent*>(event);
            eventList.append(Event(event->type(), touch->touchPoints()));
            if (filterTouch)
                event->accept();
            return true;
        }
        return false;
    }
};

class tst_TouchMouse : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_TouchMouse()
        :device(QTest::createTouchDevice())
    {}

private slots:
    void initTestCase();

    void simpleTouchEvent();
    void testEventFilter();
    void mouse();
    void touchOverMouse();
    void mouseOverTouch();

    void buttonOnFlickable();
    void buttonOnDelayedPressFlickable_data();
    void buttonOnDelayedPressFlickable();
    void buttonOnTouch();

    void pinchOnFlickable();
    void flickableOnPinch();
    void mouseOnFlickableOnPinch();

    void tapOnDismissiveTopMouseAreaClicksBottomOne();

    void touchGrabCausesMouseUngrab();
    void touchPointDeliveryOrder();

    void hoverEnabled();

protected:
    bool eventFilter(QObject *, QEvent *event)
    {
        if (event->type() == QEvent::MouseButtonPress ||
                event->type() == QEvent::MouseMove ||
                event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            filteredEventList.append(Event(me->type(), me->pos(), me->globalPos()));
        }
        return false;
    }

private:
    QQuickView *createView();
    QTouchDevice *device;
    QList<Event> filteredEventList;
};

QQuickView *tst_TouchMouse::createView()
{
    QQuickView *window = new QQuickView(0);
    window->setGeometry(0,0,240,320);

    return window;
}

void tst_TouchMouse::initTestCase()
{
    // This test assumes that we don't get synthesized mouse events from QGuiApplication
    qApp->setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents, false);

    QQmlDataTest::initTestCase();
    qmlRegisterType<EventItem>("Qt.test", 1, 0, "EventItem");
}

void tst_TouchMouse::simpleTouchEvent()
{
    QQuickView *window = createView();

    window->setSource(testFileUrl("singleitem.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(window->rootObject() != 0);

    EventItem *eventItem1 = window->rootObject()->findChild<EventItem*>("eventItem1");
    QVERIFY(eventItem1);

    // Do not accept touch or mouse
    QPoint p1;
    p1 = QPoint(20, 20);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    // Get a touch and then mouse event offered
    QCOMPARE(eventItem1->eventList.size(), 2);
    QCOMPARE(eventItem1->eventList.at(0).type, QEvent::TouchBegin);
    p1 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window);
    QQuickTouchUtils::flush(window);
    // Not accepted, no updates
    QCOMPARE(eventItem1->eventList.size(), 2);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 2);
    eventItem1->eventList.clear();

    // Accept touch
    eventItem1->acceptTouch = true;
    p1 = QPoint(20, 20);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 1);
    p1 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 2);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 3);
    eventItem1->eventList.clear();

    // wait to avoid getting a double click event
    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 10);

    // Accept mouse
    eventItem1->acceptTouch = false;
    eventItem1->acceptMouse = true;
    eventItem1->setAcceptedMouseButtons(Qt::LeftButton);
    p1 = QPoint(20, 20);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 2);
    QCOMPARE(eventItem1->eventList.at(0).type, QEvent::TouchBegin);
    QCOMPARE(eventItem1->eventList.at(1).type, QEvent::MouseButtonPress);
    QCOMPARE(window->mouseGrabberItem(), eventItem1);

    QPoint localPos = eventItem1->mapFromScene(p1).toPoint();
    QPoint globalPos = window->mapToGlobal(p1);
    QPoint scenePos = p1; // item is at 0,0
    QCOMPARE(eventItem1->eventList.at(0).points.at(0).pos().toPoint(), localPos);
    QCOMPARE(eventItem1->eventList.at(0).points.at(0).scenePos().toPoint(), scenePos);
    QCOMPARE(eventItem1->eventList.at(0).points.at(0).screenPos().toPoint(), globalPos);
    QCOMPARE(eventItem1->eventList.at(1).mousePos, localPos);
    QCOMPARE(eventItem1->eventList.at(1).mousePosGlobal, globalPos);

    p1 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 4);
    QCOMPARE(eventItem1->eventList.at(2).type, QEvent::TouchUpdate);
    QCOMPARE(eventItem1->eventList.at(3).type, QEvent::MouseMove);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 7);
    QCOMPARE(eventItem1->eventList.at(4).type, QEvent::TouchEnd);
    QCOMPARE(eventItem1->eventList.at(5).type, QEvent::MouseButtonRelease);
    QCOMPARE(eventItem1->eventList.at(6).type, QEvent::UngrabMouse);
    eventItem1->eventList.clear();

    // wait to avoid getting a double click event
    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 10);

    // Accept mouse buttons but not the event
    eventItem1->acceptTouch = false;
    eventItem1->acceptMouse = false;
    eventItem1->setAcceptedMouseButtons(Qt::LeftButton);
    p1 = QPoint(20, 20);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 2);
    QCOMPARE(eventItem1->eventList.at(0).type, QEvent::TouchBegin);
    QCOMPARE(eventItem1->eventList.at(1).type, QEvent::MouseButtonPress);
    p1 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 2);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 2);
    eventItem1->eventList.clear();

    // wait to avoid getting a double click event
    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 10);

    // Accept touch and mouse
    eventItem1->acceptTouch = true;
    eventItem1->setAcceptedMouseButtons(Qt::LeftButton);
    p1 = QPoint(20, 20);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 1);
    QCOMPARE(eventItem1->eventList.at(0).type, QEvent::TouchBegin);
    p1 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 2);
    QCOMPARE(eventItem1->eventList.at(1).type, QEvent::TouchUpdate);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 3);
    QCOMPARE(eventItem1->eventList.at(2).type, QEvent::TouchEnd);
    eventItem1->eventList.clear();

    delete window;
}

void tst_TouchMouse::testEventFilter()
{
//    // install event filter on item and see that it can grab events
//    QQuickView *window = createView();

//    window->setSource(testFileUrl("singleitem.qml"));
//    window->show();
//    window->requestActivate();
//    QVERIFY(window->rootObject() != 0);

//    EventItem *eventItem1 = window->rootObject()->findChild<EventItem*>("eventItem1");
//    QVERIFY(eventItem1);
//    eventItem1->acceptTouch = true;

//    EventItem *filter = new EventItem;
//    filter->filterTouch = true;
//    eventItem1->installEventFilter(filter);

//    QPoint p1 = QPoint(20, 20);
//    QTest::touchEvent(window, device).press(0, p1, window);
//    // QEXPECT_FAIL("", "We do not implement event filters correctly", Abort);
//    QCOMPARE(eventItem1->eventList.size(), 0);
//    QCOMPARE(filter->eventList.size(), 1);
//    QTest::touchEvent(window, device).release(0, p1, window);
//    QCOMPARE(eventItem1->eventList.size(), 0);
//    QCOMPARE(filter->eventList.size(), 2);

//    delete filter;
//    delete window;
}

void tst_TouchMouse::mouse()
{
    // eventItem1
    //   - eventItem2

    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 10);
    QQuickView *window = createView();

    window->setSource(testFileUrl("twoitems.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(window->rootObject() != 0);

    EventItem *eventItem1 = window->rootObject()->findChild<EventItem*>("eventItem1");
    QVERIFY(eventItem1);
    EventItem *eventItem2 = window->rootObject()->findChild<EventItem*>("eventItem2");
    QVERIFY(eventItem2);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    // bottom item likes mouse, top likes touch
    eventItem1->setAcceptedMouseButtons(Qt::LeftButton);
    eventItem1->acceptMouse = true;
    // item 2 doesn't accept anything, thus it sees a touch pass by
    QPoint p1 = QPoint(30, 30);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);

    QCOMPARE(eventItem1->eventList.size(), 2);
    QCOMPARE(eventItem1->eventList.at(0).type, QEvent::TouchBegin);
    QCOMPARE(eventItem1->eventList.at(1).type, QEvent::MouseButtonPress);

    delete window;
}

void tst_TouchMouse::touchOverMouse()
{
    // eventItem1
    //   - eventItem2

    QQuickView *window = createView();

    window->setSource(testFileUrl("twoitems.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(window->rootObject() != 0);

    EventItem *eventItem1 = window->rootObject()->findChild<EventItem*>("eventItem1");
    QVERIFY(eventItem1);
    EventItem *eventItem2 = window->rootObject()->findChild<EventItem*>("eventItem2");
    QVERIFY(eventItem2);

    // bottom item likes mouse, top likes touch
    eventItem1->setAcceptedMouseButtons(Qt::LeftButton);
    eventItem2->acceptTouch = true;

    QVERIFY(QTest::qWaitForWindowExposed(window));

    QCOMPARE(eventItem1->eventList.size(), 0);
    QPoint p1 = QPoint(20, 20);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 0);
    QCOMPARE(eventItem2->eventList.size(), 1);
    QCOMPARE(eventItem2->eventList.at(0).type, QEvent::TouchBegin);
    p1 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem2->eventList.size(), 2);
    QCOMPARE(eventItem2->eventList.at(1).type, QEvent::TouchUpdate);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem2->eventList.size(), 3);
    QCOMPARE(eventItem2->eventList.at(2).type, QEvent::TouchEnd);
    eventItem2->eventList.clear();

    delete window;
}

void tst_TouchMouse::mouseOverTouch()
{
    // eventItem1
    //   - eventItem2

    QQuickView *window = createView();

    window->setSource(testFileUrl("twoitems.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(window->rootObject() != 0);

    EventItem *eventItem1 = window->rootObject()->findChild<EventItem*>("eventItem1");
    QVERIFY(eventItem1);
    EventItem *eventItem2 = window->rootObject()->findChild<EventItem*>("eventItem2");
    QVERIFY(eventItem2);

    // bottom item likes mouse, top likes touch
    eventItem1->acceptTouch = true;
    eventItem2->setAcceptedMouseButtons(Qt::LeftButton);
    eventItem2->acceptMouse = true;

    QVERIFY(QTest::qWaitForWindowExposed(window));

    QPoint p1 = QPoint(20, 20);
    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 10);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 0);
    QCOMPARE(eventItem2->eventList.size(), 2);
    QCOMPARE(eventItem2->eventList.at(0).type, QEvent::TouchBegin);
    QCOMPARE(eventItem2->eventList.at(1).type, QEvent::MouseButtonPress);


//    p1 += QPoint(10, 0);
//    QTest::touchEvent(window, device).move(0, p1, window);
//    QCOMPARE(eventItem2->eventList.size(), 1);
//    QTest::touchEvent(window, device).release(0, p1, window);
//    QCOMPARE(eventItem2->eventList.size(), 1);
//    eventItem2->eventList.clear();

    delete window;
}

void tst_TouchMouse::buttonOnFlickable()
{
    // flickable - height 500 / 1000
    //   - eventItem1 y: 100, height 100
    //   - eventItem2 y: 300, height 100

    QQuickView *window = createView();

    window->setSource(testFileUrl("buttononflickable.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = window->rootObject()->findChild<QQuickFlickable*>("flickable");
    QVERIFY(flickable);

    // should a mouse area button be clickable on top of flickable? yes :)
    EventItem *eventItem1 = window->rootObject()->findChild<EventItem*>("eventItem1");
    QVERIFY(eventItem1);
    eventItem1->setAcceptedMouseButtons(Qt::LeftButton);
    eventItem1->acceptMouse = true;

    // should a touch button be touchable on top of flickable? yes :)
    EventItem *eventItem2 = window->rootObject()->findChild<EventItem*>("eventItem2");
    QVERIFY(eventItem2);
    QCOMPARE(eventItem2->eventList.size(), 0);
    eventItem2->acceptTouch = true;

    // wait to avoid getting a double click event
    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 10);

    // check that buttons are clickable
    // mouse button
    QCOMPARE(eventItem1->eventList.size(), 0);
    QPoint p1 = QPoint(20, 130);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QTRY_COMPARE(eventItem1->eventList.size(), 2);
    QCOMPARE(eventItem1->eventList.at(0).type, QEvent::TouchBegin);
    QCOMPARE(eventItem1->eventList.at(1).type, QEvent::MouseButtonPress);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 5);
    QCOMPARE(eventItem1->eventList.at(2).type, QEvent::TouchEnd);
    QCOMPARE(eventItem1->eventList.at(3).type, QEvent::MouseButtonRelease);
    QCOMPARE(eventItem1->eventList.at(4).type, QEvent::UngrabMouse);
    eventItem1->eventList.clear();

    // touch button
    p1 = QPoint(10, 310);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem2->eventList.size(), 1);
    QCOMPARE(eventItem2->eventList.at(0).type, QEvent::TouchBegin);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem2->eventList.size(), 2);
    QCOMPARE(eventItem2->eventList.at(1).type, QEvent::TouchEnd);
    QCOMPARE(eventItem1->eventList.size(), 0);
    eventItem2->eventList.clear();

    // wait to avoid getting a double click event
    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 10);

    // click above button, no events please
    p1 = QPoint(10, 90);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 0);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 0);
    eventItem1->eventList.clear();

    // wait to avoid getting a double click event
    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 10);

    // check that flickable moves - mouse button
    QCOMPARE(eventItem1->eventList.size(), 0);
    p1 = QPoint(10, 110);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 2);
    QCOMPARE(eventItem1->eventList.at(0).type, QEvent::TouchBegin);
    QCOMPARE(eventItem1->eventList.at(1).type, QEvent::MouseButtonPress);

    QQuickWindowPrivate *windowPriv = QQuickWindowPrivate::get(window);
    QCOMPARE(windowPriv->touchMouseId, 0);
    auto pointerEvent = QQuickPointerDevice::touchDevices().at(0)->pointerEvent();
    QCOMPARE(pointerEvent->point(0)->grabber(), eventItem1);
    QCOMPARE(window->mouseGrabberItem(), eventItem1);

    p1 += QPoint(0, -10);
    QPoint p2 = p1 + QPoint(0, -10);
    QPoint p3 = p2 + QPoint(0, -10);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).move(0, p1, window);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).move(0, p2, window);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).move(0, p3, window);
    QQuickTouchUtils::flush(window);

    // we cannot really know when the events get grabbed away
    QVERIFY(eventItem1->eventList.size() >= 4);
    QCOMPARE(eventItem1->eventList.at(2).type, QEvent::TouchUpdate);
    QCOMPARE(eventItem1->eventList.at(3).type, QEvent::MouseMove);

    QCOMPARE(window->mouseGrabberItem(), flickable);
    QCOMPARE(windowPriv->touchMouseId, 0);
    QCOMPARE(pointerEvent->point(0)->grabber(), flickable);
    QVERIFY(flickable->isMovingVertically());

    QTest::touchEvent(window, device).release(0, p3, window);
    QQuickTouchUtils::flush(window);
    delete window;
}

void tst_TouchMouse::buttonOnDelayedPressFlickable_data()
{
    QTest::addColumn<bool>("scrollBeforeDelayIsOver");

    // the item should never see the event,
    // due to the pressDelay which never delivers if we start moving
    QTest::newRow("scroll before press delay is over") << true;

    // wait until the "button" sees the press but then
    // start moving: the button gets a press and cancel event
    QTest::newRow("scroll after press delay is over") << false;
}

void tst_TouchMouse::buttonOnDelayedPressFlickable()
{
    // flickable - height 500 / 1000
    //   - eventItem1 y: 100, height 100
    //   - eventItem2 y: 300, height 100
    QFETCH(bool, scrollBeforeDelayIsOver);

    qApp->setAttribute(Qt::AA_SynthesizeMouseForUnhandledTouchEvents, true);
    filteredEventList.clear();

    QQuickView *window = createView();

    window->setSource(testFileUrl("buttononflickable.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(window->rootObject() != 0);

    QQuickFlickable *flickable = window->rootObject()->findChild<QQuickFlickable*>("flickable");
    QVERIFY(flickable);

    window->installEventFilter(this);

    // wait 600 ms before letting the child see the press event
    flickable->setPressDelay(600);

    // should a mouse area button be clickable on top of flickable? yes :)
    EventItem *eventItem1 = window->rootObject()->findChild<EventItem*>("eventItem1");
    QVERIFY(eventItem1);
    eventItem1->setAcceptedMouseButtons(Qt::LeftButton);
    eventItem1->acceptMouse = true;

    // should a touch button be touchable on top of flickable? yes :)
    EventItem *eventItem2 = window->rootObject()->findChild<EventItem*>("eventItem2");
    QVERIFY(eventItem2);
    QCOMPARE(eventItem2->eventList.size(), 0);
    eventItem2->acceptTouch = true;

    // wait to avoid getting a double click event
    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 10);
    QQuickWindowPrivate *windowPriv = QQuickWindowPrivate::get(window);
    QCOMPARE(windowPriv->touchMouseId, -1); // no grabber

    // touch press
    QPoint p1 = QPoint(10, 110);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);

    if (scrollBeforeDelayIsOver) {
        // no events, the flickable got scrolled, the button sees nothing
        QCOMPARE(eventItem1->eventList.size(), 0);
    } else {
        // wait until the button sees the press
        QTRY_COMPARE(eventItem1->eventList.size(), 1);
        QCOMPARE(eventItem1->eventList.at(0).type, QEvent::MouseButtonPress);
        QCOMPARE(filteredEventList.count(), 1);
    }

    p1 += QPoint(0, -10);
    QPoint p2 = p1 + QPoint(0, -10);
    QPoint p3 = p2 + QPoint(0, -10);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).move(0, p1, window);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).move(0, p2, window);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).move(0, p3, window);
    QQuickTouchUtils::flush(window);
    QTRY_VERIFY(flickable->isMovingVertically());

    if (scrollBeforeDelayIsOver) {
        QCOMPARE(eventItem1->eventList.size(), 0);
        QCOMPARE(filteredEventList.count(), 0);
    } else {
        // see at least press, move and ungrab
        QTRY_VERIFY(eventItem1->eventList.size() > 2);
        QCOMPARE(eventItem1->eventList.at(0).type, QEvent::MouseButtonPress);
        QCOMPARE(eventItem1->eventList.last().type, QEvent::UngrabMouse);
        QCOMPARE(filteredEventList.count(), 1);
    }

    // flickable should have the mouse grab, and have moved the itemForTouchPointId
    // for the touchMouseId to the new grabber.
    QCOMPARE(window->mouseGrabberItem(), flickable);
    QCOMPARE(windowPriv->touchMouseId, 0);
    auto pointerEvent = QQuickPointerDevice::touchDevices().at(0)->pointerEvent();
    QCOMPARE(pointerEvent->point(0)->grabber(), flickable);

    QTest::touchEvent(window, device).release(0, p3, window);
    QQuickTouchUtils::flush(window);

    // We should not have received any synthesised mouse events from Qt gui,
    // just the delayed press.
    if (scrollBeforeDelayIsOver)
        QCOMPARE(filteredEventList.count(), 0);
    else
        QCOMPARE(filteredEventList.count(), 1);

    delete window;
}

void tst_TouchMouse::buttonOnTouch()
{
    // 400x800
    //   PinchArea - height 400
    //     - eventItem1 y: 100, height 100
    //     - eventItem2 y: 300, height 100
    //   MultiPointTouchArea - height 400
    //     - eventItem1 y: 100, height 100
    //     - eventItem2 y: 300, height 100

    QQuickView *window = createView();
    window->setSource(testFileUrl("buttonontouch.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(window->rootObject() != 0);

    QQuickPinchArea *pinchArea = window->rootObject()->findChild<QQuickPinchArea*>("pincharea");
    QVERIFY(pinchArea);
    QQuickItem *button1 = window->rootObject()->findChild<QQuickItem*>("button1");
    QVERIFY(button1);
    EventItem *eventItem1 = window->rootObject()->findChild<EventItem*>("eventItem1");
    QVERIFY(eventItem1);
    EventItem *eventItem2 = window->rootObject()->findChild<EventItem*>("eventItem2");
    QVERIFY(eventItem2);

    QQuickMultiPointTouchArea *touchArea = window->rootObject()->findChild<QQuickMultiPointTouchArea*>("toucharea");
    QVERIFY(touchArea);
    EventItem *eventItem3 = window->rootObject()->findChild<EventItem*>("eventItem3");
    QVERIFY(eventItem3);
    EventItem *eventItem4 = window->rootObject()->findChild<EventItem*>("eventItem4");
    QVERIFY(eventItem4);


    // Test the common case of a mouse area on top of pinch
    eventItem1->setAcceptedMouseButtons(Qt::LeftButton);
    eventItem1->acceptMouse = true;


    // wait to avoid getting a double click event
    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 10);

    // Normal touch click
    QPoint p1 = QPoint(10, 110);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(eventItem1->eventList.size(), 5);
    QCOMPARE(eventItem1->eventList.at(0).type, QEvent::TouchBegin);
    QCOMPARE(eventItem1->eventList.at(1).type, QEvent::MouseButtonPress);
    QCOMPARE(eventItem1->eventList.at(2).type, QEvent::TouchEnd);
    QCOMPARE(eventItem1->eventList.at(3).type, QEvent::MouseButtonRelease);
    QCOMPARE(eventItem1->eventList.at(4).type, QEvent::UngrabMouse);
    eventItem1->eventList.clear();

    // Normal mouse click
    QTest::mouseClick(window, Qt::LeftButton, 0, p1);
    QCOMPARE(eventItem1->eventList.size(), 3);
    QCOMPARE(eventItem1->eventList.at(0).type, QEvent::MouseButtonPress);
    QCOMPARE(eventItem1->eventList.at(1).type, QEvent::MouseButtonRelease);
    QCOMPARE(eventItem1->eventList.at(2).type, QEvent::UngrabMouse);
    eventItem1->eventList.clear();

    // Pinch starting on the PinchArea should work
    p1 = QPoint(40, 10);
    QPoint p2 = QPoint(60, 10);

    // Start the events after each other
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).stationary(0).press(1, p2, window);
    QQuickTouchUtils::flush(window);

    QCOMPARE(button1->scale(), 1.0);

    // This event seems to be discarded, let's ignore it for now until someone digs into pincharea
    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window).move(1, p2, window);
    QQuickTouchUtils::flush(window);

    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window).move(1, p2, window);
    QQuickTouchUtils::flush(window);
//    QCOMPARE(button1->scale(), 1.5);
    qDebug() << "Button scale: " << button1->scale();

    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window).move(1, p2, window);
    QQuickTouchUtils::flush(window);
//    QCOMPARE(button1->scale(), 2.0);
    qDebug() << "Button scale: " << button1->scale();

    QTest::touchEvent(window, device).release(0, p1, window).release(1, p2, window);
    QQuickTouchUtils::flush(window);
//    QVERIFY(eventItem1->eventList.isEmpty());
//    QCOMPARE(button1->scale(), 2.0);
    qDebug() << "Button scale: " << button1->scale();


    // wait to avoid getting a double click event
    QTest::qWait(qApp->styleHints()->mouseDoubleClickInterval() + 10);

    // Start pinching while on the button
    button1->setScale(1.0);
    p1 = QPoint(40, 110);
    p2 = QPoint(60, 110);
    QTest::touchEvent(window, device).press(0, p1, window).press(1, p2, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(button1->scale(), 1.0);
    QCOMPARE(eventItem1->eventList.count(), 2);
    QCOMPARE(eventItem1->eventList.at(0).type, QEvent::TouchBegin);
    QCOMPARE(eventItem1->eventList.at(1).type, QEvent::MouseButtonPress);

    // This event seems to be discarded, let's ignore it for now until someone digs into pincharea
    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window).move(1, p2, window);
    QQuickTouchUtils::flush(window);

    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window).move(1, p2, window);
    QQuickTouchUtils::flush(window);
    //QCOMPARE(button1->scale(), 1.5);
    qDebug() << button1->scale();

    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p1, window).move(1, p2, window);
    QQuickTouchUtils::flush(window);
    qDebug() << button1->scale();
    //QCOMPARE(button1->scale(), 2.0);

    QTest::touchEvent(window, device).release(0, p1, window).release(1, p2, window);
    QQuickTouchUtils::flush(window);
//    QCOMPARE(eventItem1->eventList.size(), 99);
    qDebug() << button1->scale();
    //QCOMPARE(button1->scale(), 2.0);

    delete window;
}

void tst_TouchMouse::pinchOnFlickable()
{
    QQuickView *window = createView();
    window->setSource(testFileUrl("pinchonflickable.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(window->rootObject() != 0);

    QQuickPinchArea *pinchArea = window->rootObject()->findChild<QQuickPinchArea*>("pincharea");
    QVERIFY(pinchArea);
    QQuickFlickable *flickable = window->rootObject()->findChild<QQuickFlickable*>("flickable");
    QVERIFY(flickable);
    QQuickItem *rect = window->rootObject()->findChild<QQuickItem*>("rect");
    QVERIFY(rect);

    // flickable - single touch point
    QCOMPARE(flickable->contentX(), 0.0);
    QPoint p = QPoint(100, 100);
    QTest::touchEvent(window, device).press(0, p, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(rect->position(), QPointF(200.0, 200.0));
    p -= QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p, window);
    QQuickTouchUtils::flush(window);
    p -= QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p, window);
    QQuickTouchUtils::flush(window);
    p -= QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p, window);
    QQuickTouchUtils::flush(window);
    p -= QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p, window);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).release(0, p, window);
    QQuickTouchUtils::flush(window);

    QGuiApplication::processEvents();
    QTest::qWait(10);
    QVERIFY(!flickable->isAtXBeginning());
    // wait until flicking is done
    QTRY_VERIFY(!flickable->isFlicking());

    // pinch
    QPoint p1 = QPoint(40, 20);
    QPoint p2 = QPoint(60, 20);

    QTest::QTouchEventSequence pinchSequence = QTest::touchEvent(window, device);
    QQuickTouchUtils::flush(window);
    pinchSequence.press(0, p1, window).commit();
    QQuickTouchUtils::flush(window);
    // In order for the stationary point to remember its previous position,
    // we have to reuse the same pinchSequence object.  Otherwise if we let it
    // be destroyed and then start a new sequence, point 0 will default to being
    // stationary at 0, 0, and PinchArea will filter out that touchpoint because
    // it is outside its bounds.
    pinchSequence.stationary(0).press(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    p1 -= QPoint(10,10);
    p2 += QPoint(10,10);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    QCOMPARE(rect->scale(), 1.0);
    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QVERIFY(!flickable->isDragging());
    QQuickTouchUtils::flush(window);
    pinchSequence.release(0, p1, window).release(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    QVERIFY(rect->scale() > 1.0);
}

void tst_TouchMouse::flickableOnPinch()
{
    QQuickView *window = createView();
    window->setSource(testFileUrl("flickableonpinch.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(window->rootObject() != 0);

    QQuickPinchArea *pinchArea = window->rootObject()->findChild<QQuickPinchArea*>("pincharea");
    QVERIFY(pinchArea);
    QQuickFlickable *flickable = window->rootObject()->findChild<QQuickFlickable*>("flickable");
    QVERIFY(flickable);
    QQuickItem *rect = window->rootObject()->findChild<QQuickItem*>("rect");
    QVERIFY(rect);

    // flickable - single touch point
    QCOMPARE(flickable->contentX(), 0.0);
    QPoint p = QPoint(100, 100);
    QTest::touchEvent(window, device).press(0, p, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(rect->position(), QPointF(200.0, 200.0));
    p -= QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p, window);
    QQuickTouchUtils::flush(window);
    p -= QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p, window);
    QQuickTouchUtils::flush(window);

    QTest::qWait(1000);

    p -= QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p, window);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).release(0, p, window);
    QQuickTouchUtils::flush(window);

    QTest::qWait(1000);

    //QVERIFY(flickable->isMovingHorizontally());
    qDebug() << "Pos: " << rect->position();
    // wait until flicking is done
    QTRY_VERIFY(!flickable->isFlicking());

    // pinch
    QPoint p1 = QPoint(40, 20);
    QPoint p2 = QPoint(60, 20);
    QTest::QTouchEventSequence pinchSequence = QTest::touchEvent(window, device);
    pinchSequence.press(0, p1, window).commit();
    QQuickTouchUtils::flush(window);
    // In order for the stationary point to remember its previous position,
    // we have to reuse the same pinchSequence object.  Otherwise if we let it
    // be destroyed and then start a new sequence, point 0 will default to being
    // stationary at 0, 0, and PinchArea will filter out that touchpoint because
    // it is outside its bounds.
    pinchSequence.stationary(0).press(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    p1 -= QPoint(10,10);
    p2 += QPoint(10,10);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    QCOMPARE(rect->scale(), 1.0);
    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    pinchSequence.release(0, p1, window).release(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    QVERIFY(rect->scale() > 1.0);
}

void tst_TouchMouse::mouseOnFlickableOnPinch()
{
    QQuickView *window = createView();
    window->setSource(testFileUrl("mouseonflickableonpinch.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->rootObject() != 0);
    QRect windowRect = QRect(window->position(), window->size());
    QCursor::setPos(windowRect.center());

    QQuickPinchArea *pinchArea = window->rootObject()->findChild<QQuickPinchArea*>("pincharea");
    QVERIFY(pinchArea);
    QQuickFlickable *flickable = window->rootObject()->findChild<QQuickFlickable*>("flickable");
    QVERIFY(flickable);
    QQuickItem *rect = window->rootObject()->findChild<QQuickItem*>("rect");
    QVERIFY(rect);

    // flickable - single touch point
    QCOMPARE(flickable->contentX(), 0.0);
    QPoint p = QPoint(100, 100);
    QTest::touchEvent(window, device).press(0, p, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(rect->position(), QPointF(200.0, 200.0));
    p -= QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p, window);
    QQuickTouchUtils::flush(window);
    p -= QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p, window);
    QQuickTouchUtils::flush(window);
    p -= QPoint(10, 0);
    QTest::touchEvent(window, device).move(0, p, window);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).release(0, p, window);
    QQuickTouchUtils::flush(window);

    //QVERIFY(flickable->isMovingHorizontally());

    // Wait for flick to end
    QTRY_VERIFY(!flickable->isMoving());
    qDebug() << "Pos: " << rect->position();

    // pinch
    QPoint p1 = QPoint(40, 20);
    QPoint p2 = QPoint(60, 20);
    QTest::QTouchEventSequence pinchSequence = QTest::touchEvent(window, device);
    pinchSequence.press(0, p1, window).commit();
    QQuickTouchUtils::flush(window);
    // In order for the stationary point to remember its previous position,
    // we have to reuse the same pinchSequence object.  Otherwise if we let it
    // be destroyed and then start a new sequence, point 0 will default to being
    // stationary at 0, 0, and PinchArea will filter out that touchpoint because
    // it is outside its bounds.
    pinchSequence.stationary(0).press(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    p1 -= QPoint(10,10);
    p2 += QPoint(10,10);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    QCOMPARE(rect->scale(), 1.0);
    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    p1 -= QPoint(10, 0);
    p2 += QPoint(10, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    pinchSequence.release(0, p1, window).release(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    QVERIFY(rect->scale() > 1.0);

    // PinchArea should steal the event after flicking started
    rect->setScale(1.0);
    flickable->setContentX(0.0);
    p = QPoint(100, 100);
    pinchSequence.press(0, p, window).commit();
    QQuickTouchUtils::flush(window);
    QCOMPARE(rect->position(), QPointF(200.0, 200.0));
    p -= QPoint(10, 0);
    pinchSequence.move(0, p, window).commit();
    QQuickTouchUtils::flush(window);
    p -= QPoint(10, 0);
    pinchSequence.move(0, p, window).commit();
    QQuickTouchUtils::flush(window);
    QGuiApplication::processEvents();
    p -= QPoint(10, 0);
    pinchSequence.move(0, p, window).commit();
    QQuickTouchUtils::flush(window);

    QCOMPARE(window->mouseGrabberItem(), flickable);

    // Add a second finger, this should lead to stealing
    p1 = QPoint(40, 100);
    p2 = QPoint(60, 100);
    pinchSequence.stationary(0).press(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    QCOMPARE(rect->scale(), 1.0);

    p1 -= QPoint(5, 0);
    p2 += QPoint(5, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    p1 -= QPoint(5, 0);
    p2 += QPoint(5, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    p1 -= QPoint(5, 0);
    p2 += QPoint(5, 0);
    pinchSequence.move(0, p1, window).move(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    pinchSequence.release(0, p1, window).release(1, p2, window).commit();
    QQuickTouchUtils::flush(window);
    QVERIFY(rect->scale() > 1.0);
    pinchSequence.release(0, p, window).commit();
    QQuickTouchUtils::flush(window);
}

/*
   Regression test for the following use case:
   You have two mouse areas, on on top of the other.
   1 - You tap the top one.
   2 - That top mouse area receives a mouse press event but doesn't accept it
   Expected outcome:
     3 - the bottom mouse area gets clicked (besides press and release mouse events)
   Bogus outcome:
     3 - the bottom mouse area gets double clicked.
 */
void tst_TouchMouse::tapOnDismissiveTopMouseAreaClicksBottomOne()
{
    QQuickView *window = createView();

    window->setSource(testFileUrl("twoMouseAreas.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(window->rootObject() != 0);

    QQuickMouseArea *bottomMouseArea =
        window->rootObject()->findChild<QQuickMouseArea*>("rear mouseArea");

    QSignalSpy bottomClickedSpy(bottomMouseArea, SIGNAL(clicked(QQuickMouseEvent*)));
    QSignalSpy bottomDoubleClickedSpy(bottomMouseArea,
                                      SIGNAL(doubleClicked(QQuickMouseEvent*)));

    // tap the front mouse area (see qml file)
    QPoint p1(20, 20);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);

    QCOMPARE(bottomClickedSpy.count(), 1);
    QCOMPARE(bottomDoubleClickedSpy.count(), 0);

    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QTest::touchEvent(window, device).release(0, p1, window);
    QQuickTouchUtils::flush(window);

    QCOMPARE(bottomClickedSpy.count(), 1);
    QCOMPARE(bottomDoubleClickedSpy.count(), 1);

    delete window;
}

/*
    If an item grabs a touch that is currently being used for mouse pointer emulation,
    the current mouse grabber should lose the mouse as mouse events will no longer
    be generated from that touch point.
 */
void tst_TouchMouse::touchGrabCausesMouseUngrab()
{
    QQuickView *window = createView();

    window->setSource(testFileUrl("twosiblingitems.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(window->rootObject() != 0);

    EventItem *leftItem = window->rootObject()->findChild<EventItem*>("leftItem");
    QVERIFY(leftItem);

    EventItem *rightItem = window->rootObject()->findChild<EventItem*>("rightItem");
    QVERIFY(leftItem);

    // Send a touch to the leftItem. But leftItem accepts only mouse events, thus
    // a mouse event will be synthesized out of this touch and will get accepted by
    // leftItem.
    leftItem->acceptMouse = true;
    leftItem->setAcceptedMouseButtons(Qt::LeftButton);
    QPoint p1;
    p1 = QPoint(leftItem->width() / 2, leftItem->height() / 2);
    QTest::touchEvent(window, device).press(0, p1, window);
    QQuickTouchUtils::flush(window);
    QCOMPARE(leftItem->eventList.size(), 2);
    QCOMPARE(leftItem->eventList.at(0).type, QEvent::TouchBegin);
    QCOMPARE(leftItem->eventList.at(1).type, QEvent::MouseButtonPress);
    QCOMPARE(window->mouseGrabberItem(), leftItem);
    leftItem->eventList.clear();

    rightItem->acceptTouch = true;
    {
        QVector<int> ids;
        ids.append(0);
        rightItem->grabTouchPoints(ids);
    }

    // leftItem should have lost the mouse as the touch point that was being used to emulate it
    // has been grabbed by another item.
    QCOMPARE(leftItem->eventList.size(), 1);
    QCOMPARE(leftItem->eventList.at(0).type, QEvent::UngrabMouse);
    QCOMPARE(window->mouseGrabberItem(), (QQuickItem*)0);

    delete window;
}

void tst_TouchMouse::touchPointDeliveryOrder()
{
    // Touch points should be first delivered to the item under the primary finger
    QScopedPointer<QQuickView> window(createView());

    window->setSource(testFileUrl("touchpointdeliveryorder.qml"));
    /*
    The items are positioned from left to right:
    |      background     |
    |   left   |
    |          |   right  |
          |  middle |
    0   150   300  450  600
    */
    QPoint pLeft = QPoint(100, 100);
    QPoint pRight = QPoint(500, 100);
    QPoint pLeftMiddle = QPoint(200, 100);
    QPoint pRightMiddle = QPoint(350, 100);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QVector<QQuickItem*> events;
    EventItem *background = window->rootObject()->findChild<EventItem*>("background");
    EventItem *left = window->rootObject()->findChild<EventItem*>("left");
    EventItem *middle = window->rootObject()->findChild<EventItem*>("middle");
    EventItem *right = window->rootObject()->findChild<EventItem*>("right");
    QVERIFY(background);
    QVERIFY(left);
    QVERIFY(middle);
    QVERIFY(right);
    connect(background, &EventItem::onTouchEvent, [&events](QQuickItem* receiver){ events.append(receiver); });
    connect(left, &EventItem::onTouchEvent, [&events](QQuickItem* receiver){ events.append(receiver); });
    connect(middle, &EventItem::onTouchEvent, [&events](QQuickItem* receiver){ events.append(receiver); });
    connect(right, &EventItem::onTouchEvent, [&events](QQuickItem* receiver){ events.append(receiver); });

    QTest::touchEvent(window.data(), device).press(0, pLeft, window.data());
    QQuickTouchUtils::flush(window.data());

    // Touch on left, then background
    QCOMPARE(events.size(), 2);
    QCOMPARE(events.at(0), left);
    QCOMPARE(events.at(1), background);
    events.clear();

    // New press events are deliverd first, the stationary point was not accepted, thus it doesn't get delivered
    QTest::touchEvent(window.data(), device).stationary(0).press(1, pRightMiddle, window.data());
    QQuickTouchUtils::flush(window.data());
    QCOMPARE(events.size(), 3);
    QCOMPARE(events.at(0), middle);
    QCOMPARE(events.at(1), right);
    QCOMPARE(events.at(2), background);
    events.clear();

    QTest::touchEvent(window.data(), device).release(0, pLeft, window.data()).release(1, pRightMiddle, window.data());
    QQuickTouchUtils::flush(window.data());
    QCOMPARE(events.size(), 0); // no accepted events

    // Two presses, the first point should come first
    QTest::touchEvent(window.data(), device).press(0, pLeft, window.data()).press(1, pRight, window.data());
    QQuickTouchUtils::flush(window.data());
    QCOMPARE(events.size(), 3);
    QCOMPARE(events.at(0), left);
    QCOMPARE(events.at(1), right);
    QCOMPARE(events.at(2), background);
    QTest::touchEvent(window.data(), device).release(0, pLeft, window.data()).release(1, pRight, window.data());
    events.clear();

    // Again, pressing right first
    QTest::touchEvent(window.data(), device).press(0, pRight, window.data()).press(1, pLeft, window.data());
    QQuickTouchUtils::flush(window.data());
    QCOMPARE(events.size(), 3);
    QCOMPARE(events.at(0), right);
    QCOMPARE(events.at(1), left);
    QCOMPARE(events.at(2), background);
    QTest::touchEvent(window.data(), device).release(0, pRight, window.data()).release(1, pLeft, window.data());
    events.clear();

    // Two presses, both hitting the middle item on top, then branching left and right, then bottom
    // Each target should be offered the events exactly once, middle first, left must come before right (id 0)
    QTest::touchEvent(window.data(), device).press(0, pLeftMiddle, window.data()).press(1, pRightMiddle, window.data());
    QCOMPARE(events.size(), 4);
    QCOMPARE(events.at(0), middle);
    QCOMPARE(events.at(1), left);
    QCOMPARE(events.at(2), right);
    QCOMPARE(events.at(3), background);
    QTest::touchEvent(window.data(), device).release(0, pLeftMiddle, window.data()).release(1, pRightMiddle, window.data());
    events.clear();

    QTest::touchEvent(window.data(), device).press(0, pRightMiddle, window.data()).press(1, pLeftMiddle, window.data());
    qDebug() << events;
    QCOMPARE(events.size(), 4);
    QCOMPARE(events.at(0), middle);
    QCOMPARE(events.at(1), right);
    QCOMPARE(events.at(2), left);
    QCOMPARE(events.at(3), background);
    QTest::touchEvent(window.data(), device).release(0, pRightMiddle, window.data()).release(1, pLeftMiddle, window.data());
}

void tst_TouchMouse::hoverEnabled()
{
    // QTouchDevice *device = new QTouchDevice;
    // device->setType(QTouchDevice::TouchScreen);
    // QWindowSystemInterface::registerTouchDevice(device);

    // Ensure the cursor is away from the window
    QCursor::setPos(0, 0);

    QQuickView *window = createView();
    window->setSource(testFileUrl("hoverMouseAreas.qml"));
    window->setPosition(10, 10);

    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QQuickItem *root = window->rootObject();
    QVERIFY(root != 0);

    QQuickMouseArea *mouseArea1 = root->findChild<QQuickMouseArea*>("mouseArea1");
    QVERIFY(mouseArea1 != 0);

    QQuickMouseArea *mouseArea2 = root->findChild<QQuickMouseArea*>("mouseArea2");
    QVERIFY(mouseArea2 != 0);

    QSignalSpy enterSpy1(mouseArea1, SIGNAL(entered()));
    QSignalSpy exitSpy1(mouseArea1, SIGNAL(exited()));
    QSignalSpy clickSpy1(mouseArea1, SIGNAL(clicked(QQuickMouseEvent *)));

    QSignalSpy enterSpy2(mouseArea2, SIGNAL(entered()));
    QSignalSpy exitSpy2(mouseArea2, SIGNAL(exited()));
    QSignalSpy clickSpy2(mouseArea2, SIGNAL(clicked(QQuickMouseEvent *)));

    QPoint p0(50, 50);
    QPoint p1(150, 150);
    QPoint p2(150, 250);

    // ------------------------- Mouse move to mouseArea1
    QTest::mouseMove(window, p1);

    QVERIFY(enterSpy1.count() == 1);
    QVERIFY(mouseArea1->hovered());
    QVERIFY(!mouseArea2->hovered());

    // ------------------------- Touch click on mouseArea1
    QTest::touchEvent(window, device).press(0, p1, window);

    QCOMPARE(enterSpy1.count(), 1);
    QCOMPARE(enterSpy2.count(), 0);
    QVERIFY(mouseArea1->pressed());
    QVERIFY(mouseArea1->hovered());
    QVERIFY(!mouseArea2->hovered());

    QTest::touchEvent(window, device).release(0, p1, window);
    QVERIFY(clickSpy1.count() == 1);
    QVERIFY(mouseArea1->hovered());
    QVERIFY(!mouseArea2->hovered());

    // ------------------------- Touch click on mouseArea2
    if (QGuiApplication::platformName().compare(QLatin1String("xcb"), Qt::CaseInsensitive) == 0)
        QSKIP("hover can be momentarily inconsistent on X11, depending on timing of flushFrameSynchronousEvents with touch and mouse movements (QTBUG-55350)");

    QTest::touchEvent(window, device).press(0, p2, window);

    QVERIFY(mouseArea1->hovered());
    QVERIFY(mouseArea2->hovered());
    QVERIFY(mouseArea2->pressed());
    QCOMPARE(enterSpy1.count(), 1);
    QCOMPARE(enterSpy2.count(), 1);

    QTest::touchEvent(window, device).release(0, p2, window);

    QVERIFY(clickSpy2.count() == 1);
    QVERIFY(mouseArea1->hovered());
    QVERIFY(!mouseArea2->hovered());
    QCOMPARE(exitSpy1.count(), 0);
    QCOMPARE(exitSpy2.count(), 1);

    // ------------------------- Another touch click on mouseArea1
    QTest::touchEvent(window, device).press(0, p1, window);

    QCOMPARE(enterSpy1.count(), 1);
    QCOMPARE(enterSpy2.count(), 1);
    QVERIFY(mouseArea1->pressed());
    QVERIFY(mouseArea1->hovered());
    QVERIFY(!mouseArea2->hovered());

    QTest::touchEvent(window, device).release(0, p1, window);
    QCOMPARE(clickSpy1.count(), 2);
    QVERIFY(mouseArea1->hovered());
    QVERIFY(!mouseArea1->pressed());
    QVERIFY(!mouseArea2->hovered());
}

QTEST_MAIN(tst_TouchMouse)

#include "tst_touchmouse.moc"

