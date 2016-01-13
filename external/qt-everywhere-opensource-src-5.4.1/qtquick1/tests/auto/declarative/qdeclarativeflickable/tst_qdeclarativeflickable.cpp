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
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/qdeclarativeview.h>
#include <private/qdeclarativeflickable_p.h>
#include <private/qdeclarativevaluetype_p.h>
#include <QtWidgets/qgraphicswidget.h>
#include <QWindow>
#include <QApplication>
#include <math.h>

class tst_qdeclarativeflickable : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativeflickable();

private slots:
    void create();
    void horizontalViewportSize();
    void verticalViewportSize();
    void properties();
    void boundsBehavior();
    void maximumFlickVelocity();
    void flickDeceleration();
    void pressDelay();
    void disabledContent();
    void nestedPressDelay();
    void flickableDirection();
    void qgraphicswidget();
    void resizeContent();
    void returnToBounds();
    void testQtQuick11Attributes();
    void testQtQuick11Attributes_data();
    void wheel();
    void flickVelocity();
    void nestedStopAtBounds();
    void nestedStopAtBounds_data();

private:
    QDeclarativeEngine engine;

    void flick(QGraphicsView *canvas, const QPoint &from, const QPoint &to, int duration);
    template<typename T>
    T *findItem(QGraphicsObject *parent, const QString &objectName);
};

tst_qdeclarativeflickable::tst_qdeclarativeflickable()
{
}

void tst_qdeclarativeflickable::create()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/flickable01.qml"));
    QDeclarativeFlickable *obj = qobject_cast<QDeclarativeFlickable*>(c.create());

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
    QCOMPARE(obj->boundsBehavior(), QDeclarativeFlickable::DragAndOvershootBounds);
    QCOMPARE(obj->pressDelay(), 0);
    QCOMPARE(obj->maximumFlickVelocity(), 2500.);

    delete obj;
}

void tst_qdeclarativeflickable::horizontalViewportSize()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/flickable02.qml"));
    QDeclarativeFlickable *obj = qobject_cast<QDeclarativeFlickable*>(c.create());

    QVERIFY(obj != 0);
    QCOMPARE(obj->contentWidth(), 800.);
    QCOMPARE(obj->contentHeight(), 300.);
    QCOMPARE(obj->isAtXBeginning(), true);
    QCOMPARE(obj->isAtXEnd(), false);
    QCOMPARE(obj->isAtYBeginning(), true);
    QCOMPARE(obj->isAtYEnd(), false);

    delete obj;
}

void tst_qdeclarativeflickable::verticalViewportSize()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/flickable03.qml"));
    QDeclarativeFlickable *obj = qobject_cast<QDeclarativeFlickable*>(c.create());

    QVERIFY(obj != 0);
    QCOMPARE(obj->contentWidth(), 200.);
    QCOMPARE(obj->contentHeight(), 1200.);
    QCOMPARE(obj->isAtXBeginning(), true);
    QCOMPARE(obj->isAtXEnd(), false);
    QCOMPARE(obj->isAtYBeginning(), true);
    QCOMPARE(obj->isAtYEnd(), false);

    delete obj;
}

void tst_qdeclarativeflickable::properties()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/flickable04.qml"));
    QDeclarativeFlickable *obj = qobject_cast<QDeclarativeFlickable*>(c.create());

    QVERIFY(obj != 0);
    QCOMPARE(obj->isInteractive(), false);
    QCOMPARE(obj->boundsBehavior(), QDeclarativeFlickable::StopAtBounds);
    QCOMPARE(obj->pressDelay(), 200);
    QCOMPARE(obj->maximumFlickVelocity(), 2000.);

    QVERIFY(obj->property("ok").toBool() == false);
    QMetaObject::invokeMethod(obj, "check");
    QVERIFY(obj->property("ok").toBool() == true);

    delete obj;
}

