/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/qtest.h>
#include <QtTest/qsignalspy.h>
#include "../shared/util.h"
#include "../shared/visualtestutil.h"

#include <QtGui/qstylehints.h>
#include <QtGui/qtouchdevice.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpa/qwindowsysteminterface.h>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtQuickTemplates2/private/qquickapplicationwindow_p.h>
#include <QtQuickTemplates2/private/qquickoverlay_p.h>
#include <QtQuickTemplates2/private/qquickpopup_p_p.h>
#include <QtQuickTemplates2/private/qquickdrawer_p.h>
#include <QtQuickTemplates2/private/qquickbutton_p.h>
#include <QtQuickTemplates2/private/qquickslider_p.h>

using namespace QQuickVisualTestUtil;

class tst_Drawer : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void visible_data();
    void visible();

    void state();

    void position_data();
    void position();

    void dragMargin_data();
    void dragMargin();

    void reposition();
    void header();

    void hover_data();
    void hover();

    void wheel_data();
    void wheel();

    void multiple();

    void touch_data();
    void touch();

    void grabber();
};

void tst_Drawer::visible_data()
{
    QTest::addColumn<QString>("source");
    QTest::newRow("Window") << "window.qml";
    QTest::newRow("ApplicationWindow") << "applicationwindow.qml";
}

void tst_Drawer::visible()
{
    QFETCH(QString, source);
    QQuickApplicationHelper helper(this, source);

    QQuickWindow *window = helper.window;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickDrawer *drawer = window->property("drawer").value<QQuickDrawer*>();
    QVERIFY(drawer);
    QQuickItem *popupItem = drawer->popupItem();

    QCOMPARE(drawer->isVisible(), false);
    QCOMPARE(drawer->position(), qreal(0.0));

    QQuickOverlay *overlay = QQuickOverlay::overlay(window);
    QVERIFY(overlay);
    QVERIFY(!overlay->childItems().contains(popupItem));

    drawer->open();
    QVERIFY(drawer->isVisible());
    QVERIFY(overlay->childItems().contains(popupItem));
    QTRY_COMPARE(drawer->position(), qreal(1.0));

    drawer->close();
    QTRY_VERIFY(!drawer->isVisible());
    QTRY_COMPARE(drawer->position(), qreal(0.0));
    QVERIFY(!overlay->childItems().contains(popupItem));

    drawer->setVisible(true);
    QVERIFY(drawer->isVisible());
    QVERIFY(overlay->childItems().contains(popupItem));
    QTRY_COMPARE(drawer->position(), qreal(1.0));

    drawer->setVisible(false);
    QTRY_VERIFY(!drawer->isVisible());
    QTRY_COMPARE(drawer->position(), qreal(0.0));
    QTRY_VERIFY(!overlay->childItems().contains(popupItem));
}

