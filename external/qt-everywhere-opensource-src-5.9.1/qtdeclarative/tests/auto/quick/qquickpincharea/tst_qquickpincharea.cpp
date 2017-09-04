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
#include <QtTest/QSignalSpy>
#include <QtGui/QStyleHints>
#include <qpa/qwindowsysteminterface.h>
#include <private/qquickpincharea_p.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQuick/qquickview.h>
#include <QtQml/qqmlcontext.h>
#include "../../shared/util.h"
#include "../shared/viewtestutil.h"

class tst_QQuickPinchArea: public QQmlDataTest
{
    Q_OBJECT
public:
    tst_QQuickPinchArea() : device(0) { }
private slots:
    void initTestCase();
    void cleanupTestCase();
    void pinchProperties();
    void scale();
    void pan();
    void retouch();
    void cancel();
    void transformedPinchArea_data();
    void transformedPinchArea();

private:
    QQuickView *createView();
    QTouchDevice *device;
};
void tst_QQuickPinchArea::initTestCase()
{
    QQmlDataTest::initTestCase();
    if (!device) {
        device = new QTouchDevice;
        device->setType(QTouchDevice::TouchScreen);
        QWindowSystemInterface::registerTouchDevice(device);
    }
}

void tst_QQuickPinchArea::cleanupTestCase()
{

}
void tst_QQuickPinchArea::pinchProperties()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("pinchproperties.qml"));
    window->show();
    QVERIFY(window->rootObject() != 0);

    QQuickPinchArea *pinchArea = window->rootObject()->findChild<QQuickPinchArea*>("pincharea");
    QQuickPinch *pinch = pinchArea->pinch();
    QVERIFY(pinchArea != 0);
    QVERIFY(pinch != 0);

    // target
    QQuickItem *blackRect = window->rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);
    QCOMPARE(blackRect, pinch->target());
    QQuickItem *rootItem = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(rootItem != 0);
    QSignalSpy targetSpy(pinch, SIGNAL(targetChanged()));
    pinch->setTarget(rootItem);
    QCOMPARE(targetSpy.count(),1);
    pinch->setTarget(rootItem);
    QCOMPARE(targetSpy.count(),1);

    // axis
    QCOMPARE(pinch->axis(), QQuickPinch::XAndYAxis);
    QSignalSpy axisSpy(pinch, SIGNAL(dragAxisChanged()));
    pinch->setAxis(QQuickPinch::XAxis);
    QCOMPARE(pinch->axis(), QQuickPinch::XAxis);
    QCOMPARE(axisSpy.count(),1);
    pinch->setAxis(QQuickPinch::XAxis);
    QCOMPARE(axisSpy.count(),1);

    // minimum and maximum drag properties
    QSignalSpy xminSpy(pinch, SIGNAL(minimumXChanged()));
    QSignalSpy xmaxSpy(pinch, SIGNAL(maximumXChanged()));
    QSignalSpy yminSpy(pinch, SIGNAL(minimumYChanged()));
    QSignalSpy ymaxSpy(pinch, SIGNAL(maximumYChanged()));

    QCOMPARE(pinch->xmin(), 0.0);
    QCOMPARE(pinch->xmax(), rootItem->width()-blackRect->width());
    QCOMPARE(pinch->ymin(), 0.0);
    QCOMPARE(pinch->ymax(), rootItem->height()-blackRect->height());

    pinch->setXmin(10);
    pinch->setXmax(10);
    pinch->setYmin(10);
    pinch->setYmax(10);

    QCOMPARE(pinch->xmin(), 10.0);
    QCOMPARE(pinch->xmax(), 10.0);
    QCOMPARE(pinch->ymin(), 10.0);
    QCOMPARE(pinch->ymax(), 10.0);

    QCOMPARE(xminSpy.count(),1);
    QCOMPARE(xmaxSpy.count(),1);
    QCOMPARE(yminSpy.count(),1);
    QCOMPARE(ymaxSpy.count(),1);

    pinch->setXmin(10);
    pinch->setXmax(10);
    pinch->setYmin(10);
    pinch->setYmax(10);

    QCOMPARE(xminSpy.count(),1);
    QCOMPARE(xmaxSpy.count(),1);
    QCOMPARE(yminSpy.count(),1);
    QCOMPARE(ymaxSpy.count(),1);

    // minimum and maximum scale properties
    QSignalSpy scaleMinSpy(pinch, SIGNAL(minimumScaleChanged()));
    QSignalSpy scaleMaxSpy(pinch, SIGNAL(maximumScaleChanged()));

    QCOMPARE(pinch->minimumScale(), 1.0);
    QCOMPARE(pinch->maximumScale(), 2.0);

    pinch->setMinimumScale(0.5);
    pinch->setMaximumScale(1.5);

    QCOMPARE(pinch->minimumScale(), 0.5);
    QCOMPARE(pinch->maximumScale(), 1.5);

    QCOMPARE(scaleMinSpy.count(),1);
    QCOMPARE(scaleMaxSpy.count(),1);

    pinch->setMinimumScale(0.5);
    pinch->setMaximumScale(1.5);

    QCOMPARE(scaleMinSpy.count(),1);
    QCOMPARE(scaleMaxSpy.count(),1);

    // minimum and maximum rotation properties
    QSignalSpy rotMinSpy(pinch, SIGNAL(minimumRotationChanged()));
    QSignalSpy rotMaxSpy(pinch, SIGNAL(maximumRotationChanged()));

    QCOMPARE(pinch->minimumRotation(), 0.0);
    QCOMPARE(pinch->maximumRotation(), 90.0);

    pinch->setMinimumRotation(-90.0);
    pinch->setMaximumRotation(45.0);

    QCOMPARE(pinch->minimumRotation(), -90.0);
    QCOMPARE(pinch->maximumRotation(), 45.0);

    QCOMPARE(rotMinSpy.count(),1);
    QCOMPARE(rotMaxSpy.count(),1);

    pinch->setMinimumRotation(-90.0);
    pinch->setMaximumRotation(45.0);

    QCOMPARE(rotMinSpy.count(),1);
    QCOMPARE(rotMaxSpy.count(),1);
}

