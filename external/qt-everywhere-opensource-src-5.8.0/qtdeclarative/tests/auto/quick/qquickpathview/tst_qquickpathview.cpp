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
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlexpression.h>
#include <QtQml/qqmlincubator.h>
#include <QtQuick/private/qquickpathview_p.h>
#include <QtQuick/private/qquickflickable_p.h>
#include <QtQuick/private/qquickpath_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQml/private/qqmllistmodel_p.h>
#include <QtQml/private/qqmlvaluetype_p.h>
#include <QtGui/qstandarditemmodel.h>
#include <QStringListModel>
#include <QFile>

#include "../../shared/util.h"
#include "../shared/viewtestutil.h"
#include "../shared/visualtestutil.h"

using namespace QQuickViewTestUtil;
using namespace QQuickVisualTestUtil;

Q_DECLARE_METATYPE(QQuickPathView::HighlightRangeMode)
Q_DECLARE_METATYPE(QQuickPathView::PositionMode)

static void initStandardTreeModel(QStandardItemModel *model)
{
    QStandardItem *item;
    item = new QStandardItem(QLatin1String("Row 1 Item"));
    model->insertRow(0, item);

    item = new QStandardItem(QLatin1String("Row 2 Item"));
    item->setCheckable(true);
    model->insertRow(1, item);

    QStandardItem *childItem = new QStandardItem(QLatin1String("Row 2 Child Item"));
    item->setChild(0, childItem);

    item = new QStandardItem(QLatin1String("Row 3 Item"));
    item->setIcon(QIcon());
    model->insertRow(2, item);
}

class tst_QQuickPathView : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_QQuickPathView();

private slots:
    void initValues();
    void items();
    void dataModel();
    void pathview2();
    void pathview3();
    void initialCurrentIndex();
    void initialCurrentItem();
    void insertModel_data();
    void insertModel();
    void removeModel_data();
    void removeModel();
    void moveModel_data();
    void moveModel();
    void consecutiveModelChanges_data();
    void consecutiveModelChanges();
    void path();
    void pathMoved();
    void offset_data();
    void offset();
    void setCurrentIndex();
    void resetModel();
    void propertyChanges();
    void pathChanges();
    void componentChanges();
    void modelChanges();
    void pathUpdateOnStartChanged();
    void package();
    void emptyModel();
    void emptyPath();
    void closed();
    void pathUpdate();
    void visualDataModel();
    void undefinedPath();
    void mouseDrag();
    void nestedMouseAreaDrag();
    void treeModel();
    void changePreferredHighlight();
    void missingPercent();
    void creationContext();
    void currentOffsetOnInsertion();
    void asynchronous();
    void cancelDrag();
    void maximumFlickVelocity();
    void snapToItem();
    void snapToItem_data();
    void snapOneItem();
    void snapOneItem_data();
    void positionViewAtIndex();
    void positionViewAtIndex_data();
    void indexAt_itemAt();
    void indexAt_itemAt_data();
    void cacheItemCount();
    void changePathDuringRefill();
    void nestedinFlickable();
    void flickableDelegate();
    void jsArrayChange();
    void qtbug37815();
    void qtbug42716();
    void qtbug53464();
    void addCustomAttribute();
    void movementDirection_data();
    void movementDirection();
};

class TestObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool error READ error WRITE setError)
    Q_PROPERTY(bool useModel READ useModel NOTIFY useModelChanged)
    Q_PROPERTY(int pathItemCount READ pathItemCount NOTIFY pathItemCountChanged)

public:
    TestObject() : QObject(), mError(true), mUseModel(true), mPathItemCount(-1) {}

    bool error() const { return mError; }
    void setError(bool err) { mError = err; }

    bool useModel() const { return mUseModel; }
    void setUseModel(bool use) { mUseModel = use; emit useModelChanged(); }

    int pathItemCount() const { return mPathItemCount; }
    void setPathItemCount(int count) { mPathItemCount = count; emit pathItemCountChanged(); }

signals:
    void useModelChanged();
    void pathItemCountChanged();

private:
    bool mError;
    bool mUseModel;
    int mPathItemCount;
};

tst_QQuickPathView::tst_QQuickPathView()
{
}

void tst_QQuickPathView::initValues()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("pathview1.qml"));
    QQuickPathView *obj = qobject_cast<QQuickPathView*>(c.create());

    QVERIFY(obj != 0);
    QVERIFY(!obj->path());
    QVERIFY(!obj->delegate());
    QCOMPARE(obj->model(), QVariant());
    QCOMPARE(obj->currentIndex(), 0);
    QCOMPARE(obj->offset(), 0.);
    QCOMPARE(obj->preferredHighlightBegin(), 0.);
    QCOMPARE(obj->dragMargin(), 0.);
    QCOMPARE(obj->count(), 0);
    QCOMPARE(obj->pathItemCount(), -1);

    delete obj;
}

void tst_QQuickPathView::items()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    model.addItem("Fred", "12345");
    model.addItem("John", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("pathview0.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = findItem<QQuickPathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    QCOMPARE(pathview->count(), model.count());
    QCOMPARE(window->rootObject()->property("count").toInt(), model.count());
    QCOMPARE(pathview->childItems().count(), model.count()+1); // assumes all are visible, including highlight

    for (int i = 0; i < model.count(); ++i) {
        QQuickText *name = findItem<QQuickText>(pathview, "textName", i);
        QVERIFY(name != 0);
        QCOMPARE(name->text(), model.name(i));
        QQuickText *number = findItem<QQuickText>(pathview, "textNumber", i);
        QVERIFY(number != 0);
        QCOMPARE(number->text(), model.number(i));
    }

    QQuickPath *path = qobject_cast<QQuickPath*>(pathview->path());
    QVERIFY(path);

    QVERIFY(pathview->highlightItem());
    QPointF start = path->pointAt(0.0);
    QPointF offset;
    offset.setX(pathview->highlightItem()->width()/2);
    offset.setY(pathview->highlightItem()->height()/2);
    QCOMPARE(pathview->highlightItem()->position() + offset, start);
}

void tst_QQuickPathView::initialCurrentItem()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    model.addItem("Jules", "12345");
    model.addItem("Vicent", "2345");
    model.addItem("Marvin", "54321");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("pathview4.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = findItem<QQuickPathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);
    QVERIFY(pathview->currentIndex() != -1);
    QVERIFY(!window->rootObject()->property("currentItemIsNull").toBool());
}

void tst_QQuickPathView::pathview2()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("pathview2.qml"));
    QQuickPathView *obj = qobject_cast<QQuickPathView*>(c.create());

    QVERIFY(obj != 0);
    QVERIFY(obj->path() != 0);
    QVERIFY(obj->delegate() != 0);
    QVERIFY(obj->model() != QVariant());
    QCOMPARE(obj->currentIndex(), 0);
    QCOMPARE(obj->offset(), 0.);
    QCOMPARE(obj->preferredHighlightBegin(), 0.);
    QCOMPARE(obj->dragMargin(), 0.);
    QCOMPARE(obj->count(), 8);
    QCOMPARE(obj->pathItemCount(), 10);

    delete obj;
}

void tst_QQuickPathView::pathview3()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("pathview3.qml"));
    QQuickPathView *obj = qobject_cast<QQuickPathView*>(c.create());

    QVERIFY(obj != 0);
    QVERIFY(obj->path() != 0);
    QVERIFY(obj->delegate() != 0);
    QVERIFY(obj->model() != QVariant());
    QCOMPARE(obj->currentIndex(), 7);
    QCOMPARE(obj->offset(), 1.0);
    QCOMPARE(obj->preferredHighlightBegin(), 0.5);
    QCOMPARE(obj->dragMargin(), 24.);
    QCOMPARE(obj->count(), 8);
    QCOMPARE(obj->pathItemCount(), 4);

    delete obj;
}

void tst_QQuickPathView::initialCurrentIndex()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("initialCurrentIndex.qml"));
    QQuickPathView *obj = qobject_cast<QQuickPathView*>(c.create());

    QVERIFY(obj != 0);
    QVERIFY(obj->path() != 0);
    QVERIFY(obj->delegate() != 0);
    QVERIFY(obj->model() != QVariant());
    QCOMPARE(obj->currentIndex(), 3);
    QCOMPARE(obj->offset(), 5.0);
    QCOMPARE(obj->preferredHighlightBegin(), 0.5);
    QCOMPARE(obj->dragMargin(), 24.);
    QCOMPARE(obj->count(), 8);
    QCOMPARE(obj->pathItemCount(), 4);

    delete obj;
}

void tst_QQuickPathView::insertModel_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("idx");
    QTest::addColumn<int>("count");
    QTest::addColumn<qreal>("offset");
    QTest::addColumn<int>("currentIndex");

    // We have 8 items, with currentIndex == 4
    QTest::newRow("insert after current")
        << int(QQuickPathView::StrictlyEnforceRange) << 6 << 1 << qreal(5.) << 4;
    QTest::newRow("insert before current")
        << int(QQuickPathView::StrictlyEnforceRange) << 2 << 1 << qreal(4.)<< 5;
    QTest::newRow("insert multiple after current")
        << int(QQuickPathView::StrictlyEnforceRange) << 5 << 2 << qreal(6.) << 4;
    QTest::newRow("insert multiple before current")
        << int(QQuickPathView::StrictlyEnforceRange) << 1 << 2 << qreal(4.) << 6;
    QTest::newRow("insert at end")
        << int(QQuickPathView::StrictlyEnforceRange) << 8 << 1 << qreal(5.) << 4;
    QTest::newRow("insert at beginning")
        << int(QQuickPathView::StrictlyEnforceRange) << 0 << 1 << qreal(4.) << 5;
    QTest::newRow("insert at current")
        << int(QQuickPathView::StrictlyEnforceRange) << 4 << 1 << qreal(4.) << 5;

    QTest::newRow("no range - insert after current")
        << int(QQuickPathView::NoHighlightRange) << 6 << 1 << qreal(5.) << 4;
    QTest::newRow("no range - insert before current")
        << int(QQuickPathView::NoHighlightRange) << 2 << 1 << qreal(4.) << 5;
    QTest::newRow("no range - insert multiple after current")
        << int(QQuickPathView::NoHighlightRange) << 5 << 2 << qreal(6.) << 4;
    QTest::newRow("no range - insert multiple before current")
        << int(QQuickPathView::NoHighlightRange) << 1 << 2 << qreal(4.) << 6;
    QTest::newRow("no range - insert at end")
        << int(QQuickPathView::NoHighlightRange) << 8 << 1 << qreal(5.) << 4;
    QTest::newRow("no range - insert at beginning")
        << int(QQuickPathView::NoHighlightRange) << 0 << 1 << qreal(4.) << 5;
    QTest::newRow("no range - insert at current")
        << int(QQuickPathView::NoHighlightRange) << 4 << 1 << qreal(4.) << 5;
}