void tst_Drawer::state()
{
    QQuickApplicationHelper helper(this, "applicationwindow.qml");

    QQuickWindow *window = helper.window;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickDrawer *drawer = window->property("drawer").value<QQuickDrawer*>();
    QVERIFY(drawer);

    QCOMPARE(drawer->isVisible(), false);

    QSignalSpy visibleChangedSpy(drawer, SIGNAL(visibleChanged()));
    QSignalSpy aboutToShowSpy(drawer, SIGNAL(aboutToShow()));
    QSignalSpy aboutToHideSpy(drawer, SIGNAL(aboutToHide()));
    QSignalSpy openedSpy(drawer, SIGNAL(opened()));
    QSignalSpy closedSpy(drawer, SIGNAL(closed()));

    QVERIFY(visibleChangedSpy.isValid());
    QVERIFY(aboutToShowSpy.isValid());
    QVERIFY(aboutToHideSpy.isValid());
    QVERIFY(openedSpy.isValid());
    QVERIFY(closedSpy.isValid());

    int visibleChangedCount = 0;
    int aboutToShowCount = 0;
    int aboutToHideCount = 0;
    int openedCount = 0;
    int closedCount = 0;

    // open programmatically...
    drawer->open();
    QCOMPARE(visibleChangedSpy.count(), ++visibleChangedCount);
    QCOMPARE(aboutToShowSpy.count(), ++aboutToShowCount);
    QCOMPARE(aboutToHideSpy.count(), aboutToHideCount);
    QCOMPARE(openedSpy.count(), openedCount);
    QCOMPARE(closedSpy.count(), closedCount);

    // ...and wait until fully open
    QVERIFY(openedSpy.wait());
    QCOMPARE(visibleChangedSpy.count(), visibleChangedCount);
    QCOMPARE(aboutToShowSpy.count(), aboutToShowCount);
    QCOMPARE(aboutToHideSpy.count(), aboutToHideCount);
    QCOMPARE(openedSpy.count(), ++openedCount);
    QCOMPARE(closedSpy.count(), closedCount);

    // close programmatically...
    drawer->close();
    QCOMPARE(visibleChangedSpy.count(), visibleChangedCount);
    QCOMPARE(aboutToShowSpy.count(), aboutToShowCount);
    QCOMPARE(aboutToHideSpy.count(), ++aboutToHideCount);
    QCOMPARE(openedSpy.count(), openedCount);
    QCOMPARE(closedSpy.count(), closedCount);

    // ...and wait until fully closed
    QVERIFY(closedSpy.wait());
    QCOMPARE(visibleChangedSpy.count(), ++visibleChangedCount);
    QCOMPARE(aboutToShowSpy.count(), aboutToShowCount);
    QCOMPARE(aboutToHideSpy.count(), aboutToHideCount);
    QCOMPARE(openedSpy.count(), openedCount);
    QCOMPARE(closedSpy.count(), ++closedCount);

    // open interactively...
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(0, drawer->height() / 2));
    QTest::mouseMove(window, QPoint(drawer->width() * 0.2, drawer->height() / 2), 16);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(drawer->width() * 0.8, drawer->height() / 2), 16);
    QCOMPARE(visibleChangedSpy.count(), ++visibleChangedCount);
    QCOMPARE(aboutToShowSpy.count(), ++aboutToShowCount);
    QCOMPARE(aboutToHideSpy.count(), aboutToHideCount);
    QCOMPARE(openedSpy.count(), openedCount);
    QCOMPARE(closedSpy.count(), closedCount);

    // ...and wait until fully open
    QVERIFY(openedSpy.wait());
    QCOMPARE(visibleChangedSpy.count(), visibleChangedCount);
    QCOMPARE(aboutToShowSpy.count(), aboutToShowCount);
    QCOMPARE(aboutToHideSpy.count(), aboutToHideCount);
    QCOMPARE(openedSpy.count(), ++openedCount);
    QCOMPARE(closedSpy.count(), closedCount);

    // close interactively...
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(drawer->width(), drawer->height() / 2));
    QTest::mouseMove(window, QPoint(drawer->width() * 0.8, drawer->height() / 2), 16);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(drawer->width() * 0.2, drawer->height() / 2), 16);
    QCOMPARE(visibleChangedSpy.count(), visibleChangedCount);
    QCOMPARE(aboutToShowSpy.count(), aboutToShowCount);
    QCOMPARE(aboutToHideSpy.count(), ++aboutToHideCount);
    QCOMPARE(openedSpy.count(), openedCount);
    QCOMPARE(closedSpy.count(), closedCount);

    // ...and wait until fully closed
    QVERIFY(closedSpy.wait());
    QCOMPARE(visibleChangedSpy.count(), ++visibleChangedCount);
    QCOMPARE(aboutToShowSpy.count(), aboutToShowCount);
    QCOMPARE(aboutToHideSpy.count(), aboutToHideCount);
    QCOMPARE(openedSpy.count(), openedCount);
    QCOMPARE(closedSpy.count(), ++closedCount);
}

