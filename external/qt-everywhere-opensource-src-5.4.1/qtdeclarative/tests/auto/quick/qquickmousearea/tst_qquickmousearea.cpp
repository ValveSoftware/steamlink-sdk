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
#include <QtTest/QSignalSpy>
#include <QtQuick/private/qquickdrag_p.h>
#include <QtQuick/private/qquickmousearea_p.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <private/qquickflickable_p.h>
#include <QtQuick/qquickview.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlengine.h>
#include "../../shared/util.h"
#include <QtGui/qstylehints.h>
#include <QtGui/QCursor>
#include <QtGui/QScreen>

// Initialize view, set Url, center in available geometry, move mouse away if desired
static bool initView(QQuickView &v, const QUrl &url, bool moveMouseOut, QByteArray *errorMessage)
{
    v.setBaseSize(QSize(240,320));
    v.setSource(url);
    while (v.status() == QQuickView::Loading)
        QTest::qWait(10);
    if (v.status() != QQuickView::Ready) {
        foreach (const QQmlError &e, v.errors())
            errorMessage->append(e.toString().toLocal8Bit() + '\n');
        return false;
    }
    const QRect screenGeometry = v.screen()->availableGeometry();
    const QSize size = v.size();
    const QPoint offset = QPoint(size.width() / 2, size.height() / 2);
    v.setFramePosition(screenGeometry.center() - offset);
#ifndef QT_NO_CURSOR // Get the cursor out of the way.
    if (moveMouseOut)
         QCursor::setPos(v.geometry().topRight() + QPoint(100, 100));
#else
    Q_UNUSED(moveMouseOut)
#endif
    return true;
}

class tst_QQuickMouseArea: public QQmlDataTest
{
    Q_OBJECT
private slots:
    void dragProperties();
    void resetDrag();
    void dragging_data() { acceptedButton_data(); }
    void dragging();
    void dragSmoothed();
    void dragThreshold();
    void invalidDrag_data() { rejectedButton_data(); }
    void invalidDrag();
    void cancelDragging();
    void setDragOnPressed();
    void updateMouseAreaPosOnClick();
    void updateMouseAreaPosOnResize();
    void noOnClickedWithPressAndHold();
    void onMousePressRejected();
    void pressedCanceledOnWindowDeactivate_data();
    void pressedCanceledOnWindowDeactivate();
    void doubleClick_data() { acceptedButton_data(); }
    void doubleClick();
    void clickTwice_data() { acceptedButton_data(); }
    void clickTwice();
    void invalidClick_data() { rejectedButton_data(); }
    void invalidClick();
    void pressedOrdering();
    void preventStealing();
    void clickThrough();
    void hoverPosition();
    void hoverPropagation();
    void hoverVisible();
    void hoverAfterPress();
    void disableAfterPress();
    void onWheel();
    void transformedMouseArea_data();
    void transformedMouseArea();
    void pressedMultipleButtons_data();
    void pressedMultipleButtons();
    void changeAxis();
#ifndef QT_NO_CURSOR
    void cursorShape();
#endif
    void moveAndReleaseWithoutPress();
    void nestedStopAtBounds();
    void nestedStopAtBounds_data();
    void containsPress_data();
    void containsPress();

private:
    void acceptedButton_data();
    void rejectedButton_data();
};

Q_DECLARE_METATYPE(Qt::MouseButton)
Q_DECLARE_METATYPE(Qt::MouseButtons)

void tst_QQuickMouseArea::acceptedButton_data()
{
    QTest::addColumn<Qt::MouseButtons>("acceptedButtons");
    QTest::addColumn<Qt::MouseButton>("button");

    QTest::newRow("left") << Qt::MouseButtons(Qt::LeftButton) << Qt::LeftButton;
    QTest::newRow("right") << Qt::MouseButtons(Qt::RightButton) << Qt::RightButton;
    QTest::newRow("middle") << Qt::MouseButtons(Qt::MiddleButton) << Qt::MiddleButton;

    QTest::newRow("left (left|right)") << Qt::MouseButtons(Qt::LeftButton | Qt::RightButton) << Qt::LeftButton;
    QTest::newRow("right (right|middle)") << Qt::MouseButtons(Qt::RightButton | Qt::MiddleButton) << Qt::RightButton;
    QTest::newRow("middle (left|middle)") << Qt::MouseButtons(Qt::LeftButton | Qt::MiddleButton) << Qt::MiddleButton;
}

void tst_QQuickMouseArea::rejectedButton_data()
{
    QTest::addColumn<Qt::MouseButtons>("acceptedButtons");
    QTest::addColumn<Qt::MouseButton>("button");

    QTest::newRow("middle (left|right)") << Qt::MouseButtons(Qt::LeftButton | Qt::RightButton) << Qt::MiddleButton;
    QTest::newRow("left (right|middle)") << Qt::MouseButtons(Qt::RightButton | Qt::MiddleButton) << Qt::LeftButton;
    QTest::newRow("right (left|middle)") << Qt::MouseButtons(Qt::LeftButton | Qt::MiddleButton) << Qt::RightButton;
}

void tst_QQuickMouseArea::dragProperties()
{

    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("dragproperties.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QTest::qWaitForWindowExposed(&window);
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseRegion = window.rootObject()->findChild<QQuickMouseArea*>("mouseregion");
    QQuickDrag *drag = mouseRegion->drag();
    QVERIFY(mouseRegion != 0);
    QVERIFY(drag != 0);

    // target
    QQuickItem *blackRect = window.rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);
    QVERIFY(blackRect == drag->target());
    QQuickItem *rootItem = qobject_cast<QQuickItem*>(window.rootObject());
    QVERIFY(rootItem != 0);
    QSignalSpy targetSpy(drag, SIGNAL(targetChanged()));
    drag->setTarget(rootItem);
    QCOMPARE(targetSpy.count(),1);
    drag->setTarget(rootItem);
    QCOMPARE(targetSpy.count(),1);

    // axis
    QCOMPARE(drag->axis(), QQuickDrag::XAndYAxis);
    QSignalSpy axisSpy(drag, SIGNAL(axisChanged()));
    drag->setAxis(QQuickDrag::XAxis);
    QCOMPARE(drag->axis(), QQuickDrag::XAxis);
    QCOMPARE(axisSpy.count(),1);
    drag->setAxis(QQuickDrag::XAxis);
    QCOMPARE(axisSpy.count(),1);

    // minimum and maximum properties
    QSignalSpy xminSpy(drag, SIGNAL(minimumXChanged()));
    QSignalSpy xmaxSpy(drag, SIGNAL(maximumXChanged()));
    QSignalSpy yminSpy(drag, SIGNAL(minimumYChanged()));
    QSignalSpy ymaxSpy(drag, SIGNAL(maximumYChanged()));

    QCOMPARE(drag->xmin(), 0.0);
    QCOMPARE(drag->xmax(), rootItem->width()-blackRect->width());
    QCOMPARE(drag->ymin(), 0.0);
    QCOMPARE(drag->ymax(), rootItem->height()-blackRect->height());

    drag->setXmin(10);
    drag->setXmax(10);
    drag->setYmin(10);
    drag->setYmax(10);

    QCOMPARE(drag->xmin(), 10.0);
    QCOMPARE(drag->xmax(), 10.0);
    QCOMPARE(drag->ymin(), 10.0);
    QCOMPARE(drag->ymax(), 10.0);

    QCOMPARE(xminSpy.count(),1);
    QCOMPARE(xmaxSpy.count(),1);
    QCOMPARE(yminSpy.count(),1);
    QCOMPARE(ymaxSpy.count(),1);

    drag->setXmin(10);
    drag->setXmax(10);
    drag->setYmin(10);
    drag->setYmax(10);

    QCOMPARE(xminSpy.count(),1);
    QCOMPARE(xmaxSpy.count(),1);
    QCOMPARE(yminSpy.count(),1);
    QCOMPARE(ymaxSpy.count(),1);

    // filterChildren
    QSignalSpy filterChildrenSpy(drag, SIGNAL(filterChildrenChanged()));

    drag->setFilterChildren(true);

    QVERIFY(drag->filterChildren());
    QCOMPARE(filterChildrenSpy.count(), 1);

    drag->setFilterChildren(true);
    QCOMPARE(filterChildrenSpy.count(), 1);

    // threshold
    QCOMPARE(int(drag->threshold()), qApp->styleHints()->startDragDistance());
    QSignalSpy thresholdSpy(drag, SIGNAL(thresholdChanged()));
    drag->setThreshold(0.0);
    QCOMPARE(drag->threshold(), 0.0);
    QCOMPARE(thresholdSpy.count(), 1);
    drag->setThreshold(99);
    QCOMPARE(thresholdSpy.count(), 2);
    drag->setThreshold(99);
    QCOMPARE(thresholdSpy.count(), 2);
    drag->resetThreshold();
    QCOMPARE(int(drag->threshold()), qApp->styleHints()->startDragDistance());
    QCOMPARE(thresholdSpy.count(), 3);
}

