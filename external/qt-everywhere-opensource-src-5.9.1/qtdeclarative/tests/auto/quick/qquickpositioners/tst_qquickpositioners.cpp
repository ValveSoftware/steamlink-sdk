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
#include <QtQuick/qquickview.h>
#include <qqmlengine.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQuick/private/qquickpositioners_p.h>
#include <QtQuick/private/qquicktransition_p.h>
#include <private/qquickitem_p.h>
#include <qqmlexpression.h>
#include "../shared/viewtestutil.h"
#include "../shared/visualtestutil.h"
#include "../../shared/util.h"

using namespace QQuickViewTestUtil;
using namespace QQuickVisualTestUtil;

class tst_qquickpositioners : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickpositioners();

private slots:
    void test_horizontal();
    void test_horizontal_padding();
    void test_horizontal_rtl();
    void test_horizontal_spacing();
    void test_horizontal_spacing_rightToLeft();
    void test_horizontal_animated();
    void test_horizontal_animated_padding();
    void test_horizontal_animated_rightToLeft();
    void test_horizontal_animated_rightToLeft_padding();
    void test_horizontal_animated_disabled();
    void test_horizontal_animated_disabled_padding();
    void test_vertical();
    void test_vertical_padding();
    void test_vertical_spacing();
    void test_vertical_animated();
    void test_vertical_animated_padding();
    void test_grid();
    void test_grid_padding();
    void test_grid_topToBottom();
    void test_grid_rightToLeft();
    void test_grid_spacing();
    void test_grid_row_column_spacing();
    void test_grid_animated();
    void test_grid_animated_padding();
    void test_grid_animated_rightToLeft();
    void test_grid_animated_rightToLeft_padding();
    void test_grid_zero_columns();
    void test_grid_H_alignment();
    void test_grid_H_alignment_padding();
    void test_grid_V_alignment();
    void test_grid_V_alignment_padding();
    void test_propertychanges();
    void test_repeater();
    void test_repeater_padding();
    void test_flow();
    void test_flow_padding();
    void test_flow_rightToLeft();
    void test_flow_topToBottom();
    void test_flow_topToBottom_padding();
    void test_flow_resize();
    void test_flow_resize_padding();
    void test_flow_resize_rightToLeft();
    void test_flow_resize_rightToLeft_padding();
    void test_flow_implicit_resize();
    void test_flow_implicit_resize_padding();
    void test_conflictinganchors();
    void test_mirroring();
    void test_allInvisible();
    void test_attachedproperties();
    void test_attachedproperties_data();
    void test_attachedproperties_dynamic();

    void populateTransitions_row();
    void populateTransitions_row_data();
    void populateTransitions_column();
    void populateTransitions_column_data();
    void populateTransitions_grid();
    void populateTransitions_grid_data();
    void populateTransitions_flow();
    void populateTransitions_flow_data();
    void addTransitions_row();
    void addTransitions_row_data();
    void addTransitions_column();
    void addTransitions_column_data();
    void addTransitions_grid();
    void addTransitions_grid_data();
    void addTransitions_flow();
    void addTransitions_flow_data();
    void moveTransitions_row();
    void moveTransitions_row_data();
    void moveTransitions_column();
    void moveTransitions_column_data();
    void moveTransitions_grid();
    void moveTransitions_grid_data();
    void moveTransitions_flow();
    void moveTransitions_flow_data();

private:
    QQuickView *createView(const QString &filename, bool wait=true);

    void populateTransitions(const QString &positionerObjectName);
    void populateTransitions_data();
    void addTransitions(const QString &positionerObjectName);
    void addTransitions_data();
    void moveTransitions(const QString &positionerObjectName);
    void moveTransitions_data();
    void matchIndexLists(const QVariantList &indexLists, const QList<int> &expectedIndexes);
    void matchItemsAndIndexes(const QVariantMap &items, const QaimModel &model, const QList<int> &expectedIndexes);
    void matchItemLists(const QVariantList &itemLists, const QList<QQuickItem *> &expectedItems);
    void checkItemPositions(QQuickItem *positioner, QaimModel *model, qreal incrementalSize);
};

void tst_qquickpositioners::populateTransitions_row()
{
    populateTransitions("row");
}

void tst_qquickpositioners::populateTransitions_row_data()
{
    populateTransitions_data();
}

void tst_qquickpositioners::populateTransitions_column()
{
    populateTransitions("column");
}

void tst_qquickpositioners::populateTransitions_column_data()
{
    populateTransitions_data();
}

void tst_qquickpositioners::populateTransitions_grid()
{
    populateTransitions("grid");
}

void tst_qquickpositioners::populateTransitions_grid_data()
{
    populateTransitions_data();
}

void tst_qquickpositioners::populateTransitions_flow()
{
    populateTransitions("flow");
}

void tst_qquickpositioners::populateTransitions_flow_data()
{
    populateTransitions_data();
}

void tst_qquickpositioners::addTransitions_row()
{
    addTransitions("row");
}

void tst_qquickpositioners::addTransitions_row_data()
{
    addTransitions_data();
}

void tst_qquickpositioners::addTransitions_column()
{
    addTransitions("column");
}

void tst_qquickpositioners::addTransitions_column_data()
{
    addTransitions_data();
}

void tst_qquickpositioners::addTransitions_grid()
{
    addTransitions("grid");
}

void tst_qquickpositioners::addTransitions_grid_data()
{
    // don't use addTransitions_data() because grid displaces items differently
    // (adding items further down the grid can cause displace transitions at
    // previous indexes, since grid is auto-resized to tightly fit all of its items)

    QTest::addColumn<QString>("qmlFile");
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<int>("insertionIndex");
    QTest::addColumn<int>("insertionCount");
    QTest::addColumn<ListRange>("expectedDisplacedIndexes");

    QTest::newRow("add one @ start") << "transitions.qml" << 10 << 0 << 1 << ListRange(0, 9);
    QTest::newRow("add one @ middle") << "transitions.qml" << 10 << 5 << 1 << ListRange(3, 3) + ListRange(5, 9);
    QTest::newRow("add one @ end") << "transitions.qml" << 10 << 10 << 1 << ListRange(3, 3) + ListRange(7, 7);
    QTest::newRow("padding, add one @ start") << "transitions-padding.qml" << 10 << 0 << 1 << ListRange(0, 9);
    QTest::newRow("padding, add one @ middle") << "transitions-padding.qml" << 10 << 5 << 1 << ListRange(3, 3) + ListRange(5, 9);
    QTest::newRow("padding, add one @ end") << "transitions-padding.qml" << 10 << 10 << 1 << ListRange(3, 3) + ListRange(7, 7);

    QTest::newRow("add multiple @ start") << "transitions.qml" << 10 << 0 << 3 << ListRange(0, 9);
    QTest::newRow("add multiple @ middle") << "transitions.qml" << 10 << 5 << 3 << ListRange(1, 3) + ListRange(5, 9);
    QTest::newRow("add multiple @ end") << "transitions.qml" << 10 << 10 << 3 << ListRange(1, 3) + ListRange(5, 7) + ListRange(9, 9);
    QTest::newRow("padding, add multiple @ start") << "transitions-padding.qml" << 10 << 0 << 3 << ListRange(0, 9);
    QTest::newRow("padding, add multiple @ middle") << "transitions-padding.qml" << 10 << 5 << 3 << ListRange(1, 3) + ListRange(5, 9);
    QTest::newRow("padding, add multiple @ end") << "transitions-padding.qml" << 10 << 10 << 3 << ListRange(1, 3) + ListRange(5, 7) + ListRange(9, 9);
}

void tst_qquickpositioners::addTransitions_flow()
{
    addTransitions("flow");
}

void tst_qquickpositioners::addTransitions_flow_data()
{
    addTransitions_data();
}

void tst_qquickpositioners::moveTransitions_row()
{
    moveTransitions("row");
}

void tst_qquickpositioners::moveTransitions_row_data()
{
    moveTransitions_data();
}

void tst_qquickpositioners::moveTransitions_column()
{
    moveTransitions("column");
}

void tst_qquickpositioners::moveTransitions_column_data()
{
    moveTransitions_data();
}

void tst_qquickpositioners::moveTransitions_grid()
{
    moveTransitions("grid");
}

void tst_qquickpositioners::moveTransitions_grid_data()
{
    // don't use moveTransitions_data() because grid displaces items differently
    // (removing items further down the grid can cause displace transitions at
    // previous indexes, since grid is auto-resized to tightly fit all of its items)

    QTest::addColumn<QString>("qmlFile");
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<ListChange>("change");
    QTest::addColumn<ListRange>("expectedDisplacedIndexes");

    QTest::newRow("remove one @ start") << "transitions.qml" << 10 << ListChange::remove(0, 1) << ListRange(1, 9);
    QTest::newRow("remove one @ middle") << "transitions.qml" << 10 << ListChange::remove(4, 1) << ListRange(2, 3) + ListRange(5, 9);
    QTest::newRow("remove one @ end") << "transitions.qml" << 10 << ListChange::remove(9, 1) << ListRange(2, 3) + ListRange(6, 7);
    QTest::newRow("padding, remove one @ start") << "transitions-padding.qml" << 10 << ListChange::remove(0, 1) << ListRange(1, 9);
    QTest::newRow("padding, remove one @ middle") << "transitions-padding.qml" << 10 << ListChange::remove(4, 1) << ListRange(2, 3) + ListRange(5, 9);
    QTest::newRow("padding, remove one @ end") << "transitions-padding.qml" << 10 << ListChange::remove(9, 1) << ListRange(2, 3) + ListRange(6, 7);

    QTest::newRow("remove multiple @ start") << "transitions.qml" << 10 << ListChange::remove(0, 3) << ListRange(3, 9);
    QTest::newRow("remove multiple @ middle") << "transitions.qml" << 10 << ListChange::remove(4, 3) << ListRange(1, 3) + ListRange(7, 9);
    QTest::newRow("remove multiple @ end") << "transitions.qml" << 10 << ListChange::remove(7, 3) << ListRange(1, 3) + ListRange(5, 6);
    QTest::newRow("padding, remove multiple @ start") << "transitions-padding.qml" << 10 << ListChange::remove(0, 3) << ListRange(3, 9);
    QTest::newRow("padding, remove multiple @ middle") << "transitions-padding.qml" << 10 << ListChange::remove(4, 3) << ListRange(1, 3) + ListRange(7, 9);
    QTest::newRow("padding, remove multiple @ end") << "transitions-padding.qml" << 10 << ListChange::remove(7, 3) << ListRange(1, 3) + ListRange(5, 6);
}

void tst_qquickpositioners::moveTransitions_flow()
{
    moveTransitions("flow");
}

void tst_qquickpositioners::moveTransitions_flow_data()
{
    moveTransitions_data();
}

tst_qquickpositioners::tst_qquickpositioners()
{
}

void tst_qquickpositioners::test_horizontal()
{
    QScopedPointer<QQuickView> window(createView(testFile("horizontal.qml")));

    window->rootObject()->setProperty("testRightToLeft", false);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 0.0);

    QQuickItem *row = window->rootObject()->findChild<QQuickItem*>("row");
    QCOMPARE(row->width(), 110.0);
    QCOMPARE(row->height(), 50.0);

    // test padding
    row->setProperty("padding", 1);
    row->setProperty("topPadding", 2);
    row->setProperty("leftPadding", 3);
    row->setProperty("rightPadding", 4);
    row->setProperty("bottomPadding", 5);

    QTRY_COMPARE(row->width(), 117.0);
    QCOMPARE(row->height(), 57.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 2.0);
}