void tst_Drawer::position_data()
{
    QTest::addColumn<Qt::Edge>("edge");
    QTest::addColumn<QPoint>("from");
    QTest::addColumn<QPoint>("to");
    QTest::addColumn<qreal>("position");

    QTest::newRow("top") << Qt::TopEdge << QPoint(100, 0) << QPoint(100, 100) << qreal(0.5);
    QTest::newRow("left") << Qt::LeftEdge << QPoint(0, 100) << QPoint(100, 100) << qreal(0.5);
    QTest::newRow("right") << Qt::RightEdge << QPoint(399, 100) << QPoint(300, 100) << qreal(0.5);
    QTest::newRow("bottom") << Qt::BottomEdge << QPoint(100, 399) << QPoint(100, 300) << qreal(0.5);
}

void tst_Drawer::position()
{
    QFETCH(Qt::Edge, edge);
    QFETCH(QPoint, from);
    QFETCH(QPoint, to);
    QFETCH(qreal, position);

    QQuickApplicationHelper helper(this, QStringLiteral("applicationwindow.qml"));

    QQuickApplicationWindow *window = helper.appWindow;
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickDrawer *drawer = helper.appWindow->property("drawer").value<QQuickDrawer*>();
    QVERIFY(drawer);
    drawer->setEdge(edge);

    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, from);
    QTest::mouseMove(window, to);
    QCOMPARE(drawer->position(), position);

    // moved half-way open at almost infinite speed => snap to open
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, to);
    QTRY_COMPARE(drawer->position(), 1.0);
}

void tst_Drawer::dragMargin_data()
{
    QTest::addColumn<Qt::Edge>("edge");
    QTest::addColumn<qreal>("dragMargin");
    QTest::addColumn<qreal>("dragFromLeft");
    QTest::addColumn<qreal>("dragFromRight");

    QTest::newRow("left:0") << Qt::LeftEdge << qreal(0) << qreal(0) << qreal(0);
    QTest::newRow("left:-1") << Qt::LeftEdge << qreal(-1) << qreal(0) << qreal(0);
    QTest::newRow("left:startDragDistance") << Qt::LeftEdge << qreal(QGuiApplication::styleHints()->startDragDistance()) << qreal(0.45) << qreal(0);
    QTest::newRow("left:startDragDistance*2") << Qt::LeftEdge << qreal(QGuiApplication::styleHints()->startDragDistance() * 2) << qreal(0.45) << qreal(0);

    QTest::newRow("right:0") << Qt::RightEdge << qreal(0) << qreal(0) << qreal(0);
    QTest::newRow("right:-1") << Qt::RightEdge << qreal(-1) << qreal(0) << qreal(0);
    QTest::newRow("right:startDragDistance") << Qt::RightEdge << qreal(QGuiApplication::styleHints()->startDragDistance()) << qreal(0) << qreal(0.75);
    QTest::newRow("right:startDragDistance*2") << Qt::RightEdge << qreal(QGuiApplication::styleHints()->startDragDistance() * 2) << qreal(0) << qreal(0.75);
}

void tst_Drawer::dragMargin()
{
    QFETCH(Qt::Edge, edge);
    QFETCH(qreal, dragMargin);
    QFETCH(qreal, dragFromLeft);
    QFETCH(qreal, dragFromRight);

    QQuickApplicationHelper helper(this, QStringLiteral("applicationwindow.qml"));

    QQuickApplicationWindow *window = helper.appWindow;
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickDrawer *drawer = helper.appWindow->property("drawer").value<QQuickDrawer*>();
    QVERIFY(drawer);
    drawer->setEdge(edge);
    drawer->setDragMargin(dragMargin);

    // drag from the left
    int leftX = qMax<int>(0, dragMargin);
    int leftDistance = drawer->width() * 0.45;
    QVERIFY(leftDistance > QGuiApplication::styleHints()->startDragDistance());
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(leftX, drawer->height() / 2));
    QTest::mouseMove(window, QPoint(leftDistance, drawer->height() / 2));
    QCOMPARE(drawer->position(), dragFromLeft);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(leftDistance, drawer->height() / 2));

    drawer->close();
    QTRY_COMPARE(drawer->position(), qreal(0.0));

    // drag from the right
    int rightX = qMin<int>(window->width() - 1, window->width() - dragMargin);
    int rightDistance = drawer->width() * 0.75;
    QVERIFY(rightDistance > QGuiApplication::styleHints()->startDragDistance());
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(rightX, drawer->height() / 2));
    QTest::mouseMove(window, QPoint(window->width() - rightDistance, drawer->height() / 2));
    QCOMPARE(drawer->position(), dragFromRight);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() - rightDistance, drawer->height() / 2));
}