void tst_QQuickMouseArea::resetDrag()
{
    QQuickView window;
    QByteArray errorMessage;
    window.rootContext()->setContextProperty("haveTarget", QVariant(true));
    QVERIFY2(initView(window, testFileUrl("dragreset.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QTest::qWaitForWindowExposed(&window);
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseRegion = window.rootObject()->findChild<QQuickMouseArea*>("mouseregion");
    QQuickDrag *drag = mouseRegion->drag();
    QVERIFY(mouseRegion != 0);
    QVERIFY(drag != 0);

    // target
    QQuickItem *blackRect = window.rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);
    QVERIFY(blackRect == drag->target());
    QQuickItem *rootItem = qobject_cast<QQuickItem*>(window.rootObject());
    QVERIFY(rootItem != 0);
    QSignalSpy targetSpy(drag, SIGNAL(targetChanged()));
    QVERIFY(drag->target() != 0);
    window.rootContext()->setContextProperty("haveTarget", QVariant(false));
    QCOMPARE(targetSpy.count(),1);
    QVERIFY(drag->target() == 0);
}

void tst_QQuickMouseArea::dragging()
{
    QFETCH(Qt::MouseButtons, acceptedButtons);
    QFETCH(Qt::MouseButton, button);

    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("dragging.qml"), true, &errorMessage), errorMessage.constData());

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseRegion = window.rootObject()->findChild<QQuickMouseArea*>("mouseregion");
    QQuickDrag *drag = mouseRegion->drag();
    QVERIFY(mouseRegion != 0);
    QVERIFY(drag != 0);

    mouseRegion->setAcceptedButtons(acceptedButtons);

    // target
    QQuickItem *blackRect = window.rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);
    QVERIFY(blackRect == drag->target());

    QVERIFY(!drag->active());

    QTest::mousePress(&window, button, 0, QPoint(100,100));

    QVERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 50.0);
    QCOMPARE(blackRect->y(), 50.0);

    // First move event triggers drag, second is acted upon.
    // This is due to possibility of higher stacked area taking precedence.
    // The item is moved relative to the position of the mouse when the drag
    // was triggered, this prevents a sudden change in position when the drag
    // threshold is exceeded.
    QTest::mouseMove(&window, QPoint(111,111), 50);
    QTest::mouseMove(&window, QPoint(116,116), 50);
    QTest::mouseMove(&window, QPoint(122,122), 50);

    QTRY_VERIFY(drag->active());
    QTRY_COMPARE(blackRect->x(), 61.0);
    QCOMPARE(blackRect->y(), 61.0);

    QTest::mouseRelease(&window, button, 0, QPoint(122,122));

    QTRY_VERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 61.0);
    QCOMPARE(blackRect->y(), 61.0);
}

void tst_QQuickMouseArea::dragSmoothed()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("dragging.qml"), true, &errorMessage), errorMessage.constData());

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseRegion = window.rootObject()->findChild<QQuickMouseArea*>("mouseregion");
    QQuickDrag *drag = mouseRegion->drag();
    drag->setThreshold(5);

    mouseRegion->setAcceptedButtons(Qt::LeftButton);
    QQuickItem *blackRect = window.rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);
    QVERIFY(blackRect == drag->target());
    QVERIFY(!drag->active());
    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(100,100));
    QVERIFY(!drag->active());
    QTest::mouseMove(&window, QPoint(100, 102), 50);
    QTest::mouseMove(&window, QPoint(100, 106), 50);
    QTest::mouseMove(&window, QPoint(100, 122), 50);
    QTRY_COMPARE(blackRect->x(), 50.0);
    QTRY_COMPARE(blackRect->y(), 66.0);
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(100,122));

    // reset rect position
    blackRect->setX(50.0);
    blackRect->setY(50.0);

    // now try with smoothed disabled
    drag->setSmoothed(false);
    QVERIFY(!drag->active());
    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(100,100));
    QVERIFY(!drag->active());
    QTest::mouseMove(&window, QPoint(100, 102), 50);
    QTest::mouseMove(&window, QPoint(100, 106), 50);
    QTest::mouseMove(&window, QPoint(100, 122), 50);
    QTRY_COMPARE(blackRect->x(), 50.0);
    QTRY_COMPARE(blackRect->y(), 72.0);
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(100, 122));
}

void tst_QQuickMouseArea::dragThreshold()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("dragging.qml"), true, &errorMessage), errorMessage.constData());

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseRegion = window.rootObject()->findChild<QQuickMouseArea*>("mouseregion");
    QQuickDrag *drag = mouseRegion->drag();

    drag->setThreshold(5);

    mouseRegion->setAcceptedButtons(Qt::LeftButton);
    QQuickItem *blackRect = window.rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);
    QVERIFY(blackRect == drag->target());
    QVERIFY(!drag->active());
    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(100,100));
    QVERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 50.0);
    QCOMPARE(blackRect->y(), 50.0);
    QTest::mouseMove(&window, QPoint(100, 102), 50);
    QVERIFY(!drag->active());
    QTest::mouseMove(&window, QPoint(100, 100), 50);
    QVERIFY(!drag->active());
    QTest::mouseMove(&window, QPoint(100, 104), 50);
    QTest::mouseMove(&window, QPoint(100, 105), 50);
    QVERIFY(!drag->active());
    QTest::mouseMove(&window, QPoint(100, 106), 50);
    QTest::mouseMove(&window, QPoint(100, 108), 50);
    QVERIFY(drag->active());
    QTest::mouseMove(&window, QPoint(100, 116), 50);
    QTest::mouseMove(&window, QPoint(100, 122), 50);
    QTRY_VERIFY(drag->active());
    QTRY_COMPARE(blackRect->x(), 50.0);
    QTRY_COMPARE(blackRect->y(), 66.0);
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(122,122));
    QTRY_VERIFY(!drag->active());

    // Immediate drag threshold
    drag->setThreshold(0);
    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(100,100));
    QTest::mouseMove(&window, QPoint(100, 122), 50);
    QVERIFY(!drag->active());
    QTest::mouseMove(&window, QPoint(100, 123), 50);
    QVERIFY(drag->active());
    QTest::mouseMove(&window, QPoint(100, 124), 50);
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(100, 124));
    QTRY_VERIFY(!drag->active());
    drag->resetThreshold();
}
void tst_QQuickMouseArea::invalidDrag()
{
    QFETCH(Qt::MouseButtons, acceptedButtons);
    QFETCH(Qt::MouseButton, button);

    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("dragging.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QTest::qWaitForWindowExposed(&window);
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseRegion = window.rootObject()->findChild<QQuickMouseArea*>("mouseregion");
    QQuickDrag *drag = mouseRegion->drag();
    QVERIFY(mouseRegion != 0);
    QVERIFY(drag != 0);

    mouseRegion->setAcceptedButtons(acceptedButtons);

    // target
    QQuickItem *blackRect = window.rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);
    QVERIFY(blackRect == drag->target());

    QVERIFY(!drag->active());

    QTest::mousePress(&window, button, 0, QPoint(100,100));

    QVERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 50.0);
    QCOMPARE(blackRect->y(), 50.0);

    // First move event triggers drag, second is acted upon.
    // This is due to possibility of higher stacked area taking precedence.

    QTest::mouseMove(&window, QPoint(111,111));
    QTest::qWait(50);
    QTest::mouseMove(&window, QPoint(122,122));
    QTest::qWait(50);

    QVERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 50.0);
    QCOMPARE(blackRect->y(), 50.0);

    QTest::mouseRelease(&window, button, 0, QPoint(122,122));
    QTest::qWait(50);

    QVERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 50.0);
    QCOMPARE(blackRect->y(), 50.0);
}