QTouchEvent::TouchPoint makeTouchPoint(int id, QPoint p, QQuickView *v, QQuickItem *i)
{
    QTouchEvent::TouchPoint touchPoint(id);
    touchPoint.setPos(i->mapFromScene(p));
    touchPoint.setScreenPos(v->mapToGlobal(p));
    touchPoint.setScenePos(p);
    return touchPoint;
}

void tst_QQuickPinchArea::scale()
{
    QQuickView *window = createView();
    QScopedPointer<QQuickView> scope(window);
    window->setSource(testFileUrl("pinchproperties.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->rootObject() != 0);
    qApp->processEvents();

    QQuickPinchArea *pinchArea = window->rootObject()->findChild<QQuickPinchArea*>("pincharea");
    QQuickPinch *pinch = pinchArea->pinch();
    QVERIFY(pinchArea != 0);
    QVERIFY(pinch != 0);

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root != 0);

    // target
    QQuickItem *blackRect = window->rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);

    QPoint p1(80, 80);
    QPoint p2(100, 100);
    {
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
        pinchSequence.move(0, p1,window).move(1, p2,window).commit();
        QQuickTouchUtils::flush(window);

        QCOMPARE(root->property("scale").toReal(), 1.0);
        QVERIFY(root->property("pinchActive").toBool());

        p1 -= QPoint(10,10);
        p2 += QPoint(10,10);
        pinchSequence.move(0, p1,window).move(1, p2,window).commit();
        QQuickTouchUtils::flush(window);

        QCOMPARE(root->property("scale").toReal(), 1.5);
        QCOMPARE(root->property("center").toPointF(), QPointF(40, 40)); // blackrect is at 50,50
        QCOMPARE(blackRect->scale(), 1.5);
    }

    // scale beyond bound
    p1 -= QPoint(50,50);
    p2 += QPoint(50,50);
    {
        QTest::QTouchEventSequence pinchSequence = QTest::touchEvent(window, device);
        pinchSequence.move(0, p1, window).move(1, p2, window).commit();
        QQuickTouchUtils::flush(window);
        QCOMPARE(blackRect->scale(), 2.0);
        pinchSequence.release(0, p1, window).release(1, p2, window).commit();
        QQuickTouchUtils::flush(window);
    }
    QVERIFY(!root->property("pinchActive").toBool());
}