static QRectF geometry(const QQuickItem *item)
{
    return QRectF(item->x(), item->y(), item->width(), item->height());
}

void tst_Drawer::reposition()
{
    QQuickApplicationHelper helper(this, QStringLiteral("reposition.qml"));

    QQuickApplicationWindow *window = helper.appWindow;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickDrawer *drawer = window->property("drawer").value<QQuickDrawer*>();
    QVERIFY(drawer);
    QQuickItem *popupItem = drawer->popupItem();
    QVERIFY(popupItem);

    drawer->open();
    QQuickItem *dimmer = QQuickPopupPrivate::get(drawer)->dimmer;
    QVERIFY(dimmer);

    QCOMPARE(geometry(dimmer), QRectF(0, 0, window->width(), window->height()));
    QTRY_COMPARE(geometry(popupItem), QRectF(0, 0, window->width() / 2, window->height()));

    drawer->setY(100);
    QCOMPARE(geometry(dimmer), QRectF(0, 100, window->width(), window->height() - 100));
    QCOMPARE(geometry(popupItem), QRectF(0, 100, window->width() / 2, window->height() - 100));

    drawer->setHeight(window->height());
    QCOMPARE(geometry(dimmer), QRectF(0, 100, window->width(), window->height()));
    QCOMPARE(geometry(popupItem), QRectF(0, 100, window->width() / 2, window->height()));

    drawer->resetHeight();
    QCOMPARE(geometry(dimmer), QRectF(0, 100, window->width(), window->height() - 100));
    QCOMPARE(geometry(popupItem), QRectF(0, 100, window->width() / 2, window->height() - 100));

    drawer->setParentItem(window->contentItem());
    QCOMPARE(geometry(dimmer), QRectF(0, 150, window->width(), window->height() - 150));
    QCOMPARE(geometry(popupItem), QRectF(0, 150, window->width() / 2, window->height() - 150));

    drawer->setEdge(Qt::RightEdge);
    QCOMPARE(geometry(dimmer), QRectF(0, 150, window->width(), window->height() - 150));
    QTRY_COMPARE(geometry(popupItem), QRectF(window->width() - drawer->width(), 150, window->width() / 2, window->height() - 150));

    window->setWidth(window->width() + 100);
    QTRY_COMPARE(geometry(dimmer), QRectF(0, 150, window->width(), window->height() - 150));
    QTRY_COMPARE(geometry(popupItem), QRectF(window->width() - drawer->width(), 150, window->width() / 2, window->height() - 150));

    drawer->close();
    QTRY_COMPARE(geometry(popupItem), QRectF(window->width(), 150, window->width() / 2, window->height() - 150));
}

void tst_Drawer::header()
{
    QQuickApplicationHelper helper(this, QStringLiteral("header.qml"));

    QQuickApplicationWindow *window = helper.appWindow;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickItem *content = window->contentItem();
    QVERIFY(content);

    QQuickOverlay *overlay = QQuickOverlay::overlay(window);
    QVERIFY(overlay);

    QQuickDrawer *drawer = window->property("drawer").value<QQuickDrawer*>();
    QVERIFY(drawer);
    QQuickItem *popupItem = drawer->popupItem();

    QQuickButton *button = window->property("button").value<QQuickButton*>();
    QVERIFY(button);

    drawer->open();
    QVERIFY(drawer->isVisible());

    QCOMPARE(drawer->parentItem(), overlay);
    QCOMPARE(drawer->height(), overlay->height());
    QCOMPARE(popupItem->height(), overlay->height());

    drawer->setParentItem(content);
    QCOMPARE(drawer->parentItem(), content);
    QCOMPARE(drawer->height(), content->height());
    QCOMPARE(popupItem->height(), content->height());

    // must be possible to interact with the header when the drawer is below the header
    QSignalSpy clickSpy(button, SIGNAL(clicked()));
    QVERIFY(clickSpy.isValid());
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(button->x() + button->width() / 2, button->y() + button->height() / 2));
    QCOMPARE(clickSpy.count(), 1);
}