void tst_QQuickMouseArea::cancelDragging()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("dragging.qml"), true, &errorMessage), errorMessage.constData());

    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseRegion = window.rootObject()->findChild<QQuickMouseArea*>("mouseregion");
    QQuickDrag *drag = mouseRegion->drag();
    QVERIFY(mouseRegion != 0);
    QVERIFY(drag != 0);

    mouseRegion->setAcceptedButtons(Qt::LeftButton);

    // target
    QQuickItem *blackRect = window.rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);
    QVERIFY(blackRect == drag->target());

    QVERIFY(!drag->active());

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(100,100));

    QVERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 50.0);
    QCOMPARE(blackRect->y(), 50.0);

    QTest::mouseMove(&window, QPoint(111,111), 50);
    QTest::mouseMove(&window, QPoint(116,116), 50);
    QTest::mouseMove(&window, QPoint(122,122), 50);

    QTRY_VERIFY(drag->active());
    QTRY_COMPARE(blackRect->x(), 61.0);
    QCOMPARE(blackRect->y(), 61.0);

    mouseRegion->QQuickItem::ungrabMouse();
    QTRY_VERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 61.0);
    QCOMPARE(blackRect->y(), 61.0);

    QTest::mouseMove(&window, QPoint(132,132), 50);
    QTRY_VERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 61.0);
    QCOMPARE(blackRect->y(), 61.0);

    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(122,122));
}

void tst_QQuickMouseArea::setDragOnPressed()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("setDragOnPressed.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QTest::qWaitForWindowExposed(&window);
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseArea = qobject_cast<QQuickMouseArea *>(window.rootObject());
    QVERIFY(mouseArea);

    // target
    QQuickItem *target = mouseArea->findChild<QQuickItem*>("target");
    QVERIFY(target);

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(100,100));

    QQuickDrag *drag = mouseArea->drag();
    QVERIFY(drag);
    QVERIFY(!drag->active());

    QCOMPARE(target->x(), 50.0);
    QCOMPARE(target->y(), 50.0);

    // First move event triggers drag, second is acted upon.
    // This is due to possibility of higher stacked area taking precedence.

    QTest::mouseMove(&window, QPoint(111,102));
    QTest::qWait(50);
    QTest::mouseMove(&window, QPoint(122,122));
    QTest::qWait(50);

    QVERIFY(drag->active());
    QCOMPARE(target->x(), 61.0);
    QCOMPARE(target->y(), 50.0);

    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(122,122));
    QTest::qWait(50);

    QVERIFY(!drag->active());
    QCOMPARE(target->x(), 61.0);
    QCOMPARE(target->y(), 50.0);
}

void tst_QQuickMouseArea::updateMouseAreaPosOnClick()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("updateMousePosOnClick.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseRegion = window.rootObject()->findChild<QQuickMouseArea*>("mouseregion");
    QVERIFY(mouseRegion != 0);

    QQuickRectangle *rect = window.rootObject()->findChild<QQuickRectangle*>("ball");
    QVERIFY(rect != 0);

    QCOMPARE(mouseRegion->mouseX(), rect->x());
    QCOMPARE(mouseRegion->mouseY(), rect->y());

    QMouseEvent event(QEvent::MouseButtonPress, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);
    QGuiApplication::sendEvent(&window, &event);

    QCOMPARE(mouseRegion->mouseX(), 100.0);
    QCOMPARE(mouseRegion->mouseY(), 100.0);

    QCOMPARE(mouseRegion->mouseX(), rect->x());
    QCOMPARE(mouseRegion->mouseY(), rect->y());
}

void tst_QQuickMouseArea::updateMouseAreaPosOnResize()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("updateMousePosOnResize.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseRegion = window.rootObject()->findChild<QQuickMouseArea*>("mouseregion");
    QVERIFY(mouseRegion != 0);

    QQuickRectangle *rect = window.rootObject()->findChild<QQuickRectangle*>("brother");
    QVERIFY(rect != 0);

    QCOMPARE(mouseRegion->mouseX(), 0.0);
    QCOMPARE(mouseRegion->mouseY(), 0.0);

    QMouseEvent event(QEvent::MouseButtonPress, rect->position().toPoint(), Qt::LeftButton, Qt::LeftButton, 0);
    QGuiApplication::sendEvent(&window, &event);

    QVERIFY(!mouseRegion->property("emitPositionChanged").toBool());
    QVERIFY(mouseRegion->property("mouseMatchesPos").toBool());

    QCOMPARE(mouseRegion->property("x1").toReal(), 0.0);
    QCOMPARE(mouseRegion->property("y1").toReal(), 0.0);

    QCOMPARE(mouseRegion->property("x2").toReal(), rect->x());
    QCOMPARE(mouseRegion->property("y2").toReal(), rect->y());

    QCOMPARE(mouseRegion->mouseX(), rect->x());
    QCOMPARE(mouseRegion->mouseY(), rect->y());
}

void tst_QQuickMouseArea::noOnClickedWithPressAndHold()
{
    {
        // We handle onPressAndHold, therefore no onClicked
        QQuickView window;
        QByteArray errorMessage;
        QVERIFY2(initView(window, testFileUrl("clickandhold.qml"), true, &errorMessage), errorMessage.constData());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(window.rootObject() != 0);
        QQuickMouseArea *mouseArea = qobject_cast<QQuickMouseArea*>(window.rootObject()->children().first());
        QVERIFY(mouseArea);

        QMouseEvent pressEvent(QEvent::MouseButtonPress, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);
        QGuiApplication::sendEvent(&window, &pressEvent);

        QVERIFY(mouseArea->pressedButtons() == Qt::LeftButton);
        QVERIFY(!window.rootObject()->property("clicked").toBool());
        QVERIFY(!window.rootObject()->property("held").toBool());

        // timeout is 800 (in qquickmousearea.cpp)
        QTest::qWait(1000);
        QCoreApplication::processEvents();

        QVERIFY(!window.rootObject()->property("clicked").toBool());
        QVERIFY(window.rootObject()->property("held").toBool());

        QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);
        QGuiApplication::sendEvent(&window, &releaseEvent);

        QTRY_VERIFY(window.rootObject()->property("held").toBool());
        QVERIFY(!window.rootObject()->property("clicked").toBool());
    }

    {
        // We do not handle onPressAndHold, therefore we get onClicked
        QQuickView window;
        QByteArray errorMessage;
        QVERIFY2(initView(window, testFileUrl("noclickandhold.qml"), true, &errorMessage), errorMessage.constData());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(window.rootObject() != 0);

        QMouseEvent pressEvent(QEvent::MouseButtonPress, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);
        QGuiApplication::sendEvent(&window, &pressEvent);

        QVERIFY(!window.rootObject()->property("clicked").toBool());

        QTest::qWait(1000);

        QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);
        QGuiApplication::sendEvent(&window, &releaseEvent);

        QVERIFY(window.rootObject()->property("clicked").toBool());
    }
}