void tst_qdeclarativeflickable::boundsBehavior()
{
    QDeclarativeComponent component(&engine);
    component.setData("import QtQuick 1.0; Flickable { boundsBehavior: Flickable.StopAtBounds }", QUrl::fromLocalFile(""));
    QDeclarativeFlickable *flickable = qobject_cast<QDeclarativeFlickable*>(component.create());
    QSignalSpy spy(flickable, SIGNAL(boundsBehaviorChanged()));

    QVERIFY(flickable);
    QVERIFY(flickable->boundsBehavior() == QDeclarativeFlickable::StopAtBounds);

    flickable->setBoundsBehavior(QDeclarativeFlickable::DragAndOvershootBounds);
    QVERIFY(flickable->boundsBehavior() == QDeclarativeFlickable::DragAndOvershootBounds);
    QCOMPARE(spy.count(),1);
    flickable->setBoundsBehavior(QDeclarativeFlickable::DragAndOvershootBounds);
    QCOMPARE(spy.count(),1);

    flickable->setBoundsBehavior(QDeclarativeFlickable::DragOverBounds);
    QVERIFY(flickable->boundsBehavior() == QDeclarativeFlickable::DragOverBounds);
    QCOMPARE(spy.count(),2);
    flickable->setBoundsBehavior(QDeclarativeFlickable::DragOverBounds);
    QCOMPARE(spy.count(),2);

    flickable->setBoundsBehavior(QDeclarativeFlickable::StopAtBounds);
    QVERIFY(flickable->boundsBehavior() == QDeclarativeFlickable::StopAtBounds);
    QCOMPARE(spy.count(),3);
    flickable->setBoundsBehavior(QDeclarativeFlickable::StopAtBounds);
    QCOMPARE(spy.count(),3);
}

void tst_qdeclarativeflickable::maximumFlickVelocity()
{
    QDeclarativeComponent component(&engine);
    component.setData("import QtQuick 1.0; Flickable { maximumFlickVelocity: 1.0; }", QUrl::fromLocalFile(""));
    QDeclarativeFlickable *flickable = qobject_cast<QDeclarativeFlickable*>(component.create());
    QSignalSpy spy(flickable, SIGNAL(maximumFlickVelocityChanged()));

    QVERIFY(flickable);
    QCOMPARE(flickable->maximumFlickVelocity(), 1.0);

    flickable->setMaximumFlickVelocity(2.0);
    QCOMPARE(flickable->maximumFlickVelocity(), 2.0);
    QCOMPARE(spy.count(),1);
    flickable->setMaximumFlickVelocity(2.0);
    QCOMPARE(spy.count(),1);
}

void tst_qdeclarativeflickable::flickDeceleration()
{
    QDeclarativeComponent component(&engine);
    component.setData("import QtQuick 1.0; Flickable { flickDeceleration: 1.0; }", QUrl::fromLocalFile(""));
    QDeclarativeFlickable *flickable = qobject_cast<QDeclarativeFlickable*>(component.create());
    QSignalSpy spy(flickable, SIGNAL(flickDecelerationChanged()));

    QVERIFY(flickable);
    QCOMPARE(flickable->flickDeceleration(), 1.0);

    flickable->setFlickDeceleration(2.0);
    QCOMPARE(flickable->flickDeceleration(), 2.0);
    QCOMPARE(spy.count(),1);
    flickable->setFlickDeceleration(2.0);
    QCOMPARE(spy.count(),1);
}

void tst_qdeclarativeflickable::pressDelay()
{
    QDeclarativeComponent component(&engine);
    component.setData("import QtQuick 1.0; Flickable { pressDelay: 100; }", QUrl::fromLocalFile(""));
    QDeclarativeFlickable *flickable = qobject_cast<QDeclarativeFlickable*>(component.create());
    QSignalSpy spy(flickable, SIGNAL(pressDelayChanged()));

    QVERIFY(flickable);
    QCOMPARE(flickable->pressDelay(), 100);

    flickable->setPressDelay(200);
    QCOMPARE(flickable->pressDelay(), 200);
    QCOMPARE(spy.count(),1);
    flickable->setPressDelay(200);
    QCOMPARE(spy.count(),1);
}

// QT-4677
void tst_qdeclarativeflickable::disabledContent()
{
    QDeclarativeView *canvas = new QDeclarativeView;
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/disabledcontent.qml"));
    canvas->show();
    canvas->setFocus();
    QVERIFY(canvas->rootObject() != 0);

    QDeclarativeFlickable *flickable = qobject_cast<QDeclarativeFlickable*>(canvas->rootObject());
    QVERIFY(flickable != 0);

    QVERIFY(flickable->contentX() == 0);
    QVERIFY(flickable->contentY() == 0);

    QTest::mousePress(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(50, 50)));
    {
        QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(QPoint(70,70)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &mv);
    }
    {
        QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(QPoint(90,90)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &mv);
    }
    {
        QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(QPoint(100,100)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &mv);
    }

    QVERIFY(flickable->contentX() < 0);
    QVERIFY(flickable->contentY() < 0);

    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(90, 90)));

    delete canvas;
}