void tst_QQuickPathView::insertModel()
{
    QFETCH(int, mode);
    QFETCH(int, idx);
    QFETCH(int, count);
    QFETCH(qreal, offset);
    QFETCH(int, currentIndex);

    QScopedPointer<QQuickView> window(createView());
    window->show();

    QaimModel model;
    model.addItem("Ben", "12345");
    model.addItem("Bohn", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");
    model.addItem("Jinny", "679");
    model.addItem("Milly", "73378");
    model.addItem("Jimmy", "3535");
    model.addItem("Barb", "9039");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("pathview0.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = findItem<QQuickPathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    pathview->setHighlightRangeMode((QQuickPathView::HighlightRangeMode)mode);

    pathview->setCurrentIndex(4);
    if (mode == QQuickPathView::StrictlyEnforceRange)
        QTRY_COMPARE(pathview->offset(), 4.0);
    else
        pathview->setOffset(4);

    QList<QPair<QString, QString> > items;
    for (int i = 0; i < count; ++i)
        items.append(qMakePair(QString("New"), QString::number(i)));

    model.insertItems(idx, items);
    QTRY_COMPARE(pathview->offset(), offset);

    QCOMPARE(pathview->currentIndex(), currentIndex);
}

void tst_QQuickPathView::removeModel_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("idx");
    QTest::addColumn<int>("count");
    QTest::addColumn<qreal>("offset");
    QTest::addColumn<int>("currentIndex");

    // We have 8 items, with currentIndex == 4
    QTest::newRow("remove after current")
        << int(QQuickPathView::StrictlyEnforceRange) << 6 << 1 << qreal(3.) << 4;
    QTest::newRow("remove before current")
        << int(QQuickPathView::StrictlyEnforceRange) << 2 << 1 << qreal(4.) << 3;
    QTest::newRow("remove multiple after current")
        << int(QQuickPathView::StrictlyEnforceRange) << 5 << 2 << qreal(2.) << 4;
    QTest::newRow("remove multiple before current")
        << int(QQuickPathView::StrictlyEnforceRange) << 1 << 2 << qreal(4.) << 2;
    QTest::newRow("remove last")
        << int(QQuickPathView::StrictlyEnforceRange) << 7 << 1 << qreal(3.) << 4;
    QTest::newRow("remove first")
        << int(QQuickPathView::StrictlyEnforceRange) << 0 << 1 << qreal(4.) << 3;
    QTest::newRow("remove current")
        << int(QQuickPathView::StrictlyEnforceRange) << 4 << 1 << qreal(3.) << 4;
    QTest::newRow("remove all")
        << int(QQuickPathView::StrictlyEnforceRange) << 0 << 8 << qreal(0.) << 0;

    QTest::newRow("no range - remove after current")
        << int(QQuickPathView::NoHighlightRange) << 6 << 1 << qreal(3.) << 4;
    QTest::newRow("no range - remove before current")
        << int(QQuickPathView::NoHighlightRange) << 2 << 1 << qreal(4.) << 3;
    QTest::newRow("no range - remove multiple after current")
        << int(QQuickPathView::NoHighlightRange) << 5 << 2 << qreal(2.) << 4;
    QTest::newRow("no range - remove multiple before current")
        << int(QQuickPathView::NoHighlightRange) << 1 << 2 << qreal(4.) << 2;
    QTest::newRow("no range - remove last")
        << int(QQuickPathView::NoHighlightRange) << 7 << 1 << qreal(3.) << 4;
    QTest::newRow("no range - remove first")
        << int(QQuickPathView::NoHighlightRange) << 0 << 1 << qreal(4.) << 3;
    QTest::newRow("no range - remove current offset")
        << int(QQuickPathView::NoHighlightRange) << 4 << 1 << qreal(4.) << 4;
    QTest::newRow("no range - remove all")
        << int(QQuickPathView::NoHighlightRange) << 0 << 8 << qreal(0.) << 0;
}

void tst_QQuickPathView::removeModel()
{
    QFETCH(int, mode);
    QFETCH(int, idx);
    QFETCH(int, count);
    QFETCH(qreal, offset);
    QFETCH(int, currentIndex);

    QScopedPointer<QQuickView> window(createView());

    window->show();

    QaimModel model;
    model.addItem("Ben", "12345");
    model.addItem("Bohn", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");
    model.addItem("Jinny", "679");
    model.addItem("Milly", "73378");
    model.addItem("Jimmy", "3535");
    model.addItem("Barb", "9039");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("pathview0.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = findItem<QQuickPathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    pathview->setHighlightRangeMode((QQuickPathView::HighlightRangeMode)mode);

    pathview->setCurrentIndex(4);
    if (mode == QQuickPathView::StrictlyEnforceRange)
        QTRY_COMPARE(pathview->offset(), 4.0);
    else
        pathview->setOffset(4);

    model.removeItems(idx, count);
    QTRY_COMPARE(pathview->offset(), offset);

    QCOMPARE(pathview->currentIndex(), currentIndex);
}


void tst_QQuickPathView::moveModel_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("from");
    QTest::addColumn<int>("to");
    QTest::addColumn<int>("count");
    QTest::addColumn<qreal>("offset");
    QTest::addColumn<int>("currentIndex");

    // We have 8 items, with currentIndex == 4
    QTest::newRow("move after current")
        << int(QQuickPathView::StrictlyEnforceRange) << 5 << 6 << 1 << qreal(4.) << 4;
    QTest::newRow("move before current")
        << int(QQuickPathView::StrictlyEnforceRange) << 2 << 3 << 1 << qreal(4.) << 4;
    QTest::newRow("move before current to after")
        << int(QQuickPathView::StrictlyEnforceRange) << 2 << 6 << 1 << qreal(5.) << 3;
    QTest::newRow("move multiple after current")
        << int(QQuickPathView::StrictlyEnforceRange) << 5 << 6 << 2 << qreal(4.) << 4;
    QTest::newRow("move multiple before current")
        << int(QQuickPathView::StrictlyEnforceRange) << 0 << 1 << 2 << qreal(4.) << 4;
    QTest::newRow("move before current to end")
        << int(QQuickPathView::StrictlyEnforceRange) << 2 << 7 << 1 << qreal(5.) << 3;
    QTest::newRow("move last to beginning")
        << int(QQuickPathView::StrictlyEnforceRange) << 7 << 0 << 1 << qreal(3.) << 5;
    QTest::newRow("move current")
        << int(QQuickPathView::StrictlyEnforceRange) << 4 << 6 << 1 << qreal(2.) << 6;

    QTest::newRow("no range - move after current")
        << int(QQuickPathView::NoHighlightRange) << 5 << 6 << 1 << qreal(4.) << 4;
    QTest::newRow("no range - move before current")
        << int(QQuickPathView::NoHighlightRange) << 2 << 3 << 1 << qreal(4.) << 4;
    QTest::newRow("no range - move before current to after")
        << int(QQuickPathView::NoHighlightRange) << 2 << 6 << 1 << qreal(5.) << 3;
    QTest::newRow("no range - move multiple after current")
        << int(QQuickPathView::NoHighlightRange) << 5 << 6 << 2 << qreal(4.) << 4;
    QTest::newRow("no range - move multiple before current")
        << int(QQuickPathView::NoHighlightRange) << 0 << 1 << 2 << qreal(4.) << 4;
    QTest::newRow("no range - move before current to end")
        << int(QQuickPathView::NoHighlightRange) << 2 << 7 << 1 << qreal(5.) << 3;
    QTest::newRow("no range - move last to beginning")
        << int(QQuickPathView::NoHighlightRange) << 7 << 0 << 1 << qreal(3.) << 5;
    QTest::newRow("no range - move current")
        << int(QQuickPathView::NoHighlightRange) << 4 << 6 << 1 << qreal(4.) << 6;
    QTest::newRow("no range - move multiple incl. current")
        << int(QQuickPathView::NoHighlightRange) << 0 << 1 << 5 << qreal(4.) << 5;
}

void tst_QQuickPathView::moveModel()
{
    QFETCH(int, mode);
    QFETCH(int, from);
    QFETCH(int, to);
    QFETCH(int, count);
    QFETCH(qreal, offset);
    QFETCH(int, currentIndex);

    QScopedPointer<QQuickView> window(createView());
    window->show();

    QaimModel model;
    model.addItem("Ben", "12345");
    model.addItem("Bohn", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");
    model.addItem("Jinny", "679");
    model.addItem("Milly", "73378");
    model.addItem("Jimmy", "3535");
    model.addItem("Barb", "9039");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("pathview0.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = findItem<QQuickPathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    pathview->setHighlightRangeMode((QQuickPathView::HighlightRangeMode)mode);

    pathview->setCurrentIndex(4);
    if (mode == QQuickPathView::StrictlyEnforceRange)
        QTRY_COMPARE(pathview->offset(), 4.0);
    else
        pathview->setOffset(4);

    model.moveItems(from, to, count);
    QTRY_COMPARE(pathview->offset(), offset);

    QCOMPARE(pathview->currentIndex(), currentIndex);
}

void tst_QQuickPathView::consecutiveModelChanges_data()
{
    QTest::addColumn<QQuickPathView::HighlightRangeMode>("mode");
    QTest::addColumn<QList<ListChange> >("changes");
    QTest::addColumn<int>("count");
    QTest::addColumn<qreal>("offset");
    QTest::addColumn<int>("currentIndex");

    QTest::newRow("no range - insert after, insert before")
            << QQuickPathView::NoHighlightRange
            << (QList<ListChange>()
                << ListChange::insert(7, 2)
                << ListChange::insert(1, 3))
            << 13
            << 6.
            << 7;
    QTest::newRow("no range - remove after, remove before")
            << QQuickPathView::NoHighlightRange
            << (QList<ListChange>()
                << ListChange::remove(6, 2)
                << ListChange::remove(1, 3))
            << 3
            << 2.
            << 1;

    QTest::newRow("no range - remove after, insert before")
            << QQuickPathView::NoHighlightRange
            << (QList<ListChange>()
                << ListChange::remove(5, 2)
                << ListChange::insert(1, 3))
            << 9
            << 2.
            << 7;

    QTest::newRow("no range - insert after, remove before")
            << QQuickPathView::NoHighlightRange
            << (QList<ListChange>()
                << ListChange::insert(6, 2)
                << ListChange::remove(1, 3))
            << 7
            << 6.
            << 1;

    QTest::newRow("no range - insert, remove all, polish, insert")
            << QQuickPathView::NoHighlightRange
            << (QList<ListChange>()
                << ListChange::insert(3, 1)
                << ListChange::remove(0, 9)
                << ListChange::polish()
                << ListChange::insert(0, 3))
            << 3
            << 0.
            << 0;
}

void tst_QQuickPathView::consecutiveModelChanges()
{
    QFETCH(QQuickPathView::HighlightRangeMode, mode);
    QFETCH(QList<ListChange>, changes);
    QFETCH(int, count);
    QFETCH(qreal, offset);
    QFETCH(int, currentIndex);

    QScopedPointer<QQuickView> window(createView());
    window->show();

    QaimModel model;
    model.addItem("Ben", "12345");
    model.addItem("Bohn", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");
    model.addItem("Jinny", "679");
    model.addItem("Milly", "73378");
    model.addItem("Jimmy", "3535");
    model.addItem("Barb", "9039");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("pathview0.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = findItem<QQuickPathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    pathview->setHighlightRangeMode(mode);

    pathview->setCurrentIndex(4);
    if (mode == QQuickPathView::StrictlyEnforceRange)
        QTRY_COMPARE(pathview->offset(), 4.0);
    else
        pathview->setOffset(4);

    for (int i=0; i<changes.count(); i++) {
        switch (changes[i].type) {
            case ListChange::Inserted:
            {
                QList<QPair<QString, QString> > items;
                for (int j=changes[i].index; j<changes[i].index + changes[i].count; ++j)
                    items << qMakePair(QString("new item %1").arg(j), QString::number(j));
                model.insertItems(changes[i].index, items);
                break;
            }
            case ListChange::Removed:
                model.removeItems(changes[i].index, changes[i].count);
                break;
            case ListChange::Moved:
                model.moveItems(changes[i].index, changes[i].to, changes[i].count);
                break;
            case ListChange::SetCurrent:
                pathview->setCurrentIndex(changes[i].index);
                break;
        case ListChange::Polish:
                QQUICK_VERIFY_POLISH(pathview);
                break;
            default:
                continue;
        }
    }
    QQUICK_VERIFY_POLISH(pathview);

    QCOMPARE(findItems<QQuickItem>(pathview, "wrapper").count(), count);
    QCOMPARE(pathview->count(), count);
    QTRY_COMPARE(pathview->offset(), offset);

    QCOMPARE(pathview->currentIndex(), currentIndex);

}

void tst_QQuickPathView::path()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("pathtest.qml"));
    QQuickPath *obj = qobject_cast<QQuickPath*>(c.create());

    QVERIFY(obj != 0);
    QCOMPARE(obj->startX(), 120.);
    QCOMPARE(obj->startY(), 100.);
    QVERIFY(obj->path() != QPainterPath());

    QQmlListReference list(obj, "pathElements");
    QCOMPARE(list.count(), 5);

    QQuickPathAttribute* attr = qobject_cast<QQuickPathAttribute*>(list.at(0));
    QVERIFY(attr != 0);
    QCOMPARE(attr->name(), QString("scale"));
    QCOMPARE(attr->value(), 1.0);

    QQuickPathQuad* quad = qobject_cast<QQuickPathQuad*>(list.at(1));
    QVERIFY(quad != 0);
    QCOMPARE(quad->x(), 120.);
    QCOMPARE(quad->y(), 25.);
    QCOMPARE(quad->controlX(), 260.);
    QCOMPARE(quad->controlY(), 75.);

    QQuickPathPercent* perc = qobject_cast<QQuickPathPercent*>(list.at(2));
    QVERIFY(perc != 0);
    QCOMPARE(perc->value(), 0.3);

    QQuickPathLine* line = qobject_cast<QQuickPathLine*>(list.at(3));
    QVERIFY(line != 0);
    QCOMPARE(line->x(), 120.);
    QCOMPARE(line->y(), 100.);

    QQuickPathCubic* cubic = qobject_cast<QQuickPathCubic*>(list.at(4));
    QVERIFY(cubic != 0);
    QCOMPARE(cubic->x(), 180.);
    QCOMPARE(cubic->y(), 0.);
    QCOMPARE(cubic->control1X(), -10.);
    QCOMPARE(cubic->control1Y(), 90.);
    QCOMPARE(cubic->control2X(), 210.);
    QCOMPARE(cubic->control2Y(), 90.);

    delete obj;
}

void tst_QQuickPathView::dataModel()
{
    QScopedPointer<QQuickView> window(createView());
    window->show();

    QQmlContext *ctxt = window->rootContext();
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    QaimModel model;
    model.addItem("red", "1");
    model.addItem("green", "2");
    model.addItem("blue", "3");
    model.addItem("purple", "4");
    model.addItem("gray", "5");
    model.addItem("brown", "6");
    model.addItem("yellow", "7");
    model.addItem("thistle", "8");
    model.addItem("cyan", "9");
    model.addItem("peachpuff", "10");
    model.addItem("powderblue", "11");
    model.addItem("gold", "12");
    model.addItem("sandybrown", "13");

    ctxt->setContextProperty("testData", &model);

    window->setSource(testFileUrl("datamodel.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    QMetaObject::invokeMethod(window->rootObject(), "checkProperties");
    QVERIFY(!testObject->error());

    QQuickItem *item = findItem<QQuickItem>(pathview, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->x(), 110.0);
    QCOMPARE(item->y(), 10.0);

    model.insertItem(4, "orange", "10");
    QTest::qWait(100);

    QCOMPARE(window->rootObject()->property("viewCount").toInt(), model.count());
    QTRY_COMPARE(findItems<QQuickItem>(pathview, "wrapper").count(), 14);

    QCOMPARE(pathview->currentIndex(), 0);
    QCOMPARE(pathview->currentItem(), findItem<QQuickItem>(pathview, "wrapper", 0));

    QQuickText *text = findItem<QQuickText>(pathview, "myText", 4);
    QVERIFY(text);
    QCOMPARE(text->text(), model.name(4));

    model.removeItem(2);
    QCOMPARE(window->rootObject()->property("viewCount").toInt(), model.count());
    text = findItem<QQuickText>(pathview, "myText", 2);
    QVERIFY(text);
    QCOMPARE(text->text(), model.name(2));
    QCOMPARE(pathview->currentItem(), findItem<QQuickItem>(pathview, "wrapper", 0));

    testObject->setPathItemCount(5);
    QMetaObject::invokeMethod(window->rootObject(), "checkProperties");
    QVERIFY(!testObject->error());

    QTRY_COMPARE(findItems<QQuickItem>(pathview, "wrapper").count(), 5);

    QQuickRectangle *testItem = findItem<QQuickRectangle>(pathview, "wrapper", 4);
    QVERIFY(testItem != 0);
    testItem = findItem<QQuickRectangle>(pathview, "wrapper", 5);
    QVERIFY(!testItem);

    pathview->setCurrentIndex(1);
    QCOMPARE(pathview->currentIndex(), 1);
    QCOMPARE(pathview->currentItem(), findItem<QQuickItem>(pathview, "wrapper", 1));

    model.insertItem(2, "pink", "2");

    QTRY_COMPARE(findItems<QQuickItem>(pathview, "wrapper").count(), 5);
    QCOMPARE(pathview->currentIndex(), 1);
    QCOMPARE(pathview->currentItem(), findItem<QQuickItem>(pathview, "wrapper", 1));

    QTRY_VERIFY(text = findItem<QQuickText>(pathview, "myText", 2));
    QCOMPARE(text->text(), model.name(2));

    model.removeItem(3);
    QTRY_COMPARE(findItems<QQuickItem>(pathview, "wrapper").count(), 5);
    text = findItem<QQuickText>(pathview, "myText", 3);
    QVERIFY(text);
    QCOMPARE(text->text(), model.name(3));
    QCOMPARE(pathview->currentItem(), findItem<QQuickItem>(pathview, "wrapper", 1));

    model.moveItem(3, 5);
    QTRY_COMPARE(findItems<QQuickItem>(pathview, "wrapper").count(), 5);
    QList<QQuickItem*> items = findItems<QQuickItem>(pathview, "wrapper");
    foreach (QQuickItem *item, items) {
        QVERIFY(item->property("onPath").toBool());
    }
    QCOMPARE(pathview->currentItem(), findItem<QQuickItem>(pathview, "wrapper", 1));

    // QTBUG-14199
    pathview->setOffset(7);
    pathview->setOffset(0);
    QCOMPARE(findItems<QQuickItem>(pathview, "wrapper").count(), 5);

    pathview->setCurrentIndex(model.count()-1);
    QTRY_COMPARE(pathview->offset(), 1.0);
    model.removeItem(model.count()-1);
    QCOMPARE(pathview->currentIndex(), model.count()-1);

    delete testObject;
}

void tst_QQuickPathView::pathMoved()
{
    QScopedPointer<QQuickView> window(createView());
    window->show();

    QaimModel model;
    model.addItem("Ben", "12345");
    model.addItem("Bohn", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("pathview0.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = findItem<QQuickPathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    QQuickRectangle *firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QQuickPath *path = qobject_cast<QQuickPath*>(pathview->path());
    QVERIFY(path);
    QPointF start = path->pointAt(0.0);
    QPointF offset;//Center of item is at point, but pos is from corner
    offset.setX(firstItem->width()/2);
    offset.setY(firstItem->height()/2);
    QTRY_COMPARE(firstItem->position() + offset, start);
    pathview->setOffset(1.0);

    for (int i=0; i<model.count(); i++) {
        QQuickRectangle *curItem = findItem<QQuickRectangle>(pathview, "wrapper", i);
        QPointF itemPos(path->pointAt(0.25 + i*0.25));
        QCOMPARE(curItem->position() + offset, QPointF(itemPos.x(), itemPos.y()));
    }

    QCOMPARE(pathview->currentIndex(), 3);

    pathview->setOffset(0.0);
    QCOMPARE(firstItem->position() + offset, start);
    QCOMPARE(pathview->currentIndex(), 0);

    // Change delegate size
    pathview->setOffset(0.1);
    pathview->setOffset(0.0);
    window->rootObject()->setProperty("delegateWidth", 30);
    QCOMPARE(firstItem->width(), 30.0);
    offset.setX(firstItem->width()/2);
    QTRY_COMPARE(firstItem->position() + offset, start);

    // Change delegate scale
    pathview->setOffset(0.1);
    pathview->setOffset(0.0);
    window->rootObject()->setProperty("delegateScale", 1.2);
    QTRY_COMPARE(firstItem->position() + offset, start);

}

void tst_QQuickPathView::offset_data()
{
    QTest::addColumn<qreal>("offset");
    QTest::addColumn<int>("currentIndex");

    QTest::newRow("0.0") << 0.0 << 0;
    QTest::newRow("1.0") << 7.0 << 1;
    QTest::newRow("5.0") << 5.0 << 3;
    QTest::newRow("4.6") << 4.6 << 3;
    QTest::newRow("4.4") << 4.4 << 4;
    QTest::newRow("5.4") << 5.4 << 3;
    QTest::newRow("5.6") << 5.6 << 2;
}

void tst_QQuickPathView::offset()
{
    QFETCH(qreal, offset);
    QFETCH(int, currentIndex);

    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("pathview3.qml"));
    QQuickPathView *view = qobject_cast<QQuickPathView*>(c.create());

    view->setOffset(offset);
    QCOMPARE(view->currentIndex(), currentIndex);

    delete view;
}

void tst_QQuickPathView::setCurrentIndex()
{
    QScopedPointer<QQuickView> window(createView());
    window->show();

    QaimModel model;
    model.addItem("Ben", "12345");
    model.addItem("Bohn", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("pathview0.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = findItem<QQuickPathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    QQuickRectangle *firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QQuickPath *path = qobject_cast<QQuickPath*>(pathview->path());
    QVERIFY(path);
    QPointF start = path->pointAt(0.0);
    QPointF offset;//Center of item is at point, but pos is from corner
    offset.setX(firstItem->width()/2);
    offset.setY(firstItem->height()/2);
    QCOMPARE(firstItem->position() + offset, start);
    QCOMPARE(window->rootObject()->property("currentA").toInt(), 0);
    QCOMPARE(window->rootObject()->property("currentB").toInt(), 0);

    pathview->setCurrentIndex(2);

    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 2);
    QTRY_COMPARE(firstItem->position() + offset, start);
    QCOMPARE(window->rootObject()->property("currentA").toInt(), 2);
    QCOMPARE(window->rootObject()->property("currentB").toInt(), 2);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(true));

    pathview->decrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 1);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 1);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->position() + offset, start);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(true));

    pathview->decrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 0);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->position() + offset, start);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(true));

    pathview->decrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 3);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 3);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->position() + offset, start);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(true));

    pathview->incrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 0);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->position() + offset, start);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(true));

    // Test positive indexes are wrapped.
    pathview->setCurrentIndex(6);
    QTRY_COMPARE(pathview->currentIndex(), 2);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 2);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->position() + offset, start);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(true));

    // Test negative indexes are wrapped.
    pathview->setCurrentIndex(-3);
    QTRY_COMPARE(pathview->currentIndex(), 1);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 1);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->position() + offset, start);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(true));

    // move an item, set move duration to 0, and change currentIndex to moved item. QTBUG-22786
    model.moveItem(0, 3);
    pathview->setHighlightMoveDuration(0);
    pathview->setCurrentIndex(3);
    QCOMPARE(pathview->currentIndex(), 3);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 3);
    QVERIFY(firstItem);
    QCOMPARE(pathview->currentItem(), firstItem);
    QTRY_COMPARE(firstItem->position() + offset, start);
    model.moveItem(3, 0);
    pathview->setCurrentIndex(0);
    pathview->setHighlightMoveDuration(300);

    // Check the current item is still created when outside the bounds of pathItemCount.
    pathview->setPathItemCount(2);
    pathview->setHighlightRangeMode(QQuickPathView::NoHighlightRange);
    QVERIFY(findItem<QQuickRectangle>(pathview, "wrapper", 0));
    QVERIFY(findItem<QQuickRectangle>(pathview, "wrapper", 1));
    QVERIFY(!findItem<QQuickRectangle>(pathview, "wrapper", 2));
    QVERIFY(!findItem<QQuickRectangle>(pathview, "wrapper", 3));

    pathview->setCurrentIndex(2);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 2);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(false));

    pathview->decrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 1);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 1);
    QVERIFY(firstItem);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(true));

    pathview->decrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 0);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(true));

    pathview->decrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 3);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 3);
    QVERIFY(firstItem);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(false));

    pathview->incrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 0);
    firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QCOMPARE(pathview->currentItem(), firstItem);
    QCOMPARE(firstItem->property("onPath"), QVariant(true));

    // check for bogus currentIndexChanged() signals
    QSignalSpy currentIndexSpy(pathview, SIGNAL(currentIndexChanged()));
    QVERIFY(currentIndexSpy.isValid());
    pathview->setHighlightMoveDuration(100);
    pathview->setHighlightRangeMode(QQuickPathView::StrictlyEnforceRange);
    pathview->setSnapMode(QQuickPathView::SnapToItem);
    pathview->setCurrentIndex(3);
    QTRY_COMPARE(pathview->currentIndex(), 3);
    QCOMPARE(currentIndexSpy.count(), 1);
}