void tst_QQuickMouseArea::onMousePressRejected()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("rejectEvent.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);
    QVERIFY(window.rootObject()->property("enabled").toBool());

    QVERIFY(!window.rootObject()->property("mr1_pressed").toBool());
    QVERIFY(!window.rootObject()->property("mr1_released").toBool());
    QVERIFY(!window.rootObject()->property("mr1_canceled").toBool());
    QVERIFY(!window.rootObject()->property("mr2_pressed").toBool());
    QVERIFY(!window.rootObject()->property("mr2_released").toBool());
    QVERIFY(!window.rootObject()->property("mr2_canceled").toBool());

    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);
    QGuiApplication::sendEvent(&window, &pressEvent);

    QVERIFY(window.rootObject()->property("mr1_pressed").toBool());
    QVERIFY(!window.rootObject()->property("mr1_released").toBool());
    QVERIFY(!window.rootObject()->property("mr1_canceled").toBool());
    QVERIFY(window.rootObject()->property("mr2_pressed").toBool());
    QVERIFY(!window.rootObject()->property("mr2_released").toBool());
    QVERIFY(window.rootObject()->property("mr2_canceled").toBool());

    QTest::qWait(200);

    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);
    QGuiApplication::sendEvent(&window, &releaseEvent);

    QVERIFY(window.rootObject()->property("mr1_released").toBool());
    QVERIFY(!window.rootObject()->property("mr1_canceled").toBool());
    QVERIFY(!window.rootObject()->property("mr2_released").toBool());
}

void tst_QQuickMouseArea::pressedCanceledOnWindowDeactivate_data()
{
    QTest::addColumn<bool>("doubleClick");
    QTest::newRow("simple click") << false;
    QTest::newRow("double click") << true;
}


void tst_QQuickMouseArea::pressedCanceledOnWindowDeactivate()
{
    QFETCH(bool, doubleClick);

    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("pressedCanceled.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);
    QVERIFY(!window.rootObject()->property("pressed").toBool());
    QVERIFY(!window.rootObject()->property("canceled").toBool());

    int expectedRelease = 0;
    int expectedClicks = 0;
    QCOMPARE(window.rootObject()->property("released").toInt(), expectedRelease);
    QCOMPARE(window.rootObject()->property("clicked").toInt(), expectedClicks);


    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);
    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);

    QGuiApplication::sendEvent(&window, &pressEvent);

    QVERIFY(window.rootObject()->property("pressed").toBool());
    QVERIFY(!window.rootObject()->property("canceled").toBool());
    QCOMPARE(window.rootObject()->property("released").toInt(), expectedRelease);
    QCOMPARE(window.rootObject()->property("clicked").toInt(), expectedClicks);

    if (doubleClick) {
        QGuiApplication::sendEvent(&window, &releaseEvent);
        QVERIFY(!window.rootObject()->property("pressed").toBool());
        QVERIFY(!window.rootObject()->property("canceled").toBool());
        QCOMPARE(window.rootObject()->property("released").toInt(), ++expectedRelease);
        QCOMPARE(window.rootObject()->property("clicked").toInt(), ++expectedClicks);

        QGuiApplication::sendEvent(&window, &pressEvent);
        QMouseEvent pressEvent2(QEvent::MouseButtonDblClick, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);
        QGuiApplication::sendEvent(&window, &pressEvent2);

        QVERIFY(window.rootObject()->property("pressed").toBool());
        QVERIFY(!window.rootObject()->property("canceled").toBool());
        QCOMPARE(window.rootObject()->property("released").toInt(), expectedRelease);
        QCOMPARE(window.rootObject()->property("clicked").toInt(), expectedClicks);
        QCOMPARE(window.rootObject()->property("doubleClicked").toInt(), 1);
    }


    QWindow *secondWindow = qvariant_cast<QWindow*>(window.rootObject()->property("secondWindow"));
    secondWindow->setProperty("visible", true);
    QTest::qWaitForWindowExposed(secondWindow);

    QVERIFY(!window.rootObject()->property("pressed").toBool());
    QVERIFY(window.rootObject()->property("canceled").toBool());
    QCOMPARE(window.rootObject()->property("released").toInt(), expectedRelease);
    QCOMPARE(window.rootObject()->property("clicked").toInt(), expectedClicks);

    //press again
    QGuiApplication::sendEvent(&window, &pressEvent);
    QVERIFY(window.rootObject()->property("pressed").toBool());
    QVERIFY(!window.rootObject()->property("canceled").toBool());
    QCOMPARE(window.rootObject()->property("released").toInt(), expectedRelease);
    QCOMPARE(window.rootObject()->property("clicked").toInt(), expectedClicks);

    QTest::qWait(200);

    //release
    QGuiApplication::sendEvent(&window, &releaseEvent);
    QVERIFY(!window.rootObject()->property("pressed").toBool());
    QVERIFY(!window.rootObject()->property("canceled").toBool());
    QCOMPARE(window.rootObject()->property("released").toInt(), ++expectedRelease);
    QCOMPARE(window.rootObject()->property("clicked").toInt(), ++expectedClicks);
}

void tst_QQuickMouseArea::doubleClick()
{
    QFETCH(Qt::MouseButtons, acceptedButtons);
    QFETCH(Qt::MouseButton, button);

    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("doubleclick.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseArea = window.rootObject()->findChild<QQuickMouseArea *>("mousearea");
    QVERIFY(mouseArea);
    mouseArea->setAcceptedButtons(acceptedButtons);

    // The sequence for a double click is:
    // press, release, (click), press, double click, release
    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPoint(100, 100), button, button, 0);
    QGuiApplication::sendEvent(&window, &pressEvent);

    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPoint(100, 100), button, button, 0);
    QGuiApplication::sendEvent(&window, &releaseEvent);

    QCOMPARE(window.rootObject()->property("released").toInt(), 1);

    QGuiApplication::sendEvent(&window, &pressEvent);
    pressEvent = QMouseEvent(QEvent::MouseButtonDblClick, QPoint(100, 100), button, button, 0);
    QGuiApplication::sendEvent(&window, &pressEvent);
    QGuiApplication::sendEvent(&window, &releaseEvent);

    QCOMPARE(window.rootObject()->property("clicked").toInt(), 1);
    QCOMPARE(window.rootObject()->property("doubleClicked").toInt(), 1);
    QCOMPARE(window.rootObject()->property("released").toInt(), 2);
}

// QTBUG-14832
void tst_QQuickMouseArea::clickTwice()
{
    QFETCH(Qt::MouseButtons, acceptedButtons);
    QFETCH(Qt::MouseButton, button);

    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("clicktwice.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseArea = window.rootObject()->findChild<QQuickMouseArea *>("mousearea");
    QVERIFY(mouseArea);
    mouseArea->setAcceptedButtons(acceptedButtons);

    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPoint(100, 100), button, button, 0);
    QGuiApplication::sendEvent(&window, &pressEvent);

    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPoint(100, 100), button, button, 0);
    QGuiApplication::sendEvent(&window, &releaseEvent);

    QCOMPARE(window.rootObject()->property("pressed").toInt(), 1);
    QCOMPARE(window.rootObject()->property("released").toInt(), 1);
    QCOMPARE(window.rootObject()->property("clicked").toInt(), 1);

    QGuiApplication::sendEvent(&window, &pressEvent);
    pressEvent = QMouseEvent(QEvent::MouseButtonDblClick, QPoint(100, 100), button, button, 0);
    QGuiApplication::sendEvent(&window, &pressEvent);
    QGuiApplication::sendEvent(&window, &releaseEvent);

    QCOMPARE(window.rootObject()->property("pressed").toInt(), 2);
    QCOMPARE(window.rootObject()->property("released").toInt(), 2);
    QCOMPARE(window.rootObject()->property("clicked").toInt(), 2);
}

