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
#include <QWidget>
#include <private/qdeclarativepincharea_p.h>
#include <private/qdeclarativerectangle_p.h>
#include <private/qdeclarativeflickable_p.h>
#include <QtDeclarative/qdeclarativeview.h>
#include <QtDeclarative/qdeclarativecontext.h>
#include <QtGui/qpa/qwindowsysteminterface.h>

class tst_QDeclarativePinchArea: public QObject
{
    Q_OBJECT
public:
    tst_QDeclarativePinchArea() : device(0) { }
private slots:
    void initTestCase() {
        if (!device) {
            device = new QTouchDevice;
            device->setType(QTouchDevice::TouchScreen);
            QWindowSystemInterface::registerTouchDevice(device);
        }
    }
    void pinchProperties();
    void scale();
    void pan();
    void flickable();

private:
    QDeclarativeView *createView();
    QTouchDevice *device;
};

void tst_QDeclarativePinchArea::pinchProperties()
{
    QDeclarativeView *canvas = createView();
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/pinchproperties.qml"));
    canvas->show();
    canvas->setFocus();
    QVERIFY(canvas->rootObject() != 0);

    QDeclarativePinchArea *pinchArea = canvas->rootObject()->findChild<QDeclarativePinchArea*>("pincharea");
    QDeclarativePinch *pinch = pinchArea->pinch();
    QVERIFY(pinchArea != 0);
    QVERIFY(pinch != 0);

    // target
    QDeclarativeItem *blackRect = canvas->rootObject()->findChild<QDeclarativeItem*>("blackrect");
    QVERIFY(blackRect != 0);
    QVERIFY(blackRect == pinch->target());
    QDeclarativeItem *rootItem = qobject_cast<QDeclarativeItem*>(canvas->rootObject());
    QVERIFY(rootItem != 0);
    QSignalSpy targetSpy(pinch, SIGNAL(targetChanged()));
    pinch->setTarget(rootItem);
    QCOMPARE(targetSpy.count(),1);
    pinch->setTarget(rootItem);
    QCOMPARE(targetSpy.count(),1);

    // axis
    QCOMPARE(pinch->axis(), QDeclarativePinch::XandYAxis);
    QSignalSpy axisSpy(pinch, SIGNAL(dragAxisChanged()));
    pinch->setAxis(QDeclarativePinch::XAxis);
    QCOMPARE(pinch->axis(), QDeclarativePinch::XAxis);
    QCOMPARE(axisSpy.count(),1);
    pinch->setAxis(QDeclarativePinch::XAxis);
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

    delete canvas;
}

QTouchEvent::TouchPoint makeTouchPoint(int id, QPoint p, QGraphicsView *v, QGraphicsItem *i)
{
    QTouchEvent::TouchPoint touchPoint(id);
    touchPoint.setPos(i->mapFromScene(p));
    touchPoint.setScreenPos(v->mapToGlobal(p));
    touchPoint.setScenePos(p);
    return touchPoint;
}

void tst_QDeclarativePinchArea::scale()
{
    QDeclarativeView *canvas = createView();
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/pinchproperties.qml"));
    canvas->show();
    canvas->setFocus();
    QVERIFY(QTest::qWaitForWindowExposed(canvas));
    QVERIFY(canvas->rootObject() != 0);
    qApp->processEvents();

    QDeclarativePinchArea *pinchArea = canvas->rootObject()->findChild<QDeclarativePinchArea*>("pincharea");
    QDeclarativePinch *pinch = pinchArea->pinch();
    QVERIFY(pinchArea != 0);
    QVERIFY(pinch != 0);

    QDeclarativeItem *root = qobject_cast<QDeclarativeItem*>(canvas->rootObject());
    QVERIFY(root != 0);

    // target
    QDeclarativeItem *blackRect = canvas->rootObject()->findChild<QDeclarativeItem*>("blackrect");
    QVERIFY(blackRect != 0);

    QWidget *vp = canvas->viewport();

    QPoint p1(80, 80);
    QPoint p2(100, 100);

    QTest::touchEvent(vp, device).press(0, p1, canvas);
    QTest::touchEvent(vp, device).stationary(0).press(1, p2, canvas);
    p1 -= QPoint(10,10);
    p2 += QPoint(10,10);
    QTest::touchEvent(vp, device).move(0, p1, canvas).move(1, p2, canvas);

    QCOMPARE(root->property("scale").toReal(), 1.0);

    p1 -= QPoint(10,10);
    p2 += QPoint(10,10);
    QTest::touchEvent(vp, device).move(0, p1, canvas).move(1, p2, canvas);

    QCOMPARE(root->property("scale").toReal(), 1.5);
    QCOMPARE(root->property("center").toPointF(), QPointF(40, 40)); // blackrect is at 50,50
    QCOMPARE(blackRect->scale(), 1.5);

    // scale beyond bound
    p1 -= QPoint(50,50);
    p2 += QPoint(50,50);
    QTest::touchEvent(vp, device).move(0, p1, canvas).move(1, p2, canvas);

    QCOMPARE(blackRect->scale(), 2.0);

    QTest::touchEvent(vp, device).release(0, p1, canvas).release(1, p2, canvas);

    delete canvas;
}