void tst_qquickpositioners::test_horizontal_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("horizontal.qml")));

    window->rootObject()->setProperty("testRightToLeft", false);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 0.0);

    QQuickItem *row = window->rootObject()->findChild<QQuickItem*>("row");
    QCOMPARE(row->width(), 110.0);
    QCOMPARE(row->height(), 50.0);

    QQuickRow *obj = qobject_cast<QQuickRow*>(row);
    QVERIFY(obj != 0);

    QCOMPARE(row->property("padding").toDouble(), 0.0);
    QCOMPARE(row->property("topPadding").toDouble(), 0.0);
    QCOMPARE(row->property("leftPadding").toDouble(), 0.0);
    QCOMPARE(row->property("rightPadding").toDouble(), 0.0);
    QCOMPARE(row->property("bottomPadding").toDouble(), 0.0);

    obj->setPadding(1.0);

    QCOMPARE(row->property("padding").toDouble(), 1.0);
    QCOMPARE(row->property("topPadding").toDouble(), 1.0);
    QCOMPARE(row->property("leftPadding").toDouble(), 1.0);
    QCOMPARE(row->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(row->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(row->width(), 112.0);
    QCOMPARE(row->height(), 52.0);

    QCOMPARE(one->x(), 1.0);
    QCOMPARE(one->y(), 1.0);
    QCOMPARE(two->x(), 51.0);
    QCOMPARE(two->y(), 1.0);
    QCOMPARE(three->x(), 71.0);
    QCOMPARE(three->y(), 1.0);

    obj->setTopPadding(2.0);

    QCOMPARE(row->property("padding").toDouble(), 1.0);
    QCOMPARE(row->property("topPadding").toDouble(), 2.0);
    QCOMPARE(row->property("leftPadding").toDouble(), 1.0);
    QCOMPARE(row->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(row->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(row->height(), 53.0);
    QCOMPARE(row->width(), 112.0);

    QCOMPARE(one->x(), 1.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 51.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 71.0);
    QCOMPARE(three->y(), 2.0);

    obj->setLeftPadding(3.0);

    QCOMPARE(row->property("padding").toDouble(), 1.0);
    QCOMPARE(row->property("topPadding").toDouble(), 2.0);
    QCOMPARE(row->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(row->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(row->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(row->width(), 114.0);
    QCOMPARE(row->height(), 53.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 2.0);

    obj->setRightPadding(4.0);

    QCOMPARE(row->property("padding").toDouble(), 1.0);
    QCOMPARE(row->property("topPadding").toDouble(), 2.0);
    QCOMPARE(row->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(row->property("rightPadding").toDouble(), 4.0);
    QCOMPARE(row->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(row->width(), 117.0);
    QCOMPARE(row->height(), 53.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 2.0);

    obj->setBottomPadding(5.0);

    QCOMPARE(row->property("padding").toDouble(), 1.0);
    QCOMPARE(row->property("topPadding").toDouble(), 2.0);
    QCOMPARE(row->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(row->property("rightPadding").toDouble(), 4.0);
    QCOMPARE(row->property("bottomPadding").toDouble(), 5.0);

    QTRY_COMPARE(row->height(), 57.0);
    QCOMPARE(row->width(), 117.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 2.0);

    obj->resetBottomPadding();
    QCOMPARE(row->property("bottomPadding").toDouble(), 1.0);
    QTRY_COMPARE(row->height(), 53.0);
    QCOMPARE(row->width(), 117.0);

    obj->resetRightPadding();
    QCOMPARE(row->property("rightPadding").toDouble(), 1.0);
    QTRY_COMPARE(row->width(), 114.0);
    QCOMPARE(row->height(), 53.0);

    obj->resetLeftPadding();
    QCOMPARE(row->property("leftPadding").toDouble(), 1.0);
    QTRY_COMPARE(row->width(), 112.0);
    QCOMPARE(row->height(), 53.0);

    obj->resetTopPadding();
    QCOMPARE(row->property("topPadding").toDouble(), 1.0);
    QTRY_COMPARE(row->height(), 52.0);
    QCOMPARE(row->width(), 112.0);

    obj->resetPadding();
    QCOMPARE(row->property("padding").toDouble(), 0.0);
    QCOMPARE(row->property("topPadding").toDouble(), 0.0);
    QCOMPARE(row->property("leftPadding").toDouble(), 0.0);
    QCOMPARE(row->property("rightPadding").toDouble(), 0.0);
    QCOMPARE(row->property("bottomPadding").toDouble(), 0.0);
    QTRY_COMPARE(row->height(), 50.0);
    QCOMPARE(row->width(), 110.0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 0.0);
}

void tst_qquickpositioners::test_horizontal_rtl()
{
      QScopedPointer<QQuickView> window(createView(testFile("horizontal.qml")));

    window->rootObject()->setProperty("testRightToLeft", true);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 60.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 40.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 0.0);

    QQuickItem *row = window->rootObject()->findChild<QQuickItem*>("row");
    QCOMPARE(row->width(), 110.0);
    QCOMPARE(row->height(), 50.0);

    // test padding
    row->setProperty("padding", 1);
    row->setProperty("topPadding", 2);
    row->setProperty("leftPadding", 3);
    row->setProperty("rightPadding", 4);
    row->setProperty("bottomPadding", 5);

    QTRY_COMPARE(row->width(), 117.0);
    QCOMPARE(row->height(), 57.0);

    QCOMPARE(one->x(), 63.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 43.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 2.0);

    row->setProperty("topPadding", 0);
    row->setProperty("leftPadding", 0);
    row->setProperty("rightPadding", 0);
    row->setProperty("bottomPadding", 0);
    row->setProperty("padding", 0);

    QTRY_COMPARE(one->x(), 60.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 40.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 0.0);

    // Change the width of the row and check that items stay to the right
    row->setWidth(200);
    QTRY_COMPARE(one->x(), 150.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 130.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 90.0);
    QCOMPARE(three->y(), 0.0);

    row->setProperty("padding", 1);
    row->setProperty("topPadding", 2);
    row->setProperty("leftPadding", 3);
    row->setProperty("rightPadding", 4);
    row->setProperty("bottomPadding", 5);

    QTRY_COMPARE(one->x(), 146.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 126.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 86.0);
    QCOMPARE(three->y(), 2.0);
}

void tst_qquickpositioners::test_horizontal_spacing()
{
    QScopedPointer<QQuickView> window(createView(testFile("horizontal-spacing.qml")));

    window->rootObject()->setProperty("testRightToLeft", false);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 60.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 90.0);
    QCOMPARE(three->y(), 0.0);

    QQuickItem *row = window->rootObject()->findChild<QQuickItem*>("row");
    QCOMPARE(row->width(), 130.0);
    QCOMPARE(row->height(), 50.0);

    // test padding
    row->setProperty("padding", 1);
    row->setProperty("topPadding", 2);
    row->setProperty("leftPadding", 3);
    row->setProperty("rightPadding", 4);
    row->setProperty("bottomPadding", 5);

    QTRY_COMPARE(row->width(), 137.0);
    QCOMPARE(row->height(), 57.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 63.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 93.0);
    QCOMPARE(three->y(), 2.0);
}

void tst_qquickpositioners::test_horizontal_spacing_rightToLeft()
{
    QScopedPointer<QQuickView> window(createView(testFile("horizontal-spacing.qml")));

    window->rootObject()->setProperty("testRightToLeft", true);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 80.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 0.0);

    QQuickItem *row = window->rootObject()->findChild<QQuickItem*>("row");
    QCOMPARE(row->width(), 130.0);
    QCOMPARE(row->height(), 50.0);

    // test padding
    row->setProperty("padding", 1);
    row->setProperty("topPadding", 2);
    row->setProperty("leftPadding", 3);
    row->setProperty("rightPadding", 4);
    row->setProperty("bottomPadding", 5);

    QTRY_COMPARE(row->width(), 137.0);
    QCOMPARE(row->height(), 57.0);

    QCOMPARE(one->x(), 83.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 2.0);
}

void tst_qquickpositioners::test_horizontal_animated()
{
    QScopedPointer<QQuickView> window(createView(testFile("horizontal-animated.qml"), false));

    window->rootObject()->setProperty("testRightToLeft", false);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    //Note that they animate in
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(three->x(), -100.0);

    QVERIFY(QTest::qWaitForWindowExposed(window.data())); //It may not relayout until the next frame, so it needs to be drawn

    QQuickItem *row = window->rootObject()->findChild<QQuickItem*>("row");
    QVERIFY(row);
    QCOMPARE(row->width(), 100.0);
    QCOMPARE(row->height(), 50.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->x(), 0.0);
    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(two->isVisible(), false);
    QTRY_COMPARE(two->x(), -100.0);//Not 'in' yet
    QTRY_COMPARE(two->y(), 0.0);
    QTRY_COMPARE(three->x(), 50.0);
    QTRY_COMPARE(three->y(), 0.0);

    //Add 'two'
    two->setVisible(true);
    QTRY_COMPARE(two->isVisible(), true);
    QTRY_COMPARE(row->width(), 150.0);
    QTRY_COMPARE(row->height(), 50.0);

    QTest::qWait(0);//Let the animation start
    QVERIFY(two->x() >= -100.0 && two->x() < 50.0);
    QVERIFY(three->x() >= 50.0 && three->x() < 100.0);

    QTRY_COMPARE(two->x(), 50.0);
    QTRY_COMPARE(three->x(), 100.0);

}

void tst_qquickpositioners::test_horizontal_animated_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("horizontal-animated.qml"), false));

    window->rootObject()->setProperty("testRightToLeft", false);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    //Note that they animate in
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(three->x(), -100.0);

    QVERIFY(QTest::qWaitForWindowExposed(window.data())); //It may not relayout until the next frame, so it needs to be drawn

    QQuickItem *row = window->rootObject()->findChild<QQuickItem*>("row");
    QVERIFY(row);
    QCOMPARE(row->width(), 100.0);
    QCOMPARE(row->height(), 50.0);

    // test padding
    row->setProperty("padding", 1);
    row->setProperty("topPadding", 2);
    row->setProperty("leftPadding", 3);
    row->setProperty("rightPadding", 4);
    row->setProperty("bottomPadding", 5);

    QTRY_COMPARE(row->width(), 107.0);
    QCOMPARE(row->height(), 57.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->x(), 3.0);
    QTRY_COMPARE(one->y(), 2.0);
    QTRY_COMPARE(two->isVisible(), false);
    QTRY_COMPARE(two->x(), -100.0);//Not 'in' yet
    QTRY_COMPARE(two->y(), 0.0);
    QTRY_COMPARE(three->x(), 53.0);
    QTRY_COMPARE(three->y(), 2.0);

    //Add 'two'
    two->setVisible(true);
    QTRY_COMPARE(two->isVisible(), true);
    QTRY_COMPARE(row->width(), 157.0);
    QTRY_COMPARE(row->height(), 57.0);

    QTest::qWait(0);//Let the animation start
    QVERIFY(two->x() >= -100.0 && two->x() < 53.0);
    QVERIFY(three->x() >= 53.0 && three->x() < 103.0);

    QTRY_COMPARE(two->y(), 2.0);
    QTRY_COMPARE(two->x(), 53.0);
    QTRY_COMPARE(three->x(), 103.0);

}

void tst_qquickpositioners::test_horizontal_animated_rightToLeft()
{
    QScopedPointer<QQuickView> window(createView(testFile("horizontal-animated.qml"), false));

    window->rootObject()->setProperty("testRightToLeft", true);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    //Note that they animate in
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(three->x(), -100.0);

    QVERIFY(QTest::qWaitForWindowExposed(window.data())); //It may not relayout until the next frame, so it needs to be drawn

    QQuickItem *row = window->rootObject()->findChild<QQuickItem*>("row");
    QVERIFY(row);
    QCOMPARE(row->width(), 100.0);
    QCOMPARE(row->height(), 50.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->x(), 50.0);
    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(two->isVisible(), false);
    QTRY_COMPARE(two->x(), -100.0);//Not 'in' yet
    QTRY_COMPARE(two->y(), 0.0);
    QTRY_COMPARE(three->x(), 0.0);
    QTRY_COMPARE(three->y(), 0.0);

    //Add 'two'
    two->setVisible(true);
    QTRY_COMPARE(two->isVisible(), true);

    // New size should propagate after visible change
    QTRY_COMPARE(row->width(), 150.0);
    QTRY_COMPARE(row->height(), 50.0);

    QTest::qWait(0);//Let the animation start
    QVERIFY(one->x() >= 50.0 && one->x() < 100);
    QVERIFY(two->x() >= -100.0 && two->x() < 50.0);

    QTRY_COMPARE(one->x(), 100.0);
    QTRY_COMPARE(two->x(), 50.0);

}

void tst_qquickpositioners::test_horizontal_animated_rightToLeft_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("horizontal-animated.qml"), false));

    window->rootObject()->setProperty("testRightToLeft", true);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    //Note that they animate in
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(three->x(), -100.0);

    QVERIFY(QTest::qWaitForWindowExposed(window.data())); //It may not relayout until the next frame, so it needs to be drawn

    QQuickItem *row = window->rootObject()->findChild<QQuickItem*>("row");
    QVERIFY(row);
    QCOMPARE(row->width(), 100.0);
    QCOMPARE(row->height(), 50.0);

    // test padding
    row->setProperty("padding", 1);
    row->setProperty("topPadding", 2);
    row->setProperty("leftPadding", 3);
    row->setProperty("rightPadding", 4);
    row->setProperty("bottomPadding", 5);

    QTRY_COMPARE(row->width(), 107.0);
    QCOMPARE(row->height(), 57.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->x(), 53.0);
    QTRY_COMPARE(one->y(), 2.0);
    QTRY_COMPARE(two->isVisible(), false);
    QTRY_COMPARE(two->x(), -100.0);//Not 'in' yet
    QTRY_COMPARE(two->y(), 0.0);
    QTRY_COMPARE(three->x(), 3.0);
    QTRY_COMPARE(three->y(), 2.0);

    //Add 'two'
    two->setVisible(true);
    QTRY_COMPARE(two->isVisible(), true);

    // New size should propagate after visible change
    QTRY_COMPARE(row->width(), 157.0);
    QTRY_COMPARE(row->height(), 57.0);

    QTest::qWait(0);//Let the animation start
    QVERIFY(one->x() >= 53.0 && one->x() < 100);
    QVERIFY(two->x() >= -100.0 && two->x() < 53.0);

    QTRY_COMPARE(one->x(), 103.0);
    QTRY_COMPARE(two->y(), 2.0);
    QTRY_COMPARE(two->x(), 53.0);

}

void tst_qquickpositioners::test_horizontal_animated_disabled()
{
    QScopedPointer<QQuickView> window(createView(testFile("horizontal-animated-disabled.qml")));

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    QQuickItem *row = window->rootObject()->findChild<QQuickItem*>("row");
    QVERIFY(row);

    qApp->processEvents();

    // test padding
    row->setProperty("padding", 1);
    row->setProperty("topPadding", 2);
    row->setProperty("leftPadding", 3);
    row->setProperty("rightPadding", 4);
    row->setProperty("bottomPadding", 5);

    QTRY_COMPARE(row->width(), 107.0);
    QCOMPARE(row->height(), 57.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->isVisible(), false);
    QCOMPARE(two->x(), -100.0);//Not 'in' yet
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 53.0);
    QCOMPARE(three->y(), 2.0);

    //Add 'two'
    two->setVisible(true);
    QCOMPARE(two->isVisible(), true);
    QTRY_COMPARE(row->width(), 157.0);
    QTRY_COMPARE(row->height(), 57.0);

    QTRY_COMPARE(two->y(), 2.0);
    QTRY_COMPARE(two->x(), 53.0);
    QTRY_COMPARE(three->x(), 103.0);

}

void tst_qquickpositioners::test_horizontal_animated_disabled_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("horizontal-animated-disabled.qml")));

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    QQuickItem *row = window->rootObject()->findChild<QQuickItem*>("row");
    QVERIFY(row);

    qApp->processEvents();

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->isVisible(), false);
    QCOMPARE(two->x(), -100.0);//Not 'in' yet
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 50.0);
    QCOMPARE(three->y(), 0.0);

    //Add 'two'
    two->setVisible(true);
    QCOMPARE(two->isVisible(), true);
    QTRY_COMPARE(row->width(), 150.0);
    QTRY_COMPARE(row->height(), 50.0);

    QTRY_COMPARE(two->x(), 50.0);
    QTRY_COMPARE(three->x(), 100.0);

}