void tst_QQuickMouseArea::invalidClick()
{
    QFETCH(Qt::MouseButtons, acceptedButtons);
    QFETCH(Qt::MouseButton, button);

    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("doubleclick.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseArea = window.rootObject()->findChild<QQuickMouseArea *>("mousearea");
    QVERIFY(mouseArea);
    mouseArea->setAcceptedButtons(acceptedButtons);

    // The sequence for a double click is:
    // press, release, (click), press, double click, release
    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPoint(100, 100), button, button, 0);
    QGuiApplication::sendEvent(&window, &pressEvent);

    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPoint(100, 100), button, button, 0);
    QGuiApplication::sendEvent(&window, &releaseEvent);

    QCOMPARE(window.rootObject()->property("released").toInt(), 0);

    QGuiApplication::sendEvent(&window, &pressEvent);
    pressEvent = QMouseEvent(QEvent::MouseButtonDblClick, QPoint(100, 100), button, button, 0);
    QGuiApplication::sendEvent(&window, &pressEvent);
    QGuiApplication::sendEvent(&window, &releaseEvent);

    QCOMPARE(window.rootObject()->property("clicked").toInt(), 0);
    QCOMPARE(window.rootObject()->property("doubleClicked").toInt(), 0);
    QCOMPARE(window.rootObject()->property("released").toInt(), 0);
}

void tst_QQuickMouseArea::pressedOrdering()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("pressedOrdering.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QCOMPARE(window.rootObject()->property("value").toString(), QLatin1String("base"));

    QMouseEvent pressEvent(QEvent::MouseButtonPress, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);
    QGuiApplication::sendEvent(&window, &pressEvent);

    QCOMPARE(window.rootObject()->property("value").toString(), QLatin1String("pressed"));

    QMouseEvent releaseEvent(QEvent::MouseButtonRelease, QPoint(100, 100), Qt::LeftButton, Qt::LeftButton, 0);
    QGuiApplication::sendEvent(&window, &releaseEvent);

    QCOMPARE(window.rootObject()->property("value").toString(), QLatin1String("toggled"));

    QGuiApplication::sendEvent(&window, &pressEvent);

    QCOMPARE(window.rootObject()->property("value").toString(), QLatin1String("pressed"));
}

void tst_QQuickMouseArea::preventStealing()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("preventstealing.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window.rootObject());
    QVERIFY(flickable != 0);

    QQuickMouseArea *mouseArea = window.rootObject()->findChild<QQuickMouseArea*>("mousearea");
    QVERIFY(mouseArea != 0);

    QSignalSpy mousePositionSpy(mouseArea, SIGNAL(positionChanged(QQuickMouseEvent*)));

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(80, 80));

    // Without preventStealing, mouse movement over MouseArea would
    // cause the Flickable to steal mouse and trigger content movement.

    QTest::mouseMove(&window,QPoint(69,69));
    QTest::mouseMove(&window,QPoint(58,58));
    QTest::mouseMove(&window,QPoint(47,47));

    // We should have received all three move events
    QCOMPARE(mousePositionSpy.count(), 3);
    QVERIFY(mouseArea->pressed());

    // Flickable content should not have moved.
    QCOMPARE(flickable->contentX(), 0.);
    QCOMPARE(flickable->contentY(), 0.);

    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(47, 47));

    // Now allow stealing and confirm Flickable does its thing.
    window.rootObject()->setProperty("stealing", false);

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(80, 80));

    // Without preventStealing, mouse movement over MouseArea would
    // cause the Flickable to steal mouse and trigger content movement.

    QTest::mouseMove(&window,QPoint(69,69));
    QTest::mouseMove(&window,QPoint(58,58));
    QTest::mouseMove(&window,QPoint(47,47));

    // We should only have received the first move event
    QCOMPARE(mousePositionSpy.count(), 4);
    // Our press should be taken away
    QVERIFY(!mouseArea->pressed());

    // Flickable content should have moved.

    QCOMPARE(flickable->contentX(), 11.);
    QCOMPARE(flickable->contentY(), 11.);

    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(50, 50));
}

void tst_QQuickMouseArea::clickThrough()
{
    //With no handlers defined click, doubleClick and PressAndHold should propagate to those with handlers
    QScopedPointer<QQuickView> window(new QQuickView);
    QByteArray errorMessage;
    QVERIFY2(initView(*window.data(), testFileUrl("clickThrough.qml"), true, &errorMessage), errorMessage.constData());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(100,100));

    QTRY_COMPARE(window->rootObject()->property("presses").toInt(), 0);
    QTRY_COMPARE(window->rootObject()->property("clicks").toInt(), 1);

    // to avoid generating a double click.
    int doubleClickInterval = qApp->styleHints()->mouseDoubleClickInterval() + 10;
    QTest::qWait(doubleClickInterval);

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(1000);
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(100,100));

    QTRY_COMPARE(window->rootObject()->property("presses").toInt(), 0);
    QTRY_COMPARE(window->rootObject()->property("clicks").toInt(), 1);
    QTRY_COMPARE(window->rootObject()->property("pressAndHolds").toInt(), 1);

    QTest::mouseDClick(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(100);

    QCOMPARE(window->rootObject()->property("presses").toInt(), 0);
    QTRY_COMPARE(window->rootObject()->property("clicks").toInt(), 2);
    QTRY_COMPARE(window->rootObject()->property("doubleClicks").toInt(), 1);
    QCOMPARE(window->rootObject()->property("pressAndHolds").toInt(), 1);

    window.reset(new QQuickView);

    //With handlers defined click, doubleClick and PressAndHold should propagate only when explicitly ignored
    QVERIFY2(initView(*window.data(), testFileUrl("clickThrough2.qml"), true, &errorMessage), errorMessage.constData());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(100,100));

    QCOMPARE(window->rootObject()->property("presses").toInt(), 0);
    QCOMPARE(window->rootObject()->property("clicks").toInt(), 0);

    QTest::qWait(doubleClickInterval); // to avoid generating a double click.

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(1000);
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(100);

    QCOMPARE(window->rootObject()->property("presses").toInt(), 0);
    QCOMPARE(window->rootObject()->property("clicks").toInt(), 0);
    QCOMPARE(window->rootObject()->property("pressAndHolds").toInt(), 0);

    QTest::mouseDClick(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(100);

    QCOMPARE(window->rootObject()->property("presses").toInt(), 0);
    QCOMPARE(window->rootObject()->property("clicks").toInt(), 0);
    QCOMPARE(window->rootObject()->property("doubleClicks").toInt(), 0);
    QCOMPARE(window->rootObject()->property("pressAndHolds").toInt(), 0);

    window->rootObject()->setProperty("letThrough", QVariant(true));

    QTest::qWait(doubleClickInterval); // to avoid generating a double click.
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(100,100));

    QCOMPARE(window->rootObject()->property("presses").toInt(), 0);
    QTRY_COMPARE(window->rootObject()->property("clicks").toInt(), 1);

    QTest::qWait(doubleClickInterval); // to avoid generating a double click.
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(1000);
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(100);

    QCOMPARE(window->rootObject()->property("presses").toInt(), 0);
    QCOMPARE(window->rootObject()->property("clicks").toInt(), 1);
    QCOMPARE(window->rootObject()->property("pressAndHolds").toInt(), 1);

    QTest::mouseDClick(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(100);

    QCOMPARE(window->rootObject()->property("presses").toInt(), 0);
    QTRY_COMPARE(window->rootObject()->property("clicks").toInt(), 2);
    QCOMPARE(window->rootObject()->property("doubleClicks").toInt(), 1);
    QCOMPARE(window->rootObject()->property("pressAndHolds").toInt(), 1);

    window->rootObject()->setProperty("noPropagation", QVariant(true));

    QTest::qWait(doubleClickInterval); // to avoid generating a double click.
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(100,100));

    QTest::qWait(doubleClickInterval); // to avoid generating a double click.
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(1000);
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(100);

    QTest::mouseDClick(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(100);

    QCOMPARE(window->rootObject()->property("presses").toInt(), 0);
    QTRY_COMPARE(window->rootObject()->property("clicks").toInt(), 2);
    QCOMPARE(window->rootObject()->property("doubleClicks").toInt(), 1);
    QCOMPARE(window->rootObject()->property("pressAndHolds").toInt(), 1);

    window.reset(new QQuickView);

    //QTBUG-34368 - Shouldn't propagate to disabled mouse areas
    QVERIFY2(initView(*window.data(), testFileUrl("qtbug34368.qml"), true, &errorMessage), errorMessage.constData());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    QVERIFY(window->rootObject() != 0);

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(100,100));

    QCOMPARE(window->rootObject()->property("clicksEnabled").toInt(), 1);
    QCOMPARE(window->rootObject()->property("clicksDisabled").toInt(), 1); //Not disabled yet

    window->rootObject()->setProperty("disableLower", QVariant(true));

    QTest::qWait(doubleClickInterval); // to avoid generating a double click.
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(100,100));
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(100,100));

    QCOMPARE(window->rootObject()->property("clicksEnabled").toInt(), 2);
    QCOMPARE(window->rootObject()->property("clicksDisabled").toInt(), 1); //disabled, shouldn't increment
}