void tst_QQuickPinchArea::pan()
{
    QQuickView *window = createView();
    QScopedPointer<QQuickView> scope(window);
    window->setSource(testFileUrl("pinchproperties.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->rootObject() != 0);
    qApp->processEvents();

    QQuickPinchArea *pinchArea = window->rootObject()->findChild<QQuickPinchArea*>("pincharea");
    QQuickPinch *pinch = pinchArea->pinch();
    QVERIFY(pinchArea != 0);
    QVERIFY(pinch != 0);

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root != 0);

    // target
    QQuickItem *blackRect = window->rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);

    QPoint p1(80, 80);
    QPoint p2(100, 100);
    {
        const int dragThreshold = QGuiApplication::styleHints()->startDragDistance();
        QTest::QTouchEventSequence pinchSequence = QTest::touchEvent(window, device);
        pinchSequence.press(0, p1, window).commit();
        QQuickTouchUtils::flush(window);
        // In order for the stationary point to remember its previous position,
        // we have to reuse the same pinchSequence object.
        pinchSequence.stationary(0).press(1, p2, window).commit();
        QQuickTouchUtils::flush(window);
        QVERIFY(!root->property("pinchActive").toBool());
        QCOMPARE(root->property("scale").toReal(), -1.0);

        p1 += QPoint(dragThreshold - 1, 0);
        p2 += QPoint(dragThreshold - 1, 0);
        pinchSequence.move(0, p1, window).move(1, p2, window).commit();
        QQuickTouchUtils::flush(window);
        // movement < dragThreshold: pinch not yet active
        QVERIFY(!root->property("pinchActive").toBool());
        QCOMPARE(root->property("scale").toReal(), -1.0);

        // exactly the dragThreshold: pinch starts
        p1 += QPoint(1, 0);
        p2 += QPoint(1, 0);
        pinchSequence.move(0, p1, window).move(1, p2, window).commit();
        QQuickTouchUtils::flush(window);
        QVERIFY(root->property("pinchActive").toBool());
        QCOMPARE(root->property("scale").toReal(), 1.0);

        // Calculation of the center point is tricky at first:
        // center point of the two touch points in item coordinates:
        // scene coordinates: (80, 80) + (dragThreshold, 0), (100, 100) + (dragThreshold, 0)
        //                    = ((180+dT)/2, 180/2) = (90+dT, 90)
        // item  coordinates: (scene) - (50, 50) = (40+dT, 40)
        QCOMPARE(root->property("center").toPointF(), QPointF(40 + dragThreshold, 40));
        // pan started, but no actual movement registered yet:
        // blackrect starts at 50,50
        QCOMPARE(blackRect->x(), 50.0);
        QCOMPARE(blackRect->y(), 50.0);

        p1 += QPoint(10, 0);
        p2 += QPoint(10, 0);
        pinchSequence.move(0, p1, window).move(1, p2, window).commit();
        QQuickTouchUtils::flush(window);
        QCOMPARE(root->property("center").toPointF(), QPointF(40 + 10 + dragThreshold, 40));
        QCOMPARE(blackRect->x(), 60.0);
        QCOMPARE(blackRect->y(), 50.0);

        p1 += QPoint(0, 10);
        p2 += QPoint(0, 10);
        pinchSequence.move(0, p1, window).move(1, p2, window).commit();
        QQuickTouchUtils::flush(window);
        // next big surprise: the center is in item local coordinates and the item was just
        // moved 10 to the right... which offsets the center point 10 to the left
        QCOMPARE(root->property("center").toPointF(), QPointF(40 + 10 - 10 + dragThreshold, 40 + 10));
        QCOMPARE(blackRect->x(), 60.0);
        QCOMPARE(blackRect->y(), 60.0);

        p1 += QPoint(10, 10);
        p2 += QPoint(10, 10);
        pinchSequence.move(0, p1, window).move(1, p2, window).commit();
        QQuickTouchUtils::flush(window);
        // now the item moved again, thus the center point of the touch is moved in total by (10, 10)
        QCOMPARE(root->property("center").toPointF(), QPointF(50 + dragThreshold, 50));
        QCOMPARE(blackRect->x(), 70.0);
        QCOMPARE(blackRect->y(), 70.0);
    }

    // pan x beyond bound
    p1 += QPoint(100,100);
    p2 += QPoint(100,100);
    QTest::touchEvent(window, device).move(0, p1, window).move(1, p2, window);
    QQuickTouchUtils::flush(window);

    QCOMPARE(blackRect->x(), 140.0);
    QCOMPARE(blackRect->y(), 170.0);

    QTest::touchEvent(window, device).release(0, p1, window).release(1, p2, window);
    QQuickTouchUtils::flush(window);
    QVERIFY(!root->property("pinchActive").toBool());
}