void tst_qquickpositioners::populateTransitions(const QString &positionerObjectName)
{
    QFETCH(QString, qmlFile);
    QFETCH(bool, dynamicallyPopulate);
    QFETCH(bool, usePopulateTransition);

    QPointF targetItems_transitionFrom(-50, -50);
    QPointF displacedItems_transitionVia(100, 100);

    QaimModel model;
    if (!dynamicallyPopulate) {
        for (int i = 0; i < 30; i++)
            model.addItem("Original item" + QString::number(i), "");
    }

    QaimModel model_targetItems_transitionFrom;
    QaimModel model_displacedItems_transitionVia;

    QScopedPointer<QQuickView> window(QQuickViewTestUtil::createView());

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("usePopulateTransition", usePopulateTransition);
    ctxt->setContextProperty("enableAddTransition", true);
    ctxt->setContextProperty("dynamicallyPopulate", dynamicallyPopulate);
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("model_targetItems_transitionFrom", &model_targetItems_transitionFrom);
    ctxt->setContextProperty("model_displacedItems_transitionVia", &model_displacedItems_transitionVia);
    ctxt->setContextProperty("targetItems_transitionFrom", targetItems_transitionFrom);
    ctxt->setContextProperty("displacedItems_transitionVia", displacedItems_transitionVia);
    ctxt->setContextProperty("testedPositioner", positionerObjectName);
    window->setSource(testFileUrl(qmlFile));

    QQuickItem *positioner = window->rootObject()->findChild<QQuickItem*>(positionerObjectName);
    QVERIFY(positioner);
    window->show();
    QTest::qWaitForWindowExposed(window.data());
    qApp->processEvents();

    if (!dynamicallyPopulate && usePopulateTransition) {
        QTRY_COMPARE(window->rootObject()->property("populateTransitionsDone").toInt(), model.count());
        QTRY_COMPARE(window->rootObject()->property("addTransitionsDone").toInt(), 0);

        QList<QPair<QString, QString> > targetData;
        QList<int> targetIndexes;
        for (int i=0; i<model.count(); i++) {
            targetData << qMakePair(model.name(i), model.number(i));
            targetIndexes << i;
        }
        QList<QQuickItem *> targetItems = findItems<QQuickItem>(positioner, "wrapper", targetIndexes);
        model_targetItems_transitionFrom.matchAgainst(targetData, "wasn't animated from target 'from' pos", "shouldn't have been animated from target 'from' pos");
        matchItemsAndIndexes(window->rootObject()->property("targetTrans_items").toMap(), model, targetIndexes);
        matchIndexLists(window->rootObject()->property("targetTrans_targetIndexes").toList(), targetIndexes);
        matchItemLists(window->rootObject()->property("targetTrans_targetItems").toList(), targetItems);

    } else if (dynamicallyPopulate) {
        QTRY_COMPARE(window->rootObject()->property("populateTransitionsDone").toInt(), 0);
        QTRY_COMPARE(window->rootObject()->property("addTransitionsDone").toInt(), model.count());
    } else {
        QTRY_COMPARE(QQuickItemPrivate::get(positioner)->polishScheduled, false);
        QTRY_COMPARE(window->rootObject()->property("populateTransitionsDone").toInt(), 0);
        QTRY_COMPARE(window->rootObject()->property("addTransitionsDone").toInt(), 0);
    }

    checkItemPositions(positioner, &model, window->rootObject()->property("incrementalSize").toInt());

    // add an item and check this is done with add transition, not populate
    window->rootObject()->setProperty("populateTransitionsDone", 0);
    window->rootObject()->setProperty("addTransitionsDone", 0);
    model.insertItem(0, "new item", "");
    QTRY_COMPARE(window->rootObject()->property("addTransitionsDone").toInt(), 1);
    QTRY_COMPARE(window->rootObject()->property("populateTransitionsDone").toInt(), 0);
}

void tst_qquickpositioners::populateTransitions_data()
{
    QTest::addColumn<QString>("qmlFile");
    QTest::addColumn<bool>("dynamicallyPopulate");
    QTest::addColumn<bool>("usePopulateTransition");

    QTest::newRow("statically populate") << "transitions.qml" << false << true;
    QTest::newRow("statically populate, no populate transition") << "transitions.qml" << false << false;
    QTest::newRow("padding, statically populate") << "transitions-padding.qml" << false << true;
    QTest::newRow("padding, statically populate, no populate transition") << "transitions-padding.qml" << false << false;

    QTest::newRow("dynamically populate") << "transitions.qml" << true << true;
    QTest::newRow("dynamically populate, no populate transition") << "transitions.qml" << true << false;
    QTest::newRow("padding, dynamically populate") << "transitions-padding.qml" << true << true;
    QTest::newRow("padding, dynamically populate, no populate transition") << "transitions-padding.qml" << true << false;
}

void tst_qquickpositioners::addTransitions(const QString &positionerObjectName)
{
    QFETCH(QString, qmlFile);
    QFETCH(int, initialItemCount);
    QFETCH(int, insertionIndex);
    QFETCH(int, insertionCount);
    QFETCH(ListRange, expectedDisplacedIndexes);

    QPointF targetItems_transitionFrom(-50, -50);
    QPointF displacedItems_transitionVia(100, 100);

    QaimModel model;
    QaimModel model_targetItems_transitionFrom;
    QaimModel model_displacedItems_transitionVia;

    QScopedPointer<QQuickView> window(QQuickViewTestUtil::createView());
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("usePopulateTransition", QVariant(false));
    ctxt->setContextProperty("enableAddTransition", QVariant(true));
    ctxt->setContextProperty("dynamicallyPopulate", QVariant(false));
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("model_targetItems_transitionFrom", &model_targetItems_transitionFrom);
    ctxt->setContextProperty("model_displacedItems_transitionVia", &model_displacedItems_transitionVia);
    ctxt->setContextProperty("targetItems_transitionFrom", targetItems_transitionFrom);
    ctxt->setContextProperty("displacedItems_transitionVia", displacedItems_transitionVia);
    ctxt->setContextProperty("testedPositioner", QString());
    window->setSource(testFileUrl(qmlFile));
    window->show();
    QTest::qWaitForWindowExposed(window.data());
    qApp->processEvents();

    QQuickItem *positioner = window->rootObject()->findChild<QQuickItem*>(positionerObjectName);
    QVERIFY(positioner);
    positioner->findChild<QQuickItem*>("repeater")->setProperty("model", QVariant::fromValue(&model));
    QTRY_COMPARE(QQuickItemPrivate::get(positioner)->polishScheduled, false);

    for (int i = 0; i < initialItemCount; i++)
        model.addItem("Original item" + QString::number(i), "");

    QList<QPair<QString,QString> > expectedDisplacedValues = expectedDisplacedIndexes.getModelDataValues(model);
    QList<QPair<QString, QString> > targetData;
    QList<int> targetIndexes;
    for (int i=0; i<model.count(); i++) {
        targetData << qMakePair(model.name(i), model.number(i));
        targetIndexes << i;
    }
    QList<QQuickItem *> targetItems = findItems<QQuickItem>(positioner, "wrapper", targetIndexes);

    // check add transition was run for first lot of added items
    QTRY_COMPARE(window->rootObject()->property("populateTransitionsDone").toInt(), 0);
    QTRY_COMPARE(window->rootObject()->property("addTransitionsDone").toInt(), initialItemCount);
    QTRY_COMPARE(window->rootObject()->property("displaceTransitionsDone").toInt(), 0);
    model_targetItems_transitionFrom.matchAgainst(targetData, "wasn't animated from target 'from' pos", "shouldn't have been animated from target 'from' pos");
    matchItemsAndIndexes(window->rootObject()->property("targetTrans_items").toMap(), model, targetIndexes);
    matchIndexLists(window->rootObject()->property("targetTrans_targetIndexes").toList(), targetIndexes);
    matchItemLists(window->rootObject()->property("targetTrans_targetItems").toList(), targetItems);

    model_targetItems_transitionFrom.clear();
    window->rootObject()->setProperty("addTransitionsDone", 0);
    window->rootObject()->setProperty("targetTrans_items", QVariantMap());
    window->rootObject()->setProperty("targetTrans_targetIndexes", QVariantList());
    window->rootObject()->setProperty("targetTrans_targetItems", QVariantList());

    // do insertion
    targetData.clear();
    targetIndexes.clear();
    for (int i=insertionIndex; i<insertionIndex+insertionCount; i++) {
        targetData << qMakePair(QString("New item %1").arg(i), QString(""));
        targetIndexes << i;
    }
    model.insertItems(insertionIndex, targetData);
    QTRY_COMPARE(model.count(), positioner->property("count").toInt());

    targetItems = findItems<QQuickItem>(positioner, "wrapper", targetIndexes);

    QTRY_COMPARE(window->rootObject()->property("addTransitionsDone").toInt(), targetData.count());
    QTRY_COMPARE(window->rootObject()->property("displaceTransitionsDone").toInt(), expectedDisplacedIndexes.count());

    // check the target and displaced items were animated
    model_targetItems_transitionFrom.matchAgainst(targetData, "wasn't animated from target 'from' pos", "shouldn't have been animated from target 'from' pos");
    model_displacedItems_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with displaced anim", "shouldn't have been animated with displaced anim");

    // check attached properties
    matchItemsAndIndexes(window->rootObject()->property("targetTrans_items").toMap(), model, targetIndexes);
    matchIndexLists(window->rootObject()->property("targetTrans_targetIndexes").toList(), targetIndexes);
    matchItemLists(window->rootObject()->property("targetTrans_targetItems").toList(), targetItems);
    if (expectedDisplacedIndexes.isValid()) {
        // adjust expectedDisplacedIndexes to their final values after the move
        QList<int> displacedIndexes = adjustIndexesForAddDisplaced(expectedDisplacedIndexes.indexes, insertionIndex, insertionCount);
        matchItemsAndIndexes(window->rootObject()->property("displacedTrans_items").toMap(), model, displacedIndexes);
        matchIndexLists(window->rootObject()->property("displacedTrans_targetIndexes").toList(), targetIndexes);
        matchItemLists(window->rootObject()->property("displacedTrans_targetItems").toList(), targetItems);
    }

    checkItemPositions(positioner, &model, window->rootObject()->property("incrementalSize").toInt());
}

void tst_qquickpositioners::addTransitions_data()
{
    // If this data changes, update addTransitions_grid_data() also

    QTest::addColumn<QString>("qmlFile");
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<int>("insertionIndex");
    QTest::addColumn<int>("insertionCount");
    QTest::addColumn<ListRange>("expectedDisplacedIndexes");

    QTest::newRow("add one @ start") << "transitions.qml" << 10 << 0 << 1 << ListRange(0, 9);
    QTest::newRow("add one @ middle") << "transitions.qml" << 10 << 5 << 1 << ListRange(5, 9);
    QTest::newRow("add one @ end") << "transitions.qml" << 10 << 10 << 1 << ListRange();
    QTest::newRow("padding, add one @ start") << "transitions-padding.qml" << 10 << 0 << 1 << ListRange(0, 9);
    QTest::newRow("padding, add one @ middle") << "transitions-padding.qml" << 10 << 5 << 1 << ListRange(5, 9);
    QTest::newRow("padding, add one @ end") << "transitions-padding.qml" << 10 << 10 << 1 << ListRange();

    QTest::newRow("add multiple @ start") << "transitions.qml" << 10 << 0 << 3 << ListRange(0, 9);
    QTest::newRow("add multiple @ middle") << "transitions.qml" << 10 << 5 << 3 << ListRange(5, 9);
    QTest::newRow("add multiple @ end") << "transitions.qml" << 10 << 10 << 3 << ListRange();
    QTest::newRow("padding, add multiple @ start") << "transitions-padding.qml" << 10 << 0 << 3 << ListRange(0, 9);
    QTest::newRow("padding, add multiple @ middle") << "transitions-padding.qml" << 10 << 5 << 3 << ListRange(5, 9);
    QTest::newRow("padding, add multiple @ end") << "transitions-padding.qml" << 10 << 10 << 3 << ListRange();
}

void tst_qquickpositioners::moveTransitions(const QString &positionerObjectName)
{
    QFETCH(QString, qmlFile);
    QFETCH(int, initialItemCount);
    QFETCH(ListChange, change);
    QFETCH(ListRange, expectedDisplacedIndexes);

    QPointF targetItems_transitionFrom(-50, -50);
    QPointF displacedItems_transitionVia(100, 100);

    QaimModel model;
    for (int i = 0; i < initialItemCount; i++)
        model.addItem("Item" + QString::number(i), "");
    QaimModel model_targetItems_transitionFrom;
    QaimModel model_displacedItems_transitionVia;

    QScopedPointer<QQuickView> window(QQuickViewTestUtil::createView());
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("usePopulateTransition", QVariant(false));
    ctxt->setContextProperty("enableAddTransition", QVariant(false));
    ctxt->setContextProperty("dynamicallyPopulate", QVariant(false));
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("model_targetItems_transitionFrom", &model_targetItems_transitionFrom);
    ctxt->setContextProperty("model_displacedItems_transitionVia", &model_displacedItems_transitionVia);
    ctxt->setContextProperty("targetItems_transitionFrom", targetItems_transitionFrom);
    ctxt->setContextProperty("displacedItems_transitionVia", displacedItems_transitionVia);
    ctxt->setContextProperty("testedPositioner", QString());
    window->setSource(testFileUrl(qmlFile));
    window->show();
    QTest::qWaitForWindowExposed(window.data());
    qApp->processEvents();

    QList<QPair<QString,QString> > expectedDisplacedValues = expectedDisplacedIndexes.getModelDataValues(model);

    QQuickItem *positioner = window->rootObject()->findChild<QQuickItem*>(positionerObjectName);
    QVERIFY(positioner);
    positioner->findChild<QQuickItem*>("repeater")->setProperty("model", QVariant::fromValue(&model));
    QTRY_COMPARE(QQuickItemPrivate::get(positioner)->polishScheduled, false);

    switch (change.type) {
        case ListChange::Removed:
            model.removeItems(change.index, change.count);
            QTRY_COMPARE(model.count(), positioner->property("count").toInt());
            break;
        case ListChange::Moved:
            model.moveItems(change.index, change.to, change.count);
            QTRY_COMPARE(QQuickItemPrivate::get(positioner)->polishScheduled, false);
            break;
        case ListChange::Inserted:
        case ListChange::SetCurrent:
        case ListChange::SetContentY:
            QVERIFY(false);
            break;
         case ListChange::Polish:
            break;
    }

    QTRY_COMPARE(window->rootObject()->property("displaceTransitionsDone").toInt(), expectedDisplacedIndexes.count());
    QCOMPARE(window->rootObject()->property("addTransitionsDone").toInt(), 0);

    // check the target and displaced items were animated
    QCOMPARE(model_targetItems_transitionFrom.count(), 0);
    model_displacedItems_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with displaced anim", "shouldn't have been animated with displaced anim");

    // check attached properties
    QCOMPARE(window->rootObject()->property("targetTrans_items").toMap().count(), 0);
    QCOMPARE(window->rootObject()->property("targetTrans_targetIndexes").toList().count(), 0);
    QCOMPARE(window->rootObject()->property("targetTrans_targetItems").toList().count(), 0);
    if (expectedDisplacedIndexes.isValid()) {
        // adjust expectedDisplacedIndexes to their final values after the move
        QList<int> displacedIndexes;
        if (change.type == ListChange::Inserted)
            displacedIndexes = adjustIndexesForAddDisplaced(expectedDisplacedIndexes.indexes, change.index, change.count);
        else if (change.type == ListChange::Moved)
            displacedIndexes = adjustIndexesForMove(expectedDisplacedIndexes.indexes, change.index, change.to, change.count);
        else if (change.type == ListChange::Removed)
            displacedIndexes = adjustIndexesForRemoveDisplaced(expectedDisplacedIndexes.indexes, change.index, change.count);
        else
            QVERIFY(false);
        matchItemsAndIndexes(window->rootObject()->property("displacedTrans_items").toMap(), model, displacedIndexes);

        QVariantList listOfEmptyIntLists;
        for (int i=0; i<displacedIndexes.count(); i++)
            listOfEmptyIntLists << QVariant::fromValue(QList<int>());
        QCOMPARE(window->rootObject()->property("displacedTrans_targetIndexes").toList(), listOfEmptyIntLists);
        QVariantList listOfEmptyObjectLists;
        for (int i=0; i<displacedIndexes.count(); i++)
            listOfEmptyObjectLists.insert(listOfEmptyObjectLists.count(), QVariantList());
        QCOMPARE(window->rootObject()->property("displacedTrans_targetItems").toList(), listOfEmptyObjectLists);
    }

    checkItemPositions(positioner, &model, window->rootObject()->property("incrementalSize").toInt());
}