void tst_QQuickMouseArea::hoverPosition()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("hoverPosition.qml"), true, &errorMessage), errorMessage.constData());
    QQuickItem *root = window.rootObject();
    QVERIFY(root != 0);

    QCOMPARE(root->property("mouseX").toReal(), qreal(0));
    QCOMPARE(root->property("mouseY").toReal(), qreal(0));

    QTest::mouseMove(&window,QPoint(10,32));


    QCOMPARE(root->property("mouseX").toReal(), qreal(10));
    QCOMPARE(root->property("mouseY").toReal(), qreal(32));
}

void tst_QQuickMouseArea::hoverPropagation()
{
    //QTBUG-18175, to behave like GV did.
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("hoverPropagation.qml"), true, &errorMessage), errorMessage.constData());
    QQuickItem *root = window.rootObject();
    QVERIFY(root != 0);

    QCOMPARE(root->property("point1").toBool(), false);
    QCOMPARE(root->property("point2").toBool(), false);

    QMouseEvent moveEvent(QEvent::MouseMove, QPoint(32, 32), Qt::NoButton, Qt::NoButton, 0);
    QGuiApplication::sendEvent(&window, &moveEvent);

    QCOMPARE(root->property("point1").toBool(), true);
    QCOMPARE(root->property("point2").toBool(), false);

    QMouseEvent moveEvent2(QEvent::MouseMove, QPoint(232, 32), Qt::NoButton, Qt::NoButton, 0);
    QGuiApplication::sendEvent(&window, &moveEvent2);
    QCOMPARE(root->property("point1").toBool(), false);
    QCOMPARE(root->property("point2").toBool(), true);
}

void tst_QQuickMouseArea::hoverVisible()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("hoverVisible.qml"), true, &errorMessage), errorMessage.constData());
    QQuickItem *root = window.rootObject();
    QVERIFY(root != 0);

    QQuickMouseArea *mouseTracker = window.rootObject()->findChild<QQuickMouseArea*>("mousetracker");
    QVERIFY(mouseTracker != 0);

    QSignalSpy enteredSpy(mouseTracker, SIGNAL(entered()));

    // Note: We need to use a position that is different from the position in the last event
    // generated in the previous test case. Otherwise it is not interpreted as a move.
    QTest::mouseMove(&window,QPoint(11,33));

    QCOMPARE(mouseTracker->hovered(), false);
    QCOMPARE(enteredSpy.count(), 0);

    mouseTracker->setVisible(true);

    QCOMPARE(mouseTracker->hovered(), true);
    QCOMPARE(enteredSpy.count(), 1);

    QCOMPARE(QPointF(mouseTracker->mouseX(), mouseTracker->mouseY()), QPointF(11,33));
}

void tst_QQuickMouseArea::hoverAfterPress()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("hoverAfterPress.qml"), true, &errorMessage), errorMessage.constData());
    QQuickItem *root = window.rootObject();
    QVERIFY(root != 0);

    QQuickMouseArea *mouseArea = window.rootObject()->findChild<QQuickMouseArea*>("mouseArea");
    QVERIFY(mouseArea != 0);
    QTest::mouseMove(&window, QPoint(22,33));
    QCOMPARE(mouseArea->hovered(), false);
    QTest::mouseMove(&window, QPoint(200,200));
    QCOMPARE(mouseArea->hovered(), true);
    QTest::mouseMove(&window, QPoint(22,33));
    QCOMPARE(mouseArea->hovered(), false);
    QTest::mouseMove(&window, QPoint(200,200));
    QCOMPARE(mouseArea->hovered(), true);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, QPoint(200,200));
    QCOMPARE(mouseArea->hovered(), true);
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, QPoint(200,200));
    QCOMPARE(mouseArea->hovered(), true);
    QTest::mouseMove(&window, QPoint(22,33));
    QCOMPARE(mouseArea->hovered(), false);
}

void tst_QQuickMouseArea::disableAfterPress()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("dragging.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseArea = window.rootObject()->findChild<QQuickMouseArea*>("mouseregion");
    QQuickDrag *drag = mouseArea->drag();
    QVERIFY(mouseArea != 0);
    QVERIFY(drag != 0);

    QSignalSpy mousePositionSpy(mouseArea, SIGNAL(positionChanged(QQuickMouseEvent*)));
    QSignalSpy mousePressSpy(mouseArea, SIGNAL(pressed(QQuickMouseEvent*)));
    QSignalSpy mouseReleaseSpy(mouseArea, SIGNAL(released(QQuickMouseEvent*)));

    // target
    QQuickItem *blackRect = window.rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);
    QVERIFY(blackRect == drag->target());

    QVERIFY(!drag->active());

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(100,100));

    QTRY_COMPARE(mousePressSpy.count(), 1);

    QVERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 50.0);
    QCOMPARE(blackRect->y(), 50.0);

    // First move event triggers drag, second is acted upon.
    // This is due to possibility of higher stacked area taking precedence.

    QTest::mouseMove(&window, QPoint(111,111));
    QTest::qWait(50);
    QTest::mouseMove(&window, QPoint(122,122));

    QTRY_COMPARE(mousePositionSpy.count(), 2);

    QVERIFY(drag->active());
    QCOMPARE(blackRect->x(), 61.0);
    QCOMPARE(blackRect->y(), 61.0);

    mouseArea->setEnabled(false);

    // move should still be acted upon
    QTest::mouseMove(&window, QPoint(133,133));
    QTest::qWait(50);
    QTest::mouseMove(&window, QPoint(144,144));

    QTRY_COMPARE(mousePositionSpy.count(), 4);

    QVERIFY(drag->active());
    QCOMPARE(blackRect->x(), 83.0);
    QCOMPARE(blackRect->y(), 83.0);

    QVERIFY(mouseArea->pressed());
    QVERIFY(mouseArea->hovered());

    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(144,144));

    QTRY_COMPARE(mouseReleaseSpy.count(), 1);

    QVERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 83.0);
    QCOMPARE(blackRect->y(), 83.0);

    QVERIFY(!mouseArea->pressed());
    QVERIFY(!mouseArea->hovered()); // since hover is not enabled

    // Next press will be ignored
    blackRect->setX(50);
    blackRect->setY(50);

    mousePressSpy.clear();
    mousePositionSpy.clear();
    mouseReleaseSpy.clear();

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(100,100));
    QTest::qWait(50);
    QCOMPARE(mousePressSpy.count(), 0);

    QTest::mouseMove(&window, QPoint(111,111));
    QTest::qWait(50);
    QTest::mouseMove(&window, QPoint(122,122));
    QTest::qWait(50);

    QCOMPARE(mousePositionSpy.count(), 0);

    QVERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 50.0);
    QCOMPARE(blackRect->y(), 50.0);

    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(122,122));
    QTest::qWait(50);

    QCOMPARE(mouseReleaseSpy.count(), 0);
}

