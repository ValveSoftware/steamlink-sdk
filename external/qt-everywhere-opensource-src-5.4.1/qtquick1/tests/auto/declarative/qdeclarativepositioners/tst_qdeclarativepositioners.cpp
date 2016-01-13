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
#include <private/qlistmodelinterface_p.h>
#include <qdeclarativeview.h>
#include <qdeclarativeengine.h>
#include <private/qdeclarativerectangle_p.h>
#include <private/qdeclarativepositioners_p.h>
#include <private/qdeclarativetransition_p.h>
#include <private/qdeclarativeitem_p.h>
#include <qdeclarativeexpression.h>
#include <QtWidgets/qgraphicswidget.h>

class tst_QDeclarativePositioners : public QObject
{
    Q_OBJECT
public:
    tst_QDeclarativePositioners();

private slots:
    void test_horizontal();
    void test_horizontal_rtl();
    void test_horizontal_spacing();
    void test_horizontal_spacing_rightToLeft();
    void test_horizontal_animated();
    void test_horizontal_animated_rightToLeft();
    void test_vertical();
    void test_vertical_spacing();
    void test_vertical_animated();
    void test_grid();
    void test_grid_topToBottom();
    void test_grid_rightToLeft();
    void test_grid_spacing();
    void test_grid_animated();
    void test_grid_animated_rightToLeft();
    void test_grid_zero_columns();
    void test_propertychanges();
    void test_repeater();
    void test_flow();
    void test_flow_rightToLeft();
    void test_flow_topToBottom();
    void test_flow_resize();
    void test_flow_resize_rightToLeft();
    void test_flow_implicit_resize();
    void test_conflictinganchors();
    void test_vertical_qgraphicswidget();
    void test_mirroring();
    void testQtQuick11Attributes();
    void testQtQuick11Attributes_data();
private:
    QDeclarativeView *createView(const QString &filename);
};

tst_QDeclarativePositioners::tst_QDeclarativePositioners()
{
}