// QTBUG-17361
void tst_qdeclarativeflickable::nestedPressDelay()
{
    QDeclarativeView *canvas = new QDeclarativeView;
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/nestedPressDelay.qml"));
    canvas->show();
    canvas->setFocus();
    QVERIFY(canvas->rootObject() != 0);

    QDeclarativeFlickable *outer = qobject_cast<QDeclarativeFlickable*>(canvas->rootObject());
    QVERIFY(outer != 0);

    QDeclarativeFlickable *inner = canvas->rootObject()->findChild<QDeclarativeFlickable*>("innerFlickable");
    QVERIFY(inner != 0);

    QTest::mousePress(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(150, 150)));
    // the MouseArea is not pressed immediately
    QVERIFY(outer->property("pressed").toBool() == false);

    // The outer pressDelay will prevail (50ms, vs. 10sec)
    // QTRY_VERIFY() has 5sec timeout, so will timeout well within 10sec.
    QTRY_VERIFY(outer->property("pressed").toBool() == true);

    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(150, 150)));

    delete canvas;
}

void tst_qdeclarativeflickable::flickableDirection()
{
    QDeclarativeComponent component(&engine);
    component.setData("import QtQuick 1.0; Flickable { flickableDirection: Flickable.VerticalFlick; }", QUrl::fromLocalFile(""));
    QDeclarativeFlickable *flickable = qobject_cast<QDeclarativeFlickable*>(component.create());
    QSignalSpy spy(flickable, SIGNAL(flickableDirectionChanged()));

    QVERIFY(flickable);
    QCOMPARE(flickable->flickableDirection(), QDeclarativeFlickable::VerticalFlick);

    flickable->setFlickableDirection(QDeclarativeFlickable::HorizontalAndVerticalFlick);
    QCOMPARE(flickable->flickableDirection(), QDeclarativeFlickable::HorizontalAndVerticalFlick);
    QCOMPARE(spy.count(),1);

    flickable->setFlickableDirection(QDeclarativeFlickable::AutoFlickDirection);
    QCOMPARE(flickable->flickableDirection(), QDeclarativeFlickable::AutoFlickDirection);
    QCOMPARE(spy.count(),2);

    flickable->setFlickableDirection(QDeclarativeFlickable::HorizontalFlick);
    QCOMPARE(flickable->flickableDirection(), QDeclarativeFlickable::HorizontalFlick);
    QCOMPARE(spy.count(),3);

    flickable->setFlickableDirection(QDeclarativeFlickable::HorizontalFlick);
    QCOMPARE(flickable->flickableDirection(), QDeclarativeFlickable::HorizontalFlick);
    QCOMPARE(spy.count(),3);
}

void tst_qdeclarativeflickable::qgraphicswidget()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/flickableqgraphicswidget.qml"));
    QDeclarativeFlickable *flickable = qobject_cast<QDeclarativeFlickable*>(c.create());

    QVERIFY(flickable != 0);
    QGraphicsWidget *widget = findItem<QGraphicsWidget>(flickable->contentItem(), "widget1");
    QVERIFY(widget);
}

// QtQuick 1.1
void tst_qdeclarativeflickable::resizeContent()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/resize.qml"));
    QDeclarativeItem *root = qobject_cast<QDeclarativeItem*>(c.create());
    QDeclarativeFlickable *obj = findItem<QDeclarativeFlickable>(root, "flick");

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

// QtQuick 1.1
void tst_qdeclarativeflickable::returnToBounds()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/resize.qml"));
    QDeclarativeItem *root = qobject_cast<QDeclarativeItem*>(c.create());
    QDeclarativeFlickable *obj = findItem<QDeclarativeFlickable>(root, "flick");

    QVERIFY(obj != 0);
    QCOMPARE(obj->contentX(), 0.);
    QCOMPARE(obj->contentY(), 0.);
    QCOMPARE(obj->contentWidth(), 300.);
    QCOMPARE(obj->contentHeight(), 300.);

    obj->setContentX(100);
    obj->setContentY(400);
    QTRY_COMPARE(obj->contentX(), 100.);
    QTRY_COMPARE(obj->contentY(), 400.);

    QMetaObject::invokeMethod(root, "returnToBounds");

    QTRY_COMPARE(obj->contentX(), 0.);
    QTRY_COMPARE(obj->contentY(), 0.);

    delete root;
}