void tst_QQuickMouseArea::onWheel()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("wheel.qml"), true, &errorMessage), errorMessage.constData());
    QQuickItem *root = window.rootObject();
    QVERIFY(root != 0);

    QWheelEvent wheelEvent(QPoint(10, 32), QPoint(10, 32), QPoint(60, 20), QPoint(0, 120),
                           0, Qt::Vertical,Qt::NoButton, Qt::ControlModifier);
    QGuiApplication::sendEvent(&window, &wheelEvent);

    QCOMPARE(root->property("angleDeltaY").toInt(), 120);
    QCOMPARE(root->property("mouseX").toReal(), qreal(10));
    QCOMPARE(root->property("mouseY").toReal(), qreal(32));
    QCOMPARE(root->property("controlPressed").toBool(), true);
}

void tst_QQuickMouseArea::transformedMouseArea_data()
{
    QTest::addColumn<bool>("insideTarget");
    QTest::addColumn<QList<QPoint> >("points");

    QList<QPoint> pointsInside;
    pointsInside << QPoint(200, 140)
                 << QPoint(140, 200)
                 << QPoint(200, 200)
                 << QPoint(260, 200)
                 << QPoint(200, 260);
    QTest::newRow("checking points inside") << true << pointsInside;

    QList<QPoint> pointsOutside;
    pointsOutside << QPoint(140, 140)
                  << QPoint(260, 140)
                  << QPoint(120, 200)
                  << QPoint(280, 200)
                  << QPoint(140, 260)
                  << QPoint(260, 260);
    QTest::newRow("checking points outside") << false << pointsOutside;
}

void tst_QQuickMouseArea::transformedMouseArea()
{
    QFETCH(bool, insideTarget);
    QFETCH(QList<QPoint>, points);

    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("transformedMouseArea.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QVERIFY(window.rootObject() != 0);

    QQuickMouseArea *mouseArea = window.rootObject()->findChild<QQuickMouseArea *>("mouseArea");
    QVERIFY(mouseArea != 0);

    foreach (const QPoint &point, points) {
        // check hover
        QTest::mouseMove(&window, point);
        QTRY_COMPARE(mouseArea->property("containsMouse").toBool(), insideTarget);

        // check mouse press
        QTest::mousePress(&window, Qt::LeftButton, 0, point);
        QTRY_COMPARE(mouseArea->property("pressed").toBool(), insideTarget);

        // check mouse release
        QTest::mouseRelease(&window, Qt::LeftButton, 0, point);
        QTRY_COMPARE(mouseArea->property("pressed").toBool(), false);
    }
}

void tst_QQuickMouseArea::pressedMultipleButtons_data()
{
    QTest::addColumn<Qt::MouseButtons>("accepted");
    QTest::addColumn<QList<Qt::MouseButtons> >("buttons");
    QTest::addColumn<QList<bool> >("pressed");
    QTest::addColumn<QList<Qt::MouseButtons> >("pressedButtons");
    QTest::addColumn<int>("changeCount");

    QList<Qt::MouseButtons> buttons;
    QList<bool> pressed;
    QList<Qt::MouseButtons> pressedButtons;
    buttons << Qt::LeftButton
            << (Qt::LeftButton | Qt::RightButton)
            << Qt::LeftButton
            << 0;
    pressed << true
            << true
            << true
            << false;
    pressedButtons << Qt::LeftButton
            << Qt::LeftButton
            << Qt::LeftButton
            << 0;
    QTest::newRow("Accept Left - Press left, Press Right, Release Right")
            << Qt::MouseButtons(Qt::LeftButton) << buttons << pressed << pressedButtons << 2;

    buttons.clear();
    pressed.clear();
    pressedButtons.clear();
    buttons << Qt::LeftButton
            << (Qt::LeftButton | Qt::RightButton)
            << Qt::RightButton
            << 0;
    pressed << true
            << true
            << false
            << false;
    pressedButtons << Qt::LeftButton
            << Qt::LeftButton
            << 0
            << 0;
    QTest::newRow("Accept Left - Press left, Press Right, Release Left")
            << Qt::MouseButtons(Qt::LeftButton) << buttons << pressed << pressedButtons << 2;

    buttons.clear();
    pressed.clear();
    pressedButtons.clear();
    buttons << Qt::LeftButton
            << (Qt::LeftButton | Qt::RightButton)
            << Qt::LeftButton
            << 0;
    pressed << true
            << true
            << true
            << false;
    pressedButtons << Qt::LeftButton
            << (Qt::LeftButton | Qt::RightButton)
            << Qt::LeftButton
            << 0;
    QTest::newRow("Accept Left|Right - Press left, Press Right, Release Right")
        << (Qt::LeftButton | Qt::RightButton) << buttons << pressed << pressedButtons << 4;

    buttons.clear();
    pressed.clear();
    pressedButtons.clear();
    buttons << Qt::RightButton
            << (Qt::LeftButton | Qt::RightButton)
            << Qt::LeftButton
            << 0;
    pressed << true
            << true
            << false
            << false;
    pressedButtons << Qt::RightButton
            << Qt::RightButton
            << 0
            << 0;
    QTest::newRow("Accept Right - Press Right, Press Left, Release Right")
            << Qt::MouseButtons(Qt::RightButton) << buttons << pressed << pressedButtons << 2;
}

void tst_QQuickMouseArea::pressedMultipleButtons()
{
    QFETCH(Qt::MouseButtons, accepted);
    QFETCH(QList<Qt::MouseButtons>, buttons);
    QFETCH(QList<bool>, pressed);
    QFETCH(QList<Qt::MouseButtons>, pressedButtons);
    QFETCH(int, changeCount);

    QQuickView view;
    QByteArray errorMessage;
    QVERIFY2(initView(view, testFileUrl("simple.qml"), true, &errorMessage), errorMessage.constData());
    view.show();
    QTest::qWaitForWindowExposed(&view);
    QVERIFY(view.rootObject() != 0);

    QQuickMouseArea *mouseArea = view.rootObject()->findChild<QQuickMouseArea *>("mousearea");
    QVERIFY(mouseArea != 0);

    QSignalSpy pressedSpy(mouseArea, SIGNAL(pressedChanged()));
    QSignalSpy pressedButtonsSpy(mouseArea, SIGNAL(pressedButtonsChanged()));
    mouseArea->setAcceptedMouseButtons(accepted);

    QPoint point(10,10);

    for (int i = 0; i < buttons.count(); ++i) {
        int btns = buttons.at(i);

        // The windowsysteminterface takes care of sending releases
        QTest::mousePress(&view, (Qt::MouseButton)btns, 0, point);

        QCOMPARE(mouseArea->pressed(), pressed.at(i));
        QCOMPARE(mouseArea->pressedButtons(), pressedButtons.at(i));
    }

    QTest::mousePress(&view, Qt::NoButton, 0, point);
    QCOMPARE(mouseArea->pressed(), false);

    QCOMPARE(pressedSpy.count(), 2);
    QCOMPARE(pressedButtonsSpy.count(), changeCount);
}

void tst_QQuickMouseArea::changeAxis()
{
    QQuickView view;
    QByteArray errorMessage;
    QVERIFY2(initView(view, testFileUrl("changeAxis.qml"), true, &errorMessage), errorMessage.constData());
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTRY_VERIFY(view.rootObject() != 0);

    QQuickMouseArea *mouseRegion = view.rootObject()->findChild<QQuickMouseArea*>("mouseregion");
    QQuickDrag *drag = mouseRegion->drag();
    QVERIFY(mouseRegion != 0);
    QVERIFY(drag != 0);

    mouseRegion->setAcceptedButtons(Qt::LeftButton);

    // target
    QQuickItem *blackRect = view.rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);
    QVERIFY(blackRect == drag->target());

    QVERIFY(!drag->active());

    // Start a diagonal drag
    QTest::mousePress(&view, Qt::LeftButton, 0, QPoint(100, 100));

    QVERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 50.0);
    QCOMPARE(blackRect->y(), 50.0);

    QTest::mouseMove(&view, QPoint(111, 111));
    QTest::qWait(50);
    QTest::mouseMove(&view, QPoint(122, 122));

    QTRY_VERIFY(drag->active());
    QCOMPARE(blackRect->x(), 61.0);
    QCOMPARE(blackRect->y(), 61.0);
    QCOMPARE(drag->axis(), QQuickDrag::XAndYAxis);

    /* When blackRect.x becomes bigger than 75, the drag axis is changed to
     * Drag.YAxis by the QML code. Verify that this happens, and that the drag
     * movement is effectively constrained to the Y axis. */
    QTest::mouseMove(&view, QPoint(144, 144));

    QTRY_COMPARE(blackRect->x(), 83.0);
    QTRY_COMPARE(blackRect->y(), 83.0);
    QTRY_COMPARE(drag->axis(), QQuickDrag::YAxis);

    QTest::mouseMove(&view, QPoint(155, 155));

    QTRY_COMPARE(blackRect->y(), 94.0);
    QCOMPARE(blackRect->x(), 83.0);

    QTest::mouseRelease(&view, Qt::LeftButton, 0, QPoint(155, 155));

    QTRY_VERIFY(!drag->active());
    QCOMPARE(blackRect->x(), 83.0);
    QCOMPARE(blackRect->y(), 94.0);
}