void tst_Drawer::hover_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<bool>("modal");

    QTest::newRow("Window:modal") << "window-hover.qml" << true;
    QTest::newRow("Window:modeless") << "window-hover.qml" << false;
    QTest::newRow("ApplicationWindow:modal") << "applicationwindow-hover.qml" << true;
    QTest::newRow("ApplicationWindow:modeless") << "applicationwindow-hover.qml" << false;
}

void tst_Drawer::hover()
{
    QFETCH(QString, source);
    QFETCH(bool, modal);

    QQuickApplicationHelper helper(this, source);
    QQuickWindow *window = helper.window;
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickDrawer *drawer = window->property("drawer").value<QQuickDrawer*>();
    QVERIFY(drawer);
    drawer->setModal(modal);

    QQuickControl *drawerItem = qobject_cast<QQuickControl *>(drawer->popupItem());
    QVERIFY(drawerItem);
    QVERIFY(drawerItem->isHoverEnabled());

    QQuickButton *backgroundButton = window->property("backgroundButton").value<QQuickButton*>();
    QVERIFY(backgroundButton);
    backgroundButton->setHoverEnabled(true);

    QQuickButton *drawerButton = window->property("drawerButton").value<QQuickButton*>();
    QVERIFY(drawerButton);
    drawerButton->setHoverEnabled(true);

    QSignalSpy openedSpy(drawer, SIGNAL(opened()));
    QVERIFY(openedSpy.isValid());
    drawer->open();
    QVERIFY(openedSpy.count() == 1 || openedSpy.wait());

    // hover the background button outside the drawer
    QTest::mouseMove(window, QPoint(window->width() - 1, window->height() - 1));
    QCOMPARE(backgroundButton->isHovered(), !modal);
    QVERIFY(!drawerButton->isHovered());
    QVERIFY(!drawerItem->isHovered());

    // hover the drawer background
    QTest::mouseMove(window, QPoint(1, 1));
    QVERIFY(!backgroundButton->isHovered());
    QVERIFY(!drawerButton->isHovered());
    QVERIFY(drawerItem->isHovered());

    // hover the button in a drawer
    QTest::mouseMove(window, QPoint(2, 2));
    QVERIFY(!backgroundButton->isHovered());
    QVERIFY(drawerButton->isHovered());
    QVERIFY(drawerItem->isHovered());

    QSignalSpy closedSpy(drawer, SIGNAL(closed()));
    QVERIFY(closedSpy.isValid());
    drawer->close();
    QVERIFY(closedSpy.count() == 1 || closedSpy.wait());

    // hover the background button after closing the drawer
    QTest::mouseMove(window, QPoint(window->width() / 2, window->height() / 2));
    QVERIFY(backgroundButton->isHovered());
    QVERIFY(!drawerButton->isHovered());
    QVERIFY(!drawerItem->isHovered());
}

void tst_Drawer::wheel_data()
{
    QTest::addColumn<QString>("source");
    QTest::addColumn<bool>("modal");

    QTest::newRow("Window:modal") << "window-wheel.qml" << true;
    QTest::newRow("Window:modeless") << "window-wheel.qml" << false;
    QTest::newRow("ApplicationWindow:modal") << "applicationwindow-wheel.qml" << true;
    QTest::newRow("ApplicationWindow:modeless") << "applicationwindow-wheel.qml" << false;
}

static bool sendWheelEvent(QQuickItem *item, const QPoint &localPos, int degrees)
{
    QQuickWindow *window = item->window();
    QWheelEvent wheelEvent(localPos, item->window()->mapToGlobal(localPos), QPoint(0, 0), QPoint(0, 8 * degrees), 0, Qt::Vertical, Qt::NoButton, 0);
    QSpontaneKeyEvent::setSpontaneous(&wheelEvent);
    return qGuiApp->notify(window, &wheelEvent);
}