// test pinch, release one point, touch again to continue pinch
void tst_QQuickPinchArea::retouch()
{
    QQuickView *window = createView();
    QScopedPointer<QQuickView> scope(window);
    window->setSource(testFileUrl("pinchproperties.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->rootObject() != 0);
    qApp->processEvents();

    QQuickPinchArea *pinchArea = window->rootObject()->findChild<QQuickPinchArea*>("pincharea");
    QQuickPinch *pinch = pinchArea->pinch();
    QVERIFY(pinchArea != 0);
    QVERIFY(pinch != 0);

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root != 0);

    QSignalSpy startedSpy(pinchArea, SIGNAL(pinchStarted(QQuickPinchEvent*)));
    QSignalSpy finishedSpy(pinchArea, SIGNAL(pinchFinished(QQuickPinchEvent*)));

    // target
    QQuickItem *blackRect = window->rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);

    QPoint p1(80, 80);
    QPoint p2(100, 100);
    {
        QTest::QTouchEventSequence pinchSequence = QTest::touchEvent(window, device);
        pinchSequence.press(0, p1, window).commit();
        QQuickTouchUtils::flush(window);
        // In order for the stationary point to remember its previous position,
        // we have to reuse the same pinchSequence object.
        pinchSequence.stationary(0).press(1, p2, window).commit();
        QQuickTouchUtils::flush(window);
        p1 -= QPoint(10,10);
        p2 += QPoint(10,10);
        pinchSequence.move(0, p1,window).move(1, p2,window).commit();
        QQuickTouchUtils::flush(window);

        QCOMPARE(root->property("scale").toReal(), 1.0);
        QVERIFY(root->property("pinchActive").toBool());

        p1 -= QPoint(10,10);
        p2 += QPoint(10,10);
        pinchSequence.move(0, p1,window).move(1, p2,window).commit();
        QQuickTouchUtils::flush(window);

        QCOMPARE(startedSpy.count(), 1);

        QCOMPARE(root->property("scale").toReal(), 1.5);
        QCOMPARE(root->property("center").toPointF(), QPointF(40, 40)); // blackrect is at 50,50
        QCOMPARE(blackRect->scale(), 1.5);

        QCOMPARE(window->rootObject()->property("pointCount").toInt(), 2);

        QCOMPARE(startedSpy.count(), 1);
        QCOMPARE(finishedSpy.count(), 0);

        // Hold down the first finger but release the second one
        pinchSequence.stationary(0).release(1, p2, window).commit();
        QQuickTouchUtils::flush(window);

        QCOMPARE(startedSpy.count(), 1);
        QCOMPARE(finishedSpy.count(), 0);

        QCOMPARE(window->rootObject()->property("pointCount").toInt(), 1);

        // Keep holding down the first finger and re-touch the second one, then move them both
        pinchSequence.stationary(0).press(1, p2, window).commit();
        QQuickTouchUtils::flush(window);
        p1 -= QPoint(10,10);
        p2 += QPoint(10,10);
        pinchSequence.move(0, p1, window).move(1, p2, window).commit();
        QQuickTouchUtils::flush(window);

        // Lifting and retouching results in onPinchStarted being called again
        QCOMPARE(startedSpy.count(), 2);
        QCOMPARE(finishedSpy.count(), 0);

        QCOMPARE(window->rootObject()->property("pointCount").toInt(), 2);

        pinchSequence.release(0, p1, window).release(1, p2, window).commit();
        QQuickTouchUtils::flush(window);

        QVERIFY(!root->property("pinchActive").toBool());
        QCOMPARE(startedSpy.count(), 2);
        QCOMPARE(finishedSpy.count(), 1);
    }
}