void tst_qquickpositioners::moveTransitions_data()
{
    // If this data changes, update moveTransitions_grid_data() also

    QTest::addColumn<QString>("qmlFile");
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<ListChange>("change");
    QTest::addColumn<ListRange>("expectedDisplacedIndexes");

    QTest::newRow("remove one @ start") << "transitions.qml" << 10 << ListChange::remove(0, 1) << ListRange(1, 9);
    QTest::newRow("remove one @ middle") << "transitions.qml" << 10 << ListChange::remove(4, 1) << ListRange(5, 9);
    QTest::newRow("remove one @ end") << "transitions.qml" << 10 << ListChange::remove(9, 1) << ListRange();
    QTest::newRow("padding, remove one @ start") << "transitions-padding.qml" << 10 << ListChange::remove(0, 1) << ListRange(1, 9);
    QTest::newRow("padding, remove one @ middle") << "transitions-padding.qml" << 10 << ListChange::remove(4, 1) << ListRange(5, 9);
    QTest::newRow("padding, remove one @ end") << "transitions-padding.qml" << 10 << ListChange::remove(9, 1) << ListRange();

    QTest::newRow("remove multiple @ start") << "transitions.qml" << 10 << ListChange::remove(0, 3) << ListRange(3, 9);
    QTest::newRow("remove multiple @ middle") << "transitions.qml" << 10 << ListChange::remove(4, 3) << ListRange(7, 9);
    QTest::newRow("remove multiple @ end") << "transitions.qml" << 10 << ListChange::remove(7, 3) << ListRange();
    QTest::newRow("padding, remove multiple @ start") << "transitions-padding.qml" << 10 << ListChange::remove(0, 3) << ListRange(3, 9);
    QTest::newRow("padding, remove multiple @ middle") << "transitions-padding.qml" << 10 << ListChange::remove(4, 3) << ListRange(7, 9);
    QTest::newRow("padding, remove multiple @ end") << "transitions-padding.qml" << 10 << ListChange::remove(7, 3) << ListRange();
}

void tst_qquickpositioners::checkItemPositions(QQuickItem *positioner, QaimModel *model, qreal incrementalSize)
{
    QVERIFY(model->count() > 0);

    QQuickBasePositioner *p = qobject_cast<QQuickBasePositioner*>(positioner);

    qreal padding = 0;
    qreal currentSize = 30;
    qreal rowX = p->leftPadding();
    qreal rowY = p->topPadding();

    for (int i=0; i<model->count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(positioner, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));

        QCOMPARE(item->width(), currentSize);
        QCOMPARE(item->height(), currentSize);

        if (qobject_cast<QQuickRow*>(positioner)) {
            QCOMPARE(item->x(), (i * 30.0) + padding + p->leftPadding());
            QCOMPARE(item->y(), p->topPadding());
        } else if (qobject_cast<QQuickColumn*>(positioner)) {
            QCOMPARE(item->x(), p->leftPadding());
            QCOMPARE(item->y(), (i * 30.0) + padding + p->topPadding());
        } else if (qobject_cast<QQuickGrid*>(positioner)) {
            int columns = 4;
            int rows = qCeil(model->count() / qreal(columns));
            int lastMatchingRowIndex = (rows * columns) - (columns - i%columns);
            if (lastMatchingRowIndex >= model->count())
                lastMatchingRowIndex -= columns;
            if (i % columns > 0) {
                QQuickItem *finalAlignedRowItem = findItem<QQuickItem>(positioner, "wrapper", lastMatchingRowIndex);
                QVERIFY(finalAlignedRowItem);
                QCOMPARE(item->x(), finalAlignedRowItem->x());
            } else {
                QCOMPARE(item->x(), p->leftPadding());
            }
            if (i / columns > 0) {
                QQuickItem *prevRowLastItem = findItem<QQuickItem>(positioner, "wrapper", (i/columns * columns) - 1);
                QVERIFY(prevRowLastItem);
                QCOMPARE(item->y(), prevRowLastItem->y() + prevRowLastItem->height());
            } else {
                QCOMPARE(item->y(), p->topPadding());
            }
        } else if (qobject_cast<QQuickFlow*>(positioner)) {
            if (rowX + item->width() > positioner->width()) {
                QQuickItem *prevItem = findItem<QQuickItem>(positioner, "wrapper", i-1);
                QVERIFY(prevItem);
                rowX = p->leftPadding();
                rowY = prevItem->y() + prevItem->height();
            }
            QCOMPARE(item->x(), rowX);
            QCOMPARE(item->y(), rowY);
            rowX += item->width();
        } else {
            QVERIFY2(false, "Unknown positioner type");
        }
        QQuickText *name = findItem<QQuickText>(positioner, "name", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model->name(i));

        padding += i * incrementalSize;
        currentSize += incrementalSize;
    }
}

void tst_qquickpositioners::test_vertical()
{
    QScopedPointer<QQuickView> window(createView(testFile("vertical.qml")));

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 0.0);
    QCOMPARE(two->y(), 50.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 60.0);

    QQuickItem *column = window->rootObject()->findChild<QQuickItem*>("column");
    QVERIFY(column);
    QCOMPARE(column->height(), 80.0);
    QCOMPARE(column->width(), 50.0);

    // test padding
    column->setProperty("padding", 1);
    column->setProperty("topPadding", 2);
    column->setProperty("leftPadding", 3);
    column->setProperty("rightPadding", 4);
    column->setProperty("bottomPadding", 5);

    QTRY_COMPARE(column->height(), 87.0);
    QCOMPARE(column->width(), 57.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 3.0);
    QCOMPARE(two->y(), 52.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 62.0);
}