void tst_QDeclarativePositioners::test_horizontal()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/horizontal.qml");

    canvas->rootObject()->setProperty("testRightToLeft", false);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 0.0);

    QDeclarativeItem *row = canvas->rootObject()->findChild<QDeclarativeItem*>("row");
    QCOMPARE(row->width(), 110.0);
    QCOMPARE(row->height(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_horizontal_rtl()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/horizontal.qml");

    canvas->rootObject()->setProperty("testRightToLeft", true);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 60.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 40.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 0.0);

    QDeclarativeItem *row = canvas->rootObject()->findChild<QDeclarativeItem*>("row");
    QCOMPARE(row->width(), 110.0);
    QCOMPARE(row->height(), 50.0);

    // Change the width of the row and check that items stay to the right
    row->setWidth(200);
    QCOMPARE(one->x(), 150.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 130.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 90.0);
    QCOMPARE(three->y(), 0.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_horizontal_spacing()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/horizontal-spacing.qml");

    canvas->rootObject()->setProperty("testRightToLeft", false);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 60.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 90.0);
    QCOMPARE(three->y(), 0.0);

    QDeclarativeItem *row = canvas->rootObject()->findChild<QDeclarativeItem*>("row");
    QCOMPARE(row->width(), 130.0);
    QCOMPARE(row->height(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_horizontal_spacing_rightToLeft()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/horizontal-spacing.qml");

    canvas->rootObject()->setProperty("testRightToLeft", true);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 80.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 00.0);
    QCOMPARE(three->y(), 0.0);

    QDeclarativeItem *row = canvas->rootObject()->findChild<QDeclarativeItem*>("row");
    QCOMPARE(row->width(), 130.0);
    QCOMPARE(row->height(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_horizontal_animated()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/horizontal-animated.qml");

    canvas->rootObject()->setProperty("testRightToLeft", false);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);

    //Note that they animate in
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(three->x(), -100.0);

    QDeclarativeItem *row = canvas->rootObject()->findChild<QDeclarativeItem*>("row");
    QVERIFY(row);
    QCOMPARE(row->width(), 100.0);
    QCOMPARE(row->height(), 50.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->x(), 0.0);
    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(two->opacity(), 0.0);
    QTRY_COMPARE(two->x(), -100.0);//Not 'in' yet
    QTRY_COMPARE(two->y(), 0.0);
    QTRY_COMPARE(three->x(), 50.0);
    QTRY_COMPARE(three->y(), 0.0);

    //Add 'two'
    two->setOpacity(1.0);
    QCOMPARE(two->opacity(), 1.0);

    // New size should be immediate
    QCOMPARE(row->width(), 150.0);
    QCOMPARE(row->height(), 50.0);

    QTest::qWait(0);//Let the animation start
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(three->x(), 50.0);

    QTRY_COMPARE(two->x(), 50.0);
    QTRY_COMPARE(three->x(), 100.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_horizontal_animated_rightToLeft()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/horizontal-animated.qml");

    canvas->rootObject()->setProperty("testRightToLeft", true);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);

    //Note that they animate in
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(three->x(), -100.0);

    QDeclarativeItem *row = canvas->rootObject()->findChild<QDeclarativeItem*>("row");
    QVERIFY(row);
    QCOMPARE(row->width(), 100.0);
    QCOMPARE(row->height(), 50.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->x(), 50.0);
    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(two->opacity(), 0.0);
    QTRY_COMPARE(two->x(), -100.0);//Not 'in' yet
    QTRY_COMPARE(two->y(), 0.0);
    QTRY_COMPARE(three->x(), 0.0);
    QTRY_COMPARE(three->y(), 0.0);

    //Add 'two'
    two->setOpacity(1.0);
    QCOMPARE(two->opacity(), 1.0);

    // New size should be immediate
    QCOMPARE(row->width(), 150.0);
    QCOMPARE(row->height(), 50.0);

    QTest::qWait(0);//Let the animation start
    QCOMPARE(one->x(), 50.0);
    QCOMPARE(two->x(), -100.0);

    QTRY_COMPARE(one->x(), 100.0);
    QTRY_COMPARE(two->x(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_vertical()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/vertical.qml");

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 0.0);
    QCOMPARE(two->y(), 50.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 60.0);

    QDeclarativeItem *column = canvas->rootObject()->findChild<QDeclarativeItem*>("column");
    QVERIFY(column);
    QCOMPARE(column->height(), 80.0);
    QCOMPARE(column->width(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_vertical_spacing()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/vertical-spacing.qml");

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 0.0);
    QCOMPARE(two->y(), 60.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 80.0);

    QDeclarativeItem *column = canvas->rootObject()->findChild<QDeclarativeItem*>("column");
    QCOMPARE(column->height(), 100.0);
    QCOMPARE(column->width(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_vertical_animated()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/vertical-animated.qml");

    //Note that they animate in
    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QCOMPARE(one->y(), -100.0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QCOMPARE(two->y(), -100.0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QCOMPARE(three->y(), -100.0);

    QDeclarativeItem *column = canvas->rootObject()->findChild<QDeclarativeItem*>("column");
    QVERIFY(column);
    QCOMPARE(column->height(), 100.0);
    QCOMPARE(column->width(), 50.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(one->x(), 0.0);
    QTRY_COMPARE(two->opacity(), 0.0);
    QTRY_COMPARE(two->y(), -100.0);//Not 'in' yet
    QTRY_COMPARE(two->x(), 0.0);
    QTRY_COMPARE(three->y(), 50.0);
    QTRY_COMPARE(three->x(), 0.0);

    //Add 'two'
    two->setOpacity(1.0);
    QTRY_COMPARE(two->opacity(), 1.0);
    QCOMPARE(column->height(), 150.0);
    QCOMPARE(column->width(), 50.0);
    QTest::qWait(0);//Let the animation start
    QCOMPARE(two->y(), -100.0);
    QCOMPARE(three->y(), 50.0);

    QTRY_COMPARE(two->y(), 50.0);
    QTRY_COMPARE(three->y(), 100.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_grid()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/gridtest.qml");

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 50.0);
    QCOMPARE(five->y(), 50.0);

    QDeclarativeGrid *grid = canvas->rootObject()->findChild<QDeclarativeGrid*>("grid");
    QCOMPARE(grid->flow(), QDeclarativeGrid::LeftToRight);
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 100.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_grid_topToBottom()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/grid-toptobottom.qml");

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 0.0);
    QCOMPARE(two->y(), 50.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 100.0);
    QCOMPARE(four->x(), 50.0);
    QCOMPARE(four->y(), 0.0);
    QCOMPARE(five->x(), 50.0);
    QCOMPARE(five->y(), 50.0);

    QDeclarativeGrid *grid = canvas->rootObject()->findChild<QDeclarativeGrid*>("grid");
    QCOMPARE(grid->flow(), QDeclarativeGrid::TopToBottom);
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 120.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_grid_rightToLeft()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/gridtest.qml");

    canvas->rootObject()->setProperty("testRightToLeft", true);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 50.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 30.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 50.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 40.0);
    QCOMPARE(five->y(), 50.0);

    QDeclarativeGrid *grid = canvas->rootObject()->findChild<QDeclarativeGrid*>("grid");
    QCOMPARE(grid->layoutDirection(), Qt::RightToLeft);
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 100.0);

    // Change the width of the grid and check that items stay to the right
    grid->setWidth(200);
    QCOMPARE(one->x(), 150.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 130.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 100.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 150.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 140.0);
    QCOMPARE(five->y(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_grid_spacing()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/grid-spacing.qml");

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 54.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 78.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 54.0);
    QCOMPARE(five->x(), 54.0);
    QCOMPARE(five->y(), 54.0);

    QDeclarativeItem *grid = canvas->rootObject()->findChild<QDeclarativeItem*>("grid");
    QCOMPARE(grid->width(), 128.0);
    QCOMPARE(grid->height(), 104.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_grid_animated()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/grid-animated.qml");

    canvas->rootObject()->setProperty("testRightToLeft", false);

    //Note that all animate in
    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(one->y(), -100.0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(two->y(), -100.0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QCOMPARE(three->x(), -100.0);
    QCOMPARE(three->y(), -100.0);

    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QCOMPARE(four->x(), -100.0);
    QCOMPARE(four->y(), -100.0);

    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);
    QCOMPARE(five->x(), -100.0);
    QCOMPARE(five->y(), -100.0);

    QDeclarativeItem *grid = canvas->rootObject()->findChild<QDeclarativeItem*>("grid");
    QVERIFY(grid);
    QCOMPARE(grid->width(), 150.0);
    QCOMPARE(grid->height(), 100.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(one->x(), 0.0);
    QTRY_COMPARE(two->opacity(), 0.0);
    QTRY_COMPARE(two->y(), -100.0);
    QTRY_COMPARE(two->x(), -100.0);
    QTRY_COMPARE(three->y(), 0.0);
    QTRY_COMPARE(three->x(), 50.0);
    QTRY_COMPARE(four->y(), 0.0);
    QTRY_COMPARE(four->x(), 100.0);
    QTRY_COMPARE(five->y(), 50.0);
    QTRY_COMPARE(five->x(), 0.0);

    //Add 'two'
    two->setOpacity(1.0);
    QCOMPARE(two->opacity(), 1.0);
    QCOMPARE(grid->width(), 150.0);
    QCOMPARE(grid->height(), 100.0);
    QTest::qWait(0);//Let the animation start
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(two->y(), -100.0);
    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(three->x(), 50.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 100.0);
    QCOMPARE(four->y(), 0.0);
    QCOMPARE(five->x(), 0.0);
    QCOMPARE(five->y(), 50.0);
    //Let the animation complete
    QTRY_COMPARE(two->x(), 50.0);
    QTRY_COMPARE(two->y(), 0.0);
    QTRY_COMPARE(one->x(), 0.0);
    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(three->x(), 100.0);
    QTRY_COMPARE(three->y(), 0.0);
    QTRY_COMPARE(four->x(), 0.0);
    QTRY_COMPARE(four->y(), 50.0);
    QTRY_COMPARE(five->x(), 50.0);
    QTRY_COMPARE(five->y(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_grid_animated_rightToLeft()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/grid-animated.qml");

    canvas->rootObject()->setProperty("testRightToLeft", true);

    //Note that all animate in
    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(one->y(), -100.0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(two->y(), -100.0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QCOMPARE(three->x(), -100.0);
    QCOMPARE(three->y(), -100.0);

    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QCOMPARE(four->x(), -100.0);
    QCOMPARE(four->y(), -100.0);

    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);
    QCOMPARE(five->x(), -100.0);
    QCOMPARE(five->y(), -100.0);

    QDeclarativeItem *grid = canvas->rootObject()->findChild<QDeclarativeItem*>("grid");
    QVERIFY(grid);
    QCOMPARE(grid->width(), 150.0);
    QCOMPARE(grid->height(), 100.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(one->x(), 100.0);
    QTRY_COMPARE(two->opacity(), 0.0);
    QTRY_COMPARE(two->y(), -100.0);
    QTRY_COMPARE(two->x(), -100.0);
    QTRY_COMPARE(three->y(), 0.0);
    QTRY_COMPARE(three->x(), 50.0);
    QTRY_COMPARE(four->y(), 0.0);
    QTRY_COMPARE(four->x(), 0.0);
    QTRY_COMPARE(five->y(), 50.0);
    QTRY_COMPARE(five->x(), 100.0);

    //Add 'two'
    two->setOpacity(1.0);
    QCOMPARE(two->opacity(), 1.0);
    QCOMPARE(grid->width(), 150.0);
    QCOMPARE(grid->height(), 100.0);
    QTest::qWait(0);//Let the animation start
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(two->y(), -100.0);
    QCOMPARE(one->x(), 100.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(three->x(), 50.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 0.0);
    QCOMPARE(five->x(), 100.0);
    QCOMPARE(five->y(), 50.0);
    //Let the animation complete
    QTRY_COMPARE(two->x(), 50.0);
    QTRY_COMPARE(two->y(), 0.0);
    QTRY_COMPARE(one->x(), 100.0);
    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(three->x(), 0.0);
    QTRY_COMPARE(three->y(), 0.0);
    QTRY_COMPARE(four->x(), 100.0);
    QTRY_COMPARE(four->y(), 50.0);
    QTRY_COMPARE(five->x(), 50.0);
    QTRY_COMPARE(five->y(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_grid_zero_columns()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/gridzerocolumns.qml");

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 120.0);
    QCOMPARE(four->y(), 0.0);
    QCOMPARE(five->x(), 0.0);
    QCOMPARE(five->y(), 50.0);

    QDeclarativeItem *grid = canvas->rootObject()->findChild<QDeclarativeItem*>("grid");
    QCOMPARE(grid->width(), 170.0);
    QCOMPARE(grid->height(), 60.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_propertychanges()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/propertychangestest.qml");

    QDeclarativeGrid *grid = qobject_cast<QDeclarativeGrid*>(canvas->rootObject());
    QVERIFY(grid != 0);
    QDeclarativeTransition *rowTransition = canvas->rootObject()->findChild<QDeclarativeTransition*>("rowTransition");
    QDeclarativeTransition *columnTransition = canvas->rootObject()->findChild<QDeclarativeTransition*>("columnTransition");

    QSignalSpy addSpy(grid, SIGNAL(addChanged()));
    QSignalSpy moveSpy(grid, SIGNAL(moveChanged()));
    QSignalSpy columnsSpy(grid, SIGNAL(columnsChanged()));
    QSignalSpy rowsSpy(grid, SIGNAL(rowsChanged()));

    QVERIFY(grid);
    QVERIFY(rowTransition);
    QVERIFY(columnTransition);
    QCOMPARE(grid->add(), columnTransition);
    QCOMPARE(grid->move(), columnTransition);
    QCOMPARE(grid->columns(), 4);
    QCOMPARE(grid->rows(), -1);

    grid->setAdd(rowTransition);
    grid->setMove(rowTransition);
    QCOMPARE(grid->add(), rowTransition);
    QCOMPARE(grid->move(), rowTransition);
    QCOMPARE(addSpy.count(),1);
    QCOMPARE(moveSpy.count(),1);

    grid->setAdd(rowTransition);
    grid->setMove(rowTransition);
    QCOMPARE(addSpy.count(),1);
    QCOMPARE(moveSpy.count(),1);

    grid->setAdd(0);
    grid->setMove(0);
    QCOMPARE(addSpy.count(),2);
    QCOMPARE(moveSpy.count(),2);

    grid->setColumns(-1);
    grid->setRows(3);
    QCOMPARE(grid->columns(), -1);
    QCOMPARE(grid->rows(), 3);
    QCOMPARE(columnsSpy.count(),1);
    QCOMPARE(rowsSpy.count(),1);

    grid->setColumns(-1);
    grid->setRows(3);
    QCOMPARE(columnsSpy.count(),1);
    QCOMPARE(rowsSpy.count(),1);

    grid->setColumns(2);
    grid->setRows(2);
    QCOMPARE(columnsSpy.count(),2);
    QCOMPARE(rowsSpy.count(),2);

    delete canvas;
}

void tst_QDeclarativePositioners::test_repeater()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/repeatertest.qml");

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);

    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);

    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 100.0);
    QCOMPARE(three->y(), 0.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_flow()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/flowtest.qml");

    canvas->rootObject()->setProperty("testRightToLeft", false);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 50.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 70.0);
    QCOMPARE(five->x(), 50.0);
    QCOMPARE(five->y(), 70.0);

    QDeclarativeItem *flow = canvas->rootObject()->findChild<QDeclarativeItem*>("flow");
    QVERIFY(flow);
    QCOMPARE(flow->width(), 90.0);
    QCOMPARE(flow->height(), 120.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_flow_rightToLeft()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/flowtest.qml");

    canvas->rootObject()->setProperty("testRightToLeft", true);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 40.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 20.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 40.0);
    QCOMPARE(three->y(), 50.0);
    QCOMPARE(four->x(), 40.0);
    QCOMPARE(four->y(), 70.0);
    QCOMPARE(five->x(), 30.0);
    QCOMPARE(five->y(), 70.0);

    QDeclarativeItem *flow = canvas->rootObject()->findChild<QDeclarativeItem*>("flow");
    QVERIFY(flow);
    QCOMPARE(flow->width(), 90.0);
    QCOMPARE(flow->height(), 120.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_flow_topToBottom()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/flowtest-toptobottom.qml");

    canvas->rootObject()->setProperty("testRightToLeft", false);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 50.0);
    QCOMPARE(three->y(), 50.0);
    QCOMPARE(four->x(), 100.0);
    QCOMPARE(four->y(), 00.0);
    QCOMPARE(five->x(), 100.0);
    QCOMPARE(five->y(), 50.0);

    QDeclarativeItem *flow = canvas->rootObject()->findChild<QDeclarativeItem*>("flow");
    QVERIFY(flow);
    QCOMPARE(flow->height(), 90.0);
    QCOMPARE(flow->width(), 150.0);

    canvas->rootObject()->setProperty("testRightToLeft", true);

    QVERIFY(flow);
    QCOMPARE(flow->height(), 90.0);
    QCOMPARE(flow->width(), 150.0);

    QCOMPARE(one->x(), 100.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 80.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 50.0);
    QCOMPARE(three->y(), 50.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 0.0);
    QCOMPARE(five->x(), 40.0);
    QCOMPARE(five->y(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_flow_resize()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/flowtest.qml");

    QDeclarativeItem *root = qobject_cast<QDeclarativeItem*>(canvas->rootObject());
    QVERIFY(root);
    root->setWidth(125);
    root->setProperty("testRightToLeft", false);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 50.0);
    QCOMPARE(five->y(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_flow_resize_rightToLeft()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/flowtest.qml");

    QDeclarativeItem *root = qobject_cast<QDeclarativeItem*>(canvas->rootObject());
    QVERIFY(root);
    root->setWidth(125);
    root->setProperty("testRightToLeft", true);

    QDeclarativeRectangle *one = canvas->rootObject()->findChild<QDeclarativeRectangle*>("one");
    QVERIFY(one != 0);
    QDeclarativeRectangle *two = canvas->rootObject()->findChild<QDeclarativeRectangle*>("two");
    QVERIFY(two != 0);
    QDeclarativeRectangle *three = canvas->rootObject()->findChild<QDeclarativeRectangle*>("three");
    QVERIFY(three != 0);
    QDeclarativeRectangle *four = canvas->rootObject()->findChild<QDeclarativeRectangle*>("four");
    QVERIFY(four != 0);
    QDeclarativeRectangle *five = canvas->rootObject()->findChild<QDeclarativeRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 75.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 55.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 5.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 75.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 65.0);
    QCOMPARE(five->y(), 50.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_flow_implicit_resize()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/flow-testimplicitsize.qml");
    QVERIFY(canvas->rootObject() != 0);

    QDeclarativeFlow *flow = canvas->rootObject()->findChild<QDeclarativeFlow*>("flow");
    QVERIFY(flow != 0);

    QCOMPARE(flow->width(), 100.0);
    QCOMPARE(flow->height(), 120.0);

    canvas->rootObject()->setProperty("flowLayout", 0);
    QCOMPARE(flow->flow(), QDeclarativeFlow::LeftToRight);
    QCOMPARE(flow->width(), 220.0);
    QCOMPARE(flow->height(), 50.0);

    canvas->rootObject()->setProperty("flowLayout", 1);
    QCOMPARE(flow->flow(), QDeclarativeFlow::TopToBottom);
    QCOMPARE(flow->width(), 100.0);
    QCOMPARE(flow->height(), 120.0);

    canvas->rootObject()->setProperty("flowLayout", 2);
    QCOMPARE(flow->layoutDirection(), Qt::RightToLeft);
    QCOMPARE(flow->width(), 220.0);
    QCOMPARE(flow->height(), 50.0);

    delete canvas;
}

QString warningMessage;

void interceptWarnings(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    Q_UNUSED( type );
    warningMessage = msg;
}

void tst_QDeclarativePositioners::test_conflictinganchors()
{
    QtMessageHandler oldMsgHandler = qInstallMessageHandler(interceptWarnings);
    QDeclarativeEngine engine;
    QDeclarativeComponent component(&engine);

    component.setData("import QtQuick 1.0\nColumn { Item {} }", QUrl::fromLocalFile(""));
    QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QVERIFY(warningMessage.isEmpty());

    component.setData("import QtQuick 1.0\nRow { Item {} }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QVERIFY(warningMessage.isEmpty());

    component.setData("import QtQuick 1.0\nGrid { Item {} }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QVERIFY(warningMessage.isEmpty());

    component.setData("import QtQuick 1.0\nFlow { Item {} }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QVERIFY(warningMessage.isEmpty());

    component.setData("import QtQuick 1.0\nColumn { Item { anchors.top: parent.top } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(warningMessage, QString("<Unknown File>:2:1: QML Column: Cannot specify top, bottom, verticalCenter, fill or centerIn anchors for items inside Column"));
    warningMessage.clear();

    component.setData("import QtQuick 1.0\nColumn { Item { anchors.centerIn: parent } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(warningMessage, QString("<Unknown File>:2:1: QML Column: Cannot specify top, bottom, verticalCenter, fill or centerIn anchors for items inside Column"));
    warningMessage.clear();

    component.setData("import QtQuick 1.0\nColumn { Item { anchors.left: parent.left } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QVERIFY(warningMessage.isEmpty());
    warningMessage.clear();

    component.setData("import QtQuick 1.0\nRow { Item { anchors.left: parent.left } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(warningMessage, QString("<Unknown File>:2:1: QML Row: Cannot specify left, right, horizontalCenter, fill or centerIn anchors for items inside Row"));
    warningMessage.clear();

    component.setData("import QtQuick 1.0\nRow { Item { anchors.fill: parent } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(warningMessage, QString("<Unknown File>:2:1: QML Row: Cannot specify left, right, horizontalCenter, fill or centerIn anchors for items inside Row"));
    warningMessage.clear();

    component.setData("import QtQuick 1.0\nRow { Item { anchors.top: parent.top } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QVERIFY(warningMessage.isEmpty());
    warningMessage.clear();

    component.setData("import QtQuick 1.0\nGrid { Item { anchors.horizontalCenter: parent.horizontalCenter } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(warningMessage, QString("<Unknown File>:2:1: QML Grid: Cannot specify anchors for items inside Grid"));
    warningMessage.clear();

    component.setData("import QtQuick 1.0\nGrid { Item { anchors.centerIn: parent } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(warningMessage, QString("<Unknown File>:2:1: QML Grid: Cannot specify anchors for items inside Grid"));
    warningMessage.clear();

    component.setData("import QtQuick 1.0\nFlow { Item { anchors.verticalCenter: parent.verticalCenter } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(warningMessage, QString("<Unknown File>:2:1: QML Flow: Cannot specify anchors for items inside Flow"));

    component.setData("import QtQuick 1.0\nFlow { Item { anchors.fill: parent } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QDeclarativeItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(warningMessage, QString("<Unknown File>:2:1: QML Flow: Cannot specify anchors for items inside Flow"));
    qInstallMessageHandler(oldMsgHandler);
}

void tst_QDeclarativePositioners::test_vertical_qgraphicswidget()
{
    QDeclarativeView *canvas = createView(SRCDIR "/data/verticalqgraphicswidget.qml");

    QGraphicsWidget *one = canvas->rootObject()->findChild<QGraphicsWidget*>("one");
    QVERIFY(one != 0);

    QGraphicsWidget *two = canvas->rootObject()->findChild<QGraphicsWidget*>("two");
    QVERIFY(two != 0);

    QGraphicsWidget *three = canvas->rootObject()->findChild<QGraphicsWidget*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 0.0);
    QCOMPARE(two->y(), 50.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 60.0);

    QDeclarativeItem *column = canvas->rootObject()->findChild<QDeclarativeItem*>("column");
    QVERIFY(column);
    QCOMPARE(column->height(), 80.0);
    QCOMPARE(column->width(), 50.0);

    two->resize(QSizeF(two->size().width(), 20.0));
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 70.0);

    two->setOpacity(0.0);
    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 50.0);

    one->setVisible(false);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 0.0);

    delete canvas;
}

void tst_QDeclarativePositioners::test_mirroring()
{
    QList<QString> qmlFiles;
    qmlFiles << "horizontal.qml" << "gridtest.qml" << "flowtest.qml";
    QList<QString> objectNames;
    objectNames << "one" << "two" << "three" << "four" << "five";

    foreach(const QString qmlFile, qmlFiles) {
        QDeclarativeView *canvasA = createView(QString(SRCDIR) + "/data/" + qmlFile);
        QDeclarativeItem *rootA = qobject_cast<QDeclarativeItem*>(canvasA->rootObject());

        QDeclarativeView *canvasB = createView(QString(SRCDIR) + "/data/" + qmlFile);
        QDeclarativeItem *rootB = qobject_cast<QDeclarativeItem*>(canvasB->rootObject());

        rootA->setProperty("testRightToLeft", true); // layoutDirection: Qt.RightToLeft

        // LTR != RTL
        foreach(const QString objectName, objectNames) {
            // horizontal.qml only has three items
            if (qmlFile == QString("horizontal.qml") && objectName == QString("four"))
                break;
            QDeclarativeItem *itemA = rootA->findChild<QDeclarativeItem*>(objectName);
            QDeclarativeItem *itemB = rootB->findChild<QDeclarativeItem*>(objectName);
            QVERIFY(itemA->x() != itemB->x());
        }

        QDeclarativeItemPrivate* rootPrivateB = QDeclarativeItemPrivate::get(rootB);

        rootPrivateB->effectiveLayoutMirror = true; // LayoutMirroring.enabled: true
        rootPrivateB->isMirrorImplicit = false;
        rootPrivateB->inheritMirrorFromItem = true; // LayoutMirroring.childrenInherit: true
        rootPrivateB->resolveLayoutMirror();

        // RTL == mirror
        foreach(const QString objectName, objectNames) {
            // horizontal.qml only has three items
            if (qmlFile == QString("horizontal.qml") && objectName == QString("four"))
                break;
            QDeclarativeItem *itemA = rootA->findChild<QDeclarativeItem*>(objectName);
            QDeclarativeItem *itemB = rootB->findChild<QDeclarativeItem*>(objectName);
            QCOMPARE(itemA->x(), itemB->x());
        }

        rootA->setProperty("testRightToLeft", false); // layoutDirection: Qt.LeftToRight
        rootB->setProperty("testRightToLeft", true); // layoutDirection: Qt.RightToLeft

        // LTR == RTL + mirror
        foreach(const QString objectName, objectNames) {
            // horizontal.qml only has three items
            if (qmlFile == QString("horizontal.qml") && objectName == QString("four"))
                break;
            QDeclarativeItem *itemA = rootA->findChild<QDeclarativeItem*>(objectName);
            QDeclarativeItem *itemB = rootB->findChild<QDeclarativeItem*>(objectName);
            QCOMPARE(itemA->x(), itemB->x());
        }
        delete canvasA;
        delete canvasB;
    }
}

void tst_QDeclarativePositioners::testQtQuick11Attributes()
{
    QFETCH(QString, code);
    QFETCH(QString, warning);
    QFETCH(QString, error);

    QDeclarativeEngine engine;
    QObject *obj;

    QDeclarativeComponent valid(&engine);
    valid.setData("import QtQuick 1.1; " + code.toUtf8(), QUrl(""));
    obj = valid.create();
    QVERIFY(obj);
    QVERIFY(valid.errorString().isEmpty());
    delete obj;

    QDeclarativeComponent invalid(&engine);
    invalid.setData("import QtQuick 1.0; " + code.toUtf8(), QUrl(""));
    QTest::ignoreMessage(QtWarningMsg, warning.toUtf8());
    obj = invalid.create();
    QCOMPARE(invalid.errorString(), error);
    delete obj;
}

void tst_QDeclarativePositioners::testQtQuick11Attributes_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QString>("warning");
    QTest::addColumn<QString>("error");

    QTest::newRow("Flow.layoutDirection") << "Flow { layoutDirection: Qt.LeftToRight }"
        << "QDeclarativeComponent: Component is not ready"
        << ":1 \"Flow.layoutDirection\" is not available in QtQuick 1.0.\n";

    QTest::newRow("Row.layoutDirection") << "Row { layoutDirection: Qt.LeftToRight }"
        << "QDeclarativeComponent: Component is not ready"
        << ":1 \"Row.layoutDirection\" is not available in QtQuick 1.0.\n";

    QTest::newRow("Grid.layoutDirection") << "Grid { layoutDirection: Qt.LeftToRight }"
        << "QDeclarativeComponent: Component is not ready"
        << ":1 \"Grid.layoutDirection\" is not available in QtQuick 1.0.\n";
}

QDeclarativeView *tst_QDeclarativePositioners::createView(const QString &filename)
{
    QDeclarativeView *canvas = new QDeclarativeView(0);

    canvas->setSource(QUrl::fromLocalFile(filename));

    return canvas;
}


QTEST_MAIN(tst_QDeclarativePositioners)

#include "tst_qdeclarativepositioners.moc"