void tst_QQuickPinchArea::cancel()
{
    QQuickView *window = createView();
    QScopedPointer<QQuickView> scope(window);
    window->setSource(testFileUrl("pinchproperties.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->rootObject() != 0);
    qApp->processEvents();

    QQuickPinchArea *pinchArea = window->rootObject()->findChild<QQuickPinchArea*>("pincharea");
    QQuickPinch *pinch = pinchArea->pinch();
    QVERIFY(pinchArea != 0);
    QVERIFY(pinch != 0);

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root != 0);

    // target
    QQuickItem *blackRect = window->rootObject()->findChild<QQuickItem*>("blackrect");
    QVERIFY(blackRect != 0);

    QPoint p1(80, 80);
    QPoint p2(100, 100);
    {
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
        pinchSequence.move(0, p1,window).move(1, p2,window).commit();
        QQuickTouchUtils::flush(window);

        QCOMPARE(root->property("scale").toReal(), 1.0);
        QVERIFY(root->property("pinchActive").toBool());

        p1 -= QPoint(10,10);
        p2 += QPoint(10,10);
        pinchSequence.move(0, p1,window).move(1, p2,window).commit();
        QQuickTouchUtils::flush(window);

        QCOMPARE(root->property("scale").toReal(), 1.5);
        QCOMPARE(root->property("center").toPointF(), QPointF(40, 40)); // blackrect is at 50,50
        QCOMPARE(blackRect->scale(), 1.5);

        QTouchEvent cancelEvent(QEvent::TouchCancel);
        cancelEvent.setDevice(device);
        QCoreApplication::sendEvent(window, &cancelEvent);
        QQuickTouchUtils::flush(window);

        QCOMPARE(root->property("scale").toReal(), 1.0);
        QCOMPARE(root->property("center").toPointF(), QPointF(40, 40)); // blackrect is at 50,50
        QCOMPARE(blackRect->scale(), 1.0);
        QVERIFY(!root->property("pinchActive").toBool());
    }
}

void tst_QQuickPinchArea::transformedPinchArea_data()
{
    QTest::addColumn<QPoint>("p1");
    QTest::addColumn<QPoint>("p2");
    QTest::addColumn<bool>("shouldPinch");

    QTest::newRow("checking inner pinch 1")
        << QPoint(200, 140) << QPoint(200, 260) << true;

    QTest::newRow("checking inner pinch 2")
        << QPoint(140, 200) << QPoint(200, 140) << true;

    QTest::newRow("checking inner pinch 3")
        << QPoint(140, 200) << QPoint(260, 200) << true;

    QTest::newRow("checking outer pinch 1")
        << QPoint(140, 140) << QPoint(260, 260) << false;

    QTest::newRow("checking outer pinch 2")
        << QPoint(140, 140) << QPoint(200, 200) << false;

    QTest::newRow("checking outer pinch 3")
        << QPoint(140, 260) << QPoint(260, 260) << false;
}

void tst_QQuickPinchArea::transformedPinchArea()
{
    QFETCH(QPoint, p1);
    QFETCH(QPoint, p2);
    QFETCH(bool, shouldPinch);

    QQuickView *view = createView();
    QScopedPointer<QQuickView> scope(view);
    view->setSource(testFileUrl("transformedPinchArea.qml"));
    view->show();
    QVERIFY(QTest::qWaitForWindowExposed(view));
    QVERIFY(view->rootObject() != 0);
    qApp->processEvents();

    QQuickPinchArea *pinchArea = view->rootObject()->findChild<QQuickPinchArea*>("pinchArea");
    QVERIFY(pinchArea != 0);

    const int threshold = qApp->styleHints()->startDragDistance();

    {
        QTest::QTouchEventSequence pinchSequence = QTest::touchEvent(view, device);
        // start pinch
        pinchSequence.press(0, p1, view).commit();
        QQuickTouchUtils::flush(view);
        // In order for the stationary point to remember its previous position,
        // we have to reuse the same pinchSequence object.
        pinchSequence.stationary(0).press(1, p2, view).commit();
        QQuickTouchUtils::flush(view);
        pinchSequence.stationary(0).move(1, p2 + QPoint(threshold * 2, 0), view).commit();
        QQuickTouchUtils::flush(view);
        QCOMPARE(pinchArea->property("pinching").toBool(), shouldPinch);

        // release pinch
        pinchSequence.release(0, p1, view).release(1, p2, view).commit();
        QQuickTouchUtils::flush(view);
        QCOMPARE(pinchArea->property("pinching").toBool(), false);
    }
}

QQuickView *tst_QQuickPinchArea::createView()
{
    QQuickView *window = new QQuickView(0);
    window->setGeometry(0,0,240,320);

    return window;
}

QTEST_MAIN(tst_QQuickPinchArea)

#include "tst_qquickpincharea.moc"