void tst_qquickpositioners::test_vertical_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("vertical.qml")));

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 0.0);
    QCOMPARE(two->y(), 50.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 60.0);

    QQuickItem *column = window->rootObject()->findChild<QQuickItem*>("column");
    QVERIFY(column);
    QCOMPARE(column->height(), 80.0);
    QCOMPARE(column->width(), 50.0);

    QQuickColumn *obj = qobject_cast<QQuickColumn*>(column);
    QVERIFY(obj != 0);

    QCOMPARE(column->property("padding").toDouble(), 0.0);
    QCOMPARE(column->property("topPadding").toDouble(), 0.0);
    QCOMPARE(column->property("leftPadding").toDouble(), 0.0);
    QCOMPARE(column->property("rightPadding").toDouble(), 0.0);
    QCOMPARE(column->property("bottomPadding").toDouble(), 0.0);

    obj->setPadding(1.0);

    QCOMPARE(column->property("padding").toDouble(), 1.0);
    QCOMPARE(column->property("topPadding").toDouble(), 1.0);
    QCOMPARE(column->property("leftPadding").toDouble(), 1.0);
    QCOMPARE(column->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(column->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(column->height(), 82.0);
    QCOMPARE(column->width(), 52.0);

    QCOMPARE(one->x(), 1.0);
    QCOMPARE(one->y(), 1.0);
    QCOMPARE(two->x(), 1.0);
    QCOMPARE(two->y(), 51.0);
    QCOMPARE(three->x(), 1.0);
    QCOMPARE(three->y(), 61.0);

    obj->setTopPadding(2.0);

    QCOMPARE(column->property("padding").toDouble(), 1.0);
    QCOMPARE(column->property("topPadding").toDouble(), 2.0);
    QCOMPARE(column->property("leftPadding").toDouble(), 1.0);
    QCOMPARE(column->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(column->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(column->height(), 83.0);
    QCOMPARE(column->width(), 52.0);

    QCOMPARE(one->x(), 1.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 1.0);
    QCOMPARE(two->y(), 52.0);
    QCOMPARE(three->x(), 1.0);
    QCOMPARE(three->y(), 62.0);

    obj->setLeftPadding(3.0);

    QCOMPARE(column->property("padding").toDouble(), 1.0);
    QCOMPARE(column->property("topPadding").toDouble(), 2.0);
    QCOMPARE(column->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(column->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(column->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(column->width(), 54.0);
    QCOMPARE(column->height(), 83.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 3.0);
    QCOMPARE(two->y(), 52.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 62.0);

    obj->setRightPadding(4.0);

    QCOMPARE(column->property("padding").toDouble(), 1.0);
    QCOMPARE(column->property("topPadding").toDouble(), 2.0);
    QCOMPARE(column->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(column->property("rightPadding").toDouble(), 4.0);
    QCOMPARE(column->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(column->width(), 57.0);
    QCOMPARE(column->height(), 83.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 3.0);
    QCOMPARE(two->y(), 52.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 62.0);

    obj->setBottomPadding(5.0);

    QCOMPARE(column->property("padding").toDouble(), 1.0);
    QCOMPARE(column->property("topPadding").toDouble(), 2.0);
    QCOMPARE(column->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(column->property("rightPadding").toDouble(), 4.0);
    QCOMPARE(column->property("bottomPadding").toDouble(), 5.0);

    QTRY_COMPARE(column->height(), 87.0);
    QCOMPARE(column->width(), 57.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 3.0);
    QCOMPARE(two->y(), 52.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 62.0);

    obj->resetBottomPadding();
    QCOMPARE(column->property("bottomPadding").toDouble(), 1.0);
    QTRY_COMPARE(column->height(), 83.0);
    QCOMPARE(column->width(), 57.0);

    obj->resetRightPadding();
    QCOMPARE(column->property("rightPadding").toDouble(), 1.0);
    QTRY_COMPARE(column->width(), 54.0);
    QCOMPARE(column->height(), 83.0);

    obj->resetLeftPadding();
    QCOMPARE(column->property("leftPadding").toDouble(), 1.0);
    QTRY_COMPARE(column->width(), 52.0);
    QCOMPARE(column->height(), 83.0);

    obj->resetTopPadding();
    QCOMPARE(column->property("topPadding").toDouble(), 1.0);
    QTRY_COMPARE(column->height(), 82.0);
    QCOMPARE(column->width(), 52.0);

    obj->resetPadding();
    QCOMPARE(column->property("padding").toDouble(), 0.0);
    QCOMPARE(column->property("topPadding").toDouble(), 0.0);
    QCOMPARE(column->property("leftPadding").toDouble(), 0.0);
    QCOMPARE(column->property("rightPadding").toDouble(), 0.0);
    QCOMPARE(column->property("bottomPadding").toDouble(), 0.0);
    QTRY_COMPARE(column->height(), 80.0);
    QCOMPARE(column->width(), 50.0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 0.0);
    QCOMPARE(two->y(), 50.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 60.0);
}

void tst_qquickpositioners::test_vertical_spacing()
{
    QScopedPointer<QQuickView> window(createView(testFile("vertical-spacing.qml")));

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 0.0);
    QCOMPARE(two->y(), 60.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 80.0);

    QQuickItem *column = window->rootObject()->findChild<QQuickItem*>("column");
    QCOMPARE(column->height(), 100.0);
    QCOMPARE(column->width(), 50.0);

    // test padding
    column->setProperty("padding", 1);
    column->setProperty("topPadding", 2);
    column->setProperty("leftPadding", 3);
    column->setProperty("rightPadding", 4);
    column->setProperty("bottomPadding", 5);

    QTRY_COMPARE(column->height(), 107.0);
    QCOMPARE(column->width(), 57.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 3.0);
    QCOMPARE(two->y(), 62.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 82.0);
}

void tst_qquickpositioners::test_vertical_animated()
{
    QScopedPointer<QQuickView> window(createView(testFile("vertical-animated.qml"), false));

    //Note that they animate in
    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QCOMPARE(one->y(), -100.0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QCOMPARE(two->y(), -100.0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QCOMPARE(three->y(), -100.0);

    QVERIFY(QTest::qWaitForWindowExposed(window.data())); //It may not relayout until the next frame, so it needs to be drawn

    QQuickItem *column = window->rootObject()->findChild<QQuickItem*>("column");
    QVERIFY(column);
    QCOMPARE(column->height(), 100.0);
    QCOMPARE(column->width(), 50.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(one->x(), 0.0);
    QTRY_COMPARE(two->isVisible(), false);
    QTRY_COMPARE(two->y(), -100.0);//Not 'in' yet
    QTRY_COMPARE(two->x(), 0.0);
    QTRY_COMPARE(three->y(), 50.0);
    QTRY_COMPARE(three->x(), 0.0);

    //Add 'two'
    two->setVisible(true);
    QTRY_COMPARE(two->isVisible(), true);
    QTRY_COMPARE(column->height(), 150.0);
    QTRY_COMPARE(column->width(), 50.0);
    QTest::qWait(0);//Let the animation start
    QVERIFY(two->y() >= -100.0 && two->y() < 50.0);
    QVERIFY(three->y() >= 50.0 && three->y() < 100.0);

    QTRY_COMPARE(two->y(), 50.0);
    QTRY_COMPARE(three->y(), 100.0);

}

void tst_qquickpositioners::test_vertical_animated_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("vertical-animated.qml"), false));

    //Note that they animate in
    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QCOMPARE(one->y(), -100.0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QCOMPARE(two->y(), -100.0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QCOMPARE(three->y(), -100.0);

    QVERIFY(QTest::qWaitForWindowExposed(window.data())); //It may not relayout until the next frame, so it needs to be drawn

    QQuickItem *column = window->rootObject()->findChild<QQuickItem*>("column");
    QVERIFY(column);
    QCOMPARE(column->height(), 100.0);
    QCOMPARE(column->width(), 50.0);

    // test padding
    column->setProperty("padding", 1);
    column->setProperty("topPadding", 2);
    column->setProperty("leftPadding", 3);
    column->setProperty("rightPadding", 4);
    column->setProperty("bottomPadding", 5);

    QTRY_COMPARE(column->height(), 107.0);
    QCOMPARE(column->width(), 57.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->y(), 2.0);
    QTRY_COMPARE(one->x(), 3.0);
    QTRY_COMPARE(two->isVisible(), false);
    QTRY_COMPARE(two->y(), -100.0);//Not 'in' yet
    QTRY_COMPARE(two->x(), 0.0);
    QTRY_COMPARE(three->y(), 52.0);
    QTRY_COMPARE(three->x(), 3.0);

    //Add 'two'
    two->setVisible(true);
    QTRY_COMPARE(two->isVisible(), true);
    QTRY_COMPARE(column->height(), 157.0);
    QTRY_COMPARE(column->width(), 57.0);
    QTest::qWait(0);//Let the animation start
    QVERIFY(two->y() >= -100.0 && two->y() < 52.0);
    QVERIFY(three->y() >= 52.0 && three->y() < 102.0);

    QTRY_COMPARE(two->x(), 3.0);
    QTRY_COMPARE(two->y(), 52.0);
    QTRY_COMPARE(three->y(), 102.0);

}

void tst_qquickpositioners::test_grid()
{
    QScopedPointer<QQuickView> window(createView(testFile("gridtest.qml")));

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
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

    QQuickGrid *grid = window->rootObject()->findChild<QQuickGrid*>("grid");
    QCOMPARE(grid->flow(), QQuickGrid::LeftToRight);
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 100.0);

    // test padding
    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(grid->width(), 107.0);
    QCOMPARE(grid->height(), 107.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 53.0);
    QCOMPARE(five->y(), 52.0);
}

void tst_qquickpositioners::test_grid_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("gridtest.qml")));

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
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

    QQuickGrid *grid = window->rootObject()->findChild<QQuickGrid*>("grid");
    QCOMPARE(grid->flow(), QQuickGrid::LeftToRight);
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 100.0);

    QCOMPARE(grid->property("padding").toDouble(), 0.0);
    QCOMPARE(grid->property("topPadding").toDouble(), 0.0);
    QCOMPARE(grid->property("leftPadding").toDouble(), 0.0);
    QCOMPARE(grid->property("rightPadding").toDouble(), 0.0);
    QCOMPARE(grid->property("bottomPadding").toDouble(), 0.0);

    grid->setPadding(1.0);

    QCOMPARE(grid->property("padding").toDouble(), 1.0);
    QCOMPARE(grid->property("topPadding").toDouble(), 1.0);
    QCOMPARE(grid->property("leftPadding").toDouble(), 1.0);
    QCOMPARE(grid->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(grid->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(grid->width(), 102.0);
    QCOMPARE(grid->height(), 102.0);

    QCOMPARE(one->x(), 1.0);
    QCOMPARE(one->y(), 1.0);
    QCOMPARE(two->x(), 51.0);
    QCOMPARE(two->y(), 1.0);
    QCOMPARE(three->x(), 71.0);
    QCOMPARE(three->y(), 1.0);
    QCOMPARE(four->x(), 1.0);
    QCOMPARE(four->y(), 51.0);
    QCOMPARE(five->x(), 51.0);
    QCOMPARE(five->y(), 51.0);

    grid->setTopPadding(2.0);

    QCOMPARE(grid->property("padding").toDouble(), 1.0);
    QCOMPARE(grid->property("topPadding").toDouble(), 2.0);
    QCOMPARE(grid->property("leftPadding").toDouble(), 1.0);
    QCOMPARE(grid->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(grid->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(grid->height(), 103.0);
    QCOMPARE(grid->width(), 102.0);

    QCOMPARE(one->x(), 1.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 51.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 71.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 1.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 51.0);
    QCOMPARE(five->y(), 52.0);

    grid->setLeftPadding(3.0);

    QCOMPARE(grid->property("padding").toDouble(), 1.0);
    QCOMPARE(grid->property("topPadding").toDouble(), 2.0);
    QCOMPARE(grid->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(grid->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(grid->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(grid->width(), 104.0);
    QCOMPARE(grid->height(), 103.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 53.0);
    QCOMPARE(five->y(), 52.0);

    grid->setRightPadding(4.0);

    QCOMPARE(grid->property("padding").toDouble(), 1.0);
    QCOMPARE(grid->property("topPadding").toDouble(), 2.0);
    QCOMPARE(grid->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(grid->property("rightPadding").toDouble(), 4.0);
    QCOMPARE(grid->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(grid->width(), 107.0);
    QCOMPARE(grid->height(), 103.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 53.0);
    QCOMPARE(five->y(), 52.0);

    grid->setBottomPadding(5.0);

    QCOMPARE(grid->property("padding").toDouble(), 1.0);
    QCOMPARE(grid->property("topPadding").toDouble(), 2.0);
    QCOMPARE(grid->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(grid->property("rightPadding").toDouble(), 4.0);
    QCOMPARE(grid->property("bottomPadding").toDouble(), 5.0);

    QTRY_COMPARE(grid->height(), 107.0);
    QCOMPARE(grid->width(), 107.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 53.0);
    QCOMPARE(five->y(), 52.0);

    grid->resetBottomPadding();
    QCOMPARE(grid->property("bottomPadding").toDouble(), 1.0);
    QTRY_COMPARE(grid->height(), 103.0);
    QCOMPARE(grid->width(), 107.0);

    grid->resetRightPadding();
    QCOMPARE(grid->property("rightPadding").toDouble(), 1.0);
    QTRY_COMPARE(grid->width(), 104.0);
    QCOMPARE(grid->height(), 103.0);

    grid->resetLeftPadding();
    QCOMPARE(grid->property("leftPadding").toDouble(), 1.0);
    QTRY_COMPARE(grid->width(), 102.0);
    QCOMPARE(grid->height(), 103.0);

    grid->resetTopPadding();
    QCOMPARE(grid->property("topPadding").toDouble(), 1.0);
    QTRY_COMPARE(grid->height(), 102.0);
    QCOMPARE(grid->width(), 102.0);

    grid->resetPadding();
    QCOMPARE(grid->property("padding").toDouble(), 0.0);
    QCOMPARE(grid->property("topPadding").toDouble(), 0.0);
    QCOMPARE(grid->property("leftPadding").toDouble(), 0.0);
    QCOMPARE(grid->property("rightPadding").toDouble(), 0.0);
    QCOMPARE(grid->property("bottomPadding").toDouble(), 0.0);
    QTRY_COMPARE(grid->height(), 100.0);
    QCOMPARE(grid->width(), 100.0);

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
}

void tst_qquickpositioners::test_grid_topToBottom()
{
    QScopedPointer<QQuickView> window(createView(testFile("grid-toptobottom.qml")));

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
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

    QQuickGrid *grid = window->rootObject()->findChild<QQuickGrid*>("grid");
    QCOMPARE(grid->flow(), QQuickGrid::TopToBottom);
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 120.0);

    // test padding
    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(grid->width(), 107.0);
    QCOMPARE(grid->height(), 127.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 3.0);
    QCOMPARE(two->y(), 52.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 102.0);
    QCOMPARE(four->x(), 53.0);
    QCOMPARE(four->y(), 2.0);
    QCOMPARE(five->x(), 53.0);
    QCOMPARE(five->y(), 52.0);
}

void tst_qquickpositioners::test_grid_rightToLeft()
{
    QScopedPointer<QQuickView> window(createView(testFile("gridtest.qml")));

    window->rootObject()->setProperty("testRightToLeft", true);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
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

    QQuickGrid *grid = window->rootObject()->findChild<QQuickGrid*>("grid");
    QCOMPARE(grid->layoutDirection(), Qt::RightToLeft);
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 100.0);

    // test padding
    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(grid->width(), 107.0);
    QCOMPARE(grid->height(), 107.0);

    QCOMPARE(one->x(), 53.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 33.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 53.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 43.0);
    QCOMPARE(five->y(), 52.0);

    grid->setProperty("topPadding", 0);
    grid->setProperty("leftPadding", 0);
    grid->setProperty("rightPadding", 0);
    grid->setProperty("bottomPadding", 0);
    grid->setProperty("padding", 0);

    QTRY_COMPARE(one->x(), 50.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 30.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 50.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 40.0);
    QCOMPARE(five->y(), 50.0);

    // Change the width of the grid and check that items stay to the right
    grid->setWidth(200);
    QTRY_COMPARE(one->x(), 150.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 130.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 100.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 150.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 140.0);
    QCOMPARE(five->y(), 50.0);

    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(one->x(), 146.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 126.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 96.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 146.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 136.0);
    QCOMPARE(five->y(), 52.0);
}

void tst_qquickpositioners::test_grid_spacing()
{
    QScopedPointer<QQuickView> window(createView(testFile("grid-spacing.qml")));

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
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

    QQuickItem *grid = window->rootObject()->findChild<QQuickItem*>("grid");
    QCOMPARE(grid->width(), 128.0);
    QCOMPARE(grid->height(), 104.0);

    // test padding
    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(grid->width(), 135.0);
    QCOMPARE(grid->height(), 111.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 57.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 81.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 56.0);
    QCOMPARE(five->x(), 57.0);
    QCOMPARE(five->y(), 56.0);
}

void tst_qquickpositioners::test_grid_row_column_spacing()
{
    QScopedPointer<QQuickView> window(createView(testFile("grid-row-column-spacing.qml")));

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 61.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 92.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 57.0);
    QCOMPARE(five->x(), 61.0);
    QCOMPARE(five->y(), 57.0);

    QQuickItem *grid = window->rootObject()->findChild<QQuickItem*>("grid");
    QCOMPARE(grid->width(), 142.0);
    QCOMPARE(grid->height(), 107.0);

    // test padding
    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(grid->width(), 149.0);
    QCOMPARE(grid->height(), 114.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 64.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 95.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 59.0);
    QCOMPARE(five->x(), 64.0);
    QCOMPARE(five->y(), 59.0);
}

void tst_qquickpositioners::test_grid_animated()
{
    QScopedPointer<QQuickView> window(createView(testFile("grid-animated.qml"), false));

    window->rootObject()->setProperty("testRightToLeft", false);

    //Note that all animate in
    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(one->y(), -100.0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(two->y(), -100.0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QCOMPARE(three->x(), -100.0);
    QCOMPARE(three->y(), -100.0);

    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QCOMPARE(four->x(), -100.0);
    QCOMPARE(four->y(), -100.0);

    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);
    QCOMPARE(five->x(), -100.0);
    QCOMPARE(five->y(), -100.0);

    QVERIFY(QTest::qWaitForWindowExposed(window.data())); //It may not relayout until the next frame, so it needs to be drawn

    QQuickItem *grid = window->rootObject()->findChild<QQuickItem*>("grid");
    QVERIFY(grid);
    QCOMPARE(grid->width(), 150.0);
    QCOMPARE(grid->height(), 100.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(one->x(), 0.0);
    QTRY_COMPARE(two->isVisible(), false);
    QTRY_COMPARE(two->y(), -100.0);
    QTRY_COMPARE(two->x(), -100.0);
    QTRY_COMPARE(three->y(), 0.0);
    QTRY_COMPARE(three->x(), 50.0);
    QTRY_COMPARE(four->y(), 0.0);
    QTRY_COMPARE(four->x(), 100.0);
    QTRY_COMPARE(five->y(), 50.0);
    QTRY_COMPARE(five->x(), 0.0);

    //Add 'two'
    two->setVisible(true);
    QCOMPARE(two->isVisible(), true);
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

}

void tst_qquickpositioners::test_grid_animated_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("grid-animated.qml"), false));

    window->rootObject()->setProperty("testRightToLeft", false);

    //Note that all animate in
    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(one->y(), -100.0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(two->y(), -100.0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QCOMPARE(three->x(), -100.0);
    QCOMPARE(three->y(), -100.0);

    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QCOMPARE(four->x(), -100.0);
    QCOMPARE(four->y(), -100.0);

    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);
    QCOMPARE(five->x(), -100.0);
    QCOMPARE(five->y(), -100.0);

    QVERIFY(QTest::qWaitForWindowExposed(window.data())); //It may not relayout until the next frame, so it needs to be drawn

    QQuickItem *grid = window->rootObject()->findChild<QQuickItem*>("grid");
    QVERIFY(grid);
    QCOMPARE(grid->width(), 150.0);
    QCOMPARE(grid->height(), 100.0);

    // test padding
    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(grid->width(), 157.0);
    QCOMPARE(grid->height(), 107.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->y(), 2.0);
    QTRY_COMPARE(one->x(), 3.0);
    QTRY_COMPARE(two->isVisible(), false);
    QTRY_COMPARE(two->y(), -100.0);
    QTRY_COMPARE(two->x(), -100.0);
    QTRY_COMPARE(three->y(), 2.0);
    QTRY_COMPARE(three->x(), 53.0);
    QTRY_COMPARE(four->y(), 2.0);
    QTRY_COMPARE(four->x(), 103.0);
    QTRY_COMPARE(five->y(), 52.0);
    QTRY_COMPARE(five->x(), 3.0);

    //Add 'two'
    two->setVisible(true);
    QCOMPARE(two->isVisible(), true);
    QCOMPARE(grid->width(), 157.0);
    QCOMPARE(grid->height(), 107.0);
    QTest::qWait(0);//Let the animation start
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(two->y(), -100.0);
    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(three->x(), 53.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 103.0);
    QCOMPARE(four->y(), 2.0);
    QCOMPARE(five->x(), 3.0);
    QCOMPARE(five->y(), 52.0);
    //Let the animation complete
    QTRY_COMPARE(two->x(), 53.0);
    QTRY_COMPARE(two->y(), 2.0);
    QTRY_COMPARE(one->x(), 3.0);
    QTRY_COMPARE(one->y(), 2.0);
    QTRY_COMPARE(three->x(), 103.0);
    QTRY_COMPARE(three->y(), 2.0);
    QTRY_COMPARE(four->x(), 3.0);
    QTRY_COMPARE(four->y(), 52.0);
    QTRY_COMPARE(five->x(), 53.0);
    QTRY_COMPARE(five->y(), 52.0);

}

void tst_qquickpositioners::test_grid_animated_rightToLeft()
{
    QScopedPointer<QQuickView> window(createView(testFile("grid-animated.qml"), false));

    window->rootObject()->setProperty("testRightToLeft", true);

    //Note that all animate in
    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(one->y(), -100.0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(two->y(), -100.0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QCOMPARE(three->x(), -100.0);
    QCOMPARE(three->y(), -100.0);

    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QCOMPARE(four->x(), -100.0);
    QCOMPARE(four->y(), -100.0);

    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);
    QCOMPARE(five->x(), -100.0);
    QCOMPARE(five->y(), -100.0);

    QVERIFY(QTest::qWaitForWindowExposed(window.data())); //It may not relayout until the next frame, so it needs to be drawn

    QQuickItem *grid = window->rootObject()->findChild<QQuickItem*>("grid");
    QVERIFY(grid);
    QCOMPARE(grid->width(), 150.0);
    QCOMPARE(grid->height(), 100.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(one->x(), 100.0);
    QTRY_COMPARE(two->isVisible(), false);
    QTRY_COMPARE(two->y(), -100.0);
    QTRY_COMPARE(two->x(), -100.0);
    QTRY_COMPARE(three->y(), 0.0);
    QTRY_COMPARE(three->x(), 50.0);
    QTRY_COMPARE(four->y(), 0.0);
    QTRY_COMPARE(four->x(), 0.0);
    QTRY_COMPARE(five->y(), 50.0);
    QTRY_COMPARE(five->x(), 100.0);

    //Add 'two'
    two->setVisible(true);
    QCOMPARE(two->isVisible(), true);
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

}

void tst_qquickpositioners::test_grid_animated_rightToLeft_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("grid-animated.qml"), false));

    window->rootObject()->setProperty("testRightToLeft", true);

    //Note that all animate in
    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QCOMPARE(one->x(), -100.0);
    QCOMPARE(one->y(), -100.0);

    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(two->y(), -100.0);

    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QCOMPARE(three->x(), -100.0);
    QCOMPARE(three->y(), -100.0);

    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QCOMPARE(four->x(), -100.0);
    QCOMPARE(four->y(), -100.0);

    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);
    QCOMPARE(five->x(), -100.0);
    QCOMPARE(five->y(), -100.0);

    QVERIFY(QTest::qWaitForWindowExposed(window.data())); //It may not relayout until the next frame, so it needs to be drawn

    QQuickItem *grid = window->rootObject()->findChild<QQuickItem*>("grid");
    QVERIFY(grid);
    QCOMPARE(grid->width(), 150.0);
    QCOMPARE(grid->height(), 100.0);

    // test padding
    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(grid->width(), 157.0);
    QCOMPARE(grid->height(), 107.0);

    //QTRY_COMPARE used instead of waiting for the expected time of animation completion
    //Note that this means the duration of the animation is NOT tested

    QTRY_COMPARE(one->y(), 2.0);
    QTRY_COMPARE(one->x(), 103.0);
    QTRY_COMPARE(two->isVisible(), false);
    QTRY_COMPARE(two->y(), -100.0);
    QTRY_COMPARE(two->x(), -100.0);
    QTRY_COMPARE(three->y(), 2.0);
    QTRY_COMPARE(three->x(), 53.0);
    QTRY_COMPARE(four->y(), 2.0);
    QTRY_COMPARE(four->x(), 3.0);
    QTRY_COMPARE(five->y(), 52.0);
    QTRY_COMPARE(five->x(), 103.0);

    //Add 'two'
    two->setVisible(true);
    QCOMPARE(two->isVisible(), true);
    QCOMPARE(grid->width(), 157.0);
    QCOMPARE(grid->height(), 107.0);
    QTest::qWait(0);//Let the animation start
    QCOMPARE(two->x(), -100.0);
    QCOMPARE(two->y(), -100.0);
    QCOMPARE(one->x(), 103.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(three->x(), 53.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 2.0);
    QCOMPARE(five->x(), 103.0);
    QCOMPARE(five->y(), 52.0);
    //Let the animation complete
    QTRY_COMPARE(two->x(), 53.0);
    QTRY_COMPARE(two->y(), 2.0);
    QTRY_COMPARE(one->x(), 103.0);
    QTRY_COMPARE(one->y(), 2.0);
    QTRY_COMPARE(three->x(), 3.0);
    QTRY_COMPARE(three->y(), 2.0);
    QTRY_COMPARE(four->x(), 103.0);
    QTRY_COMPARE(four->y(), 52.0);
    QTRY_COMPARE(five->x(), 53.0);
    QTRY_COMPARE(five->y(), 52.0);

}

void tst_qquickpositioners::test_grid_zero_columns()
{
    QScopedPointer<QQuickView> window(createView(testFile("gridzerocolumns.qml")));

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
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

    QQuickItem *grid = window->rootObject()->findChild<QQuickItem*>("grid");
    QCOMPARE(grid->width(), 170.0);
    QCOMPARE(grid->height(), 60.0);

    // test padding
    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(grid->width(), 177.0);
    QCOMPARE(grid->height(), 67.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 123.0);
    QCOMPARE(four->y(), 2.0);
    QCOMPARE(five->x(), 3.0);
    QCOMPARE(five->y(), 52.0);
}

void tst_qquickpositioners::test_grid_H_alignment()
{
    QScopedPointer<QQuickView> window(createView(testFile("gridtest.qml")));

    window->rootObject()->setProperty("testHAlignment", QQuickGrid::AlignHCenter);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 55.0);
    QCOMPARE(five->y(), 50.0);

    QQuickItem *grid = window->rootObject()->findChild<QQuickItem*>("grid");
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 100.0);

    window->rootObject()->setProperty("testHAlignment", QQuickGrid::AlignRight);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 60.0);
    QCOMPARE(five->y(), 50.0);
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 100.0);

    window->rootObject()->setProperty("testRightToLeft", true);

    QCOMPARE(one->x(), 50.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 30.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 50.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 30.0);
    QCOMPARE(five->y(), 50.0);
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 100.0);

    window->rootObject()->setProperty("testHAlignment", QQuickGrid::AlignHCenter);

    QCOMPARE(one->x(), 50.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 30.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 0.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 50.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 35.0);
    QCOMPARE(five->y(), 50.0);
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 100.0);

}

void tst_qquickpositioners::test_grid_H_alignment_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("gridtest.qml")));

    window->rootObject()->setProperty("testHAlignment", QQuickGrid::AlignHCenter);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 0.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 55.0);
    QCOMPARE(five->y(), 50.0);

    QQuickItem *grid = window->rootObject()->findChild<QQuickItem*>("grid");
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 100.0);

    // test padding
    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(grid->width(), 107.0);
    QCOMPARE(grid->height(), 107.0);

    window->rootObject()->setProperty("testHAlignment", QQuickGrid::AlignRight);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 63.0);
    QCOMPARE(five->y(), 52.0);
    QCOMPARE(grid->width(), 107.0);
    QCOMPARE(grid->height(), 107.0);

    window->rootObject()->setProperty("testRightToLeft", true);

    QCOMPARE(one->x(), 53.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 33.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 53.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 33.0);
    QCOMPARE(five->y(), 52.0);
    QCOMPARE(grid->width(), 107.0);
    QCOMPARE(grid->height(), 107.0);

    window->rootObject()->setProperty("testHAlignment", QQuickGrid::AlignHCenter);

    QCOMPARE(one->x(), 53.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 33.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 2.0);
    QCOMPARE(four->x(), 53.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 38.0);
    QCOMPARE(five->y(), 52.0);
    QCOMPARE(grid->width(), 107.0);
    QCOMPARE(grid->height(), 107.0);

}