void tst_Drawer::wheel()
{
    QFETCH(QString, source);
    QFETCH(bool, modal);

    QQuickApplicationHelper helper(this, source);
    QQuickWindow *window = helper.window;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickSlider *contentSlider = window->property("contentSlider").value<QQuickSlider*>();
    QVERIFY(contentSlider);

    QQuickDrawer *drawer = window->property("drawer").value<QQuickDrawer*>();
    QVERIFY(drawer && drawer->contentItem());
    drawer->setModal(modal);

    QQuickSlider *drawerSlider = window->property("drawerSlider").value<QQuickSlider*>();
    QVERIFY(drawerSlider);

    {
        // wheel over the content
        qreal oldContentValue = contentSlider->value();
        qreal oldDrawerValue = drawerSlider->value();

        QVERIFY(sendWheelEvent(contentSlider, QPoint(contentSlider->width() / 2, contentSlider->height() / 2), 15));

        QVERIFY(!qFuzzyCompare(contentSlider->value(), oldContentValue)); // must have moved
        QVERIFY(qFuzzyCompare(drawerSlider->value(), oldDrawerValue)); // must not have moved
    }

    QSignalSpy openedSpy(drawer, SIGNAL(opened()));
    QVERIFY(openedSpy.isValid());
    drawer->open();
    QVERIFY(openedSpy.count() == 1 || openedSpy.wait());

    {
        // wheel over the drawer content
        qreal oldContentValue = contentSlider->value();
        qreal oldDrawerValue = drawerSlider->value();

        QVERIFY(sendWheelEvent(drawerSlider, QPoint(drawerSlider->width() / 2, drawerSlider->height() / 2), 15));

        QVERIFY(qFuzzyCompare(contentSlider->value(), oldContentValue)); // must not have moved
        QVERIFY(!qFuzzyCompare(drawerSlider->value(), oldDrawerValue)); // must have moved
    }

    {
        // wheel over the overlay
        qreal oldContentValue = contentSlider->value();
        qreal oldDrawerValue = drawerSlider->value();

        QVERIFY(sendWheelEvent(QQuickOverlay::overlay(window), QPoint(0, 0), 15));

        if (modal) {
            // the content below a modal overlay must not move
            QVERIFY(qFuzzyCompare(contentSlider->value(), oldContentValue));
        } else {
            // the content below a modeless overlay must move
            QVERIFY(!qFuzzyCompare(contentSlider->value(), oldContentValue));
        }
        QVERIFY(qFuzzyCompare(drawerSlider->value(), oldDrawerValue)); // must not have moved
    }
}