void tst_qdeclarativeflickable::testQtQuick11Attributes()
{
    QFETCH(QString, code);
    QFETCH(QString, warning);
    QFETCH(QString, error);

    QDeclarativeEngine engine;
    QObject *obj;

    QDeclarativeComponent invalid(&engine);
    invalid.setData("import QtQuick 1.0; Flickable { " + code.toUtf8() + " }", QUrl(""));
    QTest::ignoreMessage(QtWarningMsg, warning.toUtf8());
    obj = invalid.create();
    QCOMPARE(invalid.errorString(), error);
    delete obj;

    QDeclarativeComponent valid(&engine);
    valid.setData("import QtQuick 1.1; Flickable { " + code.toUtf8() + " }", QUrl(""));
    obj = valid.create();
    QVERIFY(obj);
    QVERIFY(valid.errorString().isEmpty());
    delete obj;
}

void tst_qdeclarativeflickable::testQtQuick11Attributes_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("warning");
    QTest::addColumn<QString>("error");

    QTest::newRow("resizeContent") << "Component.onCompleted: resizeContent(100,100,Qt.point(50,50))"
            << "<Unknown File>:1: ReferenceError: Can't find variable: resizeContent"
            << "";

    QTest::newRow("returnToBounds") << "Component.onCompleted: returnToBounds()"
            << "<Unknown File>:1: ReferenceError: Can't find variable: returnToBounds"
            << "";

}

void tst_qdeclarativeflickable::wheel()
{
    QDeclarativeView *canvas = new QDeclarativeView;
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/wheel.qml"));
    canvas->show();
    canvas->setFocus();
    QVERIFY(canvas->rootObject() != 0);

    QDeclarativeFlickable *flick = canvas->rootObject()->findChild<QDeclarativeFlickable*>("flick");
    QVERIFY(flick != 0);

    QGraphicsScene *scene = canvas->scene();
    QGraphicsSceneWheelEvent event(QEvent::GraphicsSceneWheel);
    event.setScenePos(QPointF(200, 200));
    event.setDelta(-120);
    event.setOrientation(Qt::Vertical);
    event.setAccepted(false);
    QApplication::sendEvent(scene, &event);

    QTRY_VERIFY(flick->contentY() > 0);
    QVERIFY(flick->contentX() == 0);

    flick->setContentY(0);
    QVERIFY(flick->contentY() == 0);

    event.setScenePos(QPointF(200, 200));
    event.setDelta(-120);
    event.setOrientation(Qt::Horizontal);
    event.setAccepted(false);
    QApplication::sendEvent(scene, &event);

    QTRY_VERIFY(flick->contentX() > 0);
    QVERIFY(flick->contentY() == 0);

    delete canvas;
}

void tst_qdeclarativeflickable::flickVelocity()
{
#ifdef Q_WS_MAC
    QSKIP("Producing flicks on Mac CI impossible due to timing problems");
#endif

    QDeclarativeView *canvas = new QDeclarativeView;
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/flickable03.qml"));
    canvas->show();
    canvas->setFocus();
    QVERIFY(canvas->rootObject() != 0);

    QDeclarativeFlickable *flickable = qobject_cast<QDeclarativeFlickable*>(canvas->rootObject());
    QVERIFY(flickable != 0);

    // flick up
    flick(canvas, QPoint(20,190), QPoint(20, 50), 200);
    QVERIFY(flickable->verticalVelocity() > 0.0);
    QTRY_VERIFY(flickable->verticalVelocity() == 0.0);

    // flick down
    flick(canvas, QPoint(20,10), QPoint(20, 140), 200);
    QVERIFY(flickable->verticalVelocity() < 0.0);
    QTRY_VERIFY(flickable->verticalVelocity() == 0.0);

    delete canvas;
}

void tst_qdeclarativeflickable::flick(QGraphicsView *canvas, const QPoint &from, const QPoint &to, int duration)
{
    const int pointCount = 5;
    QPoint diff = to - from;

    // send press, five equally spaced moves, and release.
    QTest::mousePress(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(from));

    for (int i = 0; i < pointCount; ++i) {
        QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(from + (i+1)*diff/pointCount), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &mv);
        QTest::qWait(duration/pointCount);
        QCoreApplication::processEvents();
    }

    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(to));
}