void tst_qquickpositioners::test_grid_V_alignment()
{
    QScopedPointer<QQuickView> window(createView(testFile("gridtest.qml")));

    window->rootObject()->setProperty("testVAlignment", QQuickGrid::AlignVCenter);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 15.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 50.0);
    QCOMPARE(five->y(), 70.0);

    window->rootObject()->setProperty("testVAlignment", QQuickGrid::AlignBottom);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 70.0);
    QCOMPARE(three->y(), 30.0);
    QCOMPARE(four->x(), 0.0);
    QCOMPARE(four->y(), 50.0);
    QCOMPARE(five->x(), 50.0);
    QCOMPARE(five->y(), 90.0);

}

void tst_qquickpositioners::test_grid_V_alignment_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("gridtest.qml")));

    window->rootObject()->setProperty("testVAlignment", QQuickGrid::AlignVCenter);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);

    QQuickItem *grid = window->rootObject()->findChild<QQuickItem*>("grid");
    QCOMPARE(grid->width(), 100.0);
    QCOMPARE(grid->height(), 100.0);

    // test padding
    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(grid->width(), 107.0);
    QCOMPARE(grid->height(), 107.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 17.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 53.0);
    QCOMPARE(five->y(), 72.0);

    window->rootObject()->setProperty("testVAlignment", QQuickGrid::AlignBottom);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 73.0);
    QCOMPARE(three->y(), 32.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 53.0);
    QCOMPARE(five->y(), 92.0);

}

void tst_qquickpositioners::test_propertychanges()
{
    QScopedPointer<QQuickView> window(createView(testFile("propertychangestest.qml")));

    QQuickGrid *grid = qobject_cast<QQuickGrid*>(window->rootObject());
    QVERIFY(grid != 0);
    QQuickTransition *rowTransition = window->rootObject()->findChild<QQuickTransition*>("rowTransition");
    QQuickTransition *columnTransition = window->rootObject()->findChild<QQuickTransition*>("columnTransition");

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

}

void tst_qquickpositioners::test_repeater()
{
    QScopedPointer<QQuickView> window(createView(testFile("repeatertest.qml")));

    QQuickRectangle *one = findItem<QQuickRectangle>(window->contentItem(), "one");
    QVERIFY(one != 0);

    QQuickRectangle *two = findItem<QQuickRectangle>(window->contentItem(), "two");
    QVERIFY(two != 0);

    QQuickRectangle *three = findItem<QQuickRectangle>(window->contentItem(), "three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 100.0);
    QCOMPARE(three->y(), 0.0);

}

void tst_qquickpositioners::test_repeater_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("repeatertest-padding.qml")));

    QQuickRectangle *one = findItem<QQuickRectangle>(window->contentItem(), "one");
    QVERIFY(one != 0);

    QQuickRectangle *two = findItem<QQuickRectangle>(window->contentItem(), "two");
    QVERIFY(two != 0);

    QQuickRectangle *three = findItem<QQuickRectangle>(window->contentItem(), "three");
    QVERIFY(three != 0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 103.0);
    QCOMPARE(three->y(), 2.0);

}

void tst_qquickpositioners::test_flow()
{
    QScopedPointer<QQuickView> window(createView(testFile("flowtest.qml")));

    window->rootObject()->setProperty("testRightToLeft", false);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
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

    QQuickItem *flow = window->rootObject()->findChild<QQuickItem*>("flow");
    QVERIFY(flow);
    QCOMPARE(flow->width(), 90.0);
    QCOMPARE(flow->height(), 120.0);

    // test padding
    flow->setProperty("padding", 1);
    flow->setProperty("topPadding", 2);
    flow->setProperty("leftPadding", 3);
    flow->setProperty("rightPadding", 4);
    flow->setProperty("bottomPadding", 5);

    QTRY_COMPARE(flow->height(), 127.0);
    QCOMPARE(flow->width(), 90.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 52.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 72.0);
    QCOMPARE(five->x(), 53.0);
    QCOMPARE(five->y(), 72.0);
}