void tst_Drawer::multiple()
{
    QQuickApplicationHelper helper(this, QStringLiteral("multiple.qml"));
    QQuickWindow *window = helper.window;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickDrawer *leftDrawer = window->property("leftDrawer").value<QQuickDrawer*>();
    QVERIFY(leftDrawer);
    QQuickButton *leftButton = window->property("leftButton").value<QQuickButton*>();
    QVERIFY(leftButton);
    QSignalSpy leftClickSpy(leftButton, SIGNAL(clicked()));
    QVERIFY(leftClickSpy.isValid());

    QQuickDrawer *rightDrawer = window->property("rightDrawer").value<QQuickDrawer*>();
    QVERIFY(rightDrawer);
    QQuickButton *rightButton = window->property("rightButton").value<QQuickButton*>();
    QVERIFY(rightButton);
    QSignalSpy rightClickSpy(rightButton, SIGNAL(clicked()));
    QVERIFY(rightClickSpy.isValid());

    QQuickButton *contentButton = window->property("contentButton").value<QQuickButton*>();
    QVERIFY(contentButton);
    QSignalSpy contentClickSpy(contentButton, SIGNAL(clicked()));
    QVERIFY(contentClickSpy.isValid());

    // no drawers open, click the content
    QTest::mouseClick(window, Qt::LeftButton);
    QCOMPARE(contentClickSpy.count(), 1);
    QCOMPARE(leftClickSpy.count(), 0);
    QCOMPARE(rightClickSpy.count(), 0);

    // drag the left drawer open
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(0, window->height() / 2));
    QTest::mouseMove(window, QPoint(leftDrawer->width() / 2, window->height() / 2));
    QCOMPARE(leftDrawer->position(), 0.5);
    QCOMPARE(rightDrawer->position(), 0.0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(leftDrawer->width() / 2, window->height() / 2));
    QTRY_COMPARE(leftDrawer->position(), 1.0);
    QCOMPARE(rightDrawer->position(), 0.0);

    // cannot drag the right drawer while the left drawer is open
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() - 1, window->height() / 2));
    QTest::mouseMove(window, QPoint(window->width() - leftDrawer->width() / 2, window->height() / 2));
    QCOMPARE(leftDrawer->position(), 1.0);
    QCOMPARE(rightDrawer->position(), 0.0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() - leftDrawer->width() / 2, window->height() / 2));
    QCOMPARE(rightDrawer->position(), 0.0);
    QCOMPARE(leftDrawer->position(), 1.0);

    // open the right drawer below the left drawer
    rightDrawer->open();
    QTRY_COMPARE(rightDrawer->position(), 1.0);

    // click the left drawer's button
    QTest::mouseClick(window, Qt::LeftButton);
    QCOMPARE(contentClickSpy.count(), 1);
    QCOMPARE(leftClickSpy.count(), 1);
    QCOMPARE(rightClickSpy.count(), 0);

    // click the left drawer's background (button disabled, don't leak through to the right drawer below)
    leftButton->setEnabled(false);
    QTest::mouseClick(window, Qt::LeftButton);
    QCOMPARE(contentClickSpy.count(), 1);
    QCOMPARE(leftClickSpy.count(), 1);
    QCOMPARE(rightClickSpy.count(), 0);
    leftButton->setEnabled(true);

    // click the overlay of the left drawer (don't leak through to right drawer below)
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() - (window->width() - leftDrawer->width()) / 2, window->height() / 2));
    QCOMPARE(contentClickSpy.count(), 1);
    QCOMPARE(leftClickSpy.count(), 1);
    QCOMPARE(rightClickSpy.count(), 0);
    QTRY_VERIFY(!leftDrawer->isVisible());

    // click the right drawer's button
    QTest::mouseClick(window, Qt::LeftButton);
    QCOMPARE(contentClickSpy.count(), 1);
    QCOMPARE(leftClickSpy.count(), 1);
    QCOMPARE(rightClickSpy.count(), 1);

    // cannot drag the left drawer while the right drawer is open
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(0, window->height() / 2));
    QTest::mouseMove(window, QPoint(leftDrawer->width() / 2, window->height() / 2));
    QCOMPARE(leftDrawer->position(), 0.0);
    QCOMPARE(rightDrawer->position(), 1.0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(leftDrawer->width() / 2, window->height() / 2));
    QCOMPARE(leftDrawer->position(), 0.0);
    QCOMPARE(rightDrawer->position(), 1.0);

    // click the right drawer's background (button disabled, don't leak through to the content below)
    rightButton->setEnabled(false);
    QTest::mouseClick(window, Qt::LeftButton);
    QCOMPARE(contentClickSpy.count(), 1);
    QCOMPARE(leftClickSpy.count(), 1);
    QCOMPARE(rightClickSpy.count(), 1);
    rightButton->setEnabled(true);

    // click the overlay of the right drawer (don't leak through to the content below)
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint((window->width() - rightDrawer->width()) / 2, window->height() / 2));
    QCOMPARE(contentClickSpy.count(), 1);
    QCOMPARE(leftClickSpy.count(), 1);
    QCOMPARE(rightClickSpy.count(), 1);
    QTRY_VERIFY(!rightDrawer->isVisible());

    // no drawers open, click the content
    QTest::mouseClick(window, Qt::LeftButton);
    QCOMPARE(contentClickSpy.count(), 2);
    QCOMPARE(leftClickSpy.count(), 1);
    QCOMPARE(rightClickSpy.count(), 1);

    // drag the right drawer open
    QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() - 1, window->height() / 2));
    QTest::mouseMove(window, QPoint(window->width() - rightDrawer->width() / 2, window->height() / 2));
    QCOMPARE(rightDrawer->position(), 0.5);
    QCOMPARE(leftDrawer->position(), 0.0);
    QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, QPoint(window->width() - rightDrawer->width() / 2, window->height() / 2));
    QTRY_COMPARE(rightDrawer->position(), 1.0);
    QCOMPARE(leftDrawer->position(), 0.0);
}