#ifndef QT_NO_CURSOR
void tst_QQuickMouseArea::cursorShape()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\n MouseArea {}", QUrl());
    QScopedPointer<QObject> object(component.create());
    QQuickMouseArea *mouseArea = qobject_cast<QQuickMouseArea *>(object.data());
    QVERIFY(mouseArea);

    QSignalSpy spy(mouseArea, SIGNAL(cursorShapeChanged()));

    QCOMPARE(mouseArea->cursorShape(), Qt::ArrowCursor);
    QCOMPARE(mouseArea->cursor().shape(), Qt::ArrowCursor);

    mouseArea->setCursorShape(Qt::IBeamCursor);
    QCOMPARE(mouseArea->cursorShape(), Qt::IBeamCursor);
    QCOMPARE(mouseArea->cursor().shape(), Qt::IBeamCursor);
    QCOMPARE(spy.count(), 1);

    mouseArea->setCursorShape(Qt::IBeamCursor);
    QCOMPARE(spy.count(), 1);

    mouseArea->setCursorShape(Qt::WaitCursor);
    QCOMPARE(mouseArea->cursorShape(), Qt::WaitCursor);
    QCOMPARE(mouseArea->cursor().shape(), Qt::WaitCursor);
    QCOMPARE(spy.count(), 2);
}
#endif

void tst_QQuickMouseArea::moveAndReleaseWithoutPress()
{
    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("moveAndReleaseWithoutPress.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QObject *root = window.rootObject();
    QVERIFY(root);

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(100,100));

    QTest::mouseMove(&window, QPoint(110,110), 50);
    QTRY_COMPARE(root->property("hadMove").toBool(), false);

    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(110,110));
    QTRY_COMPARE(root->property("hadRelease").toBool(), false);
}

void tst_QQuickMouseArea::nestedStopAtBounds_data()
{
    QTest::addColumn<bool>("transpose");
    QTest::addColumn<bool>("invert");

    QTest::newRow("left") << false << false;
    QTest::newRow("right") << false << true;
    QTest::newRow("top") << true << false;
    QTest::newRow("bottom") << true << true;
}

void tst_QQuickMouseArea::nestedStopAtBounds()
{
    QFETCH(bool, transpose);
    QFETCH(bool, invert);

    QQuickView view;
    QByteArray errorMessage;
    QVERIFY2(initView(view, testFileUrl("nestedStopAtBounds.qml"), true, &errorMessage), errorMessage.constData());
    view.show();
    view.requestActivate();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.rootObject());

    QQuickMouseArea *outer =  view.rootObject()->findChild<QQuickMouseArea*>("outer");
    QVERIFY(outer);

    QQuickMouseArea *inner = outer->findChild<QQuickMouseArea*>("inner");
    QVERIFY(inner);
    inner->drag()->setAxis(transpose ? QQuickDrag::YAxis : QQuickDrag::XAxis);
    inner->setX(invert ? 100 : 0);
    inner->setY(invert ? 100 : 0);

    const int threshold = qApp->styleHints()->startDragDistance();

    QPoint position(200, 200);
    int &axis = transpose ? position.ry() : position.rx();

    // drag toward the aligned boundary.  Outer mouse area dragged.
    QTest::mousePress(&view, Qt::LeftButton, 0, position);
    QTest::qWait(10);
    axis += invert ? threshold * 2 : -threshold * 2;
    QTest::mouseMove(&view, position);
    axis += invert ? threshold : -threshold;
    QTest::mouseMove(&view, position);
    QCOMPARE(outer->drag()->active(), true);
    QCOMPARE(inner->drag()->active(), false);
    QTest::mouseRelease(&view, Qt::LeftButton, 0, position);

    QVERIFY(!outer->drag()->active());

    axis = 200;
    outer->setX(50);
    outer->setY(50);

    // drag away from the aligned boundary.  Inner mouse area dragged.
    QTest::mousePress(&view, Qt::LeftButton, 0, position);
    QTest::qWait(10);
    axis += invert ? -threshold * 2 : threshold * 2;
    QTest::mouseMove(&view, position);
    axis += invert ? -threshold : threshold;
    QTest::mouseMove(&view, position);
    QTRY_COMPARE(outer->drag()->active(), false);
    QTRY_COMPARE(inner->drag()->active(), true);
    QTest::mouseRelease(&view, Qt::LeftButton, 0, position);
}

void tst_QQuickMouseArea::containsPress_data()
{
    QTest::addColumn<bool>("hoverEnabled");

    QTest::newRow("hover enabled") << true;
    QTest::newRow("hover disaabled") << false;
}

void tst_QQuickMouseArea::containsPress()
{
    QFETCH(bool, hoverEnabled);

    QQuickView window;
    QByteArray errorMessage;
    QVERIFY2(initView(window, testFileUrl("containsPress.qml"), true, &errorMessage), errorMessage.constData());
    window.show();
    window.requestActivate();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQuickItem *root = window.rootObject();
    QVERIFY(root != 0);

    QQuickMouseArea *mouseArea = window.rootObject()->findChild<QQuickMouseArea*>("mouseArea");
    QVERIFY(mouseArea != 0);

    QSignalSpy containsPressSpy(mouseArea, SIGNAL(containsPressChanged()));

    mouseArea->setHoverEnabled(hoverEnabled);

    QTest::mouseMove(&window, QPoint(22,33));
    QCOMPARE(mouseArea->hovered(), false);
    QCOMPARE(mouseArea->pressed(), false);
    QCOMPARE(mouseArea->containsPress(), false);

    QTest::mouseMove(&window, QPoint(200,200));
    QCOMPARE(mouseArea->hovered(), hoverEnabled);
    QCOMPARE(mouseArea->pressed(), false);
    QCOMPARE(mouseArea->containsPress(), false);

    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, QPoint(200,200));
    QCOMPARE(mouseArea->hovered(), true);
    QTRY_COMPARE(mouseArea->pressed(), true);
    QCOMPARE(mouseArea->containsPress(), true);
    QCOMPARE(containsPressSpy.count(), 1);

    QTest::mouseMove(&window, QPoint(22,33));
    QCOMPARE(mouseArea->hovered(), false);
    QCOMPARE(mouseArea->pressed(), true);
    QCOMPARE(mouseArea->containsPress(), false);
    QCOMPARE(containsPressSpy.count(), 2);

    QTest::mouseMove(&window, QPoint(200,200));
    QCOMPARE(mouseArea->hovered(), true);
    QCOMPARE(mouseArea->pressed(), true);
    QCOMPARE(mouseArea->containsPress(), true);
    QCOMPARE(containsPressSpy.count(), 3);

    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, QPoint(200,200));
    QCOMPARE(mouseArea->hovered(), hoverEnabled);
    QCOMPARE(mouseArea->pressed(), false);
    QCOMPARE(mouseArea->containsPress(), false);
    QCOMPARE(containsPressSpy.count(), 4);
}

QTEST_MAIN(tst_QQuickMouseArea)

#include "tst_qquickmousearea.moc"