void tst_qquickpositioners::test_flow_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("flowtest.qml")));

    window->rootObject()->setProperty("testRightToLeft", false);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
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

    QQuickItem *flow = window->rootObject()->findChild<QQuickItem*>("flow");
    QVERIFY(flow);
    QCOMPARE(flow->width(), 90.0);
    QCOMPARE(flow->height(), 120.0);

    QQuickFlow *obj = qobject_cast<QQuickFlow*>(flow);
    QVERIFY(obj != 0);

    QCOMPARE(flow->property("padding").toDouble(), 0.0);
    QCOMPARE(flow->property("topPadding").toDouble(), 0.0);
    QCOMPARE(flow->property("leftPadding").toDouble(), 0.0);
    QCOMPARE(flow->property("rightPadding").toDouble(), 0.0);
    QCOMPARE(flow->property("bottomPadding").toDouble(), 0.0);

    obj->setPadding(1.0);

    QCOMPARE(flow->property("padding").toDouble(), 1.0);
    QCOMPARE(flow->property("topPadding").toDouble(), 1.0);
    QCOMPARE(flow->property("leftPadding").toDouble(), 1.0);
    QCOMPARE(flow->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(flow->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(flow->height(), 122.0);
    QCOMPARE(flow->width(), 90.0);

    QCOMPARE(one->x(), 1.0);
    QCOMPARE(one->y(), 1.0);
    QCOMPARE(two->x(), 51.0);
    QCOMPARE(two->y(), 1.0);
    QCOMPARE(three->x(), 1.0);
    QCOMPARE(three->y(), 51.0);
    QCOMPARE(four->x(), 1.0);
    QCOMPARE(four->y(), 71.0);
    QCOMPARE(five->x(), 51.0);
    QCOMPARE(five->y(), 71.0);

    obj->setTopPadding(2.0);

    QCOMPARE(flow->property("padding").toDouble(), 1.0);
    QCOMPARE(flow->property("topPadding").toDouble(), 2.0);
    QCOMPARE(flow->property("leftPadding").toDouble(), 1.0);
    QCOMPARE(flow->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(flow->property("bottomPadding").toDouble(), 1.0);

    QTRY_COMPARE(flow->height(), 123.0);
    QCOMPARE(flow->width(), 90.0);

    QCOMPARE(one->x(), 1.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 51.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 1.0);
    QCOMPARE(three->y(), 52.0);
    QCOMPARE(four->x(), 1.0);
    QCOMPARE(four->y(), 72.0);
    QCOMPARE(five->x(), 51.0);
    QCOMPARE(five->y(), 72.0);

    obj->setLeftPadding(3.0);

    QCOMPARE(flow->property("padding").toDouble(), 1.0);
    QCOMPARE(flow->property("topPadding").toDouble(), 2.0);
    QCOMPARE(flow->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(flow->property("rightPadding").toDouble(), 1.0);
    QCOMPARE(flow->property("bottomPadding").toDouble(), 1.0);

    QCOMPARE(flow->height(), 123.0);
    QCOMPARE(flow->width(), 90.0);

    QTRY_COMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 52.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 72.0);
    QCOMPARE(five->x(), 53.0);
    QCOMPARE(five->y(), 72.0);

    obj->setRightPadding(4.0);

    QCOMPARE(flow->property("padding").toDouble(), 1.0);
    QCOMPARE(flow->property("topPadding").toDouble(), 2.0);
    QCOMPARE(flow->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(flow->property("rightPadding").toDouble(), 4.0);
    QCOMPARE(flow->property("bottomPadding").toDouble(), 1.0);

    QCOMPARE(flow->height(), 123.0);
    QCOMPARE(flow->width(), 90.0);

    QTRY_COMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 52.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 72.0);
    QCOMPARE(five->x(), 53.0);
    QCOMPARE(five->y(), 72.0);

    obj->setBottomPadding(5.0);

    QCOMPARE(flow->property("padding").toDouble(), 1.0);
    QCOMPARE(flow->property("topPadding").toDouble(), 2.0);
    QCOMPARE(flow->property("leftPadding").toDouble(), 3.0);
    QCOMPARE(flow->property("rightPadding").toDouble(), 4.0);
    QCOMPARE(flow->property("bottomPadding").toDouble(), 5.0);

    QTRY_COMPARE(flow->height(), 127.0);
    QCOMPARE(flow->width(), 90.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 3.0);
    QCOMPARE(three->y(), 52.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 72.0);
    QCOMPARE(five->x(), 53.0);
    QCOMPARE(five->y(), 72.0);

    obj->resetBottomPadding();
    QCOMPARE(flow->property("bottomPadding").toDouble(), 1.0);
    QTRY_COMPARE(flow->height(), 123.0);
    QCOMPARE(flow->width(), 90.0);

    obj->resetRightPadding();
    QCOMPARE(flow->property("rightPadding").toDouble(), 1.0);
    QTRY_COMPARE(flow->height(), 123.0);
    QCOMPARE(flow->width(), 90.0);

    obj->resetLeftPadding();
    QCOMPARE(flow->property("leftPadding").toDouble(), 1.0);
    QTRY_COMPARE(flow->height(), 123.0);
    QCOMPARE(flow->width(), 90.0);

    obj->resetTopPadding();
    QCOMPARE(flow->property("topPadding").toDouble(), 1.0);
    QTRY_COMPARE(flow->height(), 122.0);
    QCOMPARE(flow->width(), 90.0);

    obj->resetPadding();
    QCOMPARE(flow->property("padding").toDouble(), 0.0);
    QCOMPARE(flow->property("topPadding").toDouble(), 0.0);
    QCOMPARE(flow->property("leftPadding").toDouble(), 0.0);
    QCOMPARE(flow->property("rightPadding").toDouble(), 0.0);
    QCOMPARE(flow->property("bottomPadding").toDouble(), 0.0);
    QTRY_COMPARE(flow->height(), 120.0);
    QCOMPARE(flow->width(), 90.0);

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
}

void tst_qquickpositioners::test_flow_rightToLeft()
{
    QScopedPointer<QQuickView> window(createView(testFile("flowtest.qml")));

    window->rootObject()->setProperty("testRightToLeft", true);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
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

    QQuickItem *flow = window->rootObject()->findChild<QQuickItem*>("flow");
    QVERIFY(flow);
    QCOMPARE(flow->width(), 90.0);
    QCOMPARE(flow->height(), 120.0);

    // test padding
    flow->setProperty("padding", 1);
    flow->setProperty("topPadding", 2);
    flow->setProperty("leftPadding", 3);
    flow->setProperty("rightPadding", 4);
    flow->setProperty("bottomPadding", 5);

    QTRY_COMPARE(flow->height(), 127.0);
    QCOMPARE(flow->width(), 90.0);

    QCOMPARE(one->x(), 36.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 16.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 36.0);
    QCOMPARE(three->y(), 52.0);
    QCOMPARE(four->x(), 36.0);
    QCOMPARE(four->y(), 72.0);
    QCOMPARE(five->x(), 26.0);
    QCOMPARE(five->y(), 72.0);
}

void tst_qquickpositioners::test_flow_topToBottom()
{
    QScopedPointer<QQuickView> window(createView(testFile("flowtest-toptobottom.qml")));

    window->rootObject()->setProperty("testRightToLeft", false);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 50.0);
    QCOMPARE(three->y(), 50.0);
    QCOMPARE(four->x(), 100.0);
    QCOMPARE(four->y(), 0.0);
    QCOMPARE(five->x(), 100.0);
    QCOMPARE(five->y(), 50.0);

    QQuickItem *flow = window->rootObject()->findChild<QQuickItem*>("flow");
    QVERIFY(flow);
    QCOMPARE(flow->height(), 90.0);
    QCOMPARE(flow->width(), 150.0);

    window->rootObject()->setProperty("testRightToLeft", true);

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

}

void tst_qquickpositioners::test_flow_topToBottom_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("flowtest-toptobottom.qml")));

    window->rootObject()->setProperty("testRightToLeft", false);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 0.0);
    QCOMPARE(one->y(), 0.0);
    QCOMPARE(two->x(), 50.0);
    QCOMPARE(two->y(), 0.0);
    QCOMPARE(three->x(), 50.0);
    QCOMPARE(three->y(), 50.0);
    QCOMPARE(four->x(), 100.0);
    QCOMPARE(four->y(), 0.0);
    QCOMPARE(five->x(), 100.0);
    QCOMPARE(five->y(), 50.0);

    QQuickItem *flow = window->rootObject()->findChild<QQuickItem*>("flow");
    QVERIFY(flow);
    QCOMPARE(flow->height(), 90.0);
    QCOMPARE(flow->width(), 150.0);

    // test padding
    flow->setProperty("padding", 1);
    flow->setProperty("topPadding", 2);
    flow->setProperty("leftPadding", 3);
    flow->setProperty("rightPadding", 4);
    flow->setProperty("bottomPadding", 5);

    QTRY_COMPARE(flow->width(), 157.0);
    QCOMPARE(flow->height(), 90.0);

    QCOMPARE(one->x(), 3.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 53.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 53.0);
    QCOMPARE(three->y(), 52.0);
    QCOMPARE(four->x(), 103.0);
    QCOMPARE(four->y(), 2.0);
    QCOMPARE(five->x(), 103.0);
    QCOMPARE(five->y(), 52.0);

    window->rootObject()->setProperty("testRightToLeft", true);

    QVERIFY(flow);
    QTRY_COMPARE(flow->width(), 157.0);
    QCOMPARE(flow->height(), 90.0);

    QCOMPARE(one->x(), 103.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 83.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 53.0);
    QCOMPARE(three->y(), 52.0);
    QCOMPARE(four->x(), 3.0);
    QCOMPARE(four->y(), 2.0);
    QCOMPARE(five->x(), 43.0);
    QCOMPARE(five->y(), 52.0);

}

void tst_qquickpositioners::test_flow_resize()
{
    QScopedPointer<QQuickView> window(createView(testFile("flowtest.qml")));

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root);
    root->setWidth(125);
    root->setProperty("testRightToLeft", false);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);

    QTRY_COMPARE(one->x(), 0.0);
    QTRY_COMPARE(one->y(), 0.0);
    QTRY_COMPARE(two->x(), 50.0);
    QTRY_COMPARE(two->y(), 0.0);
    QTRY_COMPARE(three->x(), 70.0);
    QTRY_COMPARE(three->y(), 0.0);
    QTRY_COMPARE(four->x(), 0.0);
    QTRY_COMPARE(four->y(), 50.0);
    QTRY_COMPARE(five->x(), 50.0);
    QTRY_COMPARE(five->y(), 50.0);

}

void tst_qquickpositioners::test_flow_resize_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("flowtest-padding.qml")));

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root);
    root->setWidth(125);
    root->setProperty("testRightToLeft", false);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QVERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);

    QTRY_COMPARE(one->x(), 3.0);
    QTRY_COMPARE(one->y(), 2.0);
    QTRY_COMPARE(two->x(), 53.0);
    QTRY_COMPARE(two->y(), 2.0);
    QTRY_COMPARE(three->x(), 3.0);
    QTRY_COMPARE(three->y(), 52.0);
    QTRY_COMPARE(four->x(), 53.0);
    QTRY_COMPARE(four->y(), 52.0);
    QTRY_COMPARE(five->x(), 103.0);
    QTRY_COMPARE(five->y(), 52.0);

}

void tst_qquickpositioners::test_flow_resize_rightToLeft()
{
    QScopedPointer<QQuickView> window(createView(testFile("flowtest.qml")));

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root);
    root->setWidth(125);
    root->setProperty("testRightToLeft", true);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QTRY_VERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
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

}

void tst_qquickpositioners::test_flow_resize_rightToLeft_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("flowtest-padding.qml")));

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root);
    root->setWidth(125);
    root->setProperty("testRightToLeft", true);

    QQuickRectangle *one = window->rootObject()->findChild<QQuickRectangle*>("one");
    QTRY_VERIFY(one != 0);
    QQuickRectangle *two = window->rootObject()->findChild<QQuickRectangle*>("two");
    QVERIFY(two != 0);
    QQuickRectangle *three = window->rootObject()->findChild<QQuickRectangle*>("three");
    QVERIFY(three != 0);
    QQuickRectangle *four = window->rootObject()->findChild<QQuickRectangle*>("four");
    QVERIFY(four != 0);
    QQuickRectangle *five = window->rootObject()->findChild<QQuickRectangle*>("five");
    QVERIFY(five != 0);

    QCOMPARE(one->x(), 71.0);
    QCOMPARE(one->y(), 2.0);
    QCOMPARE(two->x(), 51.0);
    QCOMPARE(two->y(), 2.0);
    QCOMPARE(three->x(), 71.0);
    QCOMPARE(three->y(), 52.0);
    QCOMPARE(four->x(), 21.0);
    QCOMPARE(four->y(), 52.0);
    QCOMPARE(five->x(), 11.0);
    QCOMPARE(five->y(), 52.0);

}

void tst_qquickpositioners::test_flow_implicit_resize()
{
    QScopedPointer<QQuickView> window(createView(testFile("flow-testimplicitsize.qml")));
    QVERIFY(window->rootObject() != 0);

    QQuickFlow *flow = window->rootObject()->findChild<QQuickFlow*>("flow");
    QVERIFY(flow != 0);

    QCOMPARE(flow->width(), 100.0);
    QCOMPARE(flow->height(), 120.0);

    window->rootObject()->setProperty("flowLayout", 0);
    QCOMPARE(flow->flow(), QQuickFlow::LeftToRight);
    QCOMPARE(flow->width(), 220.0);
    QCOMPARE(flow->height(), 50.0);

    window->rootObject()->setProperty("flowLayout", 1);
    QCOMPARE(flow->flow(), QQuickFlow::TopToBottom);
    QCOMPARE(flow->width(), 100.0);
    QCOMPARE(flow->height(), 120.0);

    window->rootObject()->setProperty("flowLayout", 2);
    QCOMPARE(flow->layoutDirection(), Qt::RightToLeft);
    QCOMPARE(flow->width(), 220.0);
    QCOMPARE(flow->height(), 50.0);

}

void tst_qquickpositioners::test_flow_implicit_resize_padding()
{
    QScopedPointer<QQuickView> window(createView(testFile("flow-testimplicitsize.qml")));
    QVERIFY(window->rootObject() != 0);

    QQuickFlow *flow = window->rootObject()->findChild<QQuickFlow*>("flow");
    QVERIFY(flow != 0);

    QCOMPARE(flow->width(), 100.0);
    QCOMPARE(flow->height(), 120.0);

    // test padding
    flow->setProperty("padding", 1);
    flow->setProperty("topPadding", 2);
    flow->setProperty("leftPadding", 3);
    flow->setProperty("rightPadding", 4);
    flow->setProperty("bottomPadding", 5);

    QTRY_COMPARE(flow->width(), 107.0);
    QCOMPARE(flow->height(), 127.0);

    window->rootObject()->setProperty("flowLayout", 0);
    QCOMPARE(flow->flow(), QQuickFlow::LeftToRight);
    QCOMPARE(flow->width(), 227.0);
    QCOMPARE(flow->height(), 57.0);

    window->rootObject()->setProperty("flowLayout", 1);
    QCOMPARE(flow->flow(), QQuickFlow::TopToBottom);
    QCOMPARE(flow->width(), 107.0);
    QCOMPARE(flow->height(), 127.0);

    window->rootObject()->setProperty("flowLayout", 2);
    QCOMPARE(flow->layoutDirection(), Qt::RightToLeft);
    QCOMPARE(flow->width(), 227.0);
    QCOMPARE(flow->height(), 57.0);

}