void tst_Drawer::touch_data()
{
    QTest::addColumn<QString>("source");
    QTest::newRow("Window") << "window.qml";
    QTest::newRow("ApplicationWindow") << "applicationwindow.qml";
}

void tst_Drawer::touch()
{
    QFETCH(QString, source);
    QQuickApplicationHelper helper(this, source);

    QQuickWindow *window = helper.window;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickDrawer *drawer = window->property("drawer").value<QQuickDrawer*>();
    QVERIFY(drawer);

    QSignalSpy drawerOpenedSpy(drawer, SIGNAL(opened()));
    QSignalSpy drawerClosedSpy(drawer, SIGNAL(closed()));
    QVERIFY(drawerOpenedSpy.isValid());
    QVERIFY(drawerClosedSpy.isValid());

    struct TouchDeviceDeleter
    {
        static inline void cleanup(QTouchDevice *device)
        {
            QWindowSystemInterface::unregisterTouchDevice(device);
            delete device;
        }
    };

    QScopedPointer<QTouchDevice, TouchDeviceDeleter> device(new QTouchDevice);
    device->setType(QTouchDevice::TouchScreen);
    QWindowSystemInterface::registerTouchDevice(device.data());

    // drag to open
    QTest::touchEvent(window, device.data()).press(0, QPoint(0, 100));
    QTest::touchEvent(window, device.data()).move(0, QPoint(100, 100));
    QTRY_COMPARE(drawer->position(), 0.5);
    QTest::touchEvent(window, device.data()).release(0, QPoint(100, 100));
    QVERIFY(drawerOpenedSpy.wait());
    QCOMPARE(drawer->position(), 1.0);

    // drag to close
    QTest::touchEvent(window, device.data()).press(0, QPoint(300, 100));
    QTest::touchEvent(window, device.data()).move(0, QPoint(300 - drawer->dragMargin(), 100));
    for (int x = 300; x > 100; x -= 10) {
        QTest::touchEvent(window, device.data()).move(0, QPoint(x, 100));
        QQuickWindowPrivate::get(window)->flushFrameSynchronousEvents();
    }
    QTest::touchEvent(window, device.data()).move(0, QPoint(100, 100));
    QTRY_COMPARE(drawer->position(), 0.5);
    QTest::touchEvent(window, device.data()).release(0, QPoint(100, 100));
    QVERIFY(drawerClosedSpy.wait());
    QCOMPARE(drawer->position(), 0.0);
}

void tst_Drawer::grabber()
{
    QQuickApplicationHelper helper(this, QStringLiteral("grabber.qml"));
    QQuickWindow *window = helper.window;
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickDrawer *drawer = window->property("drawer").value<QQuickDrawer *>();
    QVERIFY(drawer);

    QSignalSpy drawerOpenedSpy(drawer, SIGNAL(opened()));
    QSignalSpy drawerClosedSpy(drawer, SIGNAL(closed()));
    QVERIFY(drawerOpenedSpy.isValid());
    QVERIFY(drawerClosedSpy.isValid());

    drawer->open();
    QVERIFY(drawerOpenedSpy.wait());

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(300, 100));
    QVERIFY(drawerClosedSpy.wait());

    QQuickPopup *popup = window->property("popup").value<QQuickPopup *>();
    QVERIFY(popup);

    QSignalSpy popupOpenedSpy(popup, SIGNAL(opened()));
    QSignalSpy popupClosedSpy(popup, SIGNAL(closed()));
    QVERIFY(popupOpenedSpy.isValid());
    QVERIFY(popupClosedSpy.isValid());

    popup->open();
    QTRY_COMPARE(popupOpenedSpy.count(), 1);

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(100, 300));
    QTRY_COMPARE(popupClosedSpy.count(), 1);
}

QTEST_MAIN(tst_Drawer)

#include "tst_drawer.moc"