void tst_qdeclarativeflickable::nestedStopAtBounds_data()
{
    QTest::addColumn<bool>("transpose");
    QTest::addColumn<bool>("invert");

    QTest::newRow("left") << false << false;
    QTest::newRow("right") << false << true;
    QTest::newRow("top") << true << false;
    QTest::newRow("bottom") << true << true;
}

void tst_qdeclarativeflickable::nestedStopAtBounds()
{
    QFETCH(bool, transpose);
    QFETCH(bool, invert);

    QDeclarativeView view;
    view.setSource(QUrl::fromLocalFile(SRCDIR "/data/nestedStopAtBounds.qml"));
    view.show();
    view.activateWindow();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QVERIFY(view.rootObject());

    QDeclarativeFlickable *outer =  qobject_cast<QDeclarativeFlickable*>(view.rootObject());
    QVERIFY(outer);

    QDeclarativeFlickable *inner = outer->findChild<QDeclarativeFlickable*>("innerFlickable");
    QVERIFY(inner);
    inner->setFlickableDirection(transpose ? QDeclarativeFlickable::VerticalFlick : QDeclarativeFlickable::HorizontalFlick);
    inner->setContentX(invert ? 0 : 100);
    inner->setContentY(invert ? 0 : 100);

    const int threshold = QApplication::startDragDistance();

    QPoint position(200, 200);
    int &axis = transpose ? position.ry() : position.rx();

    QGraphicsSceneMouseEvent moveEvent(QEvent::GraphicsSceneMouseMove);
    moveEvent.setButton(Qt::LeftButton);
    moveEvent.setButtons(Qt::LeftButton);

    // drag toward the aligned boundary.  Outer mouse area dragged.
    QTest::mousePress(view.viewport(), Qt::LeftButton, 0, position);
    QTest::qWait(10);
    axis += invert ? threshold * 2 : -threshold * 2;
    moveEvent.setScenePos(position);
    QApplication::sendEvent(view.scene(), &moveEvent);
    axis += invert ? threshold : -threshold;
    moveEvent.setScenePos(position);
    QApplication::sendEvent(view.scene(), &moveEvent);
    axis += invert ? threshold : -threshold;
    moveEvent.setScenePos(position);
    QApplication::sendEvent(view.scene(), &moveEvent);
    QVERIFY(outer->contentX() != 50 || outer->contentY() != 50);
    QVERIFY((inner->contentX() == 0 || inner->contentX() == 100)
            && (inner->contentY() == 0 || inner->contentY() == 100));
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, 0, position);

    axis = 200;
    outer->setContentX(50);
    outer->setContentY(50);

    // drag away from the aligned boundary.  Inner mouse area dragged.
    QTest::mousePress(view.viewport(), Qt::LeftButton, 0, position);
    QTest::qWait(10);
    axis += invert ? -threshold * 2 : threshold * 2;
    moveEvent.setScenePos(position);
    QApplication::sendEvent(view.scene(), &moveEvent);
    axis += invert ? -threshold : threshold;
    moveEvent.setScenePos(position);
    QApplication::sendEvent(view.scene(), &moveEvent);
    axis += invert ? -threshold : threshold;
    moveEvent.setScenePos(position);
    QApplication::sendEvent(view.scene(), &moveEvent);
    QVERIFY(outer->contentX() == 50 && outer->contentY() == 50);
    QVERIFY((inner->contentX() != 0 && inner->contentX() != 100)
            || (inner->contentY() != 0 && inner->contentY() != 100));
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, 0, position);
}

template<typename T>
T *tst_qdeclarativeflickable::findItem(QGraphicsObject *parent, const QString &objectName)
{
    const QMetaObject &mo = T::staticMetaObject;
    //qDebug() << parent->childItems().count() << "children";
    for (int i = 0; i < parent->childItems().count(); ++i) {
        QGraphicsObject *item = qobject_cast<QGraphicsObject*>(parent->childItems().at(i));
        if(!item)
            continue;
        //qDebug() << "try" << item;
        if (mo.cast(item) && (objectName.isEmpty() || item->objectName() == objectName)) {
            return static_cast<T*>(item);
        }
        item = findItem<T>(item, objectName);
        if (item)
            return static_cast<T*>(item);
    }

    return 0;
}

QTEST_MAIN(tst_qdeclarativeflickable)

#include "tst_qdeclarativeflickable.moc"