void tst_qquickpositioners::test_conflictinganchors()
{
    QQmlTestMessageHandler messageHandler;
    QQmlEngine engine;
    QQmlComponent component(&engine);

    component.setData("import QtQuick 2.0\nColumn { Item { width: 100; height: 100; } }", QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QVERIFY2(messageHandler.messages().isEmpty(), qPrintable(messageHandler.messageString()));
    delete item;

    component.setData("import QtQuick 2.0\nRow { Item { width: 100; height: 100; } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QVERIFY2(messageHandler.messages().isEmpty(), qPrintable(messageHandler.messageString()));
    delete item;

    component.setData("import QtQuick 2.0\nGrid { Item { width: 100; height: 100; } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QVERIFY2(messageHandler.messages().isEmpty(), qPrintable(messageHandler.messageString()));
    delete item;

    component.setData("import QtQuick 2.0\nFlow { Item { width: 100; height: 100; } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QVERIFY2(messageHandler.messages().isEmpty(), qPrintable(messageHandler.messageString()));
    delete item;

    component.setData("import QtQuick 2.0\nColumn { Item { width: 100; height: 100; anchors.top: parent.top } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(messageHandler.messages().size(), 1);
    QCOMPARE(messageHandler.messages().back(), QString("<Unknown File>:2:1: QML Column: Cannot specify top, bottom, verticalCenter, fill or centerIn anchors for items inside Column. Column will not function."));
    messageHandler.clear();
    delete item;

    component.setData("import QtQuick 2.0\nColumn { Item { width: 100; height: 100; anchors.centerIn: parent } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(messageHandler.messages().size(), 1);
    QCOMPARE(messageHandler.messages().back(), QString("<Unknown File>:2:1: QML Column: Cannot specify top, bottom, verticalCenter, fill or centerIn anchors for items inside Column. Column will not function."));
    messageHandler.clear();
    delete item;

    component.setData("import QtQuick 2.0\nColumn { Item { width: 100; height: 100; anchors.left: parent.left } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QVERIFY2(messageHandler.messages().isEmpty(), qPrintable(messageHandler.messageString()));
    delete item;

    component.setData("import QtQuick 2.0\nRow { Item { width: 100; height: 100; anchors.left: parent.left } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(messageHandler.messages().size(), 1);
    QCOMPARE(messageHandler.messages().back(), QString("<Unknown File>:2:1: QML Row: Cannot specify left, right, horizontalCenter, fill or centerIn anchors for items inside Row. Row will not function."));
    messageHandler.clear();
    delete item;

    component.setData("import QtQuick 2.0\nRow { width: 100; height: 100; Item { anchors.fill: parent } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(messageHandler.messages().size(), 1);
    QCOMPARE(messageHandler.messages().back(), QString("<Unknown File>:2:1: QML Row: Cannot specify left, right, horizontalCenter, fill or centerIn anchors for items inside Row. Row will not function."));
    messageHandler.clear();
    delete item;

    component.setData("import QtQuick 2.0\nRow { Item { width: 100; height: 100; anchors.top: parent.top } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QVERIFY2(messageHandler.messages().isEmpty(), qPrintable(messageHandler.messageString()));
    delete item;

    component.setData("import QtQuick 2.0\nGrid { Item { width: 100; height: 100; anchors.horizontalCenter: parent.horizontalCenter } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(messageHandler.messages().size(), 1);
    QCOMPARE(messageHandler.messages().back(), QString("<Unknown File>:2:1: QML Grid: Cannot specify anchors for items inside Grid. Grid will not function."));
    messageHandler.clear();
    delete item;

    component.setData("import QtQuick 2.0\nGrid { Item { width: 100; height: 100; anchors.centerIn: parent } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(messageHandler.messages().size(), 1);
    QCOMPARE(messageHandler.messages().back(), QString("<Unknown File>:2:1: QML Grid: Cannot specify anchors for items inside Grid. Grid will not function."));
    messageHandler.clear();
    delete item;

    component.setData("import QtQuick 2.0\nFlow { Item { width: 100; height: 100; anchors.verticalCenter: parent.verticalCenter } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(messageHandler.messages().size(), 1);
    QCOMPARE(messageHandler.messages().back(), QString("<Unknown File>:2:1: QML Flow: Cannot specify anchors for items inside Flow. Flow will not function."));
    messageHandler.clear();
    delete item;

    component.setData("import QtQuick 2.0\nFlow {  width: 100; height: 100; Item { anchors.fill: parent } }", QUrl::fromLocalFile(""));
    item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(messageHandler.messages().size(), 1);
    QCOMPARE(messageHandler.messages().back(), QString("<Unknown File>:2:1: QML Flow: Cannot specify anchors for items inside Flow. Flow will not function."));
    delete item;
}

void tst_qquickpositioners::test_mirroring()
{
    QList<QString> qmlFiles;
    qmlFiles << "horizontal.qml" << "horizontal-padding.qml"
             << "gridtest.qml" << "gridtest-padding.qml"
             << "flowtest.qml" << "flowtest-padding.qml";
    QList<QString> objectNames;
    objectNames << "one" << "two" << "three" << "four" << "five";

    foreach (const QString qmlFile, qmlFiles) {
        QScopedPointer<QQuickView> windowA(createView(testFile(qmlFile)));
        QQuickItem *rootA = qobject_cast<QQuickItem*>(windowA->rootObject());

        QScopedPointer<QQuickView> windowB(createView(testFile(qmlFile)));
        QQuickItem *rootB = qobject_cast<QQuickItem*>(windowB->rootObject());
        // On OS X the windows might get positioned exactly on top of each other
        // that means no repaint for the bottom window will ever occur
        windowB->setPosition(windowB->position() + QPoint(10, 10));

        rootA->setProperty("testRightToLeft", true); // layoutDirection: Qt.RightToLeft

        // LTR != RTL
        foreach (const QString objectName, objectNames) {
            // horizontal.qml and horizontal-padding.qml only have three items
            if (qmlFile.startsWith(QString("horizontal")) && objectName == QString("four"))
                break;
            QQuickItem *itemA = rootA->findChild<QQuickItem*>(objectName);
            QQuickItem *itemB = rootB->findChild<QQuickItem*>(objectName);
            QTRY_VERIFY(itemA->x() != itemB->x());
        }

        QQmlProperty enabledProp(rootB, "LayoutMirroring.enabled", qmlContext(rootB));
        enabledProp.write(true);
        QQmlProperty inheritProp(rootB, "LayoutMirroring.childrenInherit", qmlContext(rootB));
        inheritProp.write(true);

        // RTL == mirror
        foreach (const QString objectName, objectNames) {
            // horizontal.qml and horizontal-padding.qml only have three items
            if (qmlFile.startsWith(QString("horizontal")) && objectName == QString("four"))
                break;
            QQuickItem *itemA = rootA->findChild<QQuickItem*>(objectName);
            QQuickItem *itemB = rootB->findChild<QQuickItem*>(objectName);
            QTRY_COMPARE(itemA->x(), itemB->x());

            // after resize (QTBUG-35095)
            QQuickItem *positionerA = itemA->parentItem();
            QQuickItem *positionerB = itemB->parentItem();
            positionerA->setWidth(positionerA->width() * 2);
            positionerB->setWidth(positionerB->width() * 2);
            QTRY_VERIFY(!QQuickItemPrivate::get(positionerA)->polishScheduled && !QQuickItemPrivate::get(positionerB)->polishScheduled);
            QTRY_COMPARE(itemA->x(), itemB->x());
        }

        rootA->setProperty("testRightToLeft", false); // layoutDirection: Qt.LeftToRight
        rootB->setProperty("testRightToLeft", true); // layoutDirection: Qt.RightToLeft

        // LTR == RTL + mirror
        foreach (const QString objectName, objectNames) {
            // horizontal.qml and horizontal-padding.qml only have three items
            if (qmlFile.startsWith(QString("horizontal")) && objectName == QString("four"))
                break;
            QQuickItem *itemA = rootA->findChild<QQuickItem*>(objectName);
            QQuickItem *itemB = rootB->findChild<QQuickItem*>(objectName);
            QTRY_COMPARE(itemA->x(), itemB->x());
        }
    }
}

void tst_qquickpositioners::test_allInvisible()
{
    //QTBUG-19361
    QScopedPointer<QQuickView> window(createView(testFile("allInvisible.qml")));

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root);

    QQuickRow *row = window->rootObject()->findChild<QQuickRow*>("row");
    QVERIFY(row != 0);
    QCOMPARE(row->width(), qreal(0));
    QCOMPARE(row->height(), qreal(0));

    // test padding
    row->setProperty("padding", 1);
    row->setProperty("topPadding", 2);
    row->setProperty("leftPadding", 3);
    row->setProperty("rightPadding", 4);
    row->setProperty("bottomPadding", 5);

    QTRY_COMPARE(row->height(), 7.0);
    QCOMPARE(row->width(), 7.0);

    QQuickColumn *column = window->rootObject()->findChild<QQuickColumn*>("column");
    QVERIFY(column != 0);
    QCOMPARE(column->width(), qreal(0));
    QCOMPARE(column->height(), qreal(0));

    // test padding
    column->setProperty("padding", 1);
    column->setProperty("topPadding", 2);
    column->setProperty("leftPadding", 3);
    column->setProperty("rightPadding", 4);
    column->setProperty("bottomPadding", 5);

    QTRY_COMPARE(column->height(), 7.0);
    QCOMPARE(column->width(), 7.0);

    QQuickGrid *grid = window->rootObject()->findChild<QQuickGrid*>("grid");
    QVERIFY(grid != 0);
    QCOMPARE(grid->width(), qreal(0));
    QCOMPARE(grid->height(), qreal(0));

    // test padding
    grid->setProperty("padding", 1);
    grid->setProperty("topPadding", 2);
    grid->setProperty("leftPadding", 3);
    grid->setProperty("rightPadding", 4);
    grid->setProperty("bottomPadding", 5);

    QTRY_COMPARE(grid->height(), 7.0);
    QCOMPARE(grid->width(), 7.0);

    QQuickFlow *flow = window->rootObject()->findChild<QQuickFlow*>("flow");
    QVERIFY(flow != 0);
    QCOMPARE(flow->width(), qreal(0));
    QCOMPARE(flow->height(), qreal(0));

    // test padding
    flow->setProperty("padding", 1);
    flow->setProperty("topPadding", 2);
    flow->setProperty("leftPadding", 3);
    flow->setProperty("rightPadding", 4);
    flow->setProperty("bottomPadding", 5);

    QTRY_COMPARE(flow->height(), 7.0);
    QCOMPARE(flow->width(), 7.0);
}

void tst_qquickpositioners::test_attachedproperties()
{
    QFETCH(QString, filename);

    QScopedPointer<QQuickView> window(createView(filename));
    QVERIFY(window->rootObject() != 0);

    QQuickRectangle *greenRect = window->rootObject()->findChild<QQuickRectangle *>("greenRect");
    QVERIFY(greenRect != 0);

    int posIndex = greenRect->property("posIndex").toInt();
    QCOMPARE(posIndex, 0);
    bool isFirst = greenRect->property("isFirstItem").toBool();
    QVERIFY(isFirst);
    bool isLast = greenRect->property("isLastItem").toBool();
    QVERIFY(!isLast);

    QQuickRectangle *yellowRect = window->rootObject()->findChild<QQuickRectangle *>("yellowRect");
    QVERIFY(yellowRect != 0);

    posIndex = yellowRect->property("posIndex").toInt();
    QCOMPARE(posIndex, -1);
    isFirst = yellowRect->property("isFirstItem").toBool();
    QVERIFY(!isFirst);
    isLast = yellowRect->property("isLastItem").toBool();
    QVERIFY(!isLast);

    yellowRect->metaObject()->invokeMethod(yellowRect, "onDemandPositioner");

    posIndex = yellowRect->property("posIndex").toInt();
    QCOMPARE(posIndex, 1);
    isFirst = yellowRect->property("isFirstItem").toBool();
    QVERIFY(!isFirst);
    isLast = yellowRect->property("isLastItem").toBool();
    QVERIFY(isLast);

}

void tst_qquickpositioners::test_attachedproperties_data()
{
    QTest::addColumn<QString>("filename");

    QTest::newRow("column") << testFile("attachedproperties-column.qml");
    QTest::newRow("row") << testFile("attachedproperties-row.qml");
    QTest::newRow("grid") << testFile("attachedproperties-grid.qml");
    QTest::newRow("flow") << testFile("attachedproperties-flow.qml");
}

void tst_qquickpositioners::test_attachedproperties_dynamic()
{
    QScopedPointer<QQuickView> window(createView(testFile("attachedproperties-dynamic.qml")));
    QVERIFY(window->rootObject() != 0);

    QQuickRow *row = window->rootObject()->findChild<QQuickRow *>("pos");
    QVERIFY(row != 0);

    QQuickRectangle *rect0 = window->rootObject()->findChild<QQuickRectangle *>("rect0");
    QVERIFY(rect0 != 0);

    int posIndex = rect0->property("index").toInt();
    QCOMPARE(posIndex, 0);
    bool isFirst = rect0->property("firstItem").toBool();
    QVERIFY(isFirst);
    bool isLast = rect0->property("lastItem").toBool();
    QVERIFY(!isLast);

    QQuickRectangle *rect1 = window->rootObject()->findChild<QQuickRectangle *>("rect1");
    QVERIFY(rect1 != 0);

    posIndex = rect1->property("index").toInt();
    QCOMPARE(posIndex, 1);
    isFirst = rect1->property("firstItem").toBool();
    QVERIFY(!isFirst);
    isLast = rect1->property("lastItem").toBool();
    QVERIFY(isLast);

    row->metaObject()->invokeMethod(row, "createSubRect");

    QTRY_COMPARE(rect1->property("index").toInt(), 1);
    QTRY_VERIFY(!rect1->property("firstItem").toBool());
    QTRY_VERIFY(!rect1->property("lastItem").toBool());

    QQuickRectangle *rect2 = window->rootObject()->findChild<QQuickRectangle *>("rect2");
    QVERIFY(rect2 != 0);

    posIndex = rect2->property("index").toInt();
    QCOMPARE(posIndex, 2);
    isFirst = rect2->property("firstItem").toBool();
    QVERIFY(!isFirst);
    isLast = rect2->property("lastItem").toBool();
    QVERIFY(isLast);

    row->metaObject()->invokeMethod(row, "destroySubRect");

    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    QTRY_COMPARE(rect1->property("index").toInt(), 1);
    QTRY_VERIFY(!rect1->property("firstItem").toBool());
    QTRY_VERIFY(rect1->property("lastItem").toBool());

}

QQuickView *tst_qquickpositioners::createView(const QString &filename, bool wait)
{
    QQuickView *window = new QQuickView(0);
    qDebug() << "1";

    window->setSource(QUrl::fromLocalFile(filename));
    qDebug() << "2";
    window->show();
    qDebug() << "3";
    if (wait)
        QTest::qWaitForWindowExposed(window); //It may not relayout until the next frame, so it needs to be drawn
    qDebug() << "4";

    return window;
}

void tst_qquickpositioners::matchIndexLists(const QVariantList &indexLists, const QList<int> &expectedIndexes)
{
    for (int i=0; i<indexLists.count(); i++) {
        QSet<int> current = indexLists[i].value<QList<int> >().toSet();
        if (current != expectedIndexes.toSet())
            qDebug() << "Cannot match actual targets" << current << "with expected" << expectedIndexes;
        QCOMPARE(current, expectedIndexes.toSet());
    }
}

void tst_qquickpositioners::matchItemsAndIndexes(const QVariantMap &items, const QaimModel &model, const QList<int> &expectedIndexes)
{
    for (QVariantMap::const_iterator it = items.begin(); it != items.end(); ++it) {
        QCOMPARE(it.value().type(), QVariant::Int);
        QString name = it.key();
        int itemIndex = it.value().toInt();
        QVERIFY2(expectedIndexes.contains(itemIndex), QTest::toString(QString("Index %1 not found in expectedIndexes").arg(itemIndex)));
        if (model.name(itemIndex) != name)
            qDebug() << itemIndex;
        QCOMPARE(model.name(itemIndex), name);
    }
    QCOMPARE(items.count(), expectedIndexes.count());
}

void tst_qquickpositioners::matchItemLists(const QVariantList &itemLists, const QList<QQuickItem *> &expectedItems)
{
    for (int i=0; i<itemLists.count(); i++) {
        QCOMPARE(itemLists[i].type(), QVariant::List);
        QVariantList current = itemLists[i].toList();
        for (int j=0; j<current.count(); j++) {
            QQuickItem *o = qobject_cast<QQuickItem*>(current[j].value<QObject*>());
            QVERIFY2(o, QTest::toString(QString("Invalid actual item at %1").arg(j)));
            QVERIFY2(expectedItems.contains(o), QTest::toString(QString("Cannot match item %1").arg(j)));
        }
        QCOMPARE(current.count(), expectedItems.count());
    }
}

QTEST_MAIN(tst_qquickpositioners)

#include "tst_qquickpositioners.moc"