void tst_QDeclarativePinchArea::pan()
{
    QDeclarativeView *canvas = createView();
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/pinchproperties.qml"));
    canvas->show();
    canvas->setFocus();
    QVERIFY(QTest::qWaitForWindowExposed(canvas));
    QVERIFY(canvas->rootObject() != 0);
    qApp->processEvents();

    QDeclarativePinchArea *pinchArea = canvas->rootObject()->findChild<QDeclarativePinchArea*>("pincharea");
    QDeclarativePinch *pinch = pinchArea->pinch();
    QVERIFY(pinchArea != 0);
    QVERIFY(pinch != 0);

    QDeclarativeItem *root = qobject_cast<QDeclarativeItem*>(canvas->rootObject());
    QVERIFY(root != 0);

    // target
    QDeclarativeItem *blackRect = canvas->rootObject()->findChild<QDeclarativeItem*>("blackrect");
    QVERIFY(blackRect != 0);

    QWidget *vp = canvas->viewport();

    QPoint p1(80, 80);
    QPoint p2(100, 100);

    QTest::touchEvent(vp, device).press(0, p1, canvas);
    QTest::touchEvent(vp, device).stationary(0).press(1, p2, canvas);
    p1 += QPoint(10,10);
    p2 += QPoint(10,10);
    QTest::touchEvent(vp, device).move(0, p1, canvas).move(1, p2, canvas);

    QCOMPARE(root->property("scale").toReal(), 1.0);

    p1 += QPoint(10,10);
    p2 += QPoint(10,10);
    QTest::touchEvent(vp, device).move(0, p1, canvas).move(1, p2, canvas);

    QCOMPARE(root->property("center").toPointF(), QPointF(60, 60)); // blackrect is at 50,50

    QCOMPARE(blackRect->x(), 60.0);
    QCOMPARE(blackRect->y(), 60.0);

    // pan x beyond bound
    p1 += QPoint(100,100);
    p2 += QPoint(100,100);
    QTest::touchEvent(vp, device).move(0, p1, canvas).move(1, p2, canvas);

    QCOMPARE(blackRect->x(), 140.0);
    QCOMPARE(blackRect->y(), 160.0);

    QTest::touchEvent(vp, device).release(0, p1, canvas).release(1, p2, canvas);

    delete canvas;
}

void tst_QDeclarativePinchArea::flickable()
{
    QDeclarativeView *canvas = createView();
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/flickresize.qml"));
    canvas->show();
    canvas->setFocus();
    QVERIFY(QTest::qWaitForWindowExposed(canvas));
    QVERIFY(canvas->rootObject() != 0);
    qApp->processEvents();

    QDeclarativePinchArea *pinchArea = canvas->rootObject()->findChild<QDeclarativePinchArea*>("pincharea");
    QDeclarativePinch *pinch = pinchArea->pinch();
    QVERIFY(pinchArea != 0);
    QVERIFY(pinch != 0);

    QDeclarativeFlickable *root = qobject_cast<QDeclarativeFlickable*>(canvas->rootObject());
    QVERIFY(root != 0);

    QWidget *vp = canvas->viewport();

    QPoint p1(110, 80);
    QPoint p2(100, 100);

    // begin by moving one touch point (mouse)
    QTest::mousePress(vp, Qt::LeftButton, 0, canvas->mapFromScene(p1));
    QTest::touchEvent(vp, device).press(0, p1, canvas);
    {
        p1 -= QPoint(10,10);
        QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(p1), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &mv);
        QTest::touchEvent(vp, device).move(0, p1, canvas);
    }
    {
        p1 -= QPoint(10,10);
        QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(p1), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(vp, &mv);
        QTest::touchEvent(vp, device).move(0, p1, canvas);
    }
    {
        p1 -= QPoint(10,10);
        QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(p1), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(vp, &mv);
        QTest::touchEvent(vp, device).move(0, p1, canvas);
    }

    // Flickable has reacted to the gesture
    QVERIFY(root->isMoving());
    QVERIFY(root->property("scale").toReal() == 1.0);

    // add another touch point and continue moving
    QTest::touchEvent(vp, device).stationary(0).press(1, p2, canvas);
    p1 -= QPoint(10,10);
    p2 += QPoint(10,10);
    QTest::touchEvent(vp, device).move(0, p1, canvas).move(1, p2, canvas);

    QCOMPARE(root->property("scale").toReal(), 1.0);

    p1 -= QPoint(10,10);
    p2 += QPoint(10,10);
    QTest::touchEvent(vp, device).move(0, p1, canvas).move(1, p2, canvas);

    // PinchArea has stolen the gesture.
    QTRY_VERIFY(!root->isMoving());
    QVERIFY(root->property("scale").toReal() > 1.0);

    QTest::mouseRelease(vp, Qt::LeftButton, 0, canvas->mapFromScene(p1));
    QTest::touchEvent(vp, device).release(0, p1, canvas).release(1, p2, canvas);

    delete canvas;
}

QDeclarativeView *tst_QDeclarativePinchArea::createView()
{
    QDeclarativeView *canvas = new QDeclarativeView(0);
    canvas->viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
    canvas->setFixedSize(240,320);

    return canvas;
}

QTEST_MAIN(tst_QDeclarativePinchArea)

#include "tst_qdeclarativepincharea.moc"