void tst_QQuickPathView::resetModel()
{
    QScopedPointer<QQuickView> window(createView());

    QStringList strings;
    strings << "one" << "two" << "three";
    QStringListModel model(strings);

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("displaypath.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = findItem<QQuickPathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    QCOMPARE(pathview->count(), model.rowCount());

    for (int i = 0; i < model.rowCount(); ++i) {
        QQuickText *display = findItem<QQuickText>(pathview, "displayText", i);
        QVERIFY(display != 0);
        QCOMPARE(display->text(), strings.at(i));
    }

    strings.clear();
    strings << "four" << "five" << "six" << "seven";
    model.setStringList(strings);

    QCOMPARE(pathview->count(), model.rowCount());

    for (int i = 0; i < model.rowCount(); ++i) {
        QQuickText *display = findItem<QQuickText>(pathview, "displayText", i);
        QVERIFY(display != 0);
        QCOMPARE(display->text(), strings.at(i));
    }

}

void tst_QQuickPathView::propertyChanges()
{
    QScopedPointer<QQuickView> window(createView());
    QVERIFY(window);
    window->setSource(testFileUrl("propertychanges.qml"));

    QQuickPathView *pathView = window->rootObject()->findChild<QQuickPathView*>("pathView");
    QVERIFY(pathView);

    QSignalSpy snapPositionSpy(pathView, SIGNAL(preferredHighlightBeginChanged()));
    QSignalSpy dragMarginSpy(pathView, SIGNAL(dragMarginChanged()));

    QCOMPARE(pathView->preferredHighlightBegin(), 0.1);
    QCOMPARE(pathView->dragMargin(), 5.0);

    pathView->setPreferredHighlightBegin(0.4);
    pathView->setPreferredHighlightEnd(0.4);
    pathView->setDragMargin(20.0);

    QCOMPARE(pathView->preferredHighlightBegin(), 0.4);
    QCOMPARE(pathView->preferredHighlightEnd(), 0.4);
    QCOMPARE(pathView->dragMargin(), 20.0);

    QCOMPARE(snapPositionSpy.count(), 1);
    QCOMPARE(dragMarginSpy.count(), 1);

    pathView->setPreferredHighlightBegin(0.4);
    pathView->setPreferredHighlightEnd(0.4);
    pathView->setDragMargin(20.0);

    QCOMPARE(snapPositionSpy.count(), 1);
    QCOMPARE(dragMarginSpy.count(), 1);

    QSignalSpy maximumFlickVelocitySpy(pathView, SIGNAL(maximumFlickVelocityChanged()));
    pathView->setMaximumFlickVelocity(1000);
    QCOMPARE(maximumFlickVelocitySpy.count(), 1);
    pathView->setMaximumFlickVelocity(1000);
    QCOMPARE(maximumFlickVelocitySpy.count(), 1);

}

void tst_QQuickPathView::pathChanges()
{
    QScopedPointer<QQuickView> window(createView());
    QVERIFY(window);
    window->setSource(testFileUrl("propertychanges.qml"));

    QQuickPathView *pathView = window->rootObject()->findChild<QQuickPathView*>("pathView");
    QVERIFY(pathView);

    QQuickPath *path = window->rootObject()->findChild<QQuickPath*>("path");
    QVERIFY(path);

    QSignalSpy startXSpy(path, SIGNAL(startXChanged()));
    QSignalSpy startYSpy(path, SIGNAL(startYChanged()));

    QCOMPARE(path->startX(), 220.0);
    QCOMPARE(path->startY(), 200.0);

    path->setStartX(240.0);
    path->setStartY(220.0);

    QCOMPARE(path->startX(), 240.0);
    QCOMPARE(path->startY(), 220.0);

    QCOMPARE(startXSpy.count(),1);
    QCOMPARE(startYSpy.count(),1);

    path->setStartX(240);
    path->setStartY(220);

    QCOMPARE(startXSpy.count(),1);
    QCOMPARE(startYSpy.count(),1);

    QQuickPath *alternatePath = window->rootObject()->findChild<QQuickPath*>("alternatePath");
    QVERIFY(alternatePath);

    QSignalSpy pathSpy(pathView, SIGNAL(pathChanged()));

    QCOMPARE(pathView->path(), path);

    pathView->setPath(alternatePath);
    QCOMPARE(pathView->path(), alternatePath);
    QCOMPARE(pathSpy.count(),1);

    pathView->setPath(alternatePath);
    QCOMPARE(pathSpy.count(),1);

    QQuickPathAttribute *pathAttribute = window->rootObject()->findChild<QQuickPathAttribute*>("pathAttribute");
    QVERIFY(pathAttribute);

    QSignalSpy nameSpy(pathAttribute, SIGNAL(nameChanged()));
    QCOMPARE(pathAttribute->name(), QString("opacity"));

    pathAttribute->setName("scale");
    QCOMPARE(pathAttribute->name(), QString("scale"));
    QCOMPARE(nameSpy.count(),1);

    pathAttribute->setName("scale");
    QCOMPARE(nameSpy.count(),1);
}

void tst_QQuickPathView::componentChanges()
{
    QScopedPointer<QQuickView> window(createView());
    QVERIFY(window);
    window->setSource(testFileUrl("propertychanges.qml"));

    QQuickPathView *pathView = window->rootObject()->findChild<QQuickPathView*>("pathView");
    QVERIFY(pathView);

    QQmlComponent delegateComponent(window->engine());
    delegateComponent.setData("import QtQuick 2.0; Text { text: '<b>Name:</b> ' + name }", QUrl::fromLocalFile(""));

    QSignalSpy delegateSpy(pathView, SIGNAL(delegateChanged()));

    pathView->setDelegate(&delegateComponent);
    QCOMPARE(pathView->delegate(), &delegateComponent);
    QCOMPARE(delegateSpy.count(),1);

    pathView->setDelegate(&delegateComponent);
    QCOMPARE(delegateSpy.count(),1);
}

void tst_QQuickPathView::modelChanges()
{
    QScopedPointer<QQuickView> window(createView());
    QVERIFY(window);
    window->setSource(testFileUrl("propertychanges.qml"));

    QQuickPathView *pathView = window->rootObject()->findChild<QQuickPathView*>("pathView");
    QVERIFY(pathView);
    pathView->setCurrentIndex(3);
    QTRY_COMPARE(pathView->offset(), 6.0);

    QQmlListModel *alternateModel = window->rootObject()->findChild<QQmlListModel*>("alternateModel");
    QVERIFY(alternateModel);
    QVariant modelVariant = QVariant::fromValue<QObject *>(alternateModel);
    QSignalSpy modelSpy(pathView, SIGNAL(modelChanged()));
    QSignalSpy currentIndexSpy(pathView, SIGNAL(currentIndexChanged()));

    QCOMPARE(pathView->currentIndex(), 3);
    pathView->setModel(modelVariant);
    QCOMPARE(pathView->model(), modelVariant);
    QCOMPARE(modelSpy.count(),1);
    QCOMPARE(pathView->currentIndex(), 0);
    QCOMPARE(currentIndexSpy.count(), 1);

    pathView->setModel(modelVariant);
    QCOMPARE(modelSpy.count(),1);

    pathView->setModel(QVariant());
    QCOMPARE(modelSpy.count(),2);
    QCOMPARE(pathView->currentIndex(), 0);
    QCOMPARE(currentIndexSpy.count(), 1);

}

void tst_QQuickPathView::pathUpdateOnStartChanged()
{
    QScopedPointer<QQuickView> window(createView());
    QVERIFY(window);
    window->setSource(testFileUrl("pathUpdateOnStartChanged.qml"));

    QQuickPathView *pathView = window->rootObject()->findChild<QQuickPathView*>("pathView");
    QVERIFY(pathView);

    QQuickPath *path = window->rootObject()->findChild<QQuickPath*>("path");
    QVERIFY(path);
    QCOMPARE(path->startX(), 400.0);
    QCOMPARE(path->startY(), 300.0);

    QQuickItem *item = findItem<QQuickItem>(pathView, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->x(), path->startX() - item->width() / 2.0);
    QCOMPARE(item->y(), path->startY() - item->height() / 2.0);

}

void tst_QQuickPathView::package()
{
    QScopedPointer<QQuickView> window(createView());
    QVERIFY(window);
    window->setSource(testFileUrl("pathview_package.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));

    QQuickPathView *pathView = window->rootObject()->findChild<QQuickPathView*>("photoPathView");
    QVERIFY(pathView);

#ifdef Q_OS_MAC
    QSKIP("QTBUG-27170 view does not reliably receive polish without a running animation");
#endif

    QQuickItem *item = findItem<QQuickItem>(pathView, "pathItem");
    QVERIFY(item);
    QVERIFY(item->scale() != 1.0);

}

//QTBUG-13017
void tst_QQuickPathView::emptyModel()
{
    QScopedPointer<QQuickView> window(createView());

    QStringListModel model;

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("emptyModel", &model);

    window->setSource(testFileUrl("emptymodel.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    QCOMPARE(pathview->offset(), qreal(0.0));

}

void tst_QQuickPathView::emptyPath()
{
    QQuickView *window = createView();

    window->setSource(testFileUrl("emptypath.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    delete window;
}

void tst_QQuickPathView::closed()
{
    QQmlEngine engine;

    {
        QQmlComponent c(&engine, testFileUrl("openPath.qml"));
        QQuickPath *obj = qobject_cast<QQuickPath*>(c.create());
        QVERIFY(obj);
        QCOMPARE(obj->isClosed(), false);
        delete obj;
    }

    {
        QQmlComponent c(&engine, testFileUrl("closedPath.qml"));
        QQuickPath *obj = qobject_cast<QQuickPath*>(c.create());
        QVERIFY(obj);
        QCOMPARE(obj->isClosed(), true);
        delete obj;
    }
}

// QTBUG-14239
void tst_QQuickPathView::pathUpdate()
{
    QScopedPointer<QQuickView> window(createView());
    QVERIFY(window);
    window->setSource(testFileUrl("pathUpdate.qml"));

    QQuickPathView *pathView = window->rootObject()->findChild<QQuickPathView*>("pathView");
    QVERIFY(pathView);

    QQuickItem *item = findItem<QQuickItem>(pathView, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->x(), 150.0);

}

void tst_QQuickPathView::visualDataModel()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("vdm.qml"));

    QQuickPathView *obj = qobject_cast<QQuickPathView*>(c.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->count(), 3);

    delete obj;
}

void tst_QQuickPathView::undefinedPath()
{
    QQmlEngine engine;

    // QPainterPath warnings are only received if QT_NO_DEBUG is not defined
    if (QLibraryInfo::isDebugBuild()) {
        QString warning1("QPainterPath::moveTo: Adding point where x or y is NaN or Inf, ignoring call");
        QTest::ignoreMessage(QtWarningMsg,qPrintable(warning1));

        QString warning2("QPainterPath::lineTo: Adding point where x or y is NaN or Inf, ignoring call");
        QTest::ignoreMessage(QtWarningMsg,qPrintable(warning2));
    }

    QQmlComponent c(&engine, testFileUrl("undefinedpath.qml"));

    QQuickPathView *obj = qobject_cast<QQuickPathView*>(c.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->count(), 3);

    delete obj;
}

void tst_QQuickPathView::mouseDrag()
{
    QScopedPointer<QQuickView> window(createView());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->setSource(testFileUrl("dragpath.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    QSignalSpy movingSpy(pathview, SIGNAL(movingChanged()));
    QSignalSpy moveStartedSpy(pathview, SIGNAL(movementStarted()));
    QSignalSpy moveEndedSpy(pathview, SIGNAL(movementEnded()));
    QSignalSpy draggingSpy(pathview, SIGNAL(draggingChanged()));
    QSignalSpy dragStartedSpy(pathview, SIGNAL(dragStarted()));
    QSignalSpy dragEndedSpy(pathview, SIGNAL(dragEnded()));

    int current = pathview->currentIndex();

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(10,100));
    QTest::qWait(100);

    {
        QMouseEvent mv(QEvent::MouseMove, QPoint(50,100), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QGuiApplication::sendEvent(window.data(), &mv);
    }
    // first move beyond threshold does not trigger drag
    QVERIFY(!pathview->isMoving());
    QVERIFY(!pathview->isDragging());
    QCOMPARE(movingSpy.count(), 0);
    QCOMPARE(moveStartedSpy.count(), 0);
    QCOMPARE(moveEndedSpy.count(), 0);
    QCOMPARE(draggingSpy.count(), 0);
    QCOMPARE(dragStartedSpy.count(), 0);
    QCOMPARE(dragEndedSpy.count(), 0);

    {
        QMouseEvent mv(QEvent::MouseMove, QPoint(90,100), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QGuiApplication::sendEvent(window.data(), &mv);
    }
    // next move beyond threshold does trigger drag
#ifdef Q_OS_WIN
    if (!pathview->isMoving())
        QSKIP("Skipping due to interference from external mouse move events.");
#endif // Q_OS_WIN
    QVERIFY(pathview->isMoving());
    QVERIFY(pathview->isDragging());
    QCOMPARE(movingSpy.count(), 1);
    QCOMPARE(moveStartedSpy.count(), 1);
    QCOMPARE(moveEndedSpy.count(), 0);
    QCOMPARE(draggingSpy.count(), 1);
    QCOMPARE(dragStartedSpy.count(), 1);
    QCOMPARE(dragEndedSpy.count(), 0);

    QVERIFY(pathview->currentIndex() != current);

    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(40,100));
    QVERIFY(!pathview->isDragging());
    QCOMPARE(draggingSpy.count(), 2);
    QCOMPARE(dragStartedSpy.count(), 1);
    QCOMPARE(dragEndedSpy.count(), 1);
    QTRY_COMPARE(movingSpy.count(), 2);
    QTRY_COMPARE(moveEndedSpy.count(), 1);
    QCOMPARE(moveStartedSpy.count(), 1);

}

void tst_QQuickPathView::nestedMouseAreaDrag()
{
    QScopedPointer<QQuickView> window(createView());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->setSource(testFileUrl("nestedmousearea.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());


    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    // Dragging the child mouse area should move it and not animate the PathView
    flick(window.data(), QPoint(200,200), QPoint(400,200), 200);
    QVERIFY(!pathview->isMoving());

    // Dragging outside the mouse are should animate the PathView.
    flick(window.data(), QPoint(75,75), QPoint(175,75), 200);
    QVERIFY(pathview->isMoving());
}

void tst_QQuickPathView::treeModel()
{
    QScopedPointer<QQuickView> window(createView());
    window->show();

    QStandardItemModel model;
    initStandardTreeModel(&model);
    window->engine()->rootContext()->setContextProperty("myModel", &model);

    window->setSource(testFileUrl("treemodel.qml"));

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);
    QCOMPARE(pathview->count(), 3);

    QQuickText *item = findItem<QQuickText>(pathview, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->text(), QLatin1String("Row 1 Item"));

    QVERIFY(QMetaObject::invokeMethod(pathview, "setRoot", Q_ARG(QVariant, 1)));
    QCOMPARE(pathview->count(), 1);

    QTRY_VERIFY(item = findItem<QQuickText>(pathview, "wrapper", 0));
    QTRY_COMPARE(item->text(), QLatin1String("Row 2 Child Item"));

}

void tst_QQuickPathView::changePreferredHighlight()
{
    QScopedPointer<QQuickView> window(createView());
    window->setGeometry(0,0,400,200);
    window->setSource(testFileUrl("dragpath.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    int current = pathview->currentIndex();
    QCOMPARE(current, 0);

    QQuickRectangle *firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QQuickPath *path = qobject_cast<QQuickPath*>(pathview->path());
    QVERIFY(path);
    QPointF start = path->pointAt(0.5);
    QPointF offset;//Center of item is at point, but pos is from corner
    offset.setX(firstItem->width()/2);
    offset.setY(firstItem->height()/2);
    QTRY_COMPARE(firstItem->position() + offset, start);

    pathview->setPreferredHighlightBegin(0.8);
    pathview->setPreferredHighlightEnd(0.8);
    start = path->pointAt(0.8);
    QTRY_COMPARE(firstItem->position() + offset, start);
    QCOMPARE(pathview->currentIndex(), 0);

}

void tst_QQuickPathView::creationContext()
{
    QQuickView window;
    window.setGeometry(0,0,240,320);
    window.setSource(testFileUrl("creationContext.qml"));

    QQuickItem *rootItem = qobject_cast<QQuickItem *>(window.rootObject());
    QVERIFY(rootItem);
    QVERIFY(rootItem->property("count").toInt() > 0);

    QQuickItem *item;
    QVERIFY(item = findItem<QQuickItem>(rootItem, "listItem", 0));
    QCOMPARE(item->property("text").toString(), QString("Hello!"));
}

// QTBUG-21320
void tst_QQuickPathView::currentOffsetOnInsertion()
{
    QScopedPointer<QQuickView> window(createView());
    window->show();

    QaimModel model;

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("pathline.qml"));
    qApp->processEvents();

    QQuickPathView *pathview = findItem<QQuickPathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    pathview->setPreferredHighlightBegin(0.5);
    pathview->setPreferredHighlightEnd(0.5);

    QCOMPARE(pathview->count(), model.count());

    model.addItem("item0", "0");

    QCOMPARE(pathview->count(), model.count());

    QQuickRectangle *item = 0;
    QTRY_VERIFY(item = findItem<QQuickRectangle>(pathview, "wrapper", 0));

    QQuickPath *path = qobject_cast<QQuickPath*>(pathview->path());
    QVERIFY(path);

    QPointF start = path->pointAt(0.5);
    QPointF offset;//Center of item is at point, but pos is from corner
    offset.setX(item->width()/2);
    offset.setY(item->height()/2);
    QCOMPARE(item->position() + offset, start);

    QSignalSpy currentIndexSpy(pathview, SIGNAL(currentIndexChanged()));

    // insert an item at the beginning
    model.insertItem(0, "item1", "1");
    qApp->processEvents();

    QCOMPARE(currentIndexSpy.count(), 1);

    // currentIndex is now 1
    QVERIFY(item = findItem<QQuickRectangle>(pathview, "wrapper", 1));

    // verify that current item (item 1) is still at offset 0.5
    QCOMPARE(item->position() + offset, start);

    // insert another item at the beginning
    model.insertItem(0, "item2", "2");
    qApp->processEvents();

    QCOMPARE(currentIndexSpy.count(), 2);

    // currentIndex is now 2
    QVERIFY(item = findItem<QQuickRectangle>(pathview, "wrapper", 2));

    // verify that current item (item 2) is still at offset 0.5
    QCOMPARE(item->position() + offset, start);

    // verify that remove before current maintains current item
    model.removeItem(0);
    qApp->processEvents();

    QCOMPARE(currentIndexSpy.count(), 3);

    // currentIndex is now 1
    QVERIFY(item = findItem<QQuickRectangle>(pathview, "wrapper", 1));

    // verify that current item (item 1) is still at offset 0.5
    QCOMPARE(item->position() + offset, start);

}

void tst_QQuickPathView::asynchronous()
{
    QScopedPointer<QQuickView> window(createView());
    window->show();
    QQmlIncubationController controller;
    window->engine()->setIncubationController(&controller);

    window->setSource(testFileUrl("asyncloader.qml"));

    QQuickItem *rootObject = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(rootObject);

    QQuickPathView *pathview = 0;
    while (!pathview) {
        bool b = false;
        controller.incubateWhile(&b);
        pathview = rootObject->findChild<QQuickPathView*>("view");
    }

    // items will be created one at a time
    for (int i = 0; i < 5; ++i) {
        QVERIFY(findItem<QQuickItem>(pathview, "wrapper", i) == 0);
        QQuickItem *item = 0;
        while (!item) {
            bool b = false;
            controller.incubateWhile(&b);
            item = findItem<QQuickItem>(pathview, "wrapper", i);
        }
    }

    {
        bool b = true;
        controller.incubateWhile(&b);
    }

    // verify positioning
    QQuickRectangle *firstItem = findItem<QQuickRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QQuickPath *path = qobject_cast<QQuickPath*>(pathview->path());
    QVERIFY(path);
    QPointF start = path->pointAt(0.0);
    QPointF offset;//Center of item is at point, but pos is from corner
    offset.setX(firstItem->width()/2);
    offset.setY(firstItem->height()/2);
    QTRY_COMPARE(firstItem->position() + offset, start);
    pathview->setOffset(1.0);

    for (int i=0; i<5; i++) {
        QQuickItem *curItem = findItem<QQuickItem>(pathview, "wrapper", i);
        QPointF itemPos(path->pointAt(0.2 + i*0.2));
        QCOMPARE(curItem->position() + offset, itemPos);
    }

}

void tst_QQuickPathView::missingPercent()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("missingPercent.qml"));
    QQuickPath *obj = qobject_cast<QQuickPath*>(c.create());
    QVERIFY(obj);
    QCOMPARE(obj->attributeAt("_qfx_percent", 1.0), qreal(1.0));
    delete obj;
}

static inline bool hasFraction(qreal o)
{
    const bool result = o != qFloor(o);
    if (!result)
        qDebug() << "o != qFloor(o)" << o;
    return result;
}

void tst_QQuickPathView::cancelDrag()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("dragpath.qml"));
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    QSignalSpy draggingSpy(pathview, SIGNAL(draggingChanged()));
    QSignalSpy dragStartedSpy(pathview, SIGNAL(dragStarted()));
    QSignalSpy dragEndedSpy(pathview, SIGNAL(dragEnded()));

    // drag between snap points
    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(10,100));
    QTest::qWait(100);
    QTest::mouseMove(window.data(), QPoint(80, 100));
    QTest::mouseMove(window.data(), QPoint(130, 100));

    QTRY_VERIFY(hasFraction(pathview->offset()));
    QTRY_VERIFY(pathview->isMoving());
    QVERIFY(pathview->isDragging());
    QCOMPARE(draggingSpy.count(), 1);
    QCOMPARE(dragStartedSpy.count(), 1);
    QCOMPARE(dragEndedSpy.count(), 0);

    // steal mouse grab - cancels PathView dragging
    QQuickItem *item = window->rootObject()->findChild<QQuickItem*>("text");
    item->grabMouse();

    // returns to a snap point.
    QTRY_COMPARE(pathview->offset(), qreal(qFloor(pathview->offset())));
    QTRY_VERIFY(!pathview->isMoving());
    QVERIFY(!pathview->isDragging());
    QCOMPARE(draggingSpy.count(), 2);
    QCOMPARE(dragStartedSpy.count(), 1);
    QCOMPARE(dragEndedSpy.count(), 1);

    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(40,100));

}

void tst_QQuickPathView::maximumFlickVelocity()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("dragpath.qml"));
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    pathview->setMaximumFlickVelocity(700);
    flick(window.data(), QPoint(200,10), QPoint(10,10), 180);
    QVERIFY(pathview->isMoving());
    QVERIFY(pathview->isFlicking());
    QTRY_VERIFY_WITH_TIMEOUT(!pathview->isMoving(), 50000);

    double dist1 = 100 - pathview->offset();

    pathview->setOffset(0.);
    pathview->setMaximumFlickVelocity(300);
    flick(window.data(), QPoint(200,10), QPoint(10,10), 180);
    QVERIFY(pathview->isMoving());
    QVERIFY(pathview->isFlicking());
    QTRY_VERIFY_WITH_TIMEOUT(!pathview->isMoving(), 50000);

    double dist2 = 100 - pathview->offset();

    pathview->setOffset(0.);
    pathview->setMaximumFlickVelocity(500);
    flick(window.data(), QPoint(200,10), QPoint(10,10), 180);
    QVERIFY(pathview->isMoving());
    QVERIFY(pathview->isFlicking());
    QTRY_VERIFY_WITH_TIMEOUT(!pathview->isMoving(), 50000);

    double dist3 = 100 - pathview->offset();

    QVERIFY(dist1 > dist2);
    QVERIFY(dist3 > dist2);
    QVERIFY(dist2 < dist1);

}

void tst_QQuickPathView::snapToItem()
{
    QFETCH(bool, enforceRange);

    QScopedPointer<QQuickView> window(createView());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->setSource(testFileUrl("panels.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathview = window->rootObject()->findChild<QQuickPathView*>("view");
    QVERIFY(pathview != 0);

    window->rootObject()->setProperty("enforceRange", enforceRange);
    QTRY_VERIFY(!pathview->isMoving()); // ensure stable

    int currentIndex = pathview->currentIndex();

    QSignalSpy snapModeSpy(pathview, SIGNAL(snapModeChanged()));

    flick(window.data(), QPoint(200,10), QPoint(10,10), 180);

    QVERIFY(pathview->isMoving());
    QTRY_VERIFY_WITH_TIMEOUT(!pathview->isMoving(), 50000);

    QCOMPARE(pathview->offset(), qreal(qFloor(pathview->offset())));

    if (enforceRange)
        QVERIFY(pathview->currentIndex() != currentIndex);
    else
        QCOMPARE(pathview->currentIndex(), currentIndex);

}

void tst_QQuickPathView::snapToItem_data()
{
    QTest::addColumn<bool>("enforceRange");

    QTest::newRow("no enforce range") << false;
    QTest::newRow("enforce range") << true;
}

void tst_QQuickPathView::snapOneItem()
{
    QFETCH(bool, enforceRange);

    QScopedPointer<QQuickView> window(createView());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->setSource(testFileUrl("panels.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathview = window->rootObject()->findChild<QQuickPathView*>("view");
    QVERIFY(pathview != 0);

    window->rootObject()->setProperty("enforceRange", enforceRange);

    QSignalSpy snapModeSpy(pathview, SIGNAL(snapModeChanged()));

    window->rootObject()->setProperty("snapOne", true);
    QCOMPARE(snapModeSpy.count(), 1);
    QTRY_VERIFY(!pathview->isMoving()); // ensure stable

    int currentIndex = pathview->currentIndex();

    double startOffset = pathview->offset();
    flick(window.data(), QPoint(200,10), QPoint(10,10), 180);

    QVERIFY(pathview->isMoving());
    QTRY_VERIFY(!pathview->isMoving());

    // must have moved only one item
    QCOMPARE(pathview->offset(), fmodf(3.0 + startOffset - 1.0, 3.0));

    if (enforceRange)
        QCOMPARE(pathview->currentIndex(), currentIndex + 1);
    else
        QCOMPARE(pathview->currentIndex(), currentIndex);

}

void tst_QQuickPathView::snapOneItem_data()
{
    QTest::addColumn<bool>("enforceRange");

    QTest::newRow("no enforce range") << false;
    QTest::newRow("enforce range") << true;
}

void tst_QQuickPathView::positionViewAtIndex()
{
    QFETCH(bool, enforceRange);
    QFETCH(int, pathItemCount);
    QFETCH(qreal, initOffset);
    QFETCH(int, index);
    QFETCH(QQuickPathView::PositionMode, mode);
    QFETCH(qreal, offset);

    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("pathview3.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    window->rootObject()->setProperty("enforceRange", enforceRange);
    if (pathItemCount == -1)
        pathview->resetPathItemCount();
    else
        pathview->setPathItemCount(pathItemCount);
    pathview->setOffset(initOffset);

    pathview->positionViewAtIndex(index, mode);

    QCOMPARE(pathview->offset(), offset);

}

void tst_QQuickPathView::positionViewAtIndex_data()
{
    QTest::addColumn<bool>("enforceRange");
    QTest::addColumn<int>("pathItemCount");
    QTest::addColumn<qreal>("initOffset");
    QTest::addColumn<int>("index");
    QTest::addColumn<QQuickPathView::PositionMode>("mode");
    QTest::addColumn<qreal>("offset");

    QTest::newRow("no range, all items, Beginning") << false << -1 << 0.0 << 3 << QQuickPathView::Beginning << 5.0;
    QTest::newRow("no range, all items, Center") << false << -1 << 0.0 << 3 << QQuickPathView::Center << 1.0;
    QTest::newRow("no range, all items, End") << false << -1 << 0.0 << 3 << QQuickPathView::End << 5.0;
    QTest::newRow("no range, 5 items, Beginning") << false << 5 << 0.0 << 3 << QQuickPathView::Beginning << 5.0;
    QTest::newRow("no range, 5 items, Center") << false << 5 << 0.0 << 3 << QQuickPathView::Center << 7.5;
    QTest::newRow("no range, 5 items, End") << false << 5 << 0.0 << 3 << QQuickPathView::End << 2.0;
    QTest::newRow("no range, 5 items, Contain") << false << 5 << 0.0 << 3 << QQuickPathView::Contain << 0.0;
    QTest::newRow("no range, 5 items, init offset 4.0, Contain") << false << 5 << 4.0 << 3 << QQuickPathView::Contain << 5.0;
    QTest::newRow("no range, 5 items, init offset 3.0, Contain") << false << 5 << 3.0 << 3 << QQuickPathView::Contain << 2.0;

    QTest::newRow("strict range, all items, Beginning") << true << -1 << 0.0 << 3 << QQuickPathView::Beginning << 1.0;
    QTest::newRow("strict range, all items, Center") << true << -1 << 0.0 << 3 << QQuickPathView::Center << 5.0;
    QTest::newRow("strict range, all items, End") << true << -1 << 0.0 << 3 << QQuickPathView::End << 0.0;
    QTest::newRow("strict range, all items, Contain") << true << -1 << 0.0 << 3 << QQuickPathView::Contain << 0.0;
    QTest::newRow("strict range, all items, init offset 1.0, Contain") << true << -1 << 1.0 << 3 << QQuickPathView::Contain << 1.0;
    QTest::newRow("strict range, all items, SnapPosition") << true << -1 << 0.0 << 3 << QQuickPathView::SnapPosition << 5.0;
    QTest::newRow("strict range, 5 items, Beginning") << true << 5 << 0.0 << 3 << QQuickPathView::Beginning << 3.0;
    QTest::newRow("strict range, 5 items, Center") << true << 5 << 0.0 << 3 << QQuickPathView::Center << 5.0;
    QTest::newRow("strict range, 5 items, End") << true << 5 << 0.0 << 3 << QQuickPathView::End << 7.0;
    QTest::newRow("strict range, 5 items, Contain") << true << 5 << 0.0 << 3 << QQuickPathView::Contain << 7.0;
    QTest::newRow("strict range, 5 items, init offset 1.0, Contain") << true << 5 << 1.0 << 3 << QQuickPathView::Contain << 7.0;
    QTest::newRow("strict range, 5 items, init offset 2.0, Contain") << true << 5 << 2.0 << 3 << QQuickPathView::Contain << 3.0;
    QTest::newRow("strict range, 5 items, SnapPosition") << true << 5 << 0.0 << 3 << QQuickPathView::SnapPosition << 5.0;
}

void tst_QQuickPathView::indexAt_itemAt()
{
    QFETCH(qreal, x);
    QFETCH(qreal, y);
    QFETCH(int, index);

    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("pathview3.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    QQuickItem *item = 0;
    if (index >= 0) {
        item = findItem<QQuickItem>(pathview, "wrapper", index);
        QVERIFY(item);
    }
    QCOMPARE(pathview->indexAt(x,y), index);
    QCOMPARE(pathview->itemAt(x,y), item);

}

void tst_QQuickPathView::indexAt_itemAt_data()
{
    QTest::addColumn<qreal>("x");
    QTest::addColumn<qreal>("y");
    QTest::addColumn<int>("index");

    QTest::newRow("Item 0 - 585, 95") << qreal(585.) << qreal(95.) << 0;
    QTest::newRow("Item 0 - 660, 165") << qreal(660.) << qreal(165.) << 0;
    QTest::newRow("No Item a - 580, 95") << qreal(580.) << qreal(95.) << -1;
    QTest::newRow("No Item b - 585, 85") << qreal(585.) << qreal(85.) << -1;
    QTest::newRow("Item 7 - 360, 200") << qreal(360.) << qreal(200.) << 7;
}

void tst_QQuickPathView::cacheItemCount()
{
    QScopedPointer<QQuickView> window(createView());

    window->setSource(testFileUrl("pathview3.qml"));
    window->show();
    qApp->processEvents();

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    QMetaObject::invokeMethod(pathview, "addColor", Q_ARG(QVariant, QString("orange")));
    QMetaObject::invokeMethod(pathview, "addColor", Q_ARG(QVariant, QString("lightsteelblue")));
    QMetaObject::invokeMethod(pathview, "addColor", Q_ARG(QVariant, QString("teal")));
    QMetaObject::invokeMethod(pathview, "addColor", Q_ARG(QVariant, QString("aqua")));

    pathview->setOffset(0);

    pathview->setCacheItemCount(3);
    QCOMPARE(pathview->cacheItemCount(), 3);

    QQmlIncubationController controller;
    window->engine()->setIncubationController(&controller);

    // Items on the path are created immediately
    QVERIFY(findItem<QQuickItem>(pathview, "wrapper", 0));
    QVERIFY(findItem<QQuickItem>(pathview, "wrapper", 1));
    QVERIFY(findItem<QQuickItem>(pathview, "wrapper", 11));
    QVERIFY(findItem<QQuickItem>(pathview, "wrapper", 10));

    const int cached[] = { 2, 3, 9, -1 }; // two appended, one prepended

    int i = 0;
    while (cached[i] >= 0) {
        // items will be created one at a time
        QVERIFY(findItem<QQuickItem>(pathview, "wrapper", cached[i]) == 0);
        QQuickItem *item = 0;
        while (!item) {
            bool b = false;
            controller.incubateWhile(&b);
            item = findItem<QQuickItem>(pathview, "wrapper", cached[i]);
        }
        ++i;
    }

    {
        bool b = true;
        controller.incubateWhile(&b);
    }

    // move view and confirm items in view are visible immediately and outside are created async
    pathview->setOffset(4);

    // Items on the path are created immediately
    QVERIFY(findItem<QQuickItem>(pathview, "wrapper", 6));
    QVERIFY(findItem<QQuickItem>(pathview, "wrapper", 7));
    QVERIFY(findItem<QQuickItem>(pathview, "wrapper", 8));
    QVERIFY(findItem<QQuickItem>(pathview, "wrapper", 9));
    // already created items within cache stay created
    QVERIFY(findItem<QQuickItem>(pathview, "wrapper", 10));
    QVERIFY(findItem<QQuickItem>(pathview, "wrapper", 11));

    // one item prepended async.
    QVERIFY(findItem<QQuickItem>(pathview, "wrapper", 5) == 0);
    QQuickItem *item = 0;
    while (!item) {
        bool b = false;
        controller.incubateWhile(&b);
        item = findItem<QQuickItem>(pathview, "wrapper", 5);
    }

    {
        bool b = true;
        controller.incubateWhile(&b);
    }
}

static void testCurrentIndexChange(QQuickPathView *pathView, const QStringList &objectNamesInOrder)
{
    for (int visualIndex = 0; visualIndex < objectNamesInOrder.size() - 1; ++visualIndex) {
        QQuickRectangle *delegate = findItem<QQuickRectangle>(pathView, objectNamesInOrder.at(visualIndex));
        QVERIFY(delegate);

        QQuickRectangle *nextDelegate = findItem<QQuickRectangle>(pathView, objectNamesInOrder.at(visualIndex + 1));
        QVERIFY(nextDelegate);

        QVERIFY(delegate->y() < nextDelegate->y());
    }
}

void tst_QQuickPathView::changePathDuringRefill()
{
    QScopedPointer<QQuickView> window(createView());

    window->setSource(testFileUrl("changePathDuringRefill.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathView = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathView != 0);

    testCurrentIndexChange(pathView, QStringList() << "delegateC" << "delegateA" << "delegateB");

    pathView->incrementCurrentIndex();
    /*
        Decrementing moves delegateA down, resulting in an offset of 1,
        so incrementing will move it up, resulting in an offset of 2:

        delegateC    delegateA
        delegateA => delegateB
        delegateB    delegateC
    */
    QTRY_COMPARE(pathView->offset(), 2.0);
    testCurrentIndexChange(pathView, QStringList() << "delegateA" << "delegateB" << "delegateC");
}

void tst_QQuickPathView::nestedinFlickable()
{
    QScopedPointer<QQuickView> window(createView());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->setSource(testFileUrl("nestedInFlickable.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathview = findItem<QQuickPathView>(window->rootObject(), "pathView");
    QVERIFY(pathview != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(window->rootObject());
    QVERIFY(flickable != 0);

    QSignalSpy movingSpy(pathview, SIGNAL(movingChanged()));
    QSignalSpy moveStartedSpy(pathview, SIGNAL(movementStarted()));
    QSignalSpy moveEndedSpy(pathview, SIGNAL(movementEnded()));

    QSignalSpy fflickingSpy(flickable, SIGNAL(flickingChanged()));
    QSignalSpy fflickStartedSpy(flickable, SIGNAL(flickStarted()));
    QSignalSpy fflickEndedSpy(flickable, SIGNAL(flickEnded()));

    int waitInterval = 5;

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(23,218));

    QTest::mouseMove(window.data(), QPoint(25,218), waitInterval);
    QTest::mouseMove(window.data(), QPoint(26,218), waitInterval);
    QTest::mouseMove(window.data(), QPoint(28,219), waitInterval);
    QTest::mouseMove(window.data(), QPoint(31,219), waitInterval);
    QTest::mouseMove(window.data(), QPoint(53,219), waitInterval);

    // first move beyond threshold does not trigger drag
    QVERIFY(!pathview->isMoving());
    QVERIFY(!pathview->isDragging());
    QCOMPARE(movingSpy.count(), 0);
    QCOMPARE(moveStartedSpy.count(), 0);
    QCOMPARE(moveEndedSpy.count(), 0);
    QCOMPARE(fflickingSpy.count(), 0);
    QCOMPARE(fflickStartedSpy.count(), 0);
    QCOMPARE(fflickEndedSpy.count(), 0);

    // no further moves after the initial move beyond threshold
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(73,219));
    QTRY_COMPARE(movingSpy.count(), 2);
    QTRY_COMPARE(moveEndedSpy.count(), 1);
    QCOMPARE(moveStartedSpy.count(), 1);
    // Flickable should not handle this
    QCOMPARE(fflickingSpy.count(), 0);
    QCOMPARE(fflickStartedSpy.count(), 0);
    QCOMPARE(fflickEndedSpy.count(), 0);
}

void tst_QQuickPathView::flickableDelegate()
{
    QScopedPointer<QQuickView> window(createView());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->setSource(testFileUrl("flickableDelegate.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathview = qobject_cast<QQuickPathView*>(window->rootObject());
    QVERIFY(pathview != 0);

    QQuickFlickable *flickable = qobject_cast<QQuickFlickable*>(pathview->currentItem());
    QVERIFY(flickable != 0);

    QSignalSpy movingSpy(pathview, SIGNAL(movingChanged()));
    QSignalSpy moveStartedSpy(pathview, SIGNAL(movementStarted()));
    QSignalSpy moveEndedSpy(pathview, SIGNAL(movementEnded()));

    QSignalSpy fflickingSpy(flickable, SIGNAL(flickingChanged()));
    QSignalSpy fflickStartedSpy(flickable, SIGNAL(flickStarted()));
    QSignalSpy fflickEndedSpy(flickable, SIGNAL(flickEnded()));

    int waitInterval = 5;

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(23,100));

    QTest::mouseMove(window.data(), QPoint(25,100), waitInterval);
    QTest::mouseMove(window.data(), QPoint(26,100), waitInterval);
    QTest::mouseMove(window.data(), QPoint(28,100), waitInterval);
    QTest::mouseMove(window.data(), QPoint(31,100), waitInterval);
    QTest::mouseMove(window.data(), QPoint(39,100), waitInterval);

    // first move beyond threshold does not trigger drag
    QVERIFY(!flickable->isMoving());
    QVERIFY(!flickable->isDragging());
    QCOMPARE(movingSpy.count(), 0);
    QCOMPARE(moveStartedSpy.count(), 0);
    QCOMPARE(moveEndedSpy.count(), 0);
    QCOMPARE(fflickingSpy.count(), 0);
    QCOMPARE(fflickStartedSpy.count(), 0);
    QCOMPARE(fflickEndedSpy.count(), 0);

    // no further moves after the initial move beyond threshold
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(53,100));
    QTRY_COMPARE(fflickingSpy.count(), 2);
    QTRY_COMPARE(fflickStartedSpy.count(), 1);
    QCOMPARE(fflickEndedSpy.count(), 1);
    // PathView should not handle this
    QTRY_COMPARE(movingSpy.count(), 0);
    QTRY_COMPARE(moveEndedSpy.count(), 0);
    QCOMPARE(moveStartedSpy.count(), 0);
}

void tst_QQuickPathView::jsArrayChange()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.4; PathView {}", QUrl());

    QScopedPointer<QQuickPathView> view(qobject_cast<QQuickPathView *>(component.create()));
    QVERIFY(!view.isNull());

    QSignalSpy spy(view.data(), SIGNAL(modelChanged()));
    QVERIFY(spy.isValid());

    QJSValue array1 = engine.newArray(3);
    QJSValue array2 = engine.newArray(3);
    for (int i = 0; i < 3; ++i) {
        array1.setProperty(i, i);
        array2.setProperty(i, i);
    }

    view->setModel(QVariant::fromValue(array1));
    QCOMPARE(spy.count(), 1);

    // no change
    view->setModel(QVariant::fromValue(array2));
    QCOMPARE(spy.count(), 1);
}

void tst_QQuickPathView::qtbug37815()
{
    QScopedPointer<QQuickView> window(createView());

    window->setSource(testFileUrl("qtbug37815.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));

    // cache items will be created async. Let's wait...
    QTest::qWait(1000);

    QQuickPathView *pathView = findItem<QQuickPathView>(window->rootObject(), "pathView");
    QVERIFY(pathView != Q_NULLPTR);

    const int pathItemCount = pathView->pathItemCount();
    const int cacheItemCount = pathView->cacheItemCount();
    int totalCount = 0;
    foreach (QQuickItem *item, pathView->childItems()) {
        if (item->objectName().startsWith(QLatin1String("delegate")))
            ++totalCount;
    }
    QCOMPARE(pathItemCount + cacheItemCount, totalCount);
}

/* This bug was one where if you jump the list such that the sole missing item needed to be
 * added in the middle of the list, it would instead move an item somewhere else in the list
 * to the middle (messing it up very badly).
 *
 * The test checks correct visual order both before and after the jump.
 */
void tst_QQuickPathView::qtbug42716()
{
    QScopedPointer<QQuickView> window(createView());

    window->setSource(testFileUrl("qtbug42716.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathView = findItem<QQuickPathView>(window->rootObject(), "pathView");
    QVERIFY(pathView != 0);

    int order1[] = {5,6,7,0,1,2,3};
    int missing1 = 4;
    int order2[] = {0,1,2,3,4,5,6};
    int missing2 = 7;

    qreal lastY = 0.0;
    for (int i = 0; i<7; i++) {
        QQuickItem *item = findItem<QQuickItem>(pathView, QString("delegate%1").arg(order1[i]));
        QVERIFY(item);
        QVERIFY(item->y() > lastY);
        lastY = item->y();
    }
    QQuickItem *itemMiss = findItem<QQuickItem>(pathView, QString("delegate%1").arg(missing1));
    QVERIFY(!itemMiss);

    pathView->setOffset(0.0882353);
    //Note refill is delayed, needs time to take effect
    QTest::qWait(100);

    lastY = 0.0;
    for (int i = 0; i<7; i++) {
        QQuickItem *item = findItem<QQuickItem>(pathView, QString("delegate%1").arg(order2[i]));
        QVERIFY(item);
        QVERIFY(item->y() > lastY);
        lastY = item->y();
    }
    itemMiss = findItem<QQuickItem>(pathView, QString("delegate%1").arg(missing2));
    QVERIFY(!itemMiss);
}

void tst_QQuickPathView::qtbug53464()
{
    QScopedPointer<QQuickView> window(createView());

    window->setSource(testFileUrl("qtbug53464.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));

    QQuickPathView *pathView = findItem<QQuickPathView>(window->rootObject(), "pathView");
    QVERIFY(pathView != Q_NULLPTR);
    const int currentIndex = pathView->currentIndex();
    QCOMPARE(currentIndex, 8);

    const int pathItemCount = pathView->pathItemCount();
    int totalCount = 0;
    foreach (QQuickItem *item, pathView->childItems()) {
        if (item->objectName().startsWith(QLatin1String("delegate")))
            ++totalCount;
    }
    QCOMPARE(pathItemCount, totalCount);
}

void tst_QQuickPathView::addCustomAttribute()
{
    const QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("customAttribute.qml"));
    window->show();
}

void tst_QQuickPathView::movementDirection_data()
{
    QTest::addColumn<QQuickPathView::MovementDirection>("movementdirection");
    QTest::addColumn<int>("toidx");
    QTest::addColumn<qreal>("fromoffset");
    QTest::addColumn<qreal>("tooffset");

    QTest::newRow("default-shortest") << QQuickPathView::Shortest << 3 << 8.0 << 5.0;
    QTest::newRow("negative") << QQuickPathView::Negative << 2 << 0.0 << 6.0;
    QTest::newRow("positive") << QQuickPathView::Positive << 3 << 8.0 << 5.0;

}

static void verify_offsets(QQuickPathView *pathview, int toidx, qreal fromoffset, qreal tooffset)
{
    pathview->setCurrentIndex(toidx);
    bool started = false;
    qreal first, second;
    QTest::qWait(100);
    first = pathview->offset();
    while (1) {
        QTest::qWait(10); // highlightMoveDuration: 1000
        second = pathview->offset();
        if (!started && second != first) { // animation started
            started = true;
            break;
        }
    }

    if (tooffset > fromoffset) {
        QVERIFY(fromoffset <= first);
        QVERIFY(first <= second);
        QVERIFY(second <= tooffset);
    } else {
        QVERIFY(fromoffset >= first);
        QVERIFY(first >= second);
        QVERIFY(second >= tooffset);
    }
    QTRY_COMPARE(pathview->offset(), tooffset);
}

void tst_QQuickPathView::movementDirection()
{
    QFETCH(QQuickPathView::MovementDirection, movementdirection);
    QFETCH(int, toidx);
    QFETCH(qreal, fromoffset);
    QFETCH(qreal, tooffset);

    QScopedPointer<QQuickView> window(createView());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->setSource(testFileUrl("movementDirection.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QCOMPARE(window.data(), qGuiApp->focusWindow());

    QQuickPathView *pathview = window->rootObject()->findChild<QQuickPathView*>("view");
    QVERIFY(pathview != 0);
    QVERIFY(pathview->offset() == 0.0);
    QVERIFY(pathview->currentIndex() == 0);
    pathview->setMovementDirection(movementdirection);
    QVERIFY(pathview->movementDirection() == movementdirection);

    verify_offsets(pathview, toidx, fromoffset, tooffset);
}

QTEST_MAIN(tst_QQuickPathView)

#include "tst_qquickpathview.moc"
