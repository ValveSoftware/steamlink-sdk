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
#include <QtCore/qstringlistmodel.h>
#include <QtQuick/qquickview.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlexpression.h>
#include <QtQml/qqmlincubator.h>
#include <QtQml/qqmlcontext.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickitemview_p_p.h>
#include <QtQuick/private/qquickgridview_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQml/private/qqmllistmodel_p.h>
#include "../../shared/util.h"
#include "../shared/viewtestutil.h"
#include "../shared/visualtestutil.h"
#include <QtGui/qguiapplication.h>
#include "qplatformdefs.h"

Q_DECLARE_METATYPE(QQuickGridView::Flow)
Q_DECLARE_METATYPE(Qt::LayoutDirection)
Q_DECLARE_METATYPE(QQuickItemView::VerticalLayoutDirection)
Q_DECLARE_METATYPE(QQuickItemView::PositionMode)
Q_DECLARE_METATYPE(Qt::Key)

using namespace QQuickViewTestUtil;
using namespace QQuickVisualTestUtil;

#define SHARE_VIEWS

class tst_QQuickGridView : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_QQuickGridView();

private slots:
    void init();
    void cleanupTestCase();
    void items();
    void changed();
    void inserted_basic();
    void inserted_defaultLayout(QQuickGridView::Flow flow = QQuickGridView::FlowLeftToRight, Qt::LayoutDirection horizLayout = Qt::LeftToRight, QQuickItemView::VerticalLayoutDirection verticalLayout = QQuickItemView::TopToBottom);
    void inserted_defaultLayout_data();
    void insertBeforeVisible();
    void insertBeforeVisible_data();
    void removed_basic();
    void removed_defaultLayout(QQuickGridView::Flow flow = QQuickGridView::FlowLeftToRight, Qt::LayoutDirection horizLayout = Qt::LeftToRight, QQuickItemView::VerticalLayoutDirection verticalLayout = QQuickItemView::TopToBottom);
    void removed_defaultLayout_data();
    void addOrRemoveBeforeVisible();
    void addOrRemoveBeforeVisible_data();
    void clear();
    void moved_defaultLayout(QQuickGridView::Flow flow = QQuickGridView::FlowLeftToRight, Qt::LayoutDirection horizLayout = Qt::LeftToRight, QQuickItemView::VerticalLayoutDirection verticalLayout = QQuickItemView::TopToBottom);
    void moved_defaultLayout_data();
    void multipleChanges_condensed() { multipleChanges(true); }
    void multipleChanges_condensed_data() { multipleChanges_data(); }
    void multipleChanges_uncondensed() { multipleChanges(false); }
    void multipleChanges_uncondensed_data() { multipleChanges_data(); }
    void swapWithFirstItem();
    void changeFlow();
    void currentIndex();
    void noCurrentIndex();
    void keyNavigation();
    void keyNavigation_data();
    void defaultValues();
    void properties();
    void propertyChanges();
    void componentChanges();
    void modelChanges();
    void positionViewAtBeginningEnd();
    void positionViewAtIndex();
    void positionViewAtIndex_data();
    void mirroring();
    void snapping();
    void resetModel();
    void enforceRange();
    void enforceRange_rightToLeft();
    void QTBUG_8456();
    void manualHighlight();
    void footer();
    void footer_data();
    void initialZValues();
    void initialZValues_data();
    void header();
    void header_data();
    void extents();
    void extents_data();
    void resetModel_headerFooter();
    void resizeViewAndRepaint();
    void resizeGrid();
    void resizeGrid_data();
    void changeColumnCount();
    void indexAt_itemAt_data();
    void indexAt_itemAt();
    void onAdd();
    void onAdd_data();
    void onRemove();
    void onRemove_data();
    void attachedProperties_QTBUG_32836();
    void columnCount();
    void margins();
    void creationContext();
    void snapToRow_data();
    void snapToRow();
    void snapOneRow_data();
    void snapOneRow();
    void unaligned();
    void cacheBuffer();
    void asynchronous();
    void unrequestedVisibility();

    void populateTransitions();
    void populateTransitions_data();
    void addTransitions();
    void addTransitions_data();
    void moveTransitions();
    void moveTransitions_data();
    void removeTransitions();
    void removeTransitions_data();
    void displacedTransitions();
    void displacedTransitions_data();
    void multipleTransitions();
    void multipleTransitions_data();
    void multipleDisplaced();

    void inserted_leftToRight_RtL_TtB();
    void inserted_leftToRight_RtL_TtB_data();
    void inserted_leftToRight_LtR_BtT();
    void inserted_leftToRight_LtR_BtT_data();
    void inserted_leftToRight_RtL_BtT();
    void inserted_leftToRight_RtL_BtT_data();
    void inserted_topToBottom_LtR_TtB();
    void inserted_topToBottom_LtR_TtB_data();
    void inserted_topToBottom_RtL_TtB();
    void inserted_topToBottom_RtL_TtB_data();
    void inserted_topToBottom_LtR_BtT();
    void inserted_topToBottom_LtR_BtT_data();
    void inserted_topToBottom_RtL_BtT();
    void inserted_topToBottom_RtL_BtT_data();

    void removed_leftToRight_RtL_TtB();
    void removed_leftToRight_RtL_TtB_data();
    void removed_leftToRight_LtR_BtT();
    void removed_leftToRight_LtR_BtT_data();
    void removed_leftToRight_RtL_BtT();
    void removed_leftToRight_RtL_BtT_data();
    void removed_topToBottom_LtR_TtB();
    void removed_topToBottom_LtR_TtB_data();
    void removed_topToBottom_RtL_TtB();
    void removed_topToBottom_RtL_TtB_data();
    void removed_topToBottom_LtR_BtT();
    void removed_topToBottom_LtR_BtT_data();
    void removed_topToBottom_RtL_BtT();
    void removed_topToBottom_RtL_BtT_data();

    void moved_leftToRight_RtL_TtB();
    void moved_leftToRight_RtL_TtB_data();
    void moved_leftToRight_LtR_BtT();
    void moved_leftToRight_LtR_BtT_data();
    void moved_leftToRight_RtL_BtT();
    void moved_leftToRight_RtL_BtT_data();
    void moved_topToBottom_LtR_TtB();
    void moved_topToBottom_LtR_TtB_data();
    void moved_topToBottom_RtL_TtB();
    void moved_topToBottom_RtL_TtB_data();
    void moved_topToBottom_LtR_BtT();
    void moved_topToBottom_LtR_BtT_data();
    void moved_topToBottom_RtL_BtT();
    void moved_topToBottom_RtL_BtT_data();

    void displayMargin();
    void negativeDisplayMargin();

    void jsArrayChange();

    void contentHeightWithDelayRemove_data();
    void contentHeightWithDelayRemove();

    void QTBUG_45640();
    void QTBUG_48870_fastModelUpdates();

    void keyNavigationEnabled();

private:
    QList<int> toIntList(const QVariantList &list);
    void matchIndexLists(const QVariantList &indexLists, const QList<int> &expectedIndexes);
    void matchItemsAndIndexes(const QVariantMap &items, const QaimModel &model, const QList<int> &expectedIndexes);
    void matchItemLists(const QVariantList &itemLists, const QList<QQuickItem *> &expectedItems);

    void multipleChanges(bool condensed);
    void multipleChanges_data();

    QPointF expectedItemPos(QQuickGridView *grid, int index, qreal rowOffset = 0) {
        qreal x;
        qreal y;
        if (grid->flow() == QQuickGridView::FlowLeftToRight) {
            int columns = grid->width() / grid->cellWidth();
            x = (index % columns) * grid->cellWidth();
            y = (index / columns) * grid->cellHeight();
            if (grid->effectiveLayoutDirection() == Qt::RightToLeft) {
                int col = (index % columns) * grid->cellWidth();
                x = grid->cellWidth() * (columns - 1) - col;
            }

            qreal offset = grid->cellHeight() * rowOffset;
            if (grid->verticalLayoutDirection() == QQuickItemView::TopToBottom)
                y += offset;
            else
                y = -grid->cellHeight() - y - offset;
        } else {
            int rows = grid->height() / grid->cellHeight();
            x = (index / rows) * grid->cellWidth();
            y = (index % rows) * grid->cellHeight();
            if (grid->effectiveLayoutDirection() == Qt::RightToLeft)
                x = -x - grid->cellWidth();

            qreal offset = grid->cellWidth() * rowOffset;
            if (grid->effectiveLayoutDirection() == Qt::RightToLeft)
                x -= offset;
            else
                x += offset;
            if (grid->verticalLayoutDirection() == QQuickItemView::BottomToTop)
                y = -grid->cellHeight() - y;
        }
        return QPointF(x, y);
    }

    // Sets contentY (or contentX in TopToBottom flow) according to given row offset
    // (in LeftToRight flow) or col offset (in TopToBottom).
    bool setContentPos(QQuickGridView *gridview, qreal rowOrColOffset) {
        bool contentPosChanged = (rowOrColOffset != 0);
        qreal contentOffset = gridview->flow() == QQuickGridView::FlowLeftToRight
                ? rowOrColOffset * gridview->cellHeight()
                : rowOrColOffset * gridview->cellWidth();

        if (gridview->flow() == QQuickGridView::FlowLeftToRight) {
            if (gridview->verticalLayoutDirection() == QQuickItemView::BottomToTop)
                contentOffset = -gridview->height() - contentOffset;
        } else {
            if (gridview->effectiveLayoutDirection() == Qt::RightToLeft)
                contentOffset = -gridview->width() - contentOffset;
        }
        if (gridview->flow() == QQuickGridView::FlowLeftToRight)
            gridview->setContentY(contentOffset);
        else
            gridview->setContentX(contentOffset);
        return contentPosChanged;
    }

#ifdef SHARE_VIEWS
    QQuickView *getView() {
        if (m_view) {
            if (QString(QTest::currentTestFunction()) != testForView) {
                delete m_view;
                m_view = 0;
            } else {
                m_view->setSource(QUrl());
                return m_view;
            }
        }

        testForView = QTest::currentTestFunction();
        m_view = createView();
        return m_view;
    }
    void releaseView(QQuickView *view) {
        Q_ASSERT(view == m_view);
        Q_UNUSED(view)
        m_view->setSource(QUrl());
    }
#else
    QQuickView *getView() {
        return createView();
    }
    void releaseView(QQuickView *view) {
        delete view;
    }
#endif

    QQuickView *m_view;
    QString testForView;
};

tst_QQuickGridView::tst_QQuickGridView() : m_view(0)
{
}

void tst_QQuickGridView::init()
{
#ifdef SHARE_VIEWS
    if (m_view && QString(QTest::currentTestFunction()) != testForView) {
        testForView = QString();
        delete m_view;
        m_view = 0;
    }
#endif
}

void tst_QQuickGridView::cleanupTestCase()
{
#ifdef SHARE_VIEWS
    testForView = QString();
    delete m_view;
    m_view = 0;
#endif
}

void tst_QQuickGridView::items()
{
    QQuickView *window = createView();

    QaimModel model;
    model.addItem("Fred", "12345");
    model.addItem("John", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Billy", "22345");
    model.addItem("Sam", "2945");
    model.addItem("Ben", "04321");
    model.addItem("Jim", "0780");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("gridview1.qml"));
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QTRY_COMPARE(gridview->count(), model.count());
    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());
    QTRY_COMPARE(contentItem->childItems().count(), model.count()+1); // assumes all are visible, +1 for the (default) highlight item

    for (int i = 0; i < model.count(); ++i) {
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QTRY_VERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
        QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", i);
        QTRY_VERIFY(number != 0);
        QTRY_COMPARE(number->text(), model.number(i));
    }

    // set an empty model and confirm that items are destroyed
    QaimModel model2;
    ctxt->setContextProperty("testModel", &model2);

    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    QTRY_COMPARE(itemCount, 0);

    delete window;
}

void tst_QQuickGridView::changed()
{
    QQuickView *window = createView();

    QaimModel model;
    model.addItem("Fred", "12345");
    model.addItem("John", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Billy", "22345");
    model.addItem("Sam", "2945");
    model.addItem("Ben", "04321");
    model.addItem("Jim", "0780");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("gridview1.qml"));
    qApp->processEvents();

    QQuickFlickable *gridview = findItem<QQuickFlickable>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    model.modifyItem(1, "Will", "9876");
    QQuickText *name = findItem<QQuickText>(contentItem, "textName", 1);
    QTRY_VERIFY(name != 0);
    QTRY_COMPARE(name->text(), model.name(1));
    QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", 1);
    QTRY_VERIFY(number != 0);
    QTRY_COMPARE(number->text(), model.number(1));

    delete window;
}

void tst_QQuickGridView::inserted_basic()
{
    QQuickView *window = createView();
    window->show();

    QaimModel model;
    model.addItem("Fred", "12345");
    model.addItem("John", "2345");
    model.addItem("Bob", "54321");

    window->rootContext()->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("gridview1.qml"));
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    model.insertItem(1, "Will", "9876");

    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());
    QTRY_COMPARE(contentItem->childItems().count(), model.count()+1); // assumes all are visible, +1 for the (default) highlight item

    QQuickText *name = findItem<QQuickText>(contentItem, "textName", 1);
    QTRY_VERIFY(name != 0);
    QTRY_COMPARE(name->text(), model.name(1));
    QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", 1);
    QTRY_VERIFY(number != 0);
    QTRY_COMPARE(number->text(), model.number(1));

    // Checks that onAdd is called
    int added = window->rootObject()->property("added").toInt();
    QTRY_COMPARE(added, 1);

    // Confirm items positioned correctly
    for (int i = 0; i < model.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_COMPARE(item->x(), (i%3)*80.0);
        QTRY_COMPARE(item->y(), (i/3)*60.0);
    }

    model.insertItem(0, "Foo", "1111"); // zero index, and current item

    QTRY_COMPARE(contentItem->childItems().count(), model.count()+1); // assumes all are visible, +1 for the (default) highlight item

    name = findItem<QQuickText>(contentItem, "textName", 0);
    QTRY_VERIFY(name != 0);
    QTRY_COMPARE(name->text(), model.name(0));
    number = findItem<QQuickText>(contentItem, "textNumber", 0);
    QTRY_VERIFY(number != 0);
    QTRY_COMPARE(number->text(), model.number(0));

    QTRY_COMPARE(gridview->currentIndex(), 1);

    // Confirm items positioned correctly
    for (int i = 0; i < model.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_COMPARE(item->x(), qreal((i%3)*80));
        QTRY_COMPARE(item->y(), qreal((i/3)*60));
    }

    for (int i = model.count(); i < 30; ++i)
        model.insertItem(i, "Hello", QString::number(i));

    gridview->setContentY(120);

    // Insert item outside visible area
    model.insertItem(1, "Hello", "1324");

    QTRY_COMPARE(gridview->contentY(), qreal(120));

    delete window;
}

void tst_QQuickGridView::inserted_defaultLayout(QQuickGridView::Flow flow,
                                       Qt::LayoutDirection horizLayout,
                                       QQuickItemView::VerticalLayoutDirection verticalLayout)
{
    QFETCH(qreal, contentYRowOffset);
    QFETCH(int, insertIndex);
    QFETCH(int, insertCount);
    QFETCH(int, insertIndex_ttb);
    QFETCH(int, insertCount_ttb);
    QFETCH(qreal, rowOffsetAfterMove);

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testTopToBottom", flow == QQuickGridView::FlowTopToBottom);
    ctxt->setContextProperty("testRightToLeft", horizLayout == Qt::RightToLeft);
    ctxt->setContextProperty("testBottomToTop", verticalLayout == QQuickGridView::BottomToTop);
    window->setSource(testFileUrl("layouts.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    if (flow == QQuickGridView::FlowTopToBottom) {
        insertIndex = insertIndex_ttb;
        insertCount = insertCount_ttb;
    }
    if (setContentPos(gridview, contentYRowOffset))
        QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QList<QPair<QString, QString> > newData;
    for (int i=0; i<insertCount; i++)
        newData << qMakePair(QString("value %1").arg(i), QString::number(i));
    model.insertItems(insertIndex, newData);
    gridview->forceLayout();
    QTRY_COMPARE(gridview->property("count").toInt(), model.count());

    // check visibleItems.first() is in correct position
    QQuickItem *item0 = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item0);
    QPointF firstPos(0, 0);
    if (horizLayout == Qt::RightToLeft)
        firstPos.rx() = flow == QQuickGridView::FlowLeftToRight ? gridview->width() - gridview->cellWidth() : -gridview->cellWidth();
    if (verticalLayout == QQuickItemView::BottomToTop)
        firstPos.ry() -= gridview->cellHeight();
    QCOMPARE(item0->position(), firstPos);

    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    int firstVisibleIndex = -1;
    for (int i=0; i<items.count(); i++) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (item && delegateVisible(item)) {
            firstVisibleIndex = i;
            break;
        }
    }
    QVERIFY2(firstVisibleIndex >= 0, QTest::toString(firstVisibleIndex));

    // Confirm items positioned correctly and indexes correct
    for (int i = firstVisibleIndex; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->position(), expectedItemPos(gridview, i, rowOffsetAfterMove));
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QCOMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickGridView::inserted_defaultLayout_data()
{
    QTest::addColumn<qreal>("contentYRowOffset");
    QTest::addColumn<int>("insertIndex");
    QTest::addColumn<int>("insertCount");
    QTest::addColumn<int>("insertIndex_ttb");
    QTest::addColumn<int>("insertCount_ttb");
    QTest::addColumn<qreal>("rowOffsetAfterMove");

    QTest::newRow("add 1, before visible items")
            << 2.0     // show 6-23
            << 5 << 1
               << 9 << 1
            << 0.0;   // insert 1 above first visible, grid is rearranged; first visible moves forward within its row
                      // new 1st visible item is at 0

    QTest::newRow("add 2, before visible items")
            << 2.0     // show 6-23
            << 5 << 2
               << 9 << 2
            << 0.0;   // insert 2 above first visible, grid is rearranged; first visible moves forward within its row

    QTest::newRow("add 3, before visible items")
            << 2.0     // show 6-23
            << 5 << 3
               << 9 << 5
            << -1.0;   // insert 3 (1 row) above first visible in negative pos, first visible does not move

    QTest::newRow("add 5, before visible items")
            << 2.0     // show 6-23
            << 5 << 5
               << 9 << 7
            << -1.0;   // insert 1 row + 2 items above first visible, 1 row added at negative pos,
                        // grid is rearranged and first visible moves forward within its row

    QTest::newRow("add 6, before visible items")
            << 2.0     // show 6-23
            << 5 << 6
               << 9 << 10
            << -1.0 * 2;   // insert 2 rows above first visible in negative pos, first visible does not move



   QTest::newRow("add 1, at start of visible, content at start")
            << 0.0
            << 0 << 1
               << 0 << 1
            << 0.0;

    QTest::newRow("add multiple, at start of visible, content at start")
            << 0.0
            << 0 << 3
               << 0 << 5
            << 0.0;

    QTest::newRow("add 1, at start of visible, content not at start")
            << 2.0     // show 6-23
            << 6 << 1
               << 10 << 1
            << 0.0;

    QTest::newRow("add multiple, at start of visible, content not at start")
            << 2.0     // show 6-23
            << 6 << 3
               << 10 << 5
            << 0.0;


    QTest::newRow("add 1, at end of visible, content at start")
            << 0.0
            << 17 << 1
               << 14 << 1
            << 0.0;

    QTest::newRow("add row, at end of visible, content at start")
            << 0.0
            << 17 << 3
               << 14 << 5
            << 0.0;

    QTest::newRow("add 1, at end of visible, content not at start")
            << 2.0     // show 6-23
            << 17+6 << 1
               << 14+10 << 1
            << 0.0;

    QTest::newRow("add multiple, at end of visible, content not at start")
            << 2.0     // show 6-23
            << 17+6 << 3
               << 14+10 << 5
            << 0.0;


    QTest::newRow("add 1, after visible, content at start")
            << 0.0
            << 20 << 1
               << 18 << 1
            << 0.0;

    QTest::newRow("add row, after visible, content at start")
            << 0.0
            << 20 << 3
               << 18 << 5
            << 0.0;

    QTest::newRow("add 1, after visible, content not at start")
            << 2.0     // show 6-23
            << 20+6 << 1
               << 18+10 << 1
            << 0.0;

    QTest::newRow("add multiple, after visible, content not at start")
            << 2.0     // show 6-23
            << 20+6 << 3
               << 18+10 << 3
            << 0.0;
}

void tst_QQuickGridView::insertBeforeVisible()
{
    QFETCH(int, insertIndex);
    QFETCH(int, insertCount);
    QFETCH(int, cacheBuffer);

    QQuickText *name;
    QQuickView *window = getView();

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("gridview1.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    gridview->setCacheBuffer(cacheBuffer);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    // trigger a refill (not just setting contentY) so that the visibleItems grid is updated
    int firstVisibleIndex = 12;     // move to an index where the top item is not visible
    gridview->setContentY(firstVisibleIndex/3 * 60.0);
    gridview->setCurrentIndex(firstVisibleIndex);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QTRY_COMPARE(gridview->currentIndex(), firstVisibleIndex);
    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", firstVisibleIndex);
    QVERIFY(item);
    QCOMPARE(item->y(), gridview->contentY());

    QList<QPair<QString, QString> > newData;
    for (int i=0; i<insertCount; i++)
        newData << qMakePair(QString("value %1").arg(i), QString::number(i));
    model.insertItems(insertIndex, newData);
    gridview->forceLayout();
    QTRY_COMPARE(gridview->property("count").toInt(), model.count());

    // now, moving to the top of the view should position the inserted items correctly
    int itemsOffsetAfterMove = (insertCount / 3) * -60.0;
    gridview->setCurrentIndex(0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
    QTRY_COMPARE(gridview->currentIndex(), 0);
    QTRY_COMPARE(gridview->contentY(), 0.0 + itemsOffsetAfterMove);

    // Confirm items positioned correctly and indexes correct
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->x(), (i%3)*80.0);
        QCOMPARE(item->y(), (i/3)*60.0 + itemsOffsetAfterMove);
        name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickGridView::insertBeforeVisible_data()
{
    QTest::addColumn<int>("insertIndex");
    QTest::addColumn<int>("insertCount");
    QTest::addColumn<int>("cacheBuffer");

    QTest::newRow("insert 1 at 0, 0 buffer") << 0 << 1 << 0;
    QTest::newRow("insert 1 at 0, 100 buffer") << 0 << 1 << 100;
    QTest::newRow("insert 1 at 0, 500 buffer") << 0 << 1 << 500;

    QTest::newRow("insert 1 at 1, 0 buffer") << 1 << 1 << 0;
    QTest::newRow("insert 1 at 1, 100 buffer") << 1 << 1 << 100;
    QTest::newRow("insert 1 at 1, 500 buffer") << 1 << 1 << 500;

    QTest::newRow("insert multiple at 0, 0 buffer") << 0 << 6 << 0;
    QTest::newRow("insert multiple at 0, 100 buffer") << 0 << 6 << 100;
    QTest::newRow("insert multiple at 0, 500 buffer") << 0 << 6 << 500;

    QTest::newRow("insert multiple at 1, 0 buffer") << 1 << 6 << 0;
    QTest::newRow("insert multiple at 1, 100 buffer") << 1 << 6 << 100;
    QTest::newRow("insert multiple at 1, 500 buffer") << 1 << 6 << 500;
}

void tst_QQuickGridView::removed_basic()
{
    QQuickView *window = createView();
    window->show();

    QaimModel model;
    for (int i = 0; i < 40; i++)
        model.addItem("Item" + QString::number(i), "");

    window->rootContext()->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("gridview1.qml"));
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    model.removeItem(1);
    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());

    QQuickText *name = findItem<QQuickText>(contentItem, "textName", 1);
    QTRY_VERIFY(name != 0);
    QTRY_COMPARE(name->text(), model.name(1));
    QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", 1);
    QTRY_VERIFY(number != 0);
    QTRY_COMPARE(number->text(), model.number(1));


    // Checks that onRemove is called
    QString removed = window->rootObject()->property("removed").toString();
    QTRY_COMPARE(removed, QString("Item1"));

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_COMPARE(item->x(), qreal((i%3)*80));
        QTRY_COMPARE(item->y(), qreal((i/3)*60));
    }

    // Remove first item (which is the current item);
    model.removeItem(0);
    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());

    name = findItem<QQuickText>(contentItem, "textName", 0);
    QTRY_VERIFY(name != 0);
    QTRY_COMPARE(name->text(), model.name(0));
    number = findItem<QQuickText>(contentItem, "textNumber", 0);
    QTRY_VERIFY(number != 0);
    QTRY_COMPARE(number->text(), model.number(0));


    // Confirm items positioned correctly
    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_COMPARE(item->x(), qreal((i%3)*80));
        QTRY_COMPARE(item->y(), qreal((i/3)*60));
    }

    // Remove items not visible
    model.removeItem(25);
    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());

    // Confirm items positioned correctly
    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_COMPARE(item->x(), qreal((i%3)*80));
        QTRY_COMPARE(item->y(), qreal((i/3)*60));
    }

    // Remove items before visible
    gridview->setContentY(120);
    gridview->setCurrentIndex(10);

    // Setting currentIndex above shouldn't cause view to scroll
    QTRY_COMPARE(gridview->contentY(), 120.0);

    model.removeItem(1);
    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());

    // Confirm items positioned correctly
    for (int i = 6; i < 18; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_COMPARE(item->x(), qreal((i%3)*80));
        QTRY_COMPARE(item->y(), qreal((i/3)*60));
    }

    // Remove currentIndex
    QQuickItem *oldCurrent = gridview->currentItem();
    model.removeItem(9);
    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());

    QTRY_COMPARE(gridview->currentIndex(), 9);
    QTRY_VERIFY(gridview->currentItem() != oldCurrent);

    gridview->setContentY(0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    // Confirm items positioned correctly
    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_COMPARE(item->x(), qreal((i%3)*80));
        QTRY_COMPARE(item->y(), qreal((i/3)*60));
    }

    // remove item outside current view.
    gridview->setCurrentIndex(32);
    gridview->setContentY(240);

    model.removeItem(30);
    QTRY_COMPARE(gridview->currentIndex(), 31);

    // remove current item beyond visible items.
    gridview->setCurrentIndex(20);
    gridview->setContentY(0);
    model.removeItem(20);

    QTRY_COMPARE(gridview->currentIndex(), 20);
    QTRY_VERIFY(gridview->currentItem() != 0);

    // remove item before current, but visible
    gridview->setCurrentIndex(8);
    gridview->setContentY(240);
    oldCurrent = gridview->currentItem();
    model.removeItem(6);

    QTRY_COMPARE(gridview->currentIndex(), 7);
    QTRY_COMPARE(gridview->currentItem(), oldCurrent);

    delete window;
}

void tst_QQuickGridView::removed_defaultLayout(QQuickGridView::Flow flow,
                                               Qt::LayoutDirection horizLayout,
                                               QQuickItemView::VerticalLayoutDirection verticalLayout)
{
    QFETCH(qreal, contentYRowOffset);
    QFETCH(int, removeIndex);
    QFETCH(int, removeCount);
    QFETCH(int, removeIndex_ttb);
    QFETCH(int, removeCount_ttb);
    QFETCH(qreal, rowOffsetAfterMove);
    QFETCH(QString, firstVisible);
    QFETCH(QString, firstVisible_ttb);

    QQuickView *window = getView();

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testTopToBottom", flow == QQuickGridView::FlowTopToBottom);
    ctxt->setContextProperty("testRightToLeft", horizLayout == Qt::RightToLeft);
    ctxt->setContextProperty("testBottomToTop", verticalLayout == QQuickGridView::BottomToTop);
    window->setSource(testFileUrl("layouts.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    if (flow == QQuickGridView::FlowTopToBottom) {
        removeIndex = removeIndex_ttb;
        removeCount = removeCount_ttb;
        firstVisible = firstVisible_ttb;
    }
    if (setContentPos(gridview, contentYRowOffset))
        QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    model.removeItems(removeIndex, removeCount);
    gridview->forceLayout();
    QTRY_COMPARE(gridview->property("count").toInt(), model.count());

    QString firstName;
    int firstVisibleIndex = -1;
    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    QRectF viewRect(gridview->contentX(), gridview->contentY(), gridview->width(), gridview->height());
    for (int i=0; i<items.count(); i++) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (item) {
            QRectF itemRect(item->x(), item->y(), item->width(), item->height());
            if (delegateVisible(item) && viewRect.intersects(itemRect)) {
                firstVisibleIndex = i;
                QQmlExpression en(qmlContext(item), item, "name");
                firstName = en.evaluate().toString();
                break;
            }
        }
    }
    QVERIFY2(firstVisibleIndex >= 0, QTest::toString(firstVisibleIndex));
    QCOMPARE(firstName, firstVisible);

    // Confirm items positioned correctly and indexes correct
    for (int i = firstVisibleIndex; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->position(), expectedItemPos(gridview, i, rowOffsetAfterMove));
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickGridView::removed_defaultLayout_data()
{
    QTest::addColumn<qreal>("contentYRowOffset");
    QTest::addColumn<int>("removeIndex");
    QTest::addColumn<int>("removeCount");
    QTest::addColumn<int>("removeIndex_ttb");
    QTest::addColumn<int>("removeCount_ttb");
    QTest::addColumn<qreal>("rowOffsetAfterMove");
    QTest::addColumn<QString>("firstVisible");
    QTest::addColumn<QString>("firstVisible_ttb");

    QTest::newRow("remove 1, before visible items")
            << 2.0     // show 6-23
            << 2 << 1
               << 4 << 1
            << 0.0 << "Item7" << "Item11";

    QTest::newRow("remove 1, before visible position")
            << 2.0     // show 6-23
            << 3 << 1
               << 5 << 1
            << 0.0 << "Item7" << "Item11";

    QTest::newRow("remove multiple (1 row), all before visible items")
            << 2.0
            << 1 << 3
               << 1 << 5
            << 1.0 << "Item6" << "Item10";    // removed top row, slide down by 1 row

    QTest::newRow("remove multiple, all before visible items, remove item 0")
            << 2.0
            << 0 << 4
               << 0 << 6
            << 1.0 << "Item7" << "Item11";    // removed top row, slide down by 1 row

    QTest::newRow("remove multiple rows, all before visible items")
            << 4.0     // show 12-29
            << 1 << 7
               << 1 << 12
            << 2.0 << "Item13" << "Item17";

    QTest::newRow("remove one row before visible, content y not on item border")
            << 1.5
            << 0 << 3
               << 0 << 5
            << 1.0 << "Item3" << "Item5"; // 1 row removed

    QTest::newRow("remove mix of visible/non-visible")
            << 2.0     // show 6-23
            << 2 << 3
               << 4 << 3
            << 1.0 << "Item6" << "Item8"; // 1 row removed


    // remove 3,4,5 before the visible pos, first row moves down to just before the visible pos,
    // items 6,7 are removed from view, item 8 slides up to original pos of item 6 (120px)
    QTest::newRow("remove multiple, mix of items from before and within visible items")
            << 2.0
            << 3 << 5
                << 5 << 7
            << 1.0 << "Item8" << "Item12";    // adjust for the 1 row removed before the visible

    QTest::newRow("remove multiple, mix of items from before and within visible items, remove item 0")
            << 2.0
            << 0 << 8
               << 0 << 12
            << 1.0 * 2 << "Item8" << "Item12";    // adjust for the 2 rows removed before the visible


    QTest::newRow("remove 1, from start of visible, content at start")
            << 0.0
            << 0 << 1
               << 0 << 1
            << 0.0 << "Item1" << "Item1";

    QTest::newRow("remove multiple, from start of visible, content at start")
            << 0.0
            << 0 << 3
               << 0 << 5
            << 0.0 << "Item3" << "Item5";

    QTest::newRow("remove 1, from start of visible, content not at start")
            << 2.0     // show 6-23
            << 4 << 1
               << 7 << 1
            << 0.0 << "Item7" << "Item11";

    QTest::newRow("remove multiple, from start of visible, content not at start")
            << 2.0     // show 6-23
            << 4 << 3
               << 7 << 5
            << 0.0 << "Item9" << "Item15";


    QTest::newRow("remove 1, from middle of visible, content at start")
            << 0.0
            << 10 << 1
               << 12 << 1
            << 0.0 << "Item0" << "Item0";

    QTest::newRow("remove multiple, from middle of visible, content at start")
            << 0.0
            << 10 << 5
               << 12 << 5
            << 0.0 << "Item0" << "Item0";

    QTest::newRow("remove 1, from middle of visible, content not at start")
            << 2.0     // show 6-23
            << 10 << 1
               << 12 << 1
            << 0.0 << "Item6" << "Item10";

    QTest::newRow("remove multiple, from middle of visible, content not at start")
            << 2.0     // show 6-23
            << 10 << 5
               << 12 << 7
            << 0.0 << "Item6" << "Item10";


    QTest::newRow("remove 1, after visible, content at start")
            << 0.0
            << 16 << 1
               << 15 << 1
            << 0.0 << "Item0" << "Item0";

    QTest::newRow("remove multiple, after visible, content at start")
            << 0.0
            << 16 << 5
               << 15 << 7
            << 0.0 << "Item0" << "Item0";

    QTest::newRow("remove 1, after visible, content not at start")
            << 2.0     // show 6-23
            << 16+4 << 1
               << 15+10 << 1
            << 0.0 << "Item6" << "Item10";

    QTest::newRow("remove multiple, after visible, content not at start")
            << 2.0     // show 6-23
            << 16+4 << 5
               << 15+10 << 7
            << 0.0 << "Item6" << "Item10";

    QTest::newRow("remove multiple, mix of items from within and after visible items")
            << 2.0     // show 6-23
            << 20 << 5
               << 22 << 7
            << 0.0 << "Item6" << "Item10";
}

void tst_QQuickGridView::addOrRemoveBeforeVisible()
{
    // QTBUG-21588: ensure re-layout is done on grid after adding or removing
    // items from before the visible area

    QFETCH(bool, doAdd);
    QFETCH(qreal, newTopContentY);

    QQuickView *window = getView();
    window->show();

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    window->rootContext()->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("gridview1.qml"));

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQuickText *name = findItem<QQuickText>(contentItem, "textName", 0);
    QTRY_COMPARE(name->text(), QString("Item0"));

    gridview->setCurrentIndex(0);
    qApp->processEvents();
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    // scroll down until item 0 is no longer drawn
    // (bug not triggered if we just move using content y, since that doesn't
    // refill and change the visible items)
    gridview->setCurrentIndex(24);
    qApp->processEvents();

    QTRY_COMPARE(gridview->currentIndex(), 24);
    QTRY_COMPARE(gridview->contentY(), 220.0);

    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
    QTRY_VERIFY(!findItem<QQuickItem>(contentItem, "wrapper", 0));  // 0 shouldn't be visible

    if (doAdd) {
        model.insertItem(0, "New Item", "New Item number");
        QTRY_COMPARE(gridview->count(), 31);
    } else {
        model.removeItem(0);
        QTRY_COMPARE(gridview->count(), 29);
    }

    // scroll back up and item 0 should be gone
    gridview->setCurrentIndex(0);
    qApp->processEvents();
    QTRY_COMPARE(gridview->currentIndex(), 0);
    QTRY_COMPARE(gridview->contentY(), newTopContentY);

    name = findItem<QQuickText>(contentItem, "textName", 0);
    if (doAdd)
        QCOMPARE(name->text(), QString("New Item"));
    else
        QCOMPARE(name->text(), QString("Item1"));

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QTRY_VERIFY(findItem<QQuickItem>(contentItem, "wrapper", i));
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_COMPARE(item->x(), qreal((i%3)*80));
        QTRY_VERIFY(item->y() == (i/3)*60 + newTopContentY);
    }

    releaseView(window);
}

void tst_QQuickGridView::addOrRemoveBeforeVisible_data()
{
    QTest::addColumn<bool>("doAdd");
    QTest::addColumn<qreal>("newTopContentY");

    QTest::newRow("add") << true << -60.0;
    QTest::newRow("remove") << false << -60.0;
}

void tst_QQuickGridView::clear()
{
    QQuickView *window = createView();

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    window->rootContext()->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("gridview1.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QVERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    model.clear();
    gridview->forceLayout();

    QCOMPARE(gridview->count(), 0);
    QVERIFY(!gridview->currentItem());
    QCOMPARE(gridview->contentY(), qreal(0));
    QCOMPARE(gridview->currentIndex(), -1);
    QCOMPARE(gridview->contentHeight(), 0.0);

    // confirm sanity when adding an item to cleared list
    model.addItem("New", "1");
    gridview->forceLayout();
    QTRY_COMPARE(gridview->count(), 1);
    QVERIFY(gridview->currentItem() != 0);
    QCOMPARE(gridview->currentIndex(), 0);

    delete window;
}

void tst_QQuickGridView::moved_defaultLayout(QQuickGridView::Flow flow,
                                             Qt::LayoutDirection horizLayout,
                                             QQuickItemView::VerticalLayoutDirection verticalLayout)
{
    QFETCH(qreal, contentYRowOffset);
    QFETCH(int, from);
    QFETCH(int, to);
    QFETCH(int, count);
    QFETCH(int, from_ttb);
    QFETCH(int, to_ttb);
    QFETCH(int, count_ttb);
    QFETCH(qreal, rowOffsetAfterMove);

    QQuickView *window = getView();

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testTopToBottom", flow == QQuickGridView::FlowTopToBottom);
    ctxt->setContextProperty("testRightToLeft", horizLayout == Qt::RightToLeft);
    ctxt->setContextProperty("testBottomToTop", verticalLayout == QQuickGridView::BottomToTop);
    window->setSource(testFileUrl("layouts.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QQuickItem *currentItem = gridview->currentItem();
    QTRY_VERIFY(currentItem != 0);

    if (flow == QQuickGridView::FlowTopToBottom) {
        from = from_ttb;
        to = to_ttb;
        count = count_ttb;
    }
    if (setContentPos(gridview, contentYRowOffset))
        QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    model.moveItems(from, to, count);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    // Confirm items positioned correctly and indexes correct
    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    int firstVisibleIndex = -1;
    for (int i=0; i<items.count(); i++) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (item && delegateVisible(item)) {
            firstVisibleIndex = i;
            break;
        }
    }
    QVERIFY2(firstVisibleIndex >= 0, QTest::toString(firstVisibleIndex));

    for (int i = firstVisibleIndex; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item &&
                (  (flow == QQuickGridView::FlowLeftToRight && i >= firstVisibleIndex + (3*6))
                || (flow == QQuickGridView::FlowTopToBottom && i >= firstVisibleIndex + (5*3)) ) ) {
            continue;   // index has moved out of view
        }
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->position(), expectedItemPos(gridview, i, rowOffsetAfterMove));
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));

        // current index should have been updated
        if (item == currentItem)
            QTRY_COMPARE(gridview->currentIndex(), i);
    }

    releaseView(window);
}

void tst_QQuickGridView::moved_defaultLayout_data()
{
    QTest::addColumn<qreal>("contentYRowOffset");
    QTest::addColumn<int>("from");
    QTest::addColumn<int>("to");
    QTest::addColumn<int>("count");
    QTest::addColumn<int>("from_ttb");
    QTest::addColumn<int>("to_ttb");
    QTest::addColumn<int>("count_ttb");
    QTest::addColumn<qreal>("rowOffsetAfterMove");

    // model starts with 30 items, each 80x60, in area 240x320
    // 18 items should be visible at a time

    // The first visible item should not move upwards and out of the view
    // if items are moved/removed before it.


    QTest::newRow("move 1 forwards, within visible items")
            << 0.0
            << 1 << 8 << 1
               << 2 << 12 << 1
            << 0.0;

    QTest::newRow("move 1 forwards, from non-visible -> visible")
            << 2.0     // show 6-23
            << 1 << 8+6 << 1
               << 2 << 12+10 << 1
            << 0.0;

    QTest::newRow("move 1 forwards, from non-visible -> visible (move first item)")
            << 2.0     // // show 6-23
            << 0 << 6 << 1
               << 0 << 10 << 1
            << 0.0;

    QTest::newRow("move 1 forwards, from visible -> non-visible")
            << 0.0
            << 1 << 20 << 1
               << 1 << 20 << 1
            << 0.0;

    QTest::newRow("move 1 forwards, from visible -> non-visible (move first item)")
            << 0.0
            << 0 << 20 << 1
               << 0 << 20 << 1
            << 0.0;


    QTest::newRow("move 1 backwards, within visible items")
            << 0.0
            << 10 << 5 << 1
               << 10 << 5 << 1
            << 0.0;

    QTest::newRow("move 1 backwards, within visible items (to first index)")
            << 0.0
            << 10 << 0 << 1
               << 10 << 0 << 1
            << 0.0;

    QTest::newRow("move 1 backwards, from non-visible -> visible")
            << 0.0
            << 28 << 8 << 1
               << 28 << 8 << 1
            << 0.0;

    QTest::newRow("move 1 backwards, from non-visible -> visible (move last item)")
            << 0.0
            << 29 << 14 << 1
               << 29 << 14 << 1
            << 0.0;

    QTest::newRow("move 1 backwards, from visible -> non-visible")
            << 2.0     // show 6-23
            << 7 << 1 << 1
               << 10 << 1 << 1
            << 0.0;     // only 1 item moved back, so items shift accordingly and first row doesn't move

    QTest::newRow("move 1 backwards, from visible -> non-visible (move first item)")
            << 2.0     // show 6-23
            << 7 << 0 << 1
               << 10 << 0 << 1
            << 0.0;     // only 1 item moved back, so items shift accordingly and first row doesn't move


    QTest::newRow("move multiple forwards, within visible items")
            << 0.0
            << 0 << 5 << 3
               << 0 << 7 << 5
            << 0.0;

    QTest::newRow("move multiple backwards, within visible items (move first item)")
            << 0.0
            << 10 << 0 << 3
               << 12 << 0 << 5
            << 0.0;

    QTest::newRow("move multiple forwards, before visible items")
            << 2.0     // show 6-23
            << 3 << 4 << 3      // 3, 4, 5 move to after 6
               << 5 << 6 << 5
            << 1.0;      // row of 3,4,5 has moved down

    QTest::newRow("move multiple forwards, from non-visible -> visible")
            << 2.0     // show 6-23
            << 1 << 6 << 3
               << 1 << 10 << 5
            << 1.0; // 1st row (it's above visible area) disappears, 0 drops down 1 row, first visible item (6) stays where it is

    QTest::newRow("move multiple forwards, from non-visible -> visible (move first item)")
            << 2.0     // show 6-23
            << 0 << 6 << 3
               << 0 << 10 << 5
            << 1.0;    // top row moved and shifted to below 3rd row, all items should shift down by 1 row

    QTest::newRow("move multiple forwards, mix of non-visible/visible")
            << 2.0
            << 3 << 16 << 6
               << 5 << 18 << 10
            << 1.0;    // top two rows removed, third row is now the first visible

    QTest::newRow("move multiple forwards, to bottom of view")
            << 0.0
            << 5 << 13 << 5
               << 1 << 8 << 6
            << 0.0;

    QTest::newRow("move multiple forwards, to bottom of view, first row -> last")
            << 0.0
            << 0 << 15 << 3
               << 0 << 10 << 5
            << 0.0;

    QTest::newRow("move multiple forwards, to bottom of view, content y not 0")
            << 2.0
            << 5+4 << 13+4 << 5
               << 11 << 19 << 6
            << 0.0;

    QTest::newRow("move multiple forwards, from visible -> non-visible")
            << 0.0
            << 1 << 16 << 3
               << 1 << 18 << 5
            << 0.0;

    QTest::newRow("move multiple forwards, from visible -> non-visible (move first item)")
            << 0.0
            << 0 << 16 << 3
               << 1 << 18 << 5
            << 0.0;


    QTest::newRow("move multiple backwards, within visible items")
            << 0.0
            << 4 << 1 << 3
               << 7 << 1 << 5
            << 0.0;

    QTest::newRow("move multiple backwards, from non-visible -> visible")
            << 0.0
            << 20 << 4 << 3
               << 20 << 4 << 5
            << 0.0;

    QTest::newRow("move multiple backwards, from non-visible -> visible (move last item)")
            << 0.0
            << 27 << 10 << 3
               << 25 << 8 << 5
            << 0.0;

    QTest::newRow("move multiple backwards, from visible -> non-visible")
            << 2.0     // show 6-23
            << 16 << 1 << 3
               << 17 << 1 << 5
            << -1.0;   // to minimize movement, items are added above visible area, all items move up by 1 row

    QTest::newRow("move multiple backwards, from visible -> non-visible (move first item)")
            << 2.0     // show 6-23
            << 16 << 0 << 3
               << 17 << 0 << 5
            << -1.0;   // 16,17,18 move to above item 0, all items move up by 1 row
}

void tst_QQuickGridView::multipleChanges(bool condensed)
{
    QFETCH(int, startCount);
    QFETCH(QList<ListChange>, changes);
    QFETCH(int, newCount);
    QFETCH(int, newCurrentIndex);

    QQuickView *window = getView();

    QaimModel model;
    for (int i = 0; i < startCount; i++)
        model.addItem("Item" + QString::number(i), "");

    window->rootContext()->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("gridview1.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

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
                gridview->setCurrentIndex(changes[i].index);
                break;
            case ListChange::SetContentY:
                gridview->setContentY(changes[i].pos);
                break;
            case ListChange::Polish:
                break;
        }
        if (condensed) {
            QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
        }
    }
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QCOMPARE(gridview->count(), newCount);
    QCOMPARE(gridview->count(), model.count());
    QCOMPARE(gridview->currentIndex(), newCurrentIndex);

    QQuickText *name;
    QQuickText *number;
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i=0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
        number = findItem<QQuickText>(contentItem, "textNumber", i);
        QVERIFY(number != 0);
        QTRY_COMPARE(number->text(), model.number(i));
    }

    releaseView(window);
}

void tst_QQuickGridView::multipleChanges_data()
{
    QTest::addColumn<int>("startCount");
    QTest::addColumn<QList<ListChange> >("changes");
    QTest::addColumn<int>("newCount");
    QTest::addColumn<int>("newCurrentIndex");

    QList<ListChange> changes;

    for (int i=1; i<30; i++)
        changes << ListChange::remove(0);
    QTest::newRow("remove all but 1, first->last") << 30 << changes << 1 << 0;

    changes << ListChange::remove(0);
    QTest::newRow("remove all") << 30 << changes << 0 << -1;

    changes.clear();
    changes << ListChange::setCurrent(29);
    for (int i=29; i>0; i--)
        changes << ListChange::remove(i);
    QTest::newRow("remove last (current) -> first") << 30 << changes << 1 << 0;

    QTest::newRow("remove then insert at 0") << 10 << (QList<ListChange>()
            << ListChange::remove(0, 1)
            << ListChange::insert(0, 1)
            ) << 10 << 1;

    QTest::newRow("remove then insert at non-zero index") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(2)
            << ListChange::remove(2, 1)
            << ListChange::insert(2, 1)
            ) << 10 << 3;

    QTest::newRow("remove current then insert below it") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(1)
            << ListChange::remove(1, 3)
            << ListChange::insert(2, 2)
            ) << 9 << 1;

    QTest::newRow("remove current index then move it down") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(2)
            << ListChange::remove(1, 3)
            << ListChange::move(1, 5, 1)
            ) << 7 << 5;

    QTest::newRow("remove current index then move it up") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(5)
            << ListChange::remove(4, 3)
            << ListChange::move(4, 1, 1)
            ) << 7 << 1;


    QTest::newRow("insert multiple times") << 0 << (QList<ListChange>()
            << ListChange::insert(0, 2)
            << ListChange::insert(0, 4)
            << ListChange::insert(0, 6)
            ) << 12 << 10;

    QTest::newRow("insert multiple times with current index changes") << 0 << (QList<ListChange>()
            << ListChange::insert(0, 2)
            << ListChange::insert(0, 4)
            << ListChange::insert(0, 6)
            << ListChange::setCurrent(3)
            << ListChange::insert(3, 2)
            ) << 14 << 5;

    QTest::newRow("insert and remove all") << 0 << (QList<ListChange>()
            << ListChange::insert(0, 30)
            << ListChange::remove(0, 30)
            ) << 0 << -1;

    QTest::newRow("insert and remove current") << 30 << (QList<ListChange>()
            << ListChange::insert(1)
            << ListChange::setCurrent(1)
            << ListChange::remove(1)
            ) << 30 << 1;

    QTest::newRow("insert before 0, then remove cross section of new and old items") << 10 << (QList<ListChange>()
            << ListChange::insert(0, 10)
            << ListChange::remove(5, 10)
            ) << 10 << 5;

    QTest::newRow("insert multiple, then move new items to end") << 10 << (QList<ListChange>()
            << ListChange::insert(0, 3)
            << ListChange::move(0, 10, 3)
            ) << 13 << 0;

    QTest::newRow("insert multiple, then move new and some old items to end") << 10 << (QList<ListChange>()
            << ListChange::insert(0, 3)
            << ListChange::move(0, 8, 5)
            ) << 13 << 11;

    QTest::newRow("insert multiple at end, then move new and some old items to start") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(9)
            << ListChange::insert(10, 3)
            << ListChange::move(8, 0, 5)
            ) << 13 << 1;


    QTest::newRow("move back and forth to same index") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(1)
            << ListChange::move(1, 2, 2)
            << ListChange::move(2, 1, 2)
            ) << 10 << 1;

    QTest::newRow("move forwards then back") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(2)
            << ListChange::move(1, 2, 3)
            << ListChange::move(3, 0, 5)
            ) << 10 << 0;

    QTest::newRow("move current, then remove it") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(5)
            << ListChange::move(5, 0, 1)
            << ListChange::remove(0)
            ) << 9 << 0;

    QTest::newRow("move current, then insert before it") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(5)
            << ListChange::move(5, 0, 1)
            << ListChange::insert(0)
            ) << 11 << 1;

    QTest::newRow("move multiple, then remove them") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(1)
            << ListChange::move(5, 1, 3)
            << ListChange::remove(1, 3)
            ) << 7 << 1;

    QTest::newRow("move multiple, then insert before them") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(5)
            << ListChange::move(5, 1, 3)
            << ListChange::insert(1, 5)
            ) << 15 << 6;

    QTest::newRow("move multiple, then insert after them") << 10 << (QList<ListChange>()
            << ListChange::setCurrent(3)
            << ListChange::move(0, 1, 2)
            << ListChange::insert(3, 5)
            ) << 15 << 8;


    QTest::newRow("clear current") << 0 << (QList<ListChange>()
            << ListChange::insert(0, 5)
            << ListChange::setCurrent(-1)
            << ListChange::remove(0, 5)
            << ListChange::insert(0, 5)
            ) << 5 << -1;

    QTest::newRow("remove, scroll") << 30 << (QList<ListChange>()
            << ListChange::remove(20, 5)
            << ListChange::setContentY(20)
            ) << 25 << 0;

    QTest::newRow("insert, scroll") << 10 << (QList<ListChange>()
            << ListChange::insert(9, 5)
            << ListChange::setContentY(20)
            ) << 15 << 0;

    QTest::newRow("move, scroll") << 20 << (QList<ListChange>()
            << ListChange::move(15, 8, 3)
            << ListChange::setContentY(0)
            ) << 20 << 0;

    QTest::newRow("clear, insert, scroll") << 30 << (QList<ListChange>()
            << ListChange::setContentY(20)
            << ListChange::remove(0, 30)
            << ListChange::insert(0, 2)
            << ListChange::setContentY(0)
            ) << 2 << 0;
}


void tst_QQuickGridView::swapWithFirstItem()
{
    // QTBUG_9697
    QQuickView *window = createView();

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    window->rootContext()->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("gridview1.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    // ensure content position is stable
    gridview->setContentY(0);
    model.moveItem(10, 0);
    QTRY_COMPARE(gridview->contentY(), qreal(0));

    delete window;
}

void tst_QQuickGridView::currentIndex()
{
    QaimModel initModel;
    for (int i = 0; i < 60; i++)
        initModel.addItem("Item" + QString::number(i), QString::number(i));

    QQuickView *window = new QQuickView(0);
    window->setGeometry(0,0,240,320);
    window->show();

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &initModel);

    QString filename(testFile("gridview-initCurrent.qml"));
    window->setSource(QUrl::fromLocalFile(filename));

    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QVERIFY(gridview != 0);
    QTRY_VERIFY(!QQuickItemPrivate::get(gridview)->polishScheduled);

    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);

    // currentIndex is initialized to 35
    // currentItem should be in view
    QCOMPARE(gridview->currentIndex(), 35);
    QCOMPARE(gridview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 35));
    QCOMPARE(gridview->currentItem()->y(), gridview->highlightItem()->y());
    QCOMPARE(gridview->contentY(), 400.0);

    // changing model should reset currentIndex to 0
    QaimModel model;
    for (int i = 0; i < 60; i++)
        model.addItem("Item" + QString::number(i), QString::number(i));
    ctxt->setContextProperty("testModel", &model);

    QCOMPARE(gridview->currentIndex(), 0);
    QCOMPARE(gridview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 0));

    // confirm that the velocity is updated
    gridview->setCurrentIndex(35);
    QTRY_VERIFY(gridview->verticalVelocity() != 0.0);
    gridview->setCurrentIndex(0);
    QTRY_COMPARE(gridview->verticalVelocity(), 0.0);

    // footer should become visible if it is out of view, and then current index moves to the first row
    window->rootObject()->setProperty("showFooter", true);
    QTRY_VERIFY(gridview->footerItem());
    gridview->setCurrentIndex(model.count()-3);
    QTRY_VERIFY(gridview->footerItem()->y() > gridview->contentY() + gridview->height());
    gridview->setCurrentIndex(model.count()-2);
    QTRY_COMPARE(gridview->contentY() + gridview->height(), (60.0 * model.count()/3) + gridview->footerItem()->height());
    window->rootObject()->setProperty("showFooter", false);

    // header should become visible if it is out of view, and then current index moves to the last row
    window->rootObject()->setProperty("showHeader", true);
    QTRY_VERIFY(gridview->headerItem());
    gridview->setCurrentIndex(3);
    QTRY_VERIFY(gridview->headerItem()->y() + gridview->headerItem()->height() < gridview->contentY());
    gridview->setCurrentIndex(1);
    QTRY_COMPARE(gridview->contentY(), -gridview->headerItem()->height());
    window->rootObject()->setProperty("showHeader", false);

    // turn off auto highlight
    gridview->setHighlightFollowsCurrentItem(false);
    QVERIFY(!gridview->highlightFollowsCurrentItem());
    QVERIFY(gridview->highlightItem());
    qreal hlPosX = gridview->highlightItem()->x();
    qreal hlPosY = gridview->highlightItem()->y();

    gridview->setCurrentIndex(5);
    QTRY_COMPARE(gridview->highlightItem()->x(), hlPosX);
    QTRY_COMPARE(gridview->highlightItem()->y(), hlPosY);

    // insert item before currentIndex
    window->rootObject()->setProperty("currentItemChangedCount", QVariant(0));
    gridview->setCurrentIndex(28);
    QTRY_COMPARE(window->rootObject()->property("currentItemChangedCount").toInt(), 1);
    model.insertItem(0, "Foo", "1111");
    QTRY_COMPARE(window->rootObject()->property("current").toInt(), 29);
    QCOMPARE(window->rootObject()->property("currentItemChangedCount").toInt(), 1);

    // check removing highlight by setting currentIndex to -1;
    gridview->setCurrentIndex(-1);

    QCOMPARE(gridview->currentIndex(), -1);
    QVERIFY(!gridview->highlightItem());
    QVERIFY(!gridview->currentItem());

    // moving currentItem out of view should make it invisible
    gridview->setCurrentIndex(0);
    QTRY_VERIFY(delegateVisible(gridview->currentItem()));
    gridview->setContentY(200);
    QTRY_VERIFY(!delegateVisible(gridview->currentItem()));

    delete window;
}

void tst_QQuickGridView::noCurrentIndex()
{
    QaimModel model;
    for (int i = 0; i < 60; i++)
        model.addItem("Item" + QString::number(i), QString::number(i));

    QQuickView *window = new QQuickView(0);
    window->setGeometry(0,0,240,320);

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    QString filename(testFile("gridview-noCurrent.qml"));
    window->setSource(QUrl::fromLocalFile(filename));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QVERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    // current index should be -1
    QCOMPARE(gridview->currentIndex(), -1);
    QVERIFY(!gridview->currentItem());
    QVERIFY(!gridview->highlightItem());
    QCOMPARE(gridview->contentY(), 0.0);

    gridview->setCurrentIndex(5);
    QCOMPARE(gridview->currentIndex(), 5);
    QVERIFY(gridview->currentItem());
    QVERIFY(gridview->highlightItem());

    delete window;
}

void tst_QQuickGridView::keyNavigation()
{
    QFETCH(QQuickGridView::Flow, flow);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(Qt::Key, forwardsKey);
    QFETCH(Qt::Key, backwardsKey);
    QFETCH(QPointF, contentPosAtFirstItem);
    QFETCH(QPointF, contentPosAtLastItem);
    QFETCH(int, indexRightOf7);
    QFETCH(int, indexLeftOf7);
    QFETCH(int, indexUpFrom7);
    QFETCH(int, indexDownFrom7);

    QaimModel model;
    for (int i = 0; i < 18; i++)
        model.addItem("Item" + QString::number(i), "");

    QQuickView *window = getView();
    window->rootContext()->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("gridview1.qml"));
    window->show();
    QTest::qWaitForWindowActive(window);

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    gridview->setFlow(flow);
    gridview->setLayoutDirection(layoutDirection);
    gridview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    window->requestActivate();
    QTest::qWaitForWindowActive(window);
    QTRY_COMPARE(qGuiApp->focusWindow(), window);
    QCOMPARE(gridview->currentIndex(), 0);

    QTest::keyClick(window, forwardsKey);
    QCOMPARE(gridview->currentIndex(), 1);

    QTest::keyClick(window, backwardsKey);
    QCOMPARE(gridview->currentIndex(), 0);

    gridview->setCurrentIndex(7);
    gridview->moveCurrentIndexRight();
    QCOMPARE(gridview->currentIndex(), indexRightOf7);
    gridview->moveCurrentIndexLeft();
    QCOMPARE(gridview->currentIndex(), 7);
    gridview->moveCurrentIndexLeft();
    QCOMPARE(gridview->currentIndex(), indexLeftOf7);
    gridview->moveCurrentIndexRight();
    QCOMPARE(gridview->currentIndex(), 7);
    gridview->moveCurrentIndexUp();
    QCOMPARE(gridview->currentIndex(), indexUpFrom7);
    gridview->moveCurrentIndexDown();
    QCOMPARE(gridview->currentIndex(), 7);
    gridview->moveCurrentIndexDown();
    QCOMPARE(gridview->currentIndex(), indexDownFrom7);

    gridview->setCurrentIndex(7);
    QTest::keyClick(window, Qt::Key_Right);
    QCOMPARE(gridview->currentIndex(), indexRightOf7);
    QTest::keyClick(window, Qt::Key_Left);
    QCOMPARE(gridview->currentIndex(), 7);
    QTest::keyClick(window, Qt::Key_Left);
    QCOMPARE(gridview->currentIndex(), indexLeftOf7);
    QTest::keyClick(window, Qt::Key_Right);
    QCOMPARE(gridview->currentIndex(), 7);
    QTest::keyClick(window, Qt::Key_Up);
    QCOMPARE(gridview->currentIndex(), indexUpFrom7);
    QTest::keyClick(window, Qt::Key_Down);
    QCOMPARE(gridview->currentIndex(), 7);
    QTest::keyClick(window, Qt::Key_Down);
    QCOMPARE(gridview->currentIndex(), indexDownFrom7);

    // hold down a key to go forwards
    gridview->setCurrentIndex(0);
    for (int i=0; i<model.count()-1; i++) {
//        QTest::qWait(500);
        QTest::simulateEvent(window, true, forwardsKey, Qt::NoModifier, "", true);
        QTRY_COMPARE(gridview->currentIndex(), i+1);
    }
    QTest::keyRelease(window, forwardsKey);
    QTRY_COMPARE(gridview->currentIndex(), model.count()-1);
    QTRY_COMPARE(gridview->contentX(), contentPosAtLastItem.x());
    QTRY_COMPARE(gridview->contentY(), contentPosAtLastItem.y());

    // hold down a key to go backwards
    for (int i=model.count()-1; i > 0; i--) {
        QTest::simulateEvent(window, true, backwardsKey, Qt::NoModifier, "", true);
        QTRY_COMPARE(gridview->currentIndex(), i-1);
    }
    QTest::keyRelease(window, backwardsKey);
    QTRY_COMPARE(gridview->currentIndex(), 0);
    QTRY_COMPARE(gridview->contentX(), contentPosAtFirstItem.x());
    QTRY_COMPARE(gridview->contentY(), contentPosAtFirstItem.y());

    // no wrap
    QVERIFY(!gridview->isWrapEnabled());
    QTest::keyClick(window, forwardsKey);
    QCOMPARE(gridview->currentIndex(), 1);
    QTest::keyClick(window, backwardsKey);
    QCOMPARE(gridview->currentIndex(), 0);

    QTest::keyClick(window, backwardsKey);
    QCOMPARE(gridview->currentIndex(), 0);

    // with wrap
    gridview->setWrapEnabled(true);
    QVERIFY(gridview->isWrapEnabled());

    QTest::keyClick(window, backwardsKey);
    QCOMPARE(gridview->currentIndex(), model.count()-1);
    QTRY_COMPARE(gridview->contentX(), contentPosAtLastItem.x());
    QTRY_COMPARE(gridview->contentY(), contentPosAtLastItem.y());

    QTest::keyClick(window, forwardsKey);
    QCOMPARE(gridview->currentIndex(), 0);
    QTRY_COMPARE(gridview->contentX(), contentPosAtFirstItem.x());
    QTRY_COMPARE(gridview->contentY(), contentPosAtFirstItem.y());

    // Test key press still accepted when position wraps to same index.
    window->rootObject()->setProperty("lastKey", 0);
    model.removeItems(1, model.count() - 1);

    QTest::keyClick(window, backwardsKey);
    QCOMPARE(window->rootObject()->property("lastKey").toInt(), 0);

    QTest::keyClick(window, forwardsKey);
    QCOMPARE(window->rootObject()->property("lastKey").toInt(), 0);

    // Test key press not accepted at limits when wrap is disabled.
    gridview->setWrapEnabled(false);

    QTest::keyClick(window, backwardsKey);
    QCOMPARE(window->rootObject()->property("lastKey").toInt(), int(backwardsKey));

    QTest::keyClick(window, forwardsKey);
    QCOMPARE(window->rootObject()->property("lastKey").toInt(), int(forwardsKey));

    releaseView(window);
}

void tst_QQuickGridView::keyNavigation_data()
{
    QTest::addColumn<QQuickGridView::Flow>("flow");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickItemView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<Qt::Key>("forwardsKey");
    QTest::addColumn<Qt::Key>("backwardsKey");
    QTest::addColumn<QPointF>("contentPosAtFirstItem");
    QTest::addColumn<QPointF>("contentPosAtLastItem");
    QTest::addColumn<int>("indexRightOf7");
    QTest::addColumn<int>("indexLeftOf7");
    QTest::addColumn<int>("indexUpFrom7");
    QTest::addColumn<int>("indexDownFrom7");

    QTest::newRow("LeftToRight, LtR, TtB")
            << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << QQuickItemView::TopToBottom
            << Qt::Key_Right << Qt::Key_Left
            << QPointF(0, 0)
            << QPointF(0, 40)
            << 8 << 6 << 4 << 10;

    QTest::newRow("LeftToRight, RtL, TtB")
            << QQuickGridView::FlowLeftToRight << Qt::RightToLeft << QQuickItemView::TopToBottom
            << Qt::Key_Left << Qt::Key_Right
            << QPointF(0, 0)
            << QPointF(0, 40)
            << 6 << 8 << 4 << 10;

    QTest::newRow("LeftToRight, LtR, BtT")
            << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << QQuickItemView::BottomToTop
            << Qt::Key_Right << Qt::Key_Left
            << QPointF(0, -320)
            << QPointF(0, -360)
            << 8 << 6 << 10 << 4;

    QTest::newRow("LeftToRight, RtL, BtT")
            << QQuickGridView::FlowLeftToRight << Qt::RightToLeft << QQuickItemView::BottomToTop
            << Qt::Key_Left << Qt::Key_Right
            << QPointF(0, -320)
            << QPointF(0, -360)
            << 6 << 8 << 10 << 4;

    QTest::newRow("TopToBottom, LtR, TtB")
            << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << QQuickItemView::TopToBottom
            << Qt::Key_Down << Qt::Key_Up
            << QPointF(0, 0)
            << QPointF(80, 0)
            << 12 << 2 << 6 << 8;

    QTest::newRow("TopToBottom, RtL, TtB")
            << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << QQuickItemView::TopToBottom
            << Qt::Key_Down << Qt::Key_Up
            << QPointF(-240, 0)
            << QPointF(-320, 0)
            << 2 << 12 << 6 << 8;

    QTest::newRow("TopToBottom, LtR, BtT")
            << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << QQuickItemView::BottomToTop
            << Qt::Key_Up << Qt::Key_Down
            << QPointF(0, -320)
            << QPointF(80, -320)
            << 12 << 2 << 8 << 6;

    QTest::newRow("TopToBottom, RtL, BtT")
            << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << QQuickItemView::BottomToTop
            << Qt::Key_Up << Qt::Key_Down
            << QPointF(-240, -320)
            << QPointF(-320, -320)
            << 2 << 12 << 8 << 6;
}

void tst_QQuickGridView::changeFlow()
{
    QQuickView *window = createView();

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), QString::number(i));

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testRightToLeft", QVariant(false));
    ctxt->setContextProperty("testTopToBottom", QVariant(false));
    ctxt->setContextProperty("testBottomToTop", QVariant(false));

    window->setSource(testFileUrl("layouts.qml"));
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    // Confirm items positioned correctly and indexes correct
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->x(), qreal((i%3)*80));
        QTRY_COMPARE(item->y(), qreal((i/3)*60));
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QTRY_VERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
        QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", i);
        QTRY_VERIFY(number != 0);
        QTRY_COMPARE(number->text(), model.number(i));
    }

    ctxt->setContextProperty("testTopToBottom", QVariant(true));

    // Confirm items positioned correctly and indexes correct
    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->x(), qreal((i/5)*80));
        QTRY_COMPARE(item->y(), qreal((i%5)*60));
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QTRY_VERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
        QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", i);
        QTRY_VERIFY(number != 0);
        QTRY_COMPARE(number->text(), model.number(i));
    }

    ctxt->setContextProperty("testRightToLeft", QVariant(true));

    // Confirm items positioned correctly and indexes correct
    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->x(), qreal(-(i/5)*80 - item->width()));
        QTRY_COMPARE(item->y(), qreal((i%5)*60));
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QTRY_VERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
        QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", i);
        QTRY_VERIFY(number != 0);
        QTRY_COMPARE(number->text(), model.number(i));
    }
    gridview->setContentX(100);
    QTRY_COMPARE(gridview->contentX(), 100.);
    ctxt->setContextProperty("testTopToBottom", QVariant(false));
    QTRY_COMPARE(gridview->contentX(), 0.);

    // Confirm items positioned correctly and indexes correct
    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->x(), qreal(240 - (i%3+1)*80));
        QTRY_COMPARE(item->y(), qreal((i/3)*60));
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QTRY_VERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
        QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", i);
        QTRY_VERIFY(number != 0);
        QTRY_COMPARE(number->text(), model.number(i));
    }

    delete window;
}

void tst_QQuickGridView::defaultValues()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("gridview3.qml"));
    QQuickGridView *obj = qobject_cast<QQuickGridView*>(c.create());

    QTRY_VERIFY(obj != 0);
    QTRY_COMPARE(obj->model(), QVariant());
    QTRY_VERIFY(!obj->delegate());
    QTRY_COMPARE(obj->currentIndex(), -1);
    QTRY_VERIFY(!obj->currentItem());
    QTRY_COMPARE(obj->count(), 0);
    QTRY_VERIFY(!obj->highlight());
    QTRY_VERIFY(!obj->highlightItem());
    QTRY_COMPARE(obj->highlightFollowsCurrentItem(), true);
    QTRY_COMPARE(obj->flow(), QQuickGridView::FlowLeftToRight);
    QTRY_COMPARE(obj->isWrapEnabled(), false);
#ifdef QML_VIEW_DEFAULTCACHEBUFFER
    QTRY_COMPARE(obj->cacheBuffer(), QML_VIEW_DEFAULTCACHEBUFFER);
#else
    QTRY_COMPARE(obj->cacheBuffer(), 320);
#endif
    QTRY_COMPARE(obj->cellWidth(), qreal(100)); //### Should 100 be the default?
    QTRY_COMPARE(obj->cellHeight(), qreal(100));
    delete obj;
}

void tst_QQuickGridView::properties()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("gridview2.qml"));
    QQuickGridView *obj = qobject_cast<QQuickGridView*>(c.create());

    QTRY_VERIFY(obj != 0);
    QTRY_VERIFY(obj->model() != QVariant());
    QTRY_VERIFY(obj->delegate() != 0);
    QTRY_COMPARE(obj->currentIndex(), 0);
    QTRY_VERIFY(obj->currentItem() != 0);
    QTRY_COMPARE(obj->count(), 4);
    QTRY_VERIFY(obj->highlight() != 0);
    QTRY_VERIFY(obj->highlightItem() != 0);
    QTRY_COMPARE(obj->highlightFollowsCurrentItem(), false);
    QTRY_COMPARE(obj->flow(), QQuickGridView::FlowLeftToRight);
    QTRY_COMPARE(obj->isWrapEnabled(), true);
    QTRY_COMPARE(obj->cacheBuffer(), 200);
    QTRY_COMPARE(obj->cellWidth(), qreal(100));
    QTRY_COMPARE(obj->cellHeight(), qreal(100));
    delete obj;
}

void tst_QQuickGridView::propertyChanges()
{
    QQuickView *window = createView();
    QTRY_VERIFY(window);
    window->setSource(testFileUrl("propertychangestest.qml"));

    QQuickGridView *gridView = window->rootObject()->findChild<QQuickGridView*>("gridView");
    QTRY_VERIFY(gridView);

    QSignalSpy keyNavigationWrapsSpy(gridView, SIGNAL(keyNavigationWrapsChanged()));
    QSignalSpy cacheBufferSpy(gridView, SIGNAL(cacheBufferChanged()));
    QSignalSpy layoutSpy(gridView, SIGNAL(layoutDirectionChanged()));
    QSignalSpy flowSpy(gridView, SIGNAL(flowChanged()));

    QTRY_COMPARE(gridView->isWrapEnabled(), true);
    QTRY_COMPARE(gridView->cacheBuffer(), 10);
    QTRY_COMPARE(gridView->flow(), QQuickGridView::FlowLeftToRight);

    gridView->setWrapEnabled(false);
    gridView->setCacheBuffer(3);
    gridView->setFlow(QQuickGridView::FlowTopToBottom);

    QTRY_COMPARE(gridView->isWrapEnabled(), false);
    QTRY_COMPARE(gridView->cacheBuffer(), 3);
    QTRY_COMPARE(gridView->flow(), QQuickGridView::FlowTopToBottom);

    QTRY_COMPARE(keyNavigationWrapsSpy.count(),1);
    QTRY_COMPARE(cacheBufferSpy.count(),1);
    QTRY_COMPARE(flowSpy.count(),1);

    gridView->setWrapEnabled(false);
    gridView->setCacheBuffer(3);
    gridView->setFlow(QQuickGridView::FlowTopToBottom);

    QTRY_COMPARE(keyNavigationWrapsSpy.count(),1);
    QTRY_COMPARE(cacheBufferSpy.count(),1);
    QTRY_COMPARE(flowSpy.count(),1);

    gridView->setFlow(QQuickGridView::FlowLeftToRight);
    QTRY_COMPARE(gridView->flow(), QQuickGridView::FlowLeftToRight);

    gridView->setWrapEnabled(true);
    gridView->setCacheBuffer(5);
    gridView->setLayoutDirection(Qt::RightToLeft);

    QTRY_COMPARE(gridView->isWrapEnabled(), true);
    QTRY_COMPARE(gridView->cacheBuffer(), 5);
    QTRY_COMPARE(gridView->layoutDirection(), Qt::RightToLeft);

    QTRY_COMPARE(keyNavigationWrapsSpy.count(),2);
    QTRY_COMPARE(cacheBufferSpy.count(),2);
    QTRY_COMPARE(layoutSpy.count(),1);
    QTRY_COMPARE(flowSpy.count(),2);

    gridView->setWrapEnabled(true);
    gridView->setCacheBuffer(5);
    gridView->setLayoutDirection(Qt::RightToLeft);

    QTRY_COMPARE(keyNavigationWrapsSpy.count(),2);
    QTRY_COMPARE(cacheBufferSpy.count(),2);
    QTRY_COMPARE(layoutSpy.count(),1);
    QTRY_COMPARE(flowSpy.count(),2);

    gridView->setFlow(QQuickGridView::FlowTopToBottom);
    QTRY_COMPARE(gridView->flow(), QQuickGridView::FlowTopToBottom);
    QTRY_COMPARE(flowSpy.count(),3);

    gridView->setFlow(QQuickGridView::FlowTopToBottom);
    QTRY_COMPARE(flowSpy.count(),3);

    delete window;
}

void tst_QQuickGridView::componentChanges()
{
    QQuickView *window = createView();
    QTRY_VERIFY(window);
    window->setSource(testFileUrl("propertychangestest.qml"));

    QQuickGridView *gridView = window->rootObject()->findChild<QQuickGridView*>("gridView");
    QTRY_VERIFY(gridView);

    QQmlComponent component(window->engine());
    component.setData("import QtQuick 2.0; Rectangle { color: \"blue\"; }", QUrl::fromLocalFile(""));

    QQmlComponent delegateComponent(window->engine());
    delegateComponent.setData("import QtQuick 2.0; Text { text: '<b>Name:</b> ' + name }", QUrl::fromLocalFile(""));

    QSignalSpy highlightSpy(gridView, SIGNAL(highlightChanged()));
    QSignalSpy delegateSpy(gridView, SIGNAL(delegateChanged()));
    QSignalSpy headerSpy(gridView, SIGNAL(headerChanged()));
    QSignalSpy footerSpy(gridView, SIGNAL(footerChanged()));
    QSignalSpy headerItemSpy(gridView, SIGNAL(headerItemChanged()));
    QSignalSpy footerItemSpy(gridView, SIGNAL(footerItemChanged()));

    gridView->setHighlight(&component);
    gridView->setDelegate(&delegateComponent);
    gridView->setHeader(&component);
    gridView->setFooter(&component);

    QTRY_COMPARE(gridView->highlight(), &component);
    QTRY_COMPARE(gridView->delegate(), &delegateComponent);
    QTRY_COMPARE(gridView->header(), &component);
    QTRY_COMPARE(gridView->footer(), &component);

    QVERIFY(gridView->headerItem());
    QVERIFY(gridView->footerItem());

    QTRY_COMPARE(highlightSpy.count(),1);
    QTRY_COMPARE(delegateSpy.count(),1);
    QTRY_COMPARE(headerSpy.count(),1);
    QTRY_COMPARE(footerSpy.count(),1);
    QTRY_COMPARE(headerItemSpy.count(),1);
    QTRY_COMPARE(footerItemSpy.count(),1);

    gridView->setHighlight(&component);
    gridView->setDelegate(&delegateComponent);
    gridView->setHeader(&component);
    gridView->setFooter(&component);

    QTRY_COMPARE(highlightSpy.count(),1);
    QTRY_COMPARE(delegateSpy.count(),1);
    QTRY_COMPARE(headerSpy.count(),1);
    QTRY_COMPARE(footerSpy.count(),1);
    QTRY_COMPARE(headerItemSpy.count(),1);
    QTRY_COMPARE(footerItemSpy.count(),1);

    delete window;
}

void tst_QQuickGridView::modelChanges()
{
    QQuickView *window = createView();
    QTRY_VERIFY(window);
    window->setSource(testFileUrl("propertychangestest.qml"));

    QQuickGridView *gridView = window->rootObject()->findChild<QQuickGridView*>("gridView");
    QTRY_VERIFY(gridView);

    QQmlListModel *alternateModel = window->rootObject()->findChild<QQmlListModel*>("alternateModel");
    QTRY_VERIFY(alternateModel);
    QVariant modelVariant = QVariant::fromValue<QObject *>(alternateModel);
    QSignalSpy modelSpy(gridView, SIGNAL(modelChanged()));

    gridView->setModel(modelVariant);
    QTRY_COMPARE(gridView->model(), modelVariant);
    QTRY_COMPARE(modelSpy.count(),1);

    gridView->setModel(modelVariant);
    QTRY_COMPARE(modelSpy.count(),1);

    gridView->setModel(QVariant());
    QTRY_COMPARE(modelSpy.count(),2);
    delete window;
}

void tst_QQuickGridView::positionViewAtBeginningEnd()
{
    QQuickView *window = createView();

    QaimModel model;
    for (int i = 0; i < 40; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testRightToLeft", QVariant(false));
    ctxt->setContextProperty("testTopToBottom", QVariant(false));
    ctxt->setContextProperty("testBottomToTop", QVariant(false));

    window->setSource(testFileUrl("layouts.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    // positionViewAtBeginning
    gridview->setContentY(150);
    gridview->positionViewAtBeginning();
    QTRY_COMPARE(gridview->contentY(), 0.);

    gridview->setContentY(80);
    window->rootObject()->setProperty("showHeader", true);
    gridview->positionViewAtBeginning();
    QTRY_COMPARE(gridview->contentY(), -30.);

    // positionViewAtEnd
    gridview->setContentY(150);
    gridview->positionViewAtEnd();
    QTRY_COMPARE(gridview->contentY(), 520.);   // 14*60 - 320   (14 rows)

    gridview->setContentY(80);
    window->rootObject()->setProperty("showFooter", true);
    gridview->positionViewAtEnd();
    QTRY_COMPARE(gridview->contentY(), 550.);

    // Test for Top To Bottom layout
    ctxt->setContextProperty("testTopToBottom", QVariant(true));
    window->rootObject()->setProperty("showHeader", false);
    window->rootObject()->setProperty("showFooter", false);

    // positionViewAtBeginning
    gridview->setContentX(150);
    gridview->positionViewAtBeginning();
    QTRY_COMPARE(gridview->contentX(), 0.);

    gridview->setContentX(80);
    window->rootObject()->setProperty("showHeader", true);
    gridview->positionViewAtBeginning();
    QTRY_COMPARE(gridview->contentX(), -30.);

    // positionViewAtEnd
    gridview->positionViewAtEnd();
    QTRY_COMPARE(gridview->contentX(), 400.);   // 8*80 - 240   (8 columns)

    gridview->setContentX(80);
    window->rootObject()->setProperty("showFooter", true);
    gridview->positionViewAtEnd();
    QTRY_COMPARE(gridview->contentX(), 430.);

    // set current item to outside visible view, position at beginning
    // and ensure highlight moves to current item
    gridview->setCurrentIndex(6);
    gridview->positionViewAtBeginning();
    QTRY_COMPARE(gridview->contentX(), -30.);
    QVERIFY(gridview->highlightItem());
    QCOMPARE(gridview->highlightItem()->x(), 80.);

    delete window;
}

void tst_QQuickGridView::positionViewAtIndex()
{
    QFETCH(bool, enforceRange);
    QFETCH(bool, topToBottom);
    QFETCH(bool, rightToLeft);
    QFETCH(qreal, initContentPos);
    QFETCH(int, index);
    QFETCH(QQuickGridView::PositionMode, mode);
    QFETCH(qreal, contentPos);

    QQuickView *window = getView();

    QaimModel model;
    for (int i = 0; i < 40; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testRightToLeft", QVariant(rightToLeft));
    ctxt->setContextProperty("testTopToBottom", QVariant(topToBottom));
    ctxt->setContextProperty("testBottomToTop", QVariant(false));

    window->setSource(testFileUrl("layouts.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    window->rootObject()->setProperty("enforceRange", enforceRange);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    if (topToBottom)
        gridview->setContentX(initContentPos);
    else
        gridview->setContentY(initContentPos);

    gridview->positionViewAtIndex(index, mode);
    if (topToBottom)
        QTRY_COMPARE(gridview->contentX(), contentPos);
    else
        QTRY_COMPARE(gridview->contentY(), contentPos);

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = index; i < model.count() && i < itemCount-index-1; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        if (topToBottom) {
            if (rightToLeft) {
                QTRY_COMPARE(item->x(), qreal(-(i/5)*80-item->width()));
                QTRY_COMPARE(item->y(), qreal((i%5)*60));
            } else {
                QTRY_COMPARE(item->x(), (i/5)*80.);
                QTRY_COMPARE(item->y(), (i%5)*60.);
            }
        } else {
            QTRY_COMPARE(item->x(), (i%3)*80.);
            QTRY_COMPARE(item->y(), (i/3)*60.);
        }
    }

    releaseView(window);
}

void tst_QQuickGridView::positionViewAtIndex_data()
{
    QTest::addColumn<bool>("enforceRange");
    QTest::addColumn<bool>("topToBottom");
    QTest::addColumn<bool>("rightToLeft");
    QTest::addColumn<qreal>("initContentPos");
    QTest::addColumn<int>("index");
    QTest::addColumn<QQuickGridView::PositionMode>("mode");
    QTest::addColumn<qreal>("contentPos");

    QTest::newRow("no range, 4 at Beginning") << false << false << false << 0. << 4 << QQuickGridView::Beginning << 60.;
    QTest::newRow("no range, 4 at End") << false << false << false << 0. << 4 << QQuickGridView::End << 0.;
    QTest::newRow("no range, 21 at Beginning") << false << false << false << 0. << 21 << QQuickGridView::Beginning << 420.;
    // Position on an item that would leave empty space if positioned at the top
    QTest::newRow("no range, 31 at Beginning") << false << false << false << 0. << 31 << QQuickGridView::Beginning << 520.;
    QTest::newRow("no range, 30 at End") << false << false << false << 0. << 30 << QQuickGridView::End << 340.;
    QTest::newRow("no range, 15 at Center") << false << false << false << 0. << 15 << QQuickGridView::Center << 170.;
    // Ensure at least partially visible
    QTest::newRow("no range, 15 visible => Visible") << false << false << false << 302. << 15 << QQuickGridView::Visible << 302.;
    QTest::newRow("no range, 15 after visible => Visible") << false << false << false << 360. << 15 << QQuickGridView::Visible << 300.;
    QTest::newRow("no range, 20 visible => Visible") << false << false << false << 60. << 20 << QQuickGridView::Visible << 60.;
    QTest::newRow("no range, 20 before visible => Visible") << false << false << false << 20. << 20 << QQuickGridView::Visible << 100.;
    // Ensure completely visible
    QTest::newRow("no range, 20 visible => Contain") << false << false << false << 120. << 20 << QQuickGridView::Contain << 120.;
    QTest::newRow("no range, 15 partially visible => Contain") << false << false << false << 302. << 15 << QQuickGridView::Contain << 300.;
    QTest::newRow("no range, 20 partially visible => Contain") << false << false << false << 60. << 20 << QQuickGridView::Contain << 100.;

    QTest::newRow("strict range, 4 at End") << true << false << false << 0. << 4 << QQuickGridView::End << -120.;
    QTest::newRow("strict range, 38 at Beginning") << true << false << false << 0. << 38 << QQuickGridView::Beginning << 660.;
    QTest::newRow("strict range, 15 at Center") << true << false << false << 0. << 15 << QQuickGridView::Center << 180.;
    QTest::newRow("strict range, 4 at SnapPosition") << true << false << false << 0. << 4 << QQuickGridView::SnapPosition << -60.;
    QTest::newRow("strict range, 10 at SnapPosition") << true << false << false << 0. << 10 << QQuickGridView::SnapPosition << 60.;
    QTest::newRow("strict range, 38 at SnapPosition") << true << false << false << 0. << 38 << QQuickGridView::SnapPosition << 600.;

    // TopToBottom
    QTest::newRow("no range, ttb, 30 at End") << false << true << false << 0. << 30 << QQuickGridView::End << 320.;
    QTest::newRow("no range, ttb, 15 at Center") << false << true << false << 0. << 15 << QQuickGridView::Center << 160.;
    QTest::newRow("no range, ttb, 15 visible => Visible") << false << true << false << 160. << 15 << QQuickGridView::Visible << 160.;
    QTest::newRow("no range, ttb, 25 partially visible => Visible") << false << true << false << 170. << 25 << QQuickGridView::Visible << 170.;
    QTest::newRow("no range, ttb, 30 before visible => Visible") << false << true << false << 170. << 30 << QQuickGridView::Visible << 320.;
    QTest::newRow("no range, ttb, 25 partially visible => Contain") << false << true << false << 170. << 25 << QQuickGridView::Contain << 240.;

    // RightToLeft
    QTest::newRow("no range, rtl, ttb, 6 at Beginning") << false << true << true << 0. << 6 << QQuickGridView::Beginning << -320.;
    QTest::newRow("no range, rtl, ttb, 21 at Beginning") << false << true << true << 0. << 21 << QQuickGridView::Beginning << -560.;
    // Position on an item that would leave empty space if positioned at the top
    QTest::newRow("no range, rtl, ttb, 31 at Beginning") << false << true << true << 0. << 31 << QQuickGridView::Beginning << -640.;
    QTest::newRow("no range, rtl, ttb, 0 at Beginning") << false << true << true << -400. << 0 << QQuickGridView::Beginning << -240.;
    QTest::newRow("no range, rtl, ttb, 30 at End") << false << true << true << 0. << 30 << QQuickGridView::End << -560.;
    QTest::newRow("no range, rtl, ttb, 15 at Center") << false << true << true << 0. << 15 << QQuickGridView::Center << -400.;
    QTest::newRow("no range, rtl, ttb, 15 visible => Visible") << false << true << true << -555. << 15 << QQuickGridView::Visible << -555.;
    QTest::newRow("no range, rtl, ttb, 15 not visible => Visible") << false << true << true << -239. << 15 << QQuickGridView::Visible << -320.;
    QTest::newRow("no range, rtl, ttb, 15 partially visible => Visible") << false << true << true << -300. << 15 << QQuickGridView::Visible << -300.;
    QTest::newRow("no range, rtl, ttb, 20 visible => Contain") << false << true << true << -400. << 20 << QQuickGridView::Contain << -400.;
    QTest::newRow("no range, rtl, ttb, 15 partially visible => Contain") << false << true << true << -315. << 15 << QQuickGridView::Contain << -320.;
}

void tst_QQuickGridView::snapping()
{
    QQuickView *window = createView();

    QaimModel model;
    for (int i = 0; i < 40; i++)
        model.addItem("Item" + QString::number(i), "");

    window->rootContext()->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("gridview1.qml"));
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    gridview->setHeight(220);
    QCOMPARE(gridview->height(), 220.);

    gridview->positionViewAtIndex(12, QQuickGridView::Visible);
    QCOMPARE(gridview->contentY(), 80.);

    gridview->setContentY(0);
    QCOMPARE(gridview->contentY(), 0.);

    gridview->setSnapMode(QQuickGridView::SnapToRow);
    QCOMPARE(gridview->snapMode(), QQuickGridView::SnapToRow);

    gridview->positionViewAtIndex(12, QQuickGridView::Visible);
    QCOMPARE(gridview->contentY(), 60.);

    gridview->positionViewAtIndex(15, QQuickGridView::End);
    QCOMPARE(gridview->contentY(), 120.);

    delete window;

}

void tst_QQuickGridView::mirroring()
{
    QQuickView *windowA = createView();
    windowA->setSource(testFileUrl("mirroring.qml"));
    QQuickGridView *gridviewA = findItem<QQuickGridView>(windowA->rootObject(), "view");
    QTRY_VERIFY(gridviewA != 0);

    QQuickView *windowB = createView();
    windowB->setSource(testFileUrl("mirroring.qml"));
    QQuickGridView *gridviewB = findItem<QQuickGridView>(windowB->rootObject(), "view");
    QTRY_VERIFY(gridviewA != 0);
    qApp->processEvents();

    QList<QString> objectNames;
    objectNames << "item1" << "item2"; // << "item3"

    gridviewA->setProperty("layoutDirection", Qt::LeftToRight);
    gridviewB->setProperty("layoutDirection", Qt::RightToLeft);
    QCOMPARE(gridviewA->layoutDirection(), gridviewA->effectiveLayoutDirection());

    // LTR != RTL
    foreach (const QString objectName, objectNames)
        QVERIFY(findItem<QQuickItem>(gridviewA, objectName)->x() != findItem<QQuickItem>(gridviewB, objectName)->x());

    gridviewA->setProperty("layoutDirection", Qt::LeftToRight);
    gridviewB->setProperty("layoutDirection", Qt::LeftToRight);

    // LTR == LTR
    foreach (const QString objectName, objectNames)
        QCOMPARE(findItem<QQuickItem>(gridviewA, objectName)->x(), findItem<QQuickItem>(gridviewB, objectName)->x());

    QCOMPARE(gridviewB->layoutDirection(), gridviewB->effectiveLayoutDirection());
    QQuickItemPrivate::get(gridviewB)->setLayoutMirror(true);
    QVERIFY(gridviewB->layoutDirection() != gridviewB->effectiveLayoutDirection());

    // LTR != LTR+mirror
    foreach (const QString objectName, objectNames)
        QVERIFY(findItem<QQuickItem>(gridviewA, objectName)->x() != findItem<QQuickItem>(gridviewB, objectName)->x());

    gridviewA->setProperty("layoutDirection", Qt::RightToLeft);

    // RTL == LTR+mirror
    foreach (const QString objectName, objectNames)
        QCOMPARE(findItem<QQuickItem>(gridviewA, objectName)->x(), findItem<QQuickItem>(gridviewB, objectName)->x());

    gridviewB->setProperty("layoutDirection", Qt::RightToLeft);

    // RTL != RTL+mirror
    foreach (const QString objectName, objectNames)
        QVERIFY(findItem<QQuickItem>(gridviewA, objectName)->x() != findItem<QQuickItem>(gridviewB, objectName)->x());

    gridviewA->setProperty("layoutDirection", Qt::LeftToRight);

    // LTR == RTL+mirror
    foreach (const QString objectName, objectNames)
        QCOMPARE(findItem<QQuickItem>(gridviewA, objectName)->x(), findItem<QQuickItem>(gridviewB, objectName)->x());

    delete windowA;
    delete windowB;
}

void tst_QQuickGridView::resetModel()
{
    QQuickView *window = createView();

    QStringList strings;
    strings << "one" << "two" << "three";
    QStringListModel model(strings);

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("displaygrid.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QTRY_COMPARE(gridview->count(), model.rowCount());

    for (int i = 0; i < model.rowCount(); ++i) {
        QQuickText *display = findItem<QQuickText>(contentItem, "displayText", i);
        QTRY_VERIFY(display != 0);
        QTRY_COMPARE(display->text(), strings.at(i));
    }

    strings.clear();
    strings << "four" << "five" << "six" << "seven";
    model.setStringList(strings);

    QTRY_COMPARE(gridview->count(), model.rowCount());

    for (int i = 0; i < model.rowCount(); ++i) {
        QQuickText *display = findItem<QQuickText>(contentItem, "displayText", i);
        QTRY_VERIFY(display != 0);
        QTRY_COMPARE(display->text(), strings.at(i));
    }

    delete window;
}

void tst_QQuickGridView::enforceRange()
{
    QQuickView *window = createView();

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testRightToLeft", QVariant(false));
    ctxt->setContextProperty("testTopToBottom", QVariant(false));

    window->setSource(testFileUrl("gridview-enforcerange.qml"));
    window->show();
    qApp->processEvents();
    QVERIFY(window->rootObject() != 0);

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    QTRY_COMPARE(gridview->preferredHighlightBegin(), 100.0);
    QTRY_COMPARE(gridview->preferredHighlightEnd(), 100.0);
    QTRY_COMPARE(gridview->highlightRangeMode(), QQuickGridView::StrictlyEnforceRange);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    // view should be positioned at the top of the range.
    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QTRY_VERIFY(item);
    QTRY_COMPARE(gridview->contentY(), -100.0);

    QQuickText *name = findItem<QQuickText>(contentItem, "textName", 0);
    QTRY_VERIFY(name != 0);
    QTRY_COMPARE(name->text(), model.name(0));
    QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", 0);
    QTRY_VERIFY(number != 0);
    QTRY_COMPARE(number->text(), model.number(0));

    // Check currentIndex is updated when contentItem moves
    gridview->setContentY(0);
    QTRY_COMPARE(gridview->currentIndex(), 2);

    gridview->setCurrentIndex(5);
    QTRY_COMPARE(gridview->contentY(), 100.);

    QaimModel model2;
    for (int i = 0; i < 5; i++)
        model2.addItem("Item" + QString::number(i), "");

    ctxt->setContextProperty("testModel", &model2);
    QCOMPARE(gridview->count(), 5);

    delete window;
}

void tst_QQuickGridView::enforceRange_rightToLeft()
{
    QQuickView *window = createView();

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testRightToLeft", QVariant(true));
    ctxt->setContextProperty("testTopToBottom", QVariant(true));

    window->setSource(testFileUrl("gridview-enforcerange.qml"));
    window->show();
    QTRY_VERIFY(window->isExposed());
    QVERIFY(window->rootObject() != 0);

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    QCOMPARE(gridview->preferredHighlightBegin(), 100.0);
    QCOMPARE(gridview->preferredHighlightEnd(), 100.0);
    QCOMPARE(gridview->highlightRangeMode(), QQuickGridView::StrictlyEnforceRange);

    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);

    // view should be positioned at the top of the range.
    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item);
    QTRY_COMPARE(gridview->contentX(), -140.);
    QTRY_COMPARE(gridview->contentY(), 0.0);

    QQuickText *name = findItem<QQuickText>(contentItem, "textName", 0);
    QTRY_VERIFY(name != 0);
    QTRY_COMPARE(name->text(), model.name(0));
    QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", 0);
    QTRY_VERIFY(number != 0);
    QTRY_COMPARE(number->text(), model.number(0));

    // Check currentIndex is updated when contentItem moves
    gridview->setContentX(-240);
    QTRY_COMPARE(gridview->currentIndex(), 3);

    gridview->setCurrentIndex(7);
    QTRY_COMPARE(gridview->contentX(), -340.);
    QTRY_COMPARE(gridview->contentY(), 0.0);

    QaimModel model2;
    for (int i = 0; i < 5; i++)
        model2.addItem("Item" + QString::number(i), "");

    ctxt->setContextProperty("testModel", &model2);
    QCOMPARE(gridview->count(), 5);

    delete window;
}

void tst_QQuickGridView::QTBUG_8456()
{
    QQuickView *window = createView();

    window->setSource(testFileUrl("setindex.qml"));
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    QTRY_COMPARE(gridview->currentIndex(), 0);

    delete window;
}

void tst_QQuickGridView::manualHighlight()
{
    QQuickView *window = createView();

    QString filename(testFile("manual-highlight.qml"));
    window->setSource(QUrl::fromLocalFile(filename));

    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QTRY_COMPARE(gridview->currentIndex(), 0);
    QTRY_COMPARE(gridview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 0));
    QTRY_COMPARE(gridview->highlightItem()->y() - 5, gridview->currentItem()->y());
    QTRY_COMPARE(gridview->highlightItem()->x() - 5, gridview->currentItem()->x());

    gridview->setCurrentIndex(2);

    QTRY_COMPARE(gridview->currentIndex(), 2);
    QTRY_COMPARE(gridview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 2));
    QTRY_COMPARE(gridview->highlightItem()->y() - 5, gridview->currentItem()->y());
    QTRY_COMPARE(gridview->highlightItem()->x() - 5, gridview->currentItem()->x());

    gridview->positionViewAtIndex(8, QQuickGridView::Contain);

    QTRY_COMPARE(gridview->currentIndex(), 2);
    QTRY_COMPARE(gridview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 2));
    QTRY_COMPARE(gridview->highlightItem()->y() - 5, gridview->currentItem()->y());
    QTRY_COMPARE(gridview->highlightItem()->x() - 5, gridview->currentItem()->x());

    gridview->setFlow(QQuickGridView::FlowTopToBottom);
    QTRY_COMPARE(gridview->flow(), QQuickGridView::FlowTopToBottom);

    gridview->setCurrentIndex(0);
    QTRY_COMPARE(gridview->currentIndex(), 0);
    QTRY_COMPARE(gridview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 0));
    QTRY_COMPARE(gridview->highlightItem()->y() - 5, gridview->currentItem()->y());
    QTRY_COMPARE(gridview->highlightItem()->x() - 5, gridview->currentItem()->x());

    delete window;
}


void tst_QQuickGridView::footer()
{
    QFETCH(QQuickGridView::Flow, flow);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(QPointF, initialFooterPos);
    QFETCH(QPointF, changedFooterPos);
    QFETCH(QPointF, initialContentPos);
    QFETCH(QPointF, firstDelegatePos);
    QFETCH(QPointF, resizeContentPos);

    QQuickView *window = getView();
    window->show();

    QaimModel model;
    for (int i = 0; i < 7; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("footer.qml"));
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    gridview->setFlow(flow);
    gridview->setLayoutDirection(layoutDirection);
    gridview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQuickText *footer = findItem<QQuickText>(contentItem, "footer");
    QVERIFY(footer);
    QCOMPARE(footer, gridview->footerItem());

    QCOMPARE(footer->position(), initialFooterPos);
    QCOMPARE(footer->width(), 100.);
    QCOMPARE(footer->height(), 30.);
    QCOMPARE(QPointF(gridview->contentX(), gridview->contentY()), initialContentPos);

    if (flow == QQuickGridView::FlowLeftToRight)
        QCOMPARE(gridview->contentHeight(), (model.count()+2) / 3 * 60. + footer->height());
    else
        QCOMPARE(gridview->contentWidth(), (model.count()+3) / 5 * 80. + footer->width());

    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->position(), firstDelegatePos);

    if (flow == QQuickGridView::FlowLeftToRight) {
        // shrink by one row
        model.removeItem(2);
        if (verticalLayoutDirection == QQuickItemView::TopToBottom)
            QTRY_COMPARE(footer->y(), initialFooterPos.y() - gridview->cellHeight());
        else
            QTRY_COMPARE(footer->y(), initialFooterPos.y() + gridview->cellHeight());
    } else {
        // shrink by one column
        model.removeItem(2);
        model.removeItem(3);
        if (layoutDirection == Qt::LeftToRight)
            QTRY_COMPARE(footer->x(), initialFooterPos.x() - gridview->cellWidth());
        else
            QTRY_COMPARE(footer->x(), initialFooterPos.x() + gridview->cellWidth());
    }

    // remove all items
    model.clear();
    if (flow == QQuickGridView::FlowLeftToRight)
        QTRY_COMPARE(gridview->contentHeight(), footer->height());
    else
        QTRY_COMPARE(gridview->contentWidth(), footer->width());

    QPointF posWhenNoItems(0, 0);
    if (layoutDirection == Qt::RightToLeft)
        posWhenNoItems.setX(flow == QQuickGridView::FlowLeftToRight ? gridview->width() - footer->width() : -footer->width());
    if (verticalLayoutDirection == QQuickItemView::BottomToTop)
        posWhenNoItems.setY(-footer->height());
    QTRY_COMPARE(footer->position(), posWhenNoItems);

    // if header is toggled, it shouldn't affect the footer position
    window->rootObject()->setProperty("showHeader", true);
    QVERIFY(findItem<QQuickItem>(contentItem, "header") != 0);
    QTRY_COMPARE(footer->position(), posWhenNoItems);
    window->rootObject()->setProperty("showHeader", false);

    // add 30 items
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QSignalSpy footerItemSpy(gridview, SIGNAL(footerItemChanged()));
    QMetaObject::invokeMethod(window->rootObject(), "changeFooter");

    QCOMPARE(footerItemSpy.count(), 1);

    footer = findItem<QQuickText>(contentItem, "footer");
    QVERIFY(!footer);
    footer = findItem<QQuickText>(contentItem, "footer2");
    QVERIFY(footer);
    QCOMPARE(footer, gridview->footerItem());

    QCOMPARE(footer->position(), changedFooterPos);
    QCOMPARE(footer->width(), 50.);
    QCOMPARE(footer->height(), 20.);

    // changing the footer shouldn't change the content pos
    QTRY_COMPARE(QPointF(gridview->contentX(), gridview->contentY()), initialContentPos);

    item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->position(), firstDelegatePos);

    gridview->positionViewAtEnd();
    footer->setHeight(10);
    footer->setWidth(40);
    QTRY_COMPARE(QPointF(gridview->contentX(), gridview->contentY()), resizeContentPos);

    releaseView(window);
}

void tst_QQuickGridView::footer_data()
{
    QTest::addColumn<QQuickGridView::Flow>("flow");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickItemView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<QPointF>("initialFooterPos");
    QTest::addColumn<QPointF>("changedFooterPos");
    QTest::addColumn<QPointF>("initialContentPos");
    QTest::addColumn<QPointF>("firstDelegatePos");
    QTest::addColumn<QPointF>("resizeContentPos");

    // footer1 = 100 x 30
    // footer2 = 50 x 20
    // cells = 80 * 60
    // view width = 240
    // view height = 320

    // footer below items, bottom left
    QTest::newRow("LeftToRight, LtR, TtB")
        << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << QQuickItemView::TopToBottom
        << QPointF(0, 3 * 60)  // 180 = height of 3 rows (cell height is 60)
        << QPointF(0, 10 * 60)  // 30 items = 10 rows
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF(0, (10 * 60) - 320 + 10);

    // footer below items, bottom right
    QTest::newRow("LeftToRight, RtL, TtB")
        << QQuickGridView::FlowLeftToRight << Qt::RightToLeft << QQuickItemView::TopToBottom
        << QPointF(240 - 100, 3 * 60)
        << QPointF((240 - 100) + 50, 10 * 60)     // 50 = width diff between old and new footers
        << QPointF(0, 0)
        << QPointF(240 - 80, 0)
        << QPointF(0, (10 * 60) - 320 + 10);

    // footer above items, top left
    QTest::newRow("LeftToRight, LtR, BtT")
        << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << QQuickItemView::BottomToTop
        << QPointF(0, -(3 * 60) - 30)
        << QPointF(0, -(10 * 60) - 20)
        << QPointF(0, -320)
        << QPointF(0, -60)
        << QPointF(0, -(10 * 60) - 10);

    // footer above items, top right
    QTest::newRow("LeftToRight, RtL, BtT")
        << QQuickGridView::FlowLeftToRight << Qt::RightToLeft << QQuickItemView::BottomToTop
        << QPointF(240 - 100, -(3 * 60) - 30)
        << QPointF((240 - 100) + 50, -(10 * 60) - 20)
        << QPointF(0, -320)
        << QPointF(240 - 80, -60)
        << QPointF(0, -(10 * 60) - 10);


    // footer to right of items, bottom right
    QTest::newRow("TopToBottom, LtR, TtB")
        << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << QQuickItemView::TopToBottom
        << QPointF(2 * 80, 0)      // 2 columns, cell width 80
        << QPointF(6 * 80, 0)      // 30 items = 6 columns
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF((6 * 80) - 240 + 40, 0);

    // footer to left of items, bottom right
    QTest::newRow("TopToBottom, RtL, TtB")
        << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << QQuickItemView::TopToBottom
        << QPointF(-(2 * 80) - 100, 0)
        << QPointF(-(6 * 80) - 50, 0)     // 50 = new footer width
        << QPointF(-240, 0)
        << QPointF(-80, 0)
        << QPointF(-(6 * 80) - 40, 0);

    // footer to right of items, top right
    QTest::newRow("TopToBottom, LtR, BtT")
        << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << QQuickItemView::BottomToTop
        << QPointF(2 * 80, -30)
        << QPointF(6 * 80, -20)
        << QPointF(0, -320)
        << QPointF(0, -60)
        << QPointF((6 * 80) - 240 + 40, -320);

    // footer to left of items, top left
    QTest::newRow("TopToBottom, RtL, BtT")
        << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << QQuickItemView::BottomToTop
        << QPointF(-(2 * 80) - 100, -30)
        << QPointF(-(6 * 80) - 50, -20)
        << QPointF(-240, -320)
        << QPointF(-80, -60)
        << QPointF(-(6 * 80) - 40, -320);
}

void tst_QQuickGridView::initialZValues()
{
    QFETCH(QString, fileName);
    QQuickView *window = createView();
    window->setSource(testFileUrl(fileName));
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QVERIFY(gridview->currentItem());
    QTRY_COMPARE(gridview->currentItem()->z(), gridview->property("itemZ").toReal());

    QVERIFY(gridview->headerItem());
    QTRY_COMPARE(gridview->headerItem()->z(), gridview->property("headerZ").toReal());

    QVERIFY(gridview->footerItem());
    QTRY_COMPARE(gridview->footerItem()->z(), gridview->property("footerZ").toReal());

    QVERIFY(gridview->highlightItem());
    QTRY_COMPARE(gridview->highlightItem()->z(), gridview->property("highlightZ").toReal());

    delete window;
}

void tst_QQuickGridView::initialZValues_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::newRow("defaults") << "defaultZValues.qml";
    QTest::newRow("constants") << "constantZValues.qml";
    QTest::newRow("bindings") << "boundZValues.qml";
}

void tst_QQuickGridView::header()
{
    QFETCH(QQuickGridView::Flow, flow);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(QPointF, initialHeaderPos);
    QFETCH(QPointF, changedHeaderPos);
    QFETCH(QPointF, initialContentPos);
    QFETCH(QPointF, changedContentPos);
    QFETCH(QPointF, firstDelegatePos);
    QFETCH(QPointF, resizeContentPos);

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQuickView *window = getView();
    window->rootContext()->setContextProperty("testModel", &model);
    window->rootContext()->setContextProperty("initialViewWidth", 240);
    window->rootContext()->setContextProperty("initialViewHeight", 320);
    window->setSource(testFileUrl("header.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    gridview->setFlow(flow);
    gridview->setLayoutDirection(layoutDirection);
    gridview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQuickText *header = findItem<QQuickText>(contentItem, "header");
    QVERIFY(header);
    QCOMPARE(header, gridview->headerItem());

    QCOMPARE(header->position(), initialHeaderPos);
    QCOMPARE(header->width(), 100.);
    QCOMPARE(header->height(), 30.);
    QCOMPARE(QPointF(gridview->contentX(), gridview->contentY()), initialContentPos);

    if (flow == QQuickGridView::FlowLeftToRight)
        QCOMPARE(gridview->contentHeight(), (model.count()+2) / 3 * 60. + header->height());
    else
        QCOMPARE(gridview->contentWidth(), (model.count()+3) / 5 * 80. + header->width());

    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->position(), firstDelegatePos);

    model.clear();
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
    QCOMPARE(header->position(), initialHeaderPos); // header should stay where it is
    if (flow == QQuickGridView::FlowLeftToRight)
        QCOMPARE(gridview->contentHeight(), header->height());
    else
        QCOMPARE(gridview->contentWidth(), header->width());

    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QSignalSpy headerItemSpy(gridview, SIGNAL(headerItemChanged()));
    QMetaObject::invokeMethod(window->rootObject(), "changeHeader");

    QCOMPARE(headerItemSpy.count(), 1);

    header = findItem<QQuickText>(contentItem, "header");
    QVERIFY(!header);
    header = findItem<QQuickText>(contentItem, "header2");
    QVERIFY(header);

    QCOMPARE(header, gridview->headerItem());

    QCOMPARE(header->position(), changedHeaderPos);
    QCOMPARE(header->width(), 50.);
    QCOMPARE(header->height(), 20.);
    QTRY_COMPARE(QPointF(gridview->contentX(), gridview->contentY()), changedContentPos);

    item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->position(), firstDelegatePos);

    header->setHeight(10);
    header->setWidth(40);
    QTRY_COMPARE(QPointF(gridview->contentX(), gridview->contentY()), resizeContentPos);

    releaseView(window);


    // QTBUG-21207 header should become visible if view resizes from initial empty size

    window = getView();
    window->rootContext()->setContextProperty("testModel", &model);
    window->rootContext()->setContextProperty("initialViewWidth", 240);
    window->rootContext()->setContextProperty("initialViewHeight", 320);
    window->setSource(testFileUrl("header.qml"));
    window->show();
    qApp->processEvents();

    gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    gridview->setFlow(flow);
    gridview->setLayoutDirection(layoutDirection);
    gridview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    gridview->setWidth(240);
    gridview->setHeight(320);
    QTRY_COMPARE(gridview->headerItem()->position(), initialHeaderPos);
    QCOMPARE(QPointF(gridview->contentX(), gridview->contentY()), initialContentPos);

    releaseView(window);
}

void tst_QQuickGridView::header_data()
{
    QTest::addColumn<QQuickGridView::Flow>("flow");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickItemView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<QPointF>("initialHeaderPos");
    QTest::addColumn<QPointF>("changedHeaderPos");
    QTest::addColumn<QPointF>("initialContentPos");
    QTest::addColumn<QPointF>("changedContentPos");
    QTest::addColumn<QPointF>("firstDelegatePos");
    QTest::addColumn<QPointF>("resizeContentPos");

    // header1 = 100 x 30
    // header2 = 50 x 20
    // cells = 80 x 60
    // view width = 240

    // header above items, top left
    QTest::newRow("LeftToRight, LtR, TtB")
        << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << QQuickItemView::TopToBottom
        << QPointF(0, -30)
        << QPointF(0, -20)
        << QPointF(0, -30)
        << QPointF(0, -20)
        << QPointF(0, 0)
        << QPointF(0, -10);

    // header above items, top right
    QTest::newRow("LeftToRight, RtL, TtB")
        << QQuickGridView::FlowLeftToRight << Qt::RightToLeft << QQuickItemView::TopToBottom
        << QPointF(240 - 100, -30)
        << QPointF((240 - 100) + 50, -20)     // 50 = width diff between old and new headers
        << QPointF(0, -30)
        << QPointF(0, -20)
        << QPointF(160, 0)
        << QPointF(0, -10);

    // header below items, bottom left
    QTest::newRow("LeftToRight, LtR, BtT")
        << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << QQuickItemView::BottomToTop
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF(0, -320 + 30)
        << QPointF(0, -320 + 20)
        << QPointF(0, -60)
        << QPointF(0, -320 + 10);

    // header above items, top right
    QTest::newRow("LeftToRight, RtL, BtT")
        << QQuickGridView::FlowLeftToRight << Qt::RightToLeft << QQuickItemView::BottomToTop
        << QPointF(240 - 100, 0)
        << QPointF((240 - 100) + 50, 0)
        << QPointF(0, -320 + 30)
        << QPointF(0, -320 + 20)
        << QPointF(160, -60)
        << QPointF(0, -320 + 10);


    // header to left of items, bottom left
    QTest::newRow("TopToBottom, LtR, TtB")
        << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << QQuickItemView::TopToBottom
        << QPointF(-100, 0)
        << QPointF(-50, 0)
        << QPointF(-100, 0)
        << QPointF(-50, 0)
        << QPointF(0, 0)
        << QPointF(-40, 0);

    // header to right of items, bottom right
    QTest::newRow("TopToBottom, RtL, TtB")
        << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << QQuickItemView::TopToBottom
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF(-(240 - 100), 0)
        << QPointF(-(240 - 50), 0)
        << QPointF(-80, 0)
        << QPointF(-(240 - 40), 0);

    // header to left of items, top left
    QTest::newRow("TopToBottom, LtR, BtT")
        << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << QQuickItemView::BottomToTop
        << QPointF(-100, -30)
        << QPointF(-50, -20)
        << QPointF(-100, -320)
        << QPointF(-50, -320)
        << QPointF(0, -60)
        << QPointF(-40, -320);

    // header to right of items, top right
    QTest::newRow("TopToBottom, RtL, BtT")
        << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << QQuickItemView::BottomToTop
        << QPointF(0, -30)
        << QPointF(0, -20)
        << QPointF(-(240 - 100), -320)
        << QPointF(-(240 - 50), -320)
        << QPointF(-80, -60)
        << QPointF(-(240 - 40), -320);
}

class GVAccessor : public QQuickGridView
{
public:
    qreal minY() const { return minYExtent(); }
    qreal maxY() const { return maxYExtent(); }
    qreal minX() const { return minXExtent(); }
    qreal maxX() const { return maxXExtent(); }
};

void tst_QQuickGridView::extents()
{
    QFETCH(QQuickGridView::Flow, flow);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(QPointF, headerPos);
    QFETCH(QPointF, footerPos);
    QFETCH(QPointF, minPos);
    QFETCH(QPointF, maxPos);
    QFETCH(QPointF, origin_empty);
    QFETCH(QPointF, origin_nonEmpty);

    QQuickView *window = getView();

    QaimModel model;
    QQmlContext *ctxt = window->rootContext();

    ctxt->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("headerfooter.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = qobject_cast<QQuickGridView*>(window->rootObject());
    QTRY_VERIFY(gridview != 0);
    gridview->setFlow(flow);
    gridview->setLayoutDirection(layoutDirection);
    gridview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQuickItem *header = findItem<QQuickItem>(contentItem, "header");
    QVERIFY(header);
    QCOMPARE(header->position(), headerPos);

    QQuickItem *footer = findItem<QQuickItem>(contentItem, "footer");
    QVERIFY(footer);
    QCOMPARE(footer->position(), footerPos);

    QCOMPARE(static_cast<GVAccessor*>(gridview)->minX(), minPos.x());
    QCOMPARE(static_cast<GVAccessor*>(gridview)->minY(), minPos.y());
    QCOMPARE(static_cast<GVAccessor*>(gridview)->maxX(), maxPos.x());
    QCOMPARE(static_cast<GVAccessor*>(gridview)->maxY(), maxPos.y());

    QCOMPARE(gridview->originX(), origin_empty.x());
    QCOMPARE(gridview->originY(), origin_empty.y());
    for (int i=0; i<30; i++)
        model.addItem("Item" + QString::number(i), "");
    gridview->forceLayout();
    QTRY_COMPARE(gridview->count(), model.count());
    QCOMPARE(gridview->originX(), origin_nonEmpty.x());
    QCOMPARE(gridview->originY(), origin_nonEmpty.y());

    releaseView(window);
}

void tst_QQuickGridView::extents_data()
{
    QTest::addColumn<QQuickGridView::Flow>("flow");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickItemView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<QPointF>("headerPos");
    QTest::addColumn<QPointF>("footerPos");
    QTest::addColumn<QPointF>("minPos");
    QTest::addColumn<QPointF>("maxPos");
    QTest::addColumn<QPointF>("origin_empty");
    QTest::addColumn<QPointF>("origin_nonEmpty");

    // header is 240x20 (or 20x320 in TopToBottom)
    // footer is 240x30 (or 30x320 in TopToBottom)
    // grid has 10 rows in LeftToRight mode and 6 columns in TopToBottom

    QTest::newRow("LeftToRight, LtR, TtB")
            << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << QQuickItemView::TopToBottom
            << QPointF(0, -20) << QPointF(0, 0)
            << QPointF(0, 20) << QPointF(240, 20)
            << QPointF(0, -20) << QPointF(0, -20);

    QTest::newRow("LeftToRight, RtL, TtB")
            << QQuickGridView::FlowLeftToRight << Qt::RightToLeft << QQuickItemView::TopToBottom
            << QPointF(0, -20) << QPointF(0, 0)
            << QPointF(0, 20) << QPointF(240, 20)
            << QPointF(0, -20) << QPointF(0, -20);

    QTest::newRow("LeftToRight, LtR, BtT")
            << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << QQuickItemView::BottomToTop
            << QPointF(0, 0) << QPointF(0, -30)
            << QPointF(0, 320 - 20) << QPointF(240, 320 - 20)  // content flow is reversed
            << QPointF(0, -30) << QPointF(0, (-60.0 * 10) - 30);

    QTest::newRow("LeftToRight, RtL, BtT")
            << QQuickGridView::FlowLeftToRight << Qt::RightToLeft << QQuickItemView::BottomToTop
            << QPointF(0, 0) << QPointF(0, -30)
            << QPointF(0, 320 - 20) << QPointF(240, 320 - 20)  // content flow is reversed
            << QPointF(0, -30) << QPointF(0, (-60.0 * 10) - 30);


    QTest::newRow("TopToBottom, LtR, TtB")
            << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << QQuickItemView::TopToBottom
            << QPointF(-20, 0) << QPointF(0, 0)
            << QPointF(20, 0) << QPointF(20, 320)
            << QPointF(-20, 0) << QPointF(-20, 0);

    QTest::newRow("TopToBottom, RtL, TtB")
            << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << QQuickItemView::TopToBottom
            << QPointF(0, 0) << QPointF(-30, 0)
            << QPointF(240 - 20, 0) << QPointF(240 - 20, 320)  // content flow is reversed
            << QPointF(-30, 0) << QPointF((-80.0 * 6) - 30, 0);

    QTest::newRow("TopToBottom, LtR, BtT")
            << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << QQuickItemView::BottomToTop
            << QPointF(-20, -320) << QPointF(0, -320)
            << QPointF(20, 0) << QPointF(20, 320)
            << QPointF(-20, 0) << QPointF(-20, 0);

    QTest::newRow("TopToBottom, RtL, BtT")
            << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << QQuickItemView::BottomToTop
            << QPointF(0, -320) << QPointF(-30, -320)
            << QPointF(240 - 20, 0) << QPointF(240 - 20, 320)  // content flow is reversed
            << QPointF(-30, 0) << QPointF((-80.0 * 6) - 30, 0);
}

void tst_QQuickGridView::resetModel_headerFooter()
{
    // Resetting a model shouldn't crash in views with header/footer

    QQuickView *window = createView();

    QaimModel model;
    for (int i = 0; i < 6; i++)
        model.addItem("Item" + QString::number(i), "");
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("headerfooter.qml"));
    qApp->processEvents();

    QQuickGridView *gridview = qobject_cast<QQuickGridView*>(window->rootObject());
    QTRY_VERIFY(gridview != 0);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQuickItem *header = findItem<QQuickItem>(contentItem, "header");
    QVERIFY(header);
    QCOMPARE(header->y(), -header->height());

    QQuickItem *footer = findItem<QQuickItem>(contentItem, "footer");
    QVERIFY(footer);
    QCOMPARE(footer->y(), 60.*2);

    model.reset();

    // A reset should not force a new header or footer to be created.
    QQuickItem *newHeader = findItem<QQuickItem>(contentItem, "header");
    QCOMPARE(newHeader, header);
    QCOMPARE(header->y(), -header->height());

    QQuickItem *newFooter = findItem<QQuickItem>(contentItem, "footer");
    QCOMPARE(newFooter, footer);
    QCOMPARE(footer->y(), 60.*2);

    delete window;
}

void tst_QQuickGridView::resizeViewAndRepaint()
{
    QQuickView *window = createView();
    window->show();

    QaimModel model;
    for (int i = 0; i < 40; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("initialWidth", 240);
    ctxt->setContextProperty("initialHeight", 100);

    window->setSource(testFileUrl("resizeview.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    // item at index 10 should not be currently visible
    QVERIFY(!findItem<QQuickItem>(contentItem, "wrapper", 10));

    gridview->setHeight(320);
    QTRY_VERIFY(findItem<QQuickItem>(contentItem, "wrapper", 10));

    gridview->setHeight(100);
    QTRY_VERIFY(!findItem<QQuickItem>(contentItem, "wrapper", 10));

    // Ensure we handle -ve sizes
    gridview->setHeight(-100);
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper", false).count(), 3);

    gridview->setCacheBuffer(120);
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper", false).count(), 9);

    // ensure items in cache become visible
    gridview->setHeight(120);
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper", false).count(), 15);

    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->x(), qreal((i%3)*80));
        QTRY_COMPARE(item->y(), qreal((i/3)*60));
        QCOMPARE(delegateVisible(item), i < 9); // inside view visible, outside not visible
    }

    // ensure items outside view become invisible
    gridview->setHeight(60);
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper", false).count(), 12);

    itemCount = findItems<QQuickItem>(contentItem, "wrapper", false).count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->x(), qreal((i%3)*80));
        QTRY_COMPARE(item->y(), qreal((i/3)*60));
        QCOMPARE(delegateVisible(item), i < 6); // inside view visible, outside not visible
    }

    delete window;
}

void tst_QQuickGridView::resizeGrid()
{
    QFETCH(QQuickGridView::Flow, flow);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(QPointF, initialContentPos);
    QFETCH(QPointF, firstItemPos);

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testTopToBottom", flow == QQuickGridView::FlowTopToBottom);
    ctxt->setContextProperty("testRightToLeft", layoutDirection == Qt::RightToLeft);
    ctxt->setContextProperty("testBottomToTop", verticalLayoutDirection == QQuickGridView::BottomToTop);
    window->setSource(testFileUrl("resizegrid.qml"));
    QQuickViewTestUtil::centerOnScreen(window, window->size());
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    // set the width to slightly larger than 3 items across, to test
    // items are aligned correctly in right-to-left
    window->rootObject()->setWidth(260);
    window->rootObject()->setHeight(320);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QCOMPARE(gridview->contentX(), initialContentPos.x());
    QCOMPARE(gridview->contentY(), initialContentPos.y());

    QQuickItem *item0 = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item0);
    QCOMPARE(item0->position(), firstItemPos);

    // Confirm items positioned correctly and indexes correct
    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    QVERIFY(items.count() >= 18 && items.count() <= 21);
    for (int i = 0; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->position(), expectedItemPos(gridview, i, 0));
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QCOMPARE(name->text(), model.name(i));
    }

    // change from 3x5 grid to 4x7
    window->rootObject()->setWidth(window->rootObject()->width() + 80);
    window->rootObject()->setHeight(window->rootObject()->height() + 60*2);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    // other than in LeftToRight+RightToLeft layout, the first item should not move
    // if view is resized
    QCOMPARE(findItem<QQuickItem>(contentItem, "wrapper", 0), item0);
    if (flow == QQuickGridView::FlowLeftToRight && layoutDirection == Qt::RightToLeft)
        firstItemPos.rx() += 80;
    QCOMPARE(item0->position(), firstItemPos);

    QPointF newContentPos = initialContentPos;
    if (flow == QQuickGridView::FlowTopToBottom && layoutDirection == Qt::RightToLeft)
        newContentPos.rx() -= 80.0;
    if (verticalLayoutDirection == QQuickItemView::BottomToTop)
        newContentPos.ry() -= 60.0 * 2;
    QCOMPARE(gridview->contentX(), newContentPos.x());
    QCOMPARE(gridview->contentY(), newContentPos.y());

    // Confirm items positioned correctly and indexes correct
    items = findItems<QQuickItem>(contentItem, "wrapper");
    QVERIFY(items.count() >= 28);
    for (int i = 0; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->position(), expectedItemPos(gridview, i, 0));
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QCOMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickGridView::resizeGrid_data()
{
    QTest::addColumn<QQuickGridView::Flow>("flow");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickItemView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<QPointF>("initialContentPos");
    QTest::addColumn<QPointF>("firstItemPos");

    // Initial view width is 260, so in LeftToRight + right-to-left mode the
    // content x should be -20

    QTest::newRow("LeftToRight, LtR, TtB")
            << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << QQuickItemView::TopToBottom
            << QPointF(0, 0)
            << QPointF(0, 0);

    QTest::newRow("LeftToRight, RtL, TtB")
            << QQuickGridView::FlowLeftToRight << Qt::RightToLeft << QQuickItemView::TopToBottom
            << QPointF(-20.0, 0)
            << QPointF(80.0 * 2, 0);

    QTest::newRow("LeftToRight, LtR, BtT")
            << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << QQuickItemView::BottomToTop
            << QPointF(0, -320)
            << QPointF(0, -60.0);

    QTest::newRow("LeftToRight, RtL, BtT")
            << QQuickGridView::FlowLeftToRight << Qt::RightToLeft << QQuickItemView::BottomToTop
            << QPointF(-20.0, -320)
            << QPointF(80.0 * 2, -60.0);


    QTest::newRow("TopToBottom, LtR, TtB")
            << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << QQuickItemView::TopToBottom
            << QPointF(0, 0)
            << QPointF(0, 0);

    QTest::newRow("TopToBottom, RtL, TtB")
            << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << QQuickItemView::TopToBottom
            << QPointF(-260, 0)
            << QPointF(-80.0, 0);

    QTest::newRow("TopToBottom, LtR, BtT")
            << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << QQuickItemView::BottomToTop
            << QPointF(0, -320)
            << QPointF(0, -60.0);

    QTest::newRow("TopToBottom, RtL, BtT")
            << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << QQuickItemView::BottomToTop
            << QPointF(-260, -320)
            << QPointF(-80.0, -60.0);
}


void tst_QQuickGridView::changeColumnCount()
{
    QaimModel model;
    for (int i = 0; i < 40; i++)
        model.addItem("Item" + QString::number(i), "");

    QQuickView *window = createView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("initialWidth", 100);
    ctxt->setContextProperty("initialHeight", 320);
    window->setSource(testFileUrl("resizeview.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    // a single column of 6 items are visible
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    QCOMPARE(itemCount, 6);
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->x(), 0.0);
        QCOMPARE(item->y(), qreal(i*60));
    }

    // now 6x3 grid is visible, plus 1 extra below for refill
    gridview->setWidth(240);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    QCOMPARE(itemCount, 6*3 + 1);
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->x(), qreal((i%3)*80));
        QCOMPARE(item->y(), qreal((i/3)*60));
    }

    // back to single column
    gridview->setWidth(100);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    QCOMPARE(itemCount, 6);
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->x(), 0.0);
        QCOMPARE(item->y(), qreal(i*60));
    }

    delete window;
}

void tst_QQuickGridView::indexAt_itemAt_data()
{
    QTest::addColumn<qreal>("x");
    QTest::addColumn<qreal>("y");
    QTest::addColumn<int>("index");

    QTest::newRow("Item 0 - 0, 0") << 0. << 0. << 0;
    QTest::newRow("Item 0 - 79, 59") << 79. << 59. << 0;
    QTest::newRow("Item 1 - 80, 0") << 80. << 0. << 1;
    QTest::newRow("Item 3 - 0, 60") << 0. << 60. << 3;
    QTest::newRow("No Item - 240, 0") << 240. << 0. << -1;
}

void tst_QQuickGridView::indexAt_itemAt()
{
    QFETCH(qreal, x);
    QFETCH(qreal, y);
    QFETCH(int, index);

    QQuickView *window = getView();

    QaimModel model;
    model.addItem("Fred", "12345");
    model.addItem("John", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Billy", "22345");
    model.addItem("Sam", "2945");
    model.addItem("Ben", "04321");
    model.addItem("Jim", "0780");

    window->rootContext()->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("gridview1.qml"));
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QTRY_COMPARE(gridview->count(), model.count());

    QQuickItem *item = 0;
    if (index >= 0) {
        item = findItem<QQuickItem>(contentItem, "wrapper", index);
        QVERIFY(item);
    }
    QCOMPARE(gridview->indexAt(x, y), index);
    QVERIFY(gridview->itemAt(x, y) == item);

    releaseView(window);
}

void tst_QQuickGridView::onAdd()
{
    QFETCH(int, initialItemCount);
    QFETCH(int, itemsToAdd);

    const int delegateWidth = 50;
    const int delegateHeight = 100;
    QaimModel model;
    QQuickView *window = getView();
    window->setGeometry(0,0,5 * delegateWidth, 5 * delegateHeight); // just ensure all items fit

    // these initial items should not trigger GridView.onAdd
    for (int i=0; i<initialItemCount; i++)
        model.addItem("dummy value", "dummy value");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("delegateWidth", delegateWidth);
    ctxt->setContextProperty("delegateHeight", delegateHeight);
    window->setSource(testFileUrl("attachedSignals.qml"));

    QQuickGridView *gridview = qobject_cast<QQuickGridView*>(window->rootObject());
    gridview->setProperty("width", window->width());
    gridview->setProperty("height", window->height());
    qApp->processEvents();

    QList<QPair<QString, QString> > items;
    for (int i=0; i<itemsToAdd; i++)
        items << qMakePair(QString("value %1").arg(i), QString::number(i));
    model.addItems(items);

    gridview->forceLayout();
    QTRY_COMPARE(model.count(), gridview->count());
    qApp->processEvents();

    QVariantList result = gridview->property("addedDelegates").toList();
    QTRY_COMPARE(result.count(), items.count());
    for (int i=0; i<items.count(); i++)
        QCOMPARE(result[i].toString(), items[i].first);

    releaseView(window);
}

void tst_QQuickGridView::onAdd_data()
{
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<int>("itemsToAdd");

    QTest::newRow("0, add 1") << 0 << 1;
    QTest::newRow("0, add 2") << 0 << 2;
    QTest::newRow("0, add 10") << 0 << 10;

    QTest::newRow("1, add 1") << 1 << 1;
    QTest::newRow("1, add 2") << 1 << 2;
    QTest::newRow("1, add 10") << 1 << 10;

    QTest::newRow("5, add 1") << 5 << 1;
    QTest::newRow("5, add 2") << 5 << 2;
    QTest::newRow("5, add 10") << 5 << 10;
}

void tst_QQuickGridView::onRemove()
{
    QFETCH(int, initialItemCount);
    QFETCH(int, indexToRemove);
    QFETCH(int, removeCount);

    const int delegateWidth = 50;
    const int delegateHeight = 100;
    QaimModel model;
    for (int i=0; i<initialItemCount; i++)
        model.addItem(QString("value %1").arg(i), "dummy value");

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("delegateWidth", delegateWidth);
    ctxt->setContextProperty("delegateHeight", delegateHeight);
    window->setSource(testFileUrl("attachedSignals.qml"));
    QQuickGridView *gridview = qobject_cast<QQuickGridView*>(window->rootObject());

    model.removeItems(indexToRemove, removeCount);
    gridview->forceLayout();
    QTRY_COMPARE(model.count(), gridview->count());
    QCOMPARE(gridview->property("removedDelegateCount"), QVariant(removeCount));

    releaseView(window);
}

void tst_QQuickGridView::onRemove_data()
{
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<int>("indexToRemove");
    QTest::addColumn<int>("removeCount");

    QTest::newRow("remove first") << 1 << 0 << 1;
    QTest::newRow("two items, remove first") << 2 << 0 << 1;
    QTest::newRow("two items, remove last") << 2 << 1 << 1;
    QTest::newRow("two items, remove all") << 2 << 0 << 2;

    QTest::newRow("four items, remove first") << 4 << 0 << 1;
    QTest::newRow("four items, remove 0-2") << 4 << 0 << 2;
    QTest::newRow("four items, remove 1-3") << 4 << 1 << 2;
    QTest::newRow("four items, remove 2-4") << 4 << 2 << 2;
    QTest::newRow("four items, remove last") << 4 << 3 << 1;
    QTest::newRow("four items, remove all") << 4 << 0 << 4;

    QTest::newRow("ten items, remove 1-8") << 10 << 0 << 8;
    QTest::newRow("ten items, remove 2-7") << 10 << 2 << 5;
    QTest::newRow("ten items, remove 4-10") << 10 << 4 << 6;
}

void tst_QQuickGridView::attachedProperties_QTBUG_32836()
{
    QQuickView *window = createView();

    window->setSource(testFileUrl("attachedProperties.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = qobject_cast<QQuickGridView*>(window->rootObject());
    QVERIFY(gridview != 0);

    QQuickItem *header = gridview->headerItem();
    QVERIFY(header);
    QCOMPARE(header->width(), gridview->width());

    QQuickItem *footer = gridview->footerItem();
    QVERIFY(footer);
    QCOMPARE(footer->width(), gridview->width());

    QQuickItem *highlight = gridview->highlightItem();
    QVERIFY(highlight);
    QCOMPARE(highlight->width(), gridview->width());

    QQuickItem *currentItem = gridview->currentItem();
    QVERIFY(currentItem);
    QCOMPARE(currentItem->width(), gridview->width());

    delete window;
}

void tst_QQuickGridView::columnCount()
{
    QQuickView window;
    window.setSource(testFileUrl("gridview4.qml"));
    window.show();
    window.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&window));

    QQuickGridView *view = qobject_cast<QQuickGridView*>(window.rootObject());

    QCOMPARE(view->cellWidth(), qreal(405)/qreal(9));
    QCOMPARE(view->cellHeight(), qreal(100));

    QList<QQuickItem*> items = findItems<QQuickItem>(view, "delegate");
    QCOMPARE(items.size(), 18);
    QCOMPARE(items.at(8)->y(), qreal(0));
    QCOMPARE(items.at(9)->y(), qreal(100));
}

void tst_QQuickGridView::margins()
{
    {
        QQuickView *window = createView();

        QaimModel model;
        for (int i = 0; i < 40; i++)
            model.addItem("Item" + QString::number(i), "");

        QQmlContext *ctxt = window->rootContext();
        ctxt->setContextProperty("testModel", &model);
        ctxt->setContextProperty("testRightToLeft", QVariant(false));

        window->setSource(testFileUrl("margins.qml"));
        window->show();
        qApp->processEvents();

        QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
        QTRY_VERIFY(gridview != 0);
        QQuickItem *contentItem = gridview->contentItem();
        QTRY_VERIFY(contentItem != 0);
        QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

        QCOMPARE(gridview->contentX(), -30.);
        QCOMPARE(gridview->originX(), 0.);

        // check end bound
        gridview->positionViewAtEnd();
        qreal pos = gridview->contentX();
        gridview->setContentX(pos + 80);
        gridview->returnToBounds();
        QTRY_COMPARE(gridview->contentX(), pos + 50);

        // remove item before visible and check that left margin is maintained
        // and originX is updated
        gridview->setContentX(200);
        model.removeItems(0, 4);
        QTest::qWait(100);
        gridview->setContentX(-50);
        gridview->returnToBounds();
        QCOMPARE(gridview->originX(), 100.);
        QTRY_COMPARE(gridview->contentX(), 70.);

        // reduce left margin
        gridview->setLeftMargin(20);
        QCOMPARE(gridview->originX(), 100.);
        QTRY_COMPARE(gridview->contentX(), 80.);

        // check end bound
        gridview->positionViewAtEnd();
        QCOMPARE(gridview->originX(), 0.); // positionViewAtEnd() resets origin
        pos = gridview->contentX();
        gridview->setContentX(pos + 80);
        gridview->returnToBounds();
        QTRY_COMPARE(gridview->contentX(), pos + 50);

        // reduce right margin
        pos = gridview->contentX();
        gridview->setRightMargin(40);
        QCOMPARE(gridview->originX(), 0.);
        QTRY_COMPARE(gridview->contentX(), pos-10);

        delete window;
    }
    {
        //RTL
        QQuickView *window = createView();
        window->show();

        QaimModel model;
        for (int i = 0; i < 40; i++)
            model.addItem("Item" + QString::number(i), "");

        QQmlContext *ctxt = window->rootContext();
        ctxt->setContextProperty("testModel", &model);
        ctxt->setContextProperty("testRightToLeft", QVariant(true));

        window->setSource(testFileUrl("margins.qml"));
        qApp->processEvents();

        QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
        QTRY_VERIFY(gridview != 0);

        QQuickItem *contentItem = gridview->contentItem();
        QTRY_VERIFY(contentItem != 0);

        QTRY_COMPARE(gridview->contentX(), -240+50.);
        QTRY_COMPARE(gridview->originX(), -100. * 10);

        // check end bound
        gridview->positionViewAtEnd();
        qreal pos = gridview->contentX();
        gridview->setContentX(pos - 80);
        gridview->returnToBounds();
        QTRY_COMPARE(gridview->contentX(), pos - 30);

        // remove item before visible and check that left margin is maintained
        // and originX is updated
        gridview->setContentX(-400);
        QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
        model.removeItems(0, 4);
        gridview->forceLayout();
        QTRY_COMPARE(model.count(), gridview->count());
        gridview->setContentX(-240+50);
        gridview->returnToBounds();
        QCOMPARE(gridview->originX(), -1000.);
        QTRY_COMPARE(gridview->contentX(), -240-50.);

        // reduce right margin
        pos = gridview->contentX();
        gridview->setRightMargin(40);
        QCOMPARE(gridview->originX(), -1000.);
        QTRY_COMPARE(gridview->contentX(), -240-100 + 40.);

        // check end bound
        gridview->positionViewAtEnd();
        QCOMPARE(gridview->originX(), -900.); // positionViewAtEnd() resets origin
        pos = gridview->contentX();
        gridview->setContentX(pos - 80);
        gridview->returnToBounds();
        QTRY_COMPARE(gridview->contentX(), pos - 30);

        // reduce left margin
        pos = gridview->contentX();
        gridview->setLeftMargin(20);
        QCOMPARE(gridview->originX(), -900.);
        QTRY_COMPARE(gridview->contentX(), pos+10);

        delete window;
    }
}

void tst_QQuickGridView::creationContext()
{
    QQuickView window;
    window.setGeometry(0,0,240,320);
    window.setSource(testFileUrl("creationContext.qml"));
    qApp->processEvents();

    QQuickItem *rootItem = qobject_cast<QQuickItem *>(window.rootObject());
    QVERIFY(rootItem);
    QVERIFY(rootItem->property("count").toInt() > 0);

    QQuickItem *item;
    QVERIFY(item = findItem<QQuickItem>(rootItem, "listItem"));
    QCOMPARE(item->property("text").toString(), QString("Hello!"));
    QVERIFY(item = rootItem->findChild<QQuickItem *>("header"));
    QCOMPARE(item->property("text").toString(), QString("Hello!"));
    QVERIFY(item = rootItem->findChild<QQuickItem *>("footer"));
    QCOMPARE(item->property("text").toString(), QString("Hello!"));
}

void tst_QQuickGridView::snapToRow_data()
{
    QTest::addColumn<QQuickGridView::Flow>("flow");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<int>("highlightRangeMode");
    QTest::addColumn<QPoint>("flickStart");
    QTest::addColumn<QPoint>("flickEnd");
    QTest::addColumn<qreal>("snapAlignment");
    QTest::addColumn<qreal>("endExtent");
    QTest::addColumn<qreal>("startExtent");

    QTest::newRow("vertical, left to right") << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << int(QQuickItemView::NoHighlightRange)
        << QPoint(20, 200) << QPoint(20, 20) << 60.0 << 800.0 << 0.0;

    QTest::newRow("horizontal, left to right") << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << int(QQuickItemView::NoHighlightRange)
        << QPoint(200, 20) << QPoint(20, 20) << 60.0 << 800.0 << 0.0;

    QTest::newRow("horizontal, right to left") << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << int(QQuickItemView::NoHighlightRange)
        << QPoint(20, 20) << QPoint(200, 20) << -60.0 << -800.0 - 240.0 << -240.0;

    QTest::newRow("vertical, left to right, enforce range") << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(20, 200) << QPoint(20, 20) << 60.0 << 940.0 << -20.0;

    QTest::newRow("horizontal, left to right, enforce range") << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(200, 20) << QPoint(20, 20) << 60.0 << 940.0 << -20.0;

    QTest::newRow("horizontal, right to left, enforce range") << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(20, 20) << QPoint(200, 20) << -60.0 << -800.0 - 240.0 - 140.0 << -220.0;
}

void tst_QQuickGridView::snapToRow()
{
    QFETCH(QQuickGridView::Flow, flow);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(int, highlightRangeMode);
    QFETCH(QPoint, flickStart);
    QFETCH(QPoint, flickEnd);
    QFETCH(qreal, snapAlignment);
    QFETCH(qreal, endExtent);
    QFETCH(qreal, startExtent);

    QQuickView *window = getView();

    QQuickViewTestUtil::moveMouseAway(window);
    window->setSource(testFileUrl("snapToRow.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    gridview->setFlow(flow);
    gridview->setLayoutDirection(layoutDirection);
    gridview->setHighlightRangeMode(QQuickItemView::HighlightRangeMode(highlightRangeMode));
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    qreal origContentY = gridview->contentY();
    qreal origContentX = gridview->contentX();
    // confirm that a flick hits an item boundary
    flick(window, flickStart, flickEnd, 180);

    // wait until it's at least one cell further
    QTRY_VERIFY(qAbs(gridview->contentX() - origContentX) > 80 ||
                qAbs(gridview->contentY() - origContentY) > 80);

    // click to stop it. Otherwise we wouldn't know how much further it will go. We don't want to it
    // to hit the endExtent, yet.
    QTest::mouseClick(window, Qt::LeftButton, 0, flickEnd);

    QTRY_VERIFY(gridview->isMoving() == false); // wait until it stops
    if (flow == QQuickGridView::FlowLeftToRight)
        QCOMPARE(qreal(fmod(gridview->contentY(),80.0)), snapAlignment);
    else
        QCOMPARE(qreal(fmod(gridview->contentX(),80.0)), snapAlignment);

    // flick to end
    do {
        flick(window, flickStart, flickEnd, 180);
        QTRY_VERIFY(gridview->isMoving() == false); // wait until it stops
    } while (flow == QQuickGridView::FlowLeftToRight
           ? !gridview->isAtYEnd()
           : layoutDirection == Qt::LeftToRight ? !gridview->isAtXEnd() : !gridview->isAtXBeginning());

    if (flow == QQuickGridView::FlowLeftToRight)
        QCOMPARE(gridview->contentY(), endExtent);
    else
        QCOMPARE(gridview->contentX(), endExtent);

    // flick to start
    do {
        flick(window, flickEnd, flickStart, 180);
        QTRY_VERIFY(gridview->isMoving() == false); // wait until it stops
    } while (flow == QQuickGridView::FlowLeftToRight
           ? !gridview->isAtYBeginning()
           : layoutDirection == Qt::LeftToRight ? !gridview->isAtXBeginning() : !gridview->isAtXEnd());

    if (flow == QQuickGridView::FlowLeftToRight)
        QCOMPARE(gridview->contentY(), startExtent);
    else
        QCOMPARE(gridview->contentX(), startExtent);

    releaseView(window);
}

void tst_QQuickGridView::snapOneRow_data()
{
    QTest::addColumn<QQuickGridView::Flow>("flow");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<int>("highlightRangeMode");
    QTest::addColumn<QPoint>("flickStart");
    QTest::addColumn<QPoint>("flickEnd");
    QTest::addColumn<qreal>("snapAlignment");
    QTest::addColumn<qreal>("endExtent");
    QTest::addColumn<qreal>("startExtent");

    QTest::newRow("vertical, left to right") << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << int(QQuickItemView::NoHighlightRange)
        << QPoint(20, 160) << QPoint(20, 20) << 100.0 << 240.0 << 0.0;

    QTest::newRow("horizontal, left to right") << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << int(QQuickItemView::NoHighlightRange)
        << QPoint(160, 20) << QPoint(20, 20) << 100.0 << 240.0 << 0.0;

    QTest::newRow("horizontal, right to left") << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << int(QQuickItemView::NoHighlightRange)
        << QPoint(20, 20) << QPoint(160, 20) << -340.0 << -240.0 - 240.0 << -240.0;

    QTest::newRow("vertical, left to right, enforce range") << QQuickGridView::FlowLeftToRight << Qt::LeftToRight << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(20, 160) << QPoint(20, 20) << 100.0 << 340.0 << -20.0;

    QTest::newRow("horizontal, left to right, enforce range") << QQuickGridView::FlowTopToBottom << Qt::LeftToRight << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(160, 20) << QPoint(20, 20) << 100.0 << 340.0 << -20.0;

    QTest::newRow("horizontal, right to left, enforce range") << QQuickGridView::FlowTopToBottom << Qt::RightToLeft << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(20, 20) << QPoint(160, 20) << -340.0 << -240.0 - 240.0 - 100.0 << -220.0;
}

void tst_QQuickGridView::snapOneRow()
{
    QFETCH(QQuickGridView::Flow, flow);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(int, highlightRangeMode);
    QFETCH(QPoint, flickStart);
    QFETCH(QPoint, flickEnd);
    QFETCH(qreal, snapAlignment);
    QFETCH(qreal, endExtent);
    QFETCH(qreal, startExtent);

    QQuickView *window = getView();
    QQuickViewTestUtil::moveMouseAway(window);

    window->setSource(testFileUrl("snapOneRow.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);

    gridview->setFlow(flow);
    gridview->setLayoutDirection(layoutDirection);
    gridview->setHighlightRangeMode(QQuickItemView::HighlightRangeMode(highlightRangeMode));
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QQuickItem *contentItem = gridview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QSignalSpy currentIndexSpy(gridview, SIGNAL(currentIndexChanged()));

    // confirm that a flick hits next row boundary
    flick(window, flickStart, flickEnd, 180);
    QTRY_VERIFY(gridview->isMoving() == false); // wait until it stops
    if (flow == QQuickGridView::FlowLeftToRight)
        QCOMPARE(gridview->contentY(), snapAlignment);
    else
        QCOMPARE(gridview->contentX(), snapAlignment);

    if (QQuickItemView::HighlightRangeMode(highlightRangeMode) == QQuickItemView::StrictlyEnforceRange) {
        QCOMPARE(gridview->currentIndex(), 2);
        QCOMPARE(currentIndexSpy.count(), 1);
    }

    // flick to end
    do {
        flick(window, flickStart, flickEnd, 180);
        QTRY_VERIFY(gridview->isMoving() == false); // wait until it stops
    } while (flow == QQuickGridView::FlowLeftToRight
           ? !gridview->isAtYEnd()
           : layoutDirection == Qt::LeftToRight ? !gridview->isAtXEnd() : !gridview->isAtXBeginning());

    if (QQuickItemView::HighlightRangeMode(highlightRangeMode) == QQuickItemView::StrictlyEnforceRange) {
        QCOMPARE(gridview->currentIndex(), 6);
        QCOMPARE(currentIndexSpy.count(), 3);
    }

    if (flow == QQuickGridView::FlowLeftToRight)
        QCOMPARE(gridview->contentY(), endExtent);
    else
        QCOMPARE(gridview->contentX(), endExtent);

    // flick to start
    do {
        flick(window, flickEnd, flickStart, 180);
        QTRY_VERIFY(gridview->isMoving() == false); // wait until it stops
    } while (flow == QQuickGridView::FlowLeftToRight
           ? !gridview->isAtYBeginning()
           : layoutDirection == Qt::LeftToRight ? !gridview->isAtXBeginning() : !gridview->isAtXEnd());

    if (flow == QQuickGridView::FlowLeftToRight)
        QCOMPARE(gridview->contentY(), startExtent);
    else
        QCOMPARE(gridview->contentX(), startExtent);

    if (QQuickItemView::HighlightRangeMode(highlightRangeMode) == QQuickItemView::StrictlyEnforceRange) {
        QCOMPARE(gridview->currentIndex(), 0);
        QCOMPARE(currentIndexSpy.count(), 6);
    }

    releaseView(window);
}


void tst_QQuickGridView::unaligned()
{
    QQuickView *window = createView();
    window->show();

    QaimModel model;
    for (int i = 0; i < 10; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("unaligned.qml"));
    qApp->processEvents();

    QQuickGridView *gridview = qobject_cast<QQuickGridView*>(window->rootObject());
    QVERIFY(gridview != 0);

    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);

    for (int i = 0; i < 10; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QVERIFY(item);
        QCOMPARE(item->x(), qreal((i%9)*gridview->cellWidth()));
        QCOMPARE(item->y(), qreal((i/9)*gridview->cellHeight()));
    }

    // appending
    for (int i = 10; i < 18; ++i) {
        model.addItem("Item" + QString::number(i), "");
        QQuickItem *item = 0;
        QTRY_VERIFY(item = findItem<QQuickItem>(contentItem, "wrapper", i));
        QCOMPARE(item->x(), qreal((i%9)*gridview->cellWidth()));
        QCOMPARE(item->y(), qreal((i/9)*gridview->cellHeight()));
    }

    // inserting
    for (int i = 0; i < 10; ++i) {
        model.insertItem(i, "Item" + QString::number(i), "");
        QQuickItem *item = 0;
        QTRY_VERIFY(item = findItem<QQuickItem>(contentItem, "wrapper", i));
        QCOMPARE(item->x(), qreal((i%9)*gridview->cellWidth()));
        QCOMPARE(item->y(), qreal((i/9)*gridview->cellHeight()));
    }

    // removing
    model.removeItems(7, 10);
    gridview->forceLayout();
    QTRY_COMPARE(model.count(), gridview->count());
    for (int i = 0; i < 18; ++i) {
        QQuickItem *item = 0;
        QTRY_VERIFY(item = findItem<QQuickItem>(contentItem, "wrapper", i));
        QCOMPARE(item->x(), qreal(i%9)*gridview->cellWidth());
        QCOMPARE(item->y(), qreal(i/9)*gridview->cellHeight());
    }

    delete window;
}

void tst_QQuickGridView::populateTransitions()
{
    QFETCH(bool, staticallyPopulate);
    QFETCH(bool, dynamicallyPopulate);
    QFETCH(bool, usePopulateTransition);

    QPointF transitionFrom(-50, -50);
    QPointF transitionVia(100, 100);
    QaimModel model_transitionFrom;
    QaimModel model_transitionVia;

    QaimModel model;
    if (staticallyPopulate) {
        for (int i = 0; i < 30; i++)
            model.addItem("item" + QString::number(i), "");
    }

    QQuickView *window = getView();
    window->rootContext()->setContextProperty("testModel", &model);
    window->rootContext()->setContextProperty("usePopulateTransition", usePopulateTransition);
    window->rootContext()->setContextProperty("dynamicallyPopulate", dynamicallyPopulate);
    window->rootContext()->setContextProperty("transitionFrom", transitionFrom);
    window->rootContext()->setContextProperty("transitionVia", transitionVia);
    window->rootContext()->setContextProperty("model_transitionFrom", &model_transitionFrom);
    window->rootContext()->setContextProperty("model_transitionVia", &model_transitionVia);
    window->setSource(testFileUrl("populateTransitions.qml"));
    window->show();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QVERIFY(gridview);
    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem);

    // check the populate transition is run
    if (staticallyPopulate && usePopulateTransition) {
        QTRY_COMPARE(gridview->property("countPopulateTransitions").toInt(), 18);
        QTRY_COMPARE(gridview->property("countAddTransitions").toInt(), 0);
    } else if (dynamicallyPopulate) {
        QTRY_COMPARE(gridview->property("countPopulateTransitions").toInt(), 0);
        QTRY_COMPARE(gridview->property("countAddTransitions").toInt(), 18);
    } else {
        QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
        QCOMPARE(gridview->property("countPopulateTransitions").toInt(), 0);
        QCOMPARE(gridview->property("countAddTransitions").toInt(), 0);
    }

    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i=0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->x(), (i%3)*80.0);
        QTRY_COMPARE(item->y(), (i/3)*60.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    gridview->setProperty("countPopulateTransitions", 0);
    gridview->setProperty("countAddTransitions", 0);

    // add an item and check this is done with add transition, not populate
    model.insertItem(0, "another item", "");
    QTRY_COMPARE(gridview->property("countAddTransitions").toInt(), 1);
    QTRY_COMPARE(gridview->property("countPopulateTransitions").toInt(), 0);

    // clear the model
    window->rootContext()->setContextProperty("testModel", QVariant());
    QTRY_COMPARE(gridview->count(), 0);
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper").count(), 0);
    gridview->setProperty("countPopulateTransitions", 0);
    gridview->setProperty("countAddTransitions", 0);

    // set to a valid model and check populate transition is run a second time
    model.clear();
    for (int i = 0; i < 30; i++)
        model.addItem("item" + QString::number(i), "");
    window->rootContext()->setContextProperty("testModel", &model);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QTRY_COMPARE(gridview->property("countPopulateTransitions").toInt(), usePopulateTransition ? 18 : 0);
    QTRY_COMPARE(gridview->property("countAddTransitions").toInt(), 0);

    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i=0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->x(), (i%3)*80.0);
        QTRY_COMPARE(item->y(), (i/3)*60.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    // reset model and check populate transition is run again
    gridview->setProperty("countPopulateTransitions", 0);
    gridview->setProperty("countAddTransitions", 0);
    model.reset();
    QTRY_COMPARE(gridview->property("countPopulateTransitions").toInt(), usePopulateTransition ? 18 : 0);
    QTRY_COMPARE(gridview->property("countAddTransitions").toInt(), 0);

    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i=0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->x(), (i%3)*80.0);
        QTRY_COMPARE(item->y(), (i/3)*60.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickGridView::populateTransitions_data()
{
    QTest::addColumn<bool>("staticallyPopulate");
    QTest::addColumn<bool>("dynamicallyPopulate");
    QTest::addColumn<bool>("usePopulateTransition");

    QTest::newRow("static") << true << false << true;
    QTest::newRow("static, no populate") << true << false << false;

    QTest::newRow("dynamic") << false << true << true;
    QTest::newRow("dynamic, no populate") << false << true << false;

    QTest::newRow("empty to start with") << false << false << true;
    QTest::newRow("empty to start with, no populate") << false << false << false;
}

void tst_QQuickGridView::addTransitions()
{
    QFETCH(int, initialItemCount);
    QFETCH(bool, shouldAnimateTargets);
    QFETCH(qreal, contentYRowOffset);
    QFETCH(int, insertionIndex);
    QFETCH(int, insertionCount);
    QFETCH(ListRange, expectedDisplacedIndexes);

    // added items should start here
    QPointF targetItems_transitionFrom(-50, -50);

    // displaced items should pass through this point
    QPointF displacedItems_transitionVia(100, 100);

    QaimModel model;
    for (int i = 0; i < initialItemCount; i++)
        model.addItem("Original item" + QString::number(i), "");
    QaimModel model_targetItems_transitionFrom;
    QaimModel model_displacedItems_transitionVia;

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("model_targetItems_transitionFrom", &model_targetItems_transitionFrom);
    ctxt->setContextProperty("model_displacedItems_transitionVia", &model_displacedItems_transitionVia);
    ctxt->setContextProperty("targetItems_transitionFrom", targetItems_transitionFrom);
    ctxt->setContextProperty("displacedItems_transitionVia", displacedItems_transitionVia);
    window->setSource(testFileUrl("addTransitions.qml"));
    window->show();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    if (contentYRowOffset != 0) {
        gridview->setContentY(contentYRowOffset * 60.0);
        QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
    }

    QList<QPair<QString,QString> > expectedDisplacedValues = expectedDisplacedIndexes.getModelDataValues(model);

    // only target items that will become visible should be animated
    QList<QPair<QString, QString> > newData;
    QList<QPair<QString, QString> > expectedTargetData;
    QList<int> targetIndexes;
    if (shouldAnimateTargets) {
        for (int i=insertionIndex; i<insertionIndex+insertionCount; i++) {
            newData << qMakePair(QString("New item %1").arg(i), QString(""));

            // last visible item is the first item of the row beneath the view
            if (i >= (gridview->contentY() / 60)*3 && i < qCeil((gridview->contentY() + gridview->height()) / 60.0)*3) {
                expectedTargetData << newData.last();
                targetIndexes << i;
            }
        }
        QVERIFY(expectedTargetData.count() > 0);
    }

    // start animation
    if (!newData.isEmpty()) {
        model.insertItems(insertionIndex, newData);
        gridview->forceLayout();
        QTRY_COMPARE(model.count(), gridview->count());
    }

    QList<QQuickItem *> targetItems = findItems<QQuickItem>(contentItem, "wrapper", targetIndexes);

    if (shouldAnimateTargets) {
        QTRY_COMPARE(gridview->property("targetTransitionsDone").toInt(), expectedTargetData.count());
        QTRY_COMPARE(gridview->property("displaceTransitionsDone").toInt(),
                     expectedDisplacedIndexes.isValid() ? expectedDisplacedIndexes.count() : 0);

        // check the target and displaced items were animated
        model_targetItems_transitionFrom.matchAgainst(expectedTargetData, "wasn't animated from target 'from' pos", "shouldn't have been animated from target 'from' pos");
        model_displacedItems_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with displaced anim", "shouldn't have been animated with displaced anim");

        // check attached properties
        matchItemsAndIndexes(gridview->property("targetTrans_items").toMap(), model, targetIndexes);
        matchIndexLists(gridview->property("targetTrans_targetIndexes").toList(), targetIndexes);
        matchItemLists(gridview->property("targetTrans_targetItems").toList(), targetItems);
        if (expectedDisplacedIndexes.isValid()) {
            // adjust expectedDisplacedIndexes to their final values after the move
            QList<int> displacedIndexes = adjustIndexesForAddDisplaced(expectedDisplacedIndexes.indexes, insertionIndex, insertionCount);
            matchItemsAndIndexes(gridview->property("displacedTrans_items").toMap(), model, displacedIndexes);
            matchIndexLists(gridview->property("displacedTrans_targetIndexes").toList(), targetIndexes);
            matchItemLists(gridview->property("displacedTrans_targetItems").toList(), targetItems);
        }
    } else {
        QTRY_COMPARE(model_targetItems_transitionFrom.count(), 0);
        QTRY_COMPARE(model_displacedItems_transitionVia.count(), 0);
    }

    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    int firstVisibleIndex = -1;
    for (int i=0; i<items.count(); i++) {
        if (items[i]->y() >= gridview->contentY()) {
            QQmlExpression e(qmlContext(items[i]), items[i], "index");
            firstVisibleIndex = e.evaluate().toInt();
            break;
        }
    }
    QVERIFY2(firstVisibleIndex >= 0, QTest::toString(firstVisibleIndex));

    // verify all items moved to the correct final positions
    for (int i = firstVisibleIndex; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->x(), (i%3)*80.0);
        QCOMPARE(item->y(), (i/3)*60.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QCOMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickGridView::addTransitions_data()
{
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<qreal>("contentYRowOffset");
    QTest::addColumn<bool>("shouldAnimateTargets");
    QTest::addColumn<int>("insertionIndex");
    QTest::addColumn<int>("insertionCount");
    QTest::addColumn<ListRange>("expectedDisplacedIndexes");

    // if inserting a full row before visible index, items don't appear or animate in, even if there are > 1 new items
    QTest::newRow("insert 1, just before start")
            << 30 << 1.0 << false
            << 0 << 1 << ListRange();
    QTest::newRow("insert 1, way before start")
            << 30 << 1.0 << false
            << 0 << 1 << ListRange();
    QTest::newRow("insert multiple, just before start")
            << 30 << 1.0 << false
            << 0 << 3 << ListRange();
    QTest::newRow("insert multiple (< 1 row), just before start")
            << 30 << 1.0 << false
            << 0 << 2 << ListRange();
    QTest::newRow("insert multiple, way before start")
            << 30 << 3.0 << false
            << 0 << 3 << ListRange();

    QTest::newRow("insert 1 at start")
            << 30 << 0.0 << true
            << 0 << 1 << ListRange(0, 17);
    QTest::newRow("insert multiple at start")
            << 30 << 0.0 << true
            << 0 << 3 << ListRange(0, 17);
    QTest::newRow("insert multiple (> 1 row) at start")
            << 30 << 0.0 << true
            << 0 << 5 << ListRange(0, 17);
    QTest::newRow("insert 1 at start, content y not 0")
            << 30 << 1.0 << true  // first visible is index 3
            << 3 << 1 << ListRange(0 + 3, 17 + 3);
    QTest::newRow("insert multiple at start, content y not 0")
            << 30 << 1.0 << true    // first visible is index 3
            << 3 << 3 << ListRange(0 + 3, 17 + 3);
    QTest::newRow("insert multiple (> 1 row) at start, content y not 0")
            << 30 << 1.0 << true    // first visible is index 3
            << 3 << 5 << ListRange(0 + 3, 17 + 3);

    QTest::newRow("insert 1 at start, to empty grid")
            << 0 << 0.0 << true
            << 0 << 1 << ListRange();
    QTest::newRow("insert multiple at start, to empty grid")
            << 0 << 0.0 << true
            << 0 << 3 << ListRange();

    QTest::newRow("insert 1 at middle")
            << 30 << 0.0 << true
            << 7 << 1 << ListRange(7, 17);
    QTest::newRow("insert multiple at middle")
            << 30 << 0.0 << true
            << 7 << 3 << ListRange(7, 17);
    QTest::newRow("insert multiple (> 1 row) at middle")
            << 30 << 0.0 << true
            << 7 << 5 << ListRange(7, 17);

    QTest::newRow("insert 1 at bottom")
            << 30 << 0.0 << true
            << 17 << 1 << ListRange(17, 17);
    QTest::newRow("insert multiple at bottom")
            << 30 << 0.0 << true
            << 17 << 3 << ListRange(17, 17);
    QTest::newRow("insert 1 at bottom, content y not 0")
            << 30 << 1.0 << true
            << 17 + 3 << 1 << ListRange(17 + 3, 17 + 3);
    QTest::newRow("insert multiple at bottom, content y not 0")
            << 30 << 1.0 << true
            << 17 + 3 << 3 << ListRange(17 + 3, 17 + 3);


    // items added after the last visible will not be animated in, since they
    // do not appear in the final view
    QTest::newRow("insert 1 after end")
            << 30 << 0.0 << false
            << 18 << 1 << ListRange();
    QTest::newRow("insert multiple after end")
            << 30 << 0.0 << false
            << 18 << 3 << ListRange();
}

void tst_QQuickGridView::moveTransitions()
{
    QFETCH(int, initialItemCount);
    QFETCH(qreal, contentYRowOffset);
    QFETCH(qreal, rowOffsetAfterMove);
    QFETCH(int, moveFrom);
    QFETCH(int, moveTo);
    QFETCH(int, moveCount);
    QFETCH(ListRange, expectedDisplacedIndexes);

    // target and displaced items should pass through these points
    QPointF targetItems_transitionVia(-50, 50);
    QPointF displacedItems_transitionVia(100, 100);

    QaimModel model;
    for (int i = 0; i < initialItemCount; i++)
        model.addItem("Original item" + QString::number(i), "");
    QaimModel model_targetItems_transitionVia;
    QaimModel model_displacedItems_transitionVia;

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("model_targetItems_transitionVia", &model_targetItems_transitionVia);
    ctxt->setContextProperty("model_displacedItems_transitionVia", &model_displacedItems_transitionVia);
    ctxt->setContextProperty("targetItems_transitionVia", targetItems_transitionVia);
    ctxt->setContextProperty("displacedItems_transitionVia", displacedItems_transitionVia);
    window->setSource(testFileUrl("moveTransitions.qml"));
    window->show();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);
    QQuickText *name;

    if (contentYRowOffset != 0) {
        gridview->setContentY(contentYRowOffset * 60.0);
        QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
    }

    QList<QPair<QString,QString> > expectedDisplacedValues = expectedDisplacedIndexes.getModelDataValues(model);

    // Items moving to *or* from visible positions should be animated.
    // Otherwise, they should not be animated.
    QList<QPair<QString, QString> > expectedTargetData;
    QList<int> targetIndexes;
    for (int i=moveFrom; i<moveFrom+moveCount; i++) {
        int toIndex = moveTo + (i - moveFrom);
        int firstVisibleIndex = (gridview->contentY() / 60) * 3;
        int lastVisibleIndex = (qCeil((gridview->contentY() + gridview->height()) / 60.0)*3) - 1;
        if ((i >= firstVisibleIndex && i <= lastVisibleIndex)
                || (toIndex >= firstVisibleIndex && toIndex <= lastVisibleIndex)) {
            expectedTargetData << qMakePair(model.name(i), model.number(i));
            targetIndexes << i;
        }
    }
    // ViewTransition.index provides the indices that items are moving to, not from
    targetIndexes = adjustIndexesForMove(targetIndexes, moveFrom, moveTo, moveCount);

    // start animation
    model.moveItems(moveFrom, moveTo, moveCount);
    gridview->forceLayout();

    QTRY_COMPARE(gridview->property("targetTransitionsDone").toInt(), expectedTargetData.count());
    QTRY_COMPARE(gridview->property("displaceTransitionsDone").toInt(),
                 expectedDisplacedIndexes.isValid() ? expectedDisplacedIndexes.count() : 0);

    QList<QQuickItem *> targetItems = findItems<QQuickItem>(contentItem, "wrapper", targetIndexes);

    // check the target and displaced items were animated
    model_targetItems_transitionVia.matchAgainst(expectedTargetData, "wasn't animated from target 'from' pos", "shouldn't have been animated from target 'from' pos");
    model_displacedItems_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with displaced anim", "shouldn't have been animated with displaced anim");

    // check attached properties
    matchItemsAndIndexes(gridview->property("targetTrans_items").toMap(), model, targetIndexes);
    matchIndexLists(gridview->property("targetTrans_targetIndexes").toList(), targetIndexes);
    matchItemLists(gridview->property("targetTrans_targetItems").toList(), targetItems);
    if (expectedDisplacedIndexes.isValid()) {
        // adjust expectedDisplacedIndexes to their final values after the move
        QList<int> displacedIndexes = adjustIndexesForMove(expectedDisplacedIndexes.indexes, moveFrom, moveTo, moveCount);
        matchItemsAndIndexes(gridview->property("displacedTrans_items").toMap(), model, displacedIndexes);
        matchIndexLists(gridview->property("displacedTrans_targetIndexes").toList(), targetIndexes);
        matchItemLists(gridview->property("displacedTrans_targetItems").toList(), targetItems);
    }

    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    int firstVisibleIndex = -1;
    for (int i=0; i<items.count(); i++) {
        if (items[i]->y() >= gridview->contentY()) {
            QQmlExpression e(qmlContext(items[i]), items[i], "index");
            firstVisibleIndex = e.evaluate().toInt();
            break;
        }
    }
    QVERIFY2(firstVisibleIndex >= 0, QTest::toString(firstVisibleIndex));

    // verify all items moved to the correct final positions
    qreal pixelOffset = 60 * rowOffsetAfterMove;
    for (int i=firstVisibleIndex; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->x(), (i%3)*80.0);
        QCOMPARE(item->y(), (i/3)*60.0 + pixelOffset);
        name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickGridView::moveTransitions_data()
{
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<qreal>("contentYRowOffset");
    QTest::addColumn<qreal>("rowOffsetAfterMove");
    QTest::addColumn<int>("moveFrom");
    QTest::addColumn<int>("moveTo");
    QTest::addColumn<int>("moveCount");
    QTest::addColumn<ListRange>("expectedDisplacedIndexes");

    QTest::newRow("move from above view, outside visible items, move 1")
            << 30 << 2.0 << 0.0
            << 1 << 10 << 1 << ListRange(6, 10);
    QTest::newRow("move from above view, outside visible items, move 1 (first item)")
            << 30 << 2.0 << 0.0
            << 0 << 10 << 1 << ListRange(6, 10);
    QTest::newRow("move from above view, outside visible items, move multiple")
            << 30 << 2.0 << 1.0
            << 1 << 10 << 3 << ListRange(13, 23);
    QTest::newRow("move from above view, mix of visible/non-visible")
            << 30 << 2.0 << 1.0
            << 1 << 10 << 6 << (ListRange(7, 15) + ListRange(16, 23));
    QTest::newRow("move from above view, mix of visible/non-visible (move first)")
            << 30 << 2.0 << 2.0
            << 0 << 10 << 6 << ListRange(16, 23);

    QTest::newRow("move within view, move 1 down")
            << 30 << 0.0 << 0.0
            << 1 << 10 << 1 << ListRange(2, 10);
    QTest::newRow("move within view, move 1 down, move first item")
            << 30 << 0.0 << 0.0
            << 0 << 10 << 1 << ListRange(1, 10);
    QTest::newRow("move within view, move 1 down, move first item, contentY not 0")
            << 30 << 2.0 << 0.0
            << 0+6 << 10+6 << 1 << ListRange(1+6, 10+6);
    QTest::newRow("move within view, move 1 down, to last item")
            << 30 << 0.0 << 0.0
            << 10 << 17 << 1 << ListRange(11, 17);
    QTest::newRow("move within view, move first->last")
            << 30 << 0.0 << 0.0
            << 0 << 17 << 1 << ListRange(1, 17);

    QTest::newRow("move within view, move multiple down")
            << 30 << 0.0 << 0.0
            << 1 << 10 << 3 << ListRange(4, 12);
    QTest::newRow("move within view, move multiple down, move first item")
            << 30 << 0.0 << 0.0
            << 0 << 10 << 3 << ListRange(3, 12);
    QTest::newRow("move within view, move multiple down, move first item, contentY not 0")
            << 30 << 1.0 << 0.0
            << 0+3 << 10+3 << 3 << ListRange(3+3, 12+3);
    QTest::newRow("move within view, move multiple down, displace last item")
            << 30 << 0.0 << 0.0
            << 5 << 15 << 3 << ListRange(8, 17);
    QTest::newRow("move within view, move multiple down, move first->last")
            << 30 << 0.0 << 0.0
            << 0 << 15 << 3 << ListRange(3, 17);

    QTest::newRow("move within view, move 1 up")
            << 30 << 0.0 << 0.0
            << 10 << 1 << 1 << ListRange(1, 9);
    QTest::newRow("move within view, move 1 up, move to first index")
            << 30 << 0.0 << 0.0
            << 10 << 0 << 1 << ListRange(0, 9);
    QTest::newRow("move within view, move 1 up, move to first index, contentY not 0")
            << 30 << 2.0 << 0.0
            << 10+6 << 0+6 << 1 << ListRange(0+6, 9+6);
    QTest::newRow("move within view, move 1 up, move to first index, contentY not on item border")
            << 30 << 1.5 << 0.0
            << 10+3 << 0+3 << 1 << ListRange(0+3, 9+3);
    QTest::newRow("move within view, move 1 up, move last item")
            << 30 << 0.0 << 0.0
            << 17 << 10 << 1 << ListRange(10, 16);
    QTest::newRow("move within view, move 1 up, move last->first")
            << 30 << 0.0 << 0.0
            << 17 << 0 << 1 << ListRange(0, 16);

    QTest::newRow("move within view, move multiple up")
            << 30 << 0.0 << 0.0
            << 10 << 1 << 3 << ListRange(1, 9);
    QTest::newRow("move within view, move multiple (> 1 row) up")
            << 30 << 0.0 << 0.0
            << 10 << 1 << 5 << ListRange(1, 9);
    QTest::newRow("move within view, move multiple up, move to first index")
            << 30 << 0.0 << 0.0
            << 10 << 0 << 3 << ListRange(0, 9);
    QTest::newRow("move within view, move multiple up, move to first index, contentY not 0")
            << 30 << 1.0 << 0.0
            << 10+3 << 0+3 << 3 << ListRange(0+3, 9+3);
    QTest::newRow("move within view, move multiple up (> 1 row), move to first index, contentY not on border")
            << 30 << 1.5 << 0.0
            << 10+3 << 0+3 << 5 << ListRange(0+3, 9+3);
    QTest::newRow("move within view, move multiple up, move last item")
            << 30 << 0.0 << 0.0
            << 15 << 5 << 3 << ListRange(5, 14);
    QTest::newRow("move within view, move multiple up, move last->first")
            << 30 << 0.0 << 0.0
            << 15 << 0 << 3 << ListRange(0, 14);

    QTest::newRow("move from below view, move 1 up")
            << 30 << 0.0 << 0.0
            << 20 << 5 << 1 << ListRange(5, 17);
    QTest::newRow("move from below view, move 1 up, move to top")
            << 30 << 0.0 << 0.0
            << 20 << 0 << 1 << ListRange(0, 17);
    QTest::newRow("move from below view, move 1 up, move to top, contentY not 0")
            << 30 << 1.0 << 0.0
            << 25 << 3 << 1 << ListRange(0+3, 17+3);
    QTest::newRow("move from below view, move multiple (> 1 row) up")
            << 30 << 0.0 << 0.0
            << 20 << 5 << 5 << ListRange(5, 17);
    QTest::newRow("move from below view, move multiple up, move to top")
            << 30 << 0.0 << 0.0
            << 20 << 0 << 3 << ListRange(0, 17);
    QTest::newRow("move from below view, move multiple up, move to top, contentY not 0")
            << 30 << 1.0 << 0.0
            << 25 << 3 << 3 << ListRange(0+3, 17+3);

    QTest::newRow("move from below view, move 1 up, move to bottom")
            << 30 << 0.0 << 0.0
            << 20 << 17 << 1 << ListRange(17, 17);
    QTest::newRow("move from below view, move 1 up, move to bottom, contentY not 0")
            << 30 << 1.0 << 0.0
            << 25 << 17+3 << 1 << ListRange(17+3, 17+3);
    QTest::newRow("move from below view, move multiple up, move to bottom")
            << 30 << 0.0 << 0.0
            << 20 << 17 << 3 << ListRange(17, 17);
    QTest::newRow("move from below view, move multiple up, move to bottom, contentY not 0")
            << 30 << 1.0 << 0.0
            << 25 << 17+3 << 3 << ListRange(17+3, 17+3);
}

void tst_QQuickGridView::removeTransitions()
{
    QFETCH(int, initialItemCount);
    QFETCH(bool, shouldAnimateTargets);
    QFETCH(qreal, contentYRowOffset);
    QFETCH(int, removalIndex);
    QFETCH(int, removalCount);
    QFETCH(ListRange, expectedDisplacedIndexes);

    // added items should end here
    QPointF targetItems_transitionTo(-50, -50);

    // displaced items should pass through this points
    QPointF displacedItems_transitionVia(100, 100);

    QaimModel model;
    for (int i = 0; i < initialItemCount; i++)
        model.addItem("Original item" + QString::number(i), "");
    QaimModel model_targetItems_transitionTo;
    QaimModel model_displacedItems_transitionVia;

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("model_targetItems_transitionTo", &model_targetItems_transitionTo);
    ctxt->setContextProperty("model_displacedItems_transitionVia", &model_displacedItems_transitionVia);
    ctxt->setContextProperty("targetItems_transitionTo", targetItems_transitionTo);
    ctxt->setContextProperty("displacedItems_transitionVia", displacedItems_transitionVia);
    window->setSource(testFileUrl("removeTransitions.qml"));
    window->show();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    if (contentYRowOffset != 0) {
        gridview->setContentY(contentYRowOffset * 60.0);
        QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
    }

    QList<QPair<QString,QString> > expectedDisplacedValues = expectedDisplacedIndexes.getModelDataValues(model);

    // only target items that are visible should be animated
    QList<QPair<QString, QString> > expectedTargetData;
    QList<int> targetIndexes;
    if (shouldAnimateTargets) {
        for (int i=removalIndex; i<removalIndex+removalCount; i++) {
            int firstVisibleIndex = (gridview->contentY() / 60.0)*3;
            int lastVisibleIndex = (qCeil((gridview->contentY() + gridview->height()) / 60.0)*3) - 1;
            if (i >= firstVisibleIndex && i <= lastVisibleIndex) {
                expectedTargetData << qMakePair(model.name(i), model.number(i));
                targetIndexes << i;
            }
        }
        QVERIFY(expectedTargetData.count() > 0);
    }

    // calculate targetItems and expectedTargets before model changes
    QList<QQuickItem *> targetItems = findItems<QQuickItem>(contentItem, "wrapper", targetIndexes);
    QVariantMap expectedTargets;
    for (int i=0; i<targetIndexes.count(); i++)
        expectedTargets[model.name(targetIndexes[i])] = targetIndexes[i];

    // start animation
    model.removeItems(removalIndex, removalCount);
    gridview->forceLayout();
    QTRY_COMPARE(model.count(), gridview->count());

    if (shouldAnimateTargets || expectedDisplacedIndexes.isValid()) {
        QTRY_COMPARE(gridview->property("targetTransitionsDone").toInt(), expectedTargetData.count());
        QTRY_COMPARE(gridview->property("displaceTransitionsDone").toInt(),
                     expectedDisplacedIndexes.isValid() ? expectedDisplacedIndexes.count() : 0);

        // check the target and displaced items were animated
        model_targetItems_transitionTo.matchAgainst(expectedTargetData, "wasn't animated to target 'to' pos", "shouldn't have been animated to target 'to' pos");
        model_displacedItems_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with displaced anim", "shouldn't have been animated with displaced anim");

        // check attached properties
        QCOMPARE(gridview->property("targetTrans_items").toMap(), expectedTargets);
        matchIndexLists(gridview->property("targetTrans_targetIndexes").toList(), targetIndexes);
        matchItemLists(gridview->property("targetTrans_targetItems").toList(), targetItems);
        if (expectedDisplacedIndexes.isValid()) {
            // adjust expectedDisplacedIndexes to their final values after the move
            QList<int> displacedIndexes = adjustIndexesForRemoveDisplaced(expectedDisplacedIndexes.indexes, removalIndex, removalCount);
            matchItemsAndIndexes(gridview->property("displacedTrans_items").toMap(), model, displacedIndexes);
            matchIndexLists(gridview->property("displacedTrans_targetIndexes").toList(), targetIndexes);
            matchItemLists(gridview->property("displacedTrans_targetItems").toList(), targetItems);
        }
    } else {
        QTRY_COMPARE(model_targetItems_transitionTo.count(), 0);
        QTRY_COMPARE(model_displacedItems_transitionVia.count(), 0);
    }

    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    int itemCount = items.count();
    int firstVisibleIndex = -1;
    for (int i=0; i<items.count(); i++) {
        QQmlExpression e(qmlContext(items[i]), items[i], "index");
        int index = e.evaluate().toInt();
        if (firstVisibleIndex < 0 && items[i]->y() >= gridview->contentY())
            firstVisibleIndex = index;
        else if (index < 0)
            itemCount--;    // exclude deleted items
    }
    QVERIFY2(firstVisibleIndex >= 0, QTest::toString(firstVisibleIndex));

    // verify all items moved to the correct final positions
    for (int i=firstVisibleIndex; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->x(), (i%3)*80.0);
        QCOMPARE(item->y(), gridview->contentY() + ((i-firstVisibleIndex)/3) * 60.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickGridView::removeTransitions_data()
{
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<qreal>("contentYRowOffset");
    QTest::addColumn<bool>("shouldAnimateTargets");
    QTest::addColumn<int>("removalIndex");
    QTest::addColumn<int>("removalCount");
    QTest::addColumn<ListRange>("expectedDisplacedIndexes");

    // All items that are visible following the remove operation should be animated.
    // Remove targets that are outside of the view should not be animated.

    // For a GridView, removing any number of items other than a full row before the start
    // should displace all items in the view
    QTest::newRow("remove 1 before start")
            << 30 << 2.0 << false
            << 2 << 1 << ListRange(6, 24);    // 6-24 are displaced
    QTest::newRow("remove 1 row, before start")
            << 30 << 2.0 << false
            << 3 << 3 << ListRange();
    QTest::newRow("remove between 1-2 rows, before start")
            << 30 << 2.0 << false
            << 0 << 5 << ListRange(6, 25);
    QTest::newRow("remove 2 rows, before start")
            << 30 << 2.0 << false
            << 0 << 6 << ListRange();
    QTest::newRow("remove mix of before and after start")
            << 30 << 1.0 << true
            << 2 << 3 << ListRange(5, 23);  // 5-23 are displaced into view


    QTest::newRow("remove 1 from start")
            << 30 << 0.0 << true
            << 0 << 1 << ListRange(1, 18);  // 1-18 are displaced into view
    QTest::newRow("remove multiple from start")
            << 30 << 0.0 << true
            << 0 << 3 << ListRange(3, 20);  // 3-18 are displaced into view
    QTest::newRow("remove 1 from start, content y not 0")
            << 30 << 1.0 << true
            << 3 << 1 << ListRange(1 + 3, 18 + 3);
    QTest::newRow("remove multiple from start, content y not 0")
            << 30 << 1.0 << true
            << 3 << 3 << ListRange(3 + 3, 20 + 3);


    QTest::newRow("remove 1 from middle")
            << 30 << 0.0 << true
            << 5 << 1 << ListRange(6, 18);
    QTest::newRow("remove multiple from middle")
            << 30 << 0.0 << true
            << 5 << 3 << ListRange(8, 20);


    QTest::newRow("remove 1 from bottom")
            << 30 << 0.0 << true
            << 17 << 1 << ListRange(18, 18);
    QTest::newRow("remove multiple (1 row) from bottom")
            << 30 << 0.0 << true
            << 15 << 3 << ListRange(18, 20);
    QTest::newRow("remove multiple (> 1 row) from bottom")
            << 30 << 0.0 << true
            << 15 << 5 << ListRange(20, 22);
    QTest::newRow("remove 1 from bottom, content y not 0")
            << 30 << 1.0 << true
            << 17 + 3 << 1 << ListRange(18 + 3, 18 + 3);
    QTest::newRow("remove multiple (1 row) from bottom, content y not 0")
            << 30 << 1.0 << true
            << 15 + 3 << 3 << ListRange(18 + 3, 20 + 3);


    QTest::newRow("remove 1 after end")
            << 30 << 0.0 << false
            << 18 << 1 << ListRange();
    QTest::newRow("remove multiple after end")
            << 30 << 0.0 << false
            << 18 << 3 << ListRange();
}

void tst_QQuickGridView::displacedTransitions()
{
    QFETCH(bool, useDisplaced);
    QFETCH(bool, displacedEnabled);
    QFETCH(bool, useAddDisplaced);
    QFETCH(bool, addDisplacedEnabled);
    QFETCH(bool, useMoveDisplaced);
    QFETCH(bool, moveDisplacedEnabled);
    QFETCH(bool, useRemoveDisplaced);
    QFETCH(bool, removeDisplacedEnabled);
    QFETCH(ListChange, change);
    QFETCH(ListRange, expectedDisplacedIndexes);

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Original item" + QString::number(i), "");
    QaimModel model_displaced_transitionVia;
    QaimModel model_addDisplaced_transitionVia;
    QaimModel model_moveDisplaced_transitionVia;
    QaimModel model_removeDisplaced_transitionVia;

    QPointF displaced_transitionVia(-50, -100);
    QPointF addDisplaced_transitionVia(-150, 100);
    QPointF moveDisplaced_transitionVia(50, -100);
    QPointF removeDisplaced_transitionVia(150, 100);

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("model_displaced_transitionVia", &model_displaced_transitionVia);
    ctxt->setContextProperty("model_addDisplaced_transitionVia", &model_addDisplaced_transitionVia);
    ctxt->setContextProperty("model_moveDisplaced_transitionVia", &model_moveDisplaced_transitionVia);
    ctxt->setContextProperty("model_removeDisplaced_transitionVia", &model_removeDisplaced_transitionVia);
    ctxt->setContextProperty("displaced_transitionVia", displaced_transitionVia);
    ctxt->setContextProperty("addDisplaced_transitionVia", addDisplaced_transitionVia);
    ctxt->setContextProperty("moveDisplaced_transitionVia", moveDisplaced_transitionVia);
    ctxt->setContextProperty("removeDisplaced_transitionVia", removeDisplaced_transitionVia);
    ctxt->setContextProperty("useDisplaced", useDisplaced);
    ctxt->setContextProperty("displacedEnabled", displacedEnabled);
    ctxt->setContextProperty("useAddDisplaced", useAddDisplaced);
    ctxt->setContextProperty("addDisplacedEnabled", addDisplacedEnabled);
    ctxt->setContextProperty("useMoveDisplaced", useMoveDisplaced);
    ctxt->setContextProperty("moveDisplacedEnabled", moveDisplacedEnabled);
    ctxt->setContextProperty("useRemoveDisplaced", useRemoveDisplaced);
    ctxt->setContextProperty("removeDisplacedEnabled", removeDisplacedEnabled);
    window->setSource(testFileUrl("displacedTransitions.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    QList<QPair<QString,QString> > expectedDisplacedValues = expectedDisplacedIndexes.getModelDataValues(model);
    gridview->setProperty("displaceTransitionsDone", false);

    switch (change.type) {
        case ListChange::Inserted:
        {
            QList<QPair<QString, QString> > targetItemData;
            for (int i=change.index; i<change.index + change.count; ++i)
                targetItemData << qMakePair(QString("new item %1").arg(i), QString::number(i));
            model.insertItems(change.index, targetItemData);
            QTRY_COMPARE(model.count(), gridview->count());
            break;
        }
        case ListChange::Removed:
            model.removeItems(change.index, change.count);
            QTRY_COMPARE(model.count(), gridview->count());
            break;
        case ListChange::Moved:
            model.moveItems(change.index, change.to, change.count);
            QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
            break;
        case ListChange::SetCurrent:
        case ListChange::SetContentY:
        case ListChange::Polish:
            break;
    }
    gridview->forceLayout();

    QVariantList resultTargetIndexes = gridview->property("displacedTargetIndexes").toList();
    QVariantList resultTargetItems = gridview->property("displacedTargetItems").toList();

    if ((useDisplaced && displacedEnabled)
            || (useAddDisplaced && addDisplacedEnabled)
            || (useMoveDisplaced && moveDisplacedEnabled)
            || (useRemoveDisplaced && removeDisplacedEnabled)) {
        QTRY_VERIFY(gridview->property("displaceTransitionsDone").toBool());

        // check the correct number of target items and indexes were received
        QCOMPARE(resultTargetIndexes.count(), expectedDisplacedIndexes.count());
        for (int i=0; i<resultTargetIndexes.count(); i++)
            QCOMPARE(resultTargetIndexes[i].value<QList<int> >().count(), change.count);
        QCOMPARE(resultTargetItems.count(), expectedDisplacedIndexes.count());
        for (int i=0; i<resultTargetItems.count(); i++)
            QCOMPARE(resultTargetItems[i].toList().count(), change.count);
    } else {
        QCOMPARE(resultTargetIndexes.count(), 0);
        QCOMPARE(resultTargetItems.count(), 0);
    }

    if (change.type == ListChange::Inserted && useAddDisplaced && addDisplacedEnabled)
        model_addDisplaced_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with add displaced", "shouldn't have been animated with add displaced");
    else
        QCOMPARE(model_addDisplaced_transitionVia.count(), 0);
    if (change.type == ListChange::Moved && useMoveDisplaced && moveDisplacedEnabled)
        model_moveDisplaced_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with move displaced", "shouldn't have been animated with move displaced");
    else
        QCOMPARE(model_moveDisplaced_transitionVia.count(), 0);
    if (change.type == ListChange::Removed && useRemoveDisplaced && removeDisplacedEnabled)
        model_removeDisplaced_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with remove displaced", "shouldn't have been animated with remove displaced");
    else
        QCOMPARE(model_removeDisplaced_transitionVia.count(), 0);

    if (useDisplaced && displacedEnabled
            && ( (change.type == ListChange::Inserted && (!useAddDisplaced || !addDisplacedEnabled))
                 || (change.type == ListChange::Moved && (!useMoveDisplaced || !moveDisplacedEnabled))
                 || (change.type == ListChange::Removed && (!useRemoveDisplaced || !removeDisplacedEnabled))) ) {
        model_displaced_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with generic displaced", "shouldn't have been animated with generic displaced");
    } else {
        QCOMPARE(model_displaced_transitionVia.count(), 0);
    }

    // verify all items moved to the correct final positions
    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    for (int i=0; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->x(), (i%3)*80.0);
        QCOMPARE(item->y(), (i/3)*60.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickGridView::displacedTransitions_data()
{
    QTest::addColumn<bool>("useDisplaced");
    QTest::addColumn<bool>("displacedEnabled");
    QTest::addColumn<bool>("useAddDisplaced");
    QTest::addColumn<bool>("addDisplacedEnabled");
    QTest::addColumn<bool>("useMoveDisplaced");
    QTest::addColumn<bool>("moveDisplacedEnabled");
    QTest::addColumn<bool>("useRemoveDisplaced");
    QTest::addColumn<bool>("removeDisplacedEnabled");
    QTest::addColumn<ListChange>("change");
    QTest::addColumn<ListRange>("expectedDisplacedIndexes");

    QTest::newRow("no displaced transitions at all")
            << false << false
            << false << false
            << false << false
            << false << false
            << ListChange::insert(0, 1) << ListRange(0, 17);

    QTest::newRow("just displaced")
            << true << true
            << false << false
            << false << false
            << false << false
            << ListChange::insert(0, 1) << ListRange(0, 17);

    QTest::newRow("just displaced (not enabled)")
            << true << false
            << false << false
            << false << false
            << false << false
            << ListChange::insert(0, 1) << ListRange(0, 17);

    QTest::newRow("displaced + addDisplaced")
            << true << true
            << true << true
            << false << false
            << false << false
            << ListChange::insert(0, 1) << ListRange(0, 17);

    QTest::newRow("displaced + addDisplaced (not enabled)")
            << true << true
            << true << false
            << false << false
            << false << false
            << ListChange::insert(0, 1) << ListRange(0, 17);

    QTest::newRow("displaced + moveDisplaced")
            << true << true
            << false << false
            << true << true
            << false << false
            << ListChange::move(0, 10, 1) << ListRange(1, 10);

    QTest::newRow("displaced + moveDisplaced (not enabled)")
            << true << true
            << false << false
            << true << false
            << false << false
            << ListChange::move(0, 10, 1) << ListRange(1, 10);

    QTest::newRow("displaced + removeDisplaced")
            << true << true
            << false << false
            << false << false
            << true << true
            << ListChange::remove(0, 1) << ListRange(1, 18);

    QTest::newRow("displaced + removeDisplaced (not enabled)")
            << true << true
            << false << false
            << false << false
            << true << false
            << ListChange::remove(0, 1) << ListRange(1, 18);


    QTest::newRow("displaced + add, should use generic displaced for a remove")
            << true << true
            << true << true
            << false << false
            << true << false
            << ListChange::remove(0, 1) << ListRange(1, 18);
}

void tst_QQuickGridView::multipleTransitions()
{
    // Tests that if you interrupt a transition in progress with another action that
    // cancels the previous transition, the resulting items are still placed correctly.

    QFETCH(int, initialCount);
    QFETCH(qreal, contentY);
    QFETCH(QList<ListChange>, changes);
    QFETCH(bool, enableAddTransitions);
    QFETCH(bool, enableMoveTransitions);
    QFETCH(bool, enableRemoveTransitions);
    QFETCH(bool, rippleAddDisplaced);

    // add transitions on the left, moves on the right
    QPointF addTargets_transitionFrom(-50, -50);
    QPointF addDisplaced_transitionFrom(-50, 50);
    QPointF moveTargets_transitionFrom(50, -50);
    QPointF moveDisplaced_transitionFrom(50, 50);
    QPointF removeTargets_transitionTo(-100, 300);
    QPointF removeDisplaced_transitionFrom(100, 300);

    QaimModel model;
    for (int i = 0; i < initialCount; i++)
        model.addItem("Original item" + QString::number(i), "");

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("addTargets_transitionFrom", addTargets_transitionFrom);
    ctxt->setContextProperty("addDisplaced_transitionFrom", addDisplaced_transitionFrom);
    ctxt->setContextProperty("moveTargets_transitionFrom", moveTargets_transitionFrom);
    ctxt->setContextProperty("moveDisplaced_transitionFrom", moveDisplaced_transitionFrom);
    ctxt->setContextProperty("removeTargets_transitionTo", removeTargets_transitionTo);
    ctxt->setContextProperty("removeDisplaced_transitionFrom", removeDisplaced_transitionFrom);
    ctxt->setContextProperty("enableAddTransitions", enableAddTransitions);
    ctxt->setContextProperty("enableMoveTransitions", enableMoveTransitions);
    ctxt->setContextProperty("enableRemoveTransitions", enableRemoveTransitions);
    ctxt->setContextProperty("rippleAddDisplaced", rippleAddDisplaced);
    window->setSource(testFileUrl("multipleTransitions.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    if (contentY != 0) {
        gridview->setContentY(contentY);
        QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
    }

    int timeBetweenActions = window->rootObject()->property("timeBetweenActions").toInt();

    for (int i=0; i<changes.count(); i++) {
        switch (changes[i].type) {
            case ListChange::Inserted:
            {
                QList<QPair<QString, QString> > targetItems;
                for (int j=changes[i].index; j<changes[i].index + changes[i].count; ++j)
                    targetItems << qMakePair(QString("new item %1").arg(j), QString::number(j));
                model.insertItems(changes[i].index, targetItems);
                gridview->forceLayout();
                QTRY_COMPARE(model.count(), gridview->count());
                if (i == changes.count() - 1) {
                    QTRY_VERIFY(!gridview->property("runningAddTargets").toBool());
                    QTRY_VERIFY(!gridview->property("runningAddDisplaced").toBool());
                } else {
                    QTest::qWait(timeBetweenActions);
                }
                break;
            }
            case ListChange::Removed:
                model.removeItems(changes[i].index, changes[i].count);
                gridview->forceLayout();
                QTRY_COMPARE(model.count(), gridview->count());
                if (i == changes.count() - 1) {
                    QTRY_VERIFY(!gridview->property("runningRemoveTargets").toBool());
                    QTRY_VERIFY(!gridview->property("runningRemoveDisplaced").toBool());
                } else {
                    QTest::qWait(timeBetweenActions);
                }
                break;
            case ListChange::Moved:
                model.moveItems(changes[i].index, changes[i].to, changes[i].count);
                gridview->forceLayout();
                QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
                if (i == changes.count() - 1) {
                    QTRY_VERIFY(!gridview->property("runningMoveTargets").toBool());
                    QTRY_VERIFY(!gridview->property("runningMoveDisplaced").toBool());
                } else {
                    QTest::qWait(timeBetweenActions);
                }
                break;
            case ListChange::SetCurrent:
                gridview->setCurrentIndex(changes[i].index);
                QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
                gridview->forceLayout();
                break;
            case ListChange::SetContentY:
                gridview->setContentY(changes[i].pos);
                QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);
                gridview->forceLayout();
                break;
            case ListChange::Polish:
                break;
        }
    }

    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    int firstVisibleIndex = -1;
    for (int i=0; i<items.count(); i++) {
        if (items[i]->y() >= contentY) {
            QQmlExpression e(qmlContext(items[i]), items[i], "index");
            firstVisibleIndex = e.evaluate().toInt();
            break;
        }
    }
    QTRY_COMPARE(gridview->count(), model.count());
    QVERIFY2(firstVisibleIndex >= 0, QTest::toString(firstVisibleIndex));

    // verify all items moved to the correct final positions
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i=firstVisibleIndex; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->x(), (i%3)*80.0);
        QTRY_COMPARE(item->y(), (i/3)*60.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickGridView::multipleTransitions_data()
{
    QTest::addColumn<int>("initialCount");
    QTest::addColumn<qreal>("contentY");
    QTest::addColumn<QList<ListChange> >("changes");
    QTest::addColumn<bool>("enableAddTransitions");
    QTest::addColumn<bool>("enableMoveTransitions");
    QTest::addColumn<bool>("enableRemoveTransitions");
    QTest::addColumn<bool>("rippleAddDisplaced");

    // the added item and displaced items should move to final dest correctly
    QTest::newRow("add item, then move it immediately") << 10 << 0.0 << (QList<ListChange>()
             << ListChange::insert(0, 1)
             << ListChange::move(0, 3, 1)
             )
             << true << true << true << false;

    // items affected by the add should change from move to add transition
    QTest::newRow("move, then insert item before the moved item") << 20 << 0.0 << (QList<ListChange>()
            << ListChange::move(1, 10, 3)
            << ListChange::insert(0, 1)
            )
            << true << true << true << false;

    // items should be placed correctly if you trigger a transition then refill for that index
    QTest::newRow("add at 0, flick down, flick back to top and add at 0 again") << 20 << 0.0 << (QList<ListChange>()
            << ListChange::insert(0, 1)
            << ListChange::setContentY(160.0)
            << ListChange::setContentY(0.0)
            << ListChange::insert(0, 1)
            )
            << true << true << true << false;

    QTest::newRow("insert then remove same index, with ripple effect on add displaced") << 20 << 0.0 << (QList<ListChange>()
            << ListChange::insert(1, 1)
            << ListChange::remove(1, 1)
            )
            << true << true << true << true;

    // if item is removed while undergoing a displaced transition, all other items should end up at their correct positions,
    // even if a remove-displace transition is not present to re-animate them
    QTest::newRow("insert then remove, with remove disabled") << 20 << 0.0 << (QList<ListChange>()
            << ListChange::insert(0, 1)
            << ListChange::remove(2, 1)
            )
            << true << true << false << false;

    // if last item is not flush with the edge of the view, it should still be refilled in correctly after a
    // remove has changed the position of where it will move to
    QTest::newRow("insert twice then remove, with remove disabled") << 20 << 0.0 << (QList<ListChange>()
            << ListChange::setContentY(-10.0)
            << ListChange::insert(0, 1)
            << ListChange::insert(0, 1)
            << ListChange::remove(2, 1)
            )
            << true << true << false << false;
}

void tst_QQuickGridView::multipleDisplaced()
{
    // multiple move() operations should only restart displace transitions for items that
    // moved from previously set positions, and not those that have moved from their current
    // item positions (which may e.g. still be changing from easing bounces in the last transition)

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Original item" + QString::number(i), "");

    QQuickView *window = createView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("multipleDisplaced.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QTRY_VERIFY(gridview != 0);
    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(gridview)->polishScheduled, false);

    model.moveItems(12, 8, 1);
    QTest::qWait(window->rootObject()->property("duration").toInt() / 2);
    model.moveItems(8, 3, 1);
    QTRY_VERIFY(gridview->property("displaceTransitionsDone").toBool());

    QVariantMap transitionsStarted = gridview->property("displaceTransitionsStarted").toMap();
    foreach (const QString &name, transitionsStarted.keys()) {
        QVERIFY2(transitionsStarted[name] == 1,
                 QTest::toString(QString("%1 was displaced %2 times").arg(name).arg(transitionsStarted[name].toInt())));
    }

    // verify all items moved to the correct final positions
    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    for (int i=0; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->x(), (i%3)*80.0);
        QTRY_COMPARE(item->y(), (i/3)*60.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    delete window;
}

void tst_QQuickGridView::cacheBuffer()
{
    QQuickView *window = createView();

    QaimModel model;
    for (int i = 0; i < 90; i++)
        model.addItem("Item" + QString::number(i), "");

    window->rootContext()->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("gridview1.qml"));
    window->show();
    qApp->processEvents();

    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QVERIFY(gridview != 0);

    QQuickItem *contentItem = gridview->contentItem();
    QVERIFY(contentItem != 0);
    QVERIFY(gridview->delegate() != 0);
    QVERIFY(gridview->model() != 0);

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper", false).count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_COMPARE(item->x(), (i%3)*80.0);
        QTRY_COMPARE(item->y(), (i/3)*60.0);
    }

    QQmlIncubationController controller;
    window->engine()->setIncubationController(&controller);

    window->rootObject()->setProperty("cacheBuffer", 200);
    QTRY_COMPARE(gridview->cacheBuffer(), 200);

    // items will be created one at a time
    for (int i = itemCount; i < qMin(itemCount+9,model.count()); ++i) {
        QVERIFY(findItem<QQuickItem>(gridview, "wrapper", i) == 0);
        QQuickItem *item = 0;
        while (!item) {
            bool b = false;
            controller.incubateWhile(&b);
            item = findItem<QQuickItem>(gridview, "wrapper", i);
        }
    }

    {
        bool b = true;
        controller.incubateWhile(&b);
    }

    int newItemCount = 0;
    newItemCount = findItems<QQuickItem>(contentItem, "wrapper", false).count();

    // Confirm items positioned correctly
    for (int i = 0; i < model.count() && i < newItemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY(item);
        QTRY_COMPARE(item->x(), (i%3)*80.0);
        QTRY_COMPARE(item->y(), (i/3)*60.0);
    }

    // move view and confirm items in view are visible immediately and outside are created async
    gridview->setContentY(300);

    for (int i = 15; i < 34; ++i) { // 34 due to staggered item creation
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY(item);
        QTRY_COMPARE(item->x(), (i%3)*80.0);
        QTRY_COMPARE(item->y(), (i/3)*60.0);
    }

    QVERIFY(findItem<QQuickItem>(gridview, "wrapper", 34) == 0);

    // ensure buffered items are created
    for (int i = 34; i < qMin(44,model.count()); ++i) {
        QQuickItem *item = 0;
        while (!item) {
            qGuiApp->processEvents(); // allow refill to happen
            bool b = false;
            controller.incubateWhile(&b);
            item = findItem<QQuickItem>(gridview, "wrapper", i);
        }
    }

    {
        bool b = true;
        controller.incubateWhile(&b);
    }

    delete window;
}

void tst_QQuickGridView::asynchronous()
{
    QQuickView *window = createView();
    window->show();
    QQmlIncubationController controller;
    window->engine()->setIncubationController(&controller);

    window->setSource(testFileUrl("asyncloader.qml"));

    QQuickItem *rootObject = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(rootObject);

    QQuickGridView *gridview = 0;
    while (!gridview) {
        bool b = false;
        controller.incubateWhile(&b);
        gridview = rootObject->findChild<QQuickGridView*>("view");
    }

    // items will be created one at a time
    for (int i = 0; i < 12; ++i) {
        QVERIFY(findItem<QQuickItem>(gridview, "wrapper", i) == 0);
        QQuickItem *item = 0;
        while (!item) {
            bool b = false;
            controller.incubateWhile(&b);
            item = findItem<QQuickItem>(gridview, "wrapper", i);
        }
    }

    {
        bool b = true;
        controller.incubateWhile(&b);
    }

    // verify positioning
    QQuickItem *contentItem = gridview->contentItem();
    for (int i = 0; i < 12; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QCOMPARE(item->x(), qreal((i%3)*100));
        QCOMPARE(item->y(), qreal((i/3)*100));
    }

    delete window;
}

void tst_QQuickGridView::unrequestedVisibility()
{
    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), QString::number(i));

    QQuickView *window = new QQuickView(0);
    window->setGeometry(0,0,240,320);

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testWrap", QVariant(false));

    window->setSource(testFileUrl("unrequestedItems.qml"));

    window->show();

    qApp->processEvents();

    QQuickGridView *leftview = findItem<QQuickGridView>(window->rootObject(), "leftGrid");
    QTRY_VERIFY(leftview != 0);

    QQuickGridView *rightview = findItem<QQuickGridView>(window->rootObject(), "rightGrid");
    QTRY_VERIFY(rightview != 0);

    QQuickItem *leftContent = leftview->contentItem();
    QTRY_VERIFY(leftContent != 0);

    QQuickItem *rightContent = rightview->contentItem();
    QTRY_VERIFY(rightContent != 0);

    rightview->setCurrentIndex(12);

    QTRY_COMPARE(leftview->contentY(), 0.0);
    QTRY_COMPARE(rightview->contentY(), 240.0);

    QQuickItem *item;

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 1));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 1));
    QCOMPARE(delegateVisible(item), false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 11));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 11));
    QCOMPARE(delegateVisible(item), true);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 9));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 10));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 3));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 4));
    QCOMPARE(delegateVisible(item), true);

    rightview->setCurrentIndex(0);

    QTRY_COMPARE(leftview->contentY(), 0.0);
    QTRY_COMPARE(rightview->contentY(), 0.0);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 1));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 1));
    QTRY_COMPARE(delegateVisible(item), true);

    QVERIFY(!findItem<QQuickItem>(leftContent, "wrapper", 11));
    QVERIFY(!findItem<QQuickItem>(rightContent, "wrapper", 11));

    leftview->setCurrentIndex(12);

    QTRY_COMPARE(leftview->contentY(), 240.0);
    QTRY_COMPARE(rightview->contentY(), 0.0);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 1));
    QTRY_COMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 1));
    QCOMPARE(delegateVisible(item), true);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 11));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 11));
    QCOMPARE(delegateVisible(item), false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 3));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 5));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 9));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 10));
    QCOMPARE(delegateVisible(item), false);

    // move a non-visible item into view
    model.moveItems(10, 9, 1);
    QTRY_COMPARE(QQuickItemPrivate::get(leftview)->polishScheduled, false);

    QTRY_VERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 1));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 1));
    QCOMPARE(delegateVisible(item), true);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 11));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 11));
    QCOMPARE(delegateVisible(item), false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 3));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 5));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 9));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 10));
    QCOMPARE(delegateVisible(item), false);

    // move a visible item out of view
    model.moveItems(5, 3, 1);
    QTRY_COMPARE(QQuickItemPrivate::get(leftview)->polishScheduled, false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 3));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 5));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 9));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 10));
    QCOMPARE(delegateVisible(item), false);

    // move a non-visible item into view
    model.moveItems(3, 5, 1);
    QTRY_COMPARE(QQuickItemPrivate::get(leftview)->polishScheduled, false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 3));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 5));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 9));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 10));
    QCOMPARE(delegateVisible(item), false);

    // move a visible item out of view
    model.moveItems(9, 10, 1);
    QTRY_COMPARE(QQuickItemPrivate::get(leftview)->polishScheduled, false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 3));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 5));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 9));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 10));
    QCOMPARE(delegateVisible(item), false);

    // move a non-visible item into view
    model.moveItems(10, 9, 1);
    QTRY_COMPARE(QQuickItemPrivate::get(leftview)->polishScheduled, false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 3));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 5));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 9));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 10));
    QCOMPARE(delegateVisible(item), false);

    delete window;
}


void tst_QQuickGridView::inserted_leftToRight_RtL_TtB()
{
    inserted_defaultLayout(QQuickGridView::FlowLeftToRight, Qt::RightToLeft);
}

void tst_QQuickGridView::inserted_leftToRight_RtL_TtB_data()
{
    inserted_defaultLayout_data();
}

void tst_QQuickGridView::inserted_topToBottom_LtR_TtB()
{
    inserted_defaultLayout(QQuickGridView::FlowTopToBottom);
}

void tst_QQuickGridView::inserted_topToBottom_LtR_TtB_data()
{
    inserted_defaultLayout_data();
}

void tst_QQuickGridView::inserted_topToBottom_RtL_TtB()
{
    inserted_defaultLayout(QQuickGridView::FlowTopToBottom, Qt::RightToLeft);
}

void tst_QQuickGridView::inserted_topToBottom_RtL_TtB_data()
{
    inserted_defaultLayout_data();
}

void tst_QQuickGridView::inserted_leftToRight_LtR_BtT()
{
    inserted_defaultLayout(QQuickGridView::FlowLeftToRight, Qt::LeftToRight, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::inserted_leftToRight_LtR_BtT_data()
{
    inserted_defaultLayout_data();
}

void tst_QQuickGridView::inserted_leftToRight_RtL_BtT()
{
    inserted_defaultLayout(QQuickGridView::FlowLeftToRight, Qt::RightToLeft, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::inserted_leftToRight_RtL_BtT_data()
{
    inserted_defaultLayout_data();
}

void tst_QQuickGridView::inserted_topToBottom_LtR_BtT()
{
    inserted_defaultLayout(QQuickGridView::FlowTopToBottom, Qt::LeftToRight, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::inserted_topToBottom_LtR_BtT_data()
{
    inserted_defaultLayout_data();
}

void tst_QQuickGridView::inserted_topToBottom_RtL_BtT()
{
    inserted_defaultLayout(QQuickGridView::FlowTopToBottom, Qt::RightToLeft, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::inserted_topToBottom_RtL_BtT_data()
{
    inserted_defaultLayout_data();
}


void tst_QQuickGridView::removed_leftToRight_RtL_TtB()
{
    removed_defaultLayout(QQuickGridView::FlowLeftToRight, Qt::RightToLeft);
}

void tst_QQuickGridView::removed_leftToRight_RtL_TtB_data()
{
    removed_defaultLayout_data();
}

void tst_QQuickGridView::removed_topToBottom_LtR_TtB()
{
    removed_defaultLayout(QQuickGridView::FlowTopToBottom);
}

void tst_QQuickGridView::removed_topToBottom_LtR_TtB_data()
{
    removed_defaultLayout_data();
}

void tst_QQuickGridView::removed_topToBottom_RtL_TtB()
{
    removed_defaultLayout(QQuickGridView::FlowTopToBottom, Qt::RightToLeft);
}

void tst_QQuickGridView::removed_topToBottom_RtL_TtB_data()
{
    removed_defaultLayout_data();
}

void tst_QQuickGridView::removed_leftToRight_LtR_BtT()
{
    removed_defaultLayout(QQuickGridView::FlowLeftToRight, Qt::LeftToRight, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::removed_leftToRight_LtR_BtT_data()
{
    removed_defaultLayout_data();
}

void tst_QQuickGridView::removed_leftToRight_RtL_BtT()
{
    removed_defaultLayout(QQuickGridView::FlowLeftToRight, Qt::RightToLeft, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::removed_leftToRight_RtL_BtT_data()
{
    removed_defaultLayout_data();
}

void tst_QQuickGridView::removed_topToBottom_LtR_BtT()
{
    removed_defaultLayout(QQuickGridView::FlowTopToBottom, Qt::LeftToRight, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::removed_topToBottom_LtR_BtT_data()
{
    removed_defaultLayout_data();
}

void tst_QQuickGridView::removed_topToBottom_RtL_BtT()
{
    removed_defaultLayout(QQuickGridView::FlowTopToBottom, Qt::RightToLeft, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::removed_topToBottom_RtL_BtT_data()
{
    removed_defaultLayout_data();
}


void tst_QQuickGridView::moved_leftToRight_RtL_TtB()
{
    moved_defaultLayout(QQuickGridView::FlowLeftToRight, Qt::RightToLeft);
}

void tst_QQuickGridView::moved_leftToRight_RtL_TtB_data()
{
    moved_defaultLayout_data();
}

void tst_QQuickGridView::moved_topToBottom_LtR_TtB()
{
    moved_defaultLayout(QQuickGridView::FlowTopToBottom);
}

void tst_QQuickGridView::moved_topToBottom_LtR_TtB_data()
{
    moved_defaultLayout_data();
}

void tst_QQuickGridView::moved_topToBottom_RtL_TtB()
{
    moved_defaultLayout(QQuickGridView::FlowTopToBottom, Qt::RightToLeft);
}

void tst_QQuickGridView::moved_topToBottom_RtL_TtB_data()
{
    moved_defaultLayout_data();
}

void tst_QQuickGridView::moved_leftToRight_LtR_BtT()
{
    moved_defaultLayout(QQuickGridView::FlowLeftToRight, Qt::LeftToRight, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::moved_leftToRight_LtR_BtT_data()
{
    moved_defaultLayout_data();
}

void tst_QQuickGridView::moved_leftToRight_RtL_BtT()
{
    moved_defaultLayout(QQuickGridView::FlowLeftToRight, Qt::RightToLeft, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::moved_leftToRight_RtL_BtT_data()
{
    moved_defaultLayout_data();
}

void tst_QQuickGridView::moved_topToBottom_LtR_BtT()
{
    moved_defaultLayout(QQuickGridView::FlowTopToBottom, Qt::LeftToRight, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::moved_topToBottom_LtR_BtT_data()
{
    moved_defaultLayout_data();
}

void tst_QQuickGridView::moved_topToBottom_RtL_BtT()
{
    moved_defaultLayout(QQuickGridView::FlowTopToBottom, Qt::RightToLeft, QQuickItemView::BottomToTop);
}

void tst_QQuickGridView::moved_topToBottom_RtL_BtT_data()
{
    moved_defaultLayout_data();
}


QList<int> tst_QQuickGridView::toIntList(const QVariantList &list)
{
    QList<int> ret;
    bool ok = true;
    for (int i=0; i<list.count(); i++) {
        ret << list[i].toInt(&ok);
        if (!ok)
            qWarning() << "tst_QQuickGridView::toIntList(): not a number:" << list[i];
    }

    return ret;
}

void tst_QQuickGridView::matchIndexLists(const QVariantList &indexLists, const QList<int> &expectedIndexes)
{
    for (int i=0; i<indexLists.count(); i++) {
        QSet<int> current = indexLists[i].value<QList<int> >().toSet();
        if (current != expectedIndexes.toSet())
            qDebug() << "Cannot match actual targets" << current << "with expected" << expectedIndexes;
        QCOMPARE(current, expectedIndexes.toSet());
    }
}

void tst_QQuickGridView::matchItemsAndIndexes(const QVariantMap &items, const QaimModel &model, const QList<int> &expectedIndexes)
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

void tst_QQuickGridView::matchItemLists(const QVariantList &itemLists, const QList<QQuickItem *> &expectedItems)
{
    for (int i=0; i<itemLists.count(); i++) {
        QVariantList current = itemLists[i].toList();
        for (int j=0; j<current.count(); j++) {
            QQuickItem *o = qobject_cast<QQuickItem*>(current[j].value<QObject*>());
            QVERIFY2(o, QTest::toString(QString("Invalid actual item at %1").arg(j)));
            QVERIFY2(expectedItems.contains(o), QTest::toString(QString("Cannot match item %1").arg(j)));
        }
        QCOMPARE(current.count(), expectedItems.count());
    }
}

void tst_QQuickGridView::displayMargin()
{
    QQuickView *window = createView();
    window->setSource(testFileUrl("displayMargin.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickGridView *gridview = window->rootObject()->findChild<QQuickGridView*>();
    QVERIFY(gridview != 0);

    QQuickItem *content = gridview->contentItem();
    QVERIFY(content != 0);

    QQuickItem *item0;
    QQuickItem *item97;

    QVERIFY(item0 = findItem<QQuickItem>(content, "delegate", 0));
    QCOMPARE(delegateVisible(item0), true);

    // the 97th item should be within the end margin
    QVERIFY(item97 = findItem<QQuickItem>(content, "delegate", 96));
    QCOMPARE(delegateVisible(item97), true);

    // GridView staggers item creation, so the 118th item should be outside the end margin.
    QVERIFY(findItem<QQuickItem>(content, "delegate", 117) == 0);

    // the first delegate should still be within the begin margin
    gridview->positionViewAtIndex(20, QQuickGridView::Beginning);
    QCOMPARE(delegateVisible(item0), true);

    // the first delegate should now be outside the begin margin
    gridview->positionViewAtIndex(36, QQuickGridView::Beginning);
    QCOMPARE(delegateVisible(item0), false);

    delete window;
}

void tst_QQuickGridView::negativeDisplayMargin()
{
    QQuickItem *item;
    QQuickView *window = createView();
    window->setSource(testFileUrl("negativeDisplayMargin.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickItem *listview = window->rootObject();
    QQuickGridView *gridview = findItem<QQuickGridView>(window->rootObject(), "grid");
    QVERIFY(gridview != 0);

    QTRY_COMPARE(gridview->property("createdItems").toInt(), 11);
    QCOMPARE(gridview->property("destroyedItem").toInt(), 0);

    QQuickItem *content = gridview->contentItem();
    QVERIFY(content != 0);

    QVERIFY(item = findItem<QQuickItem>(content, "delegate", 0));
    QCOMPARE(delegateVisible(item), true);

    QVERIFY(item = findItem<QQuickItem>(content, "delegate", 7));
    QCOMPARE(delegateVisible(item), true);

    QVERIFY(item = findItem<QQuickItem>(content, "delegate", 8));
    QCOMPARE(delegateVisible(item), false);

    // Flick until contentY means that delegate8 should be visible
    listview->setProperty("contentY", 500);
    QVERIFY(item = findItem<QQuickItem>(content, "delegate", 8));
    QTRY_COMPARE(delegateVisible(item), true);

    listview->setProperty("contentY", 1000);
    QTRY_VERIFY(item = findItem<QQuickItem>(content, "delegate", 14));
    QTRY_COMPARE(delegateVisible(item), true);

    listview->setProperty("contentY", 0);
    QVERIFY(item = findItem<QQuickItem>(content, "delegate", 4));
    QTRY_COMPARE(delegateVisible(item), true);

    delete window;
}

void tst_QQuickGridView::jsArrayChange()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.4; GridView {}", QUrl());

    QScopedPointer<QQuickGridView> view(qobject_cast<QQuickGridView *>(component.create()));
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

void tst_QQuickGridView::contentHeightWithDelayRemove_data()
{
    QTest::addColumn<bool>("useDelayRemove");
    QTest::addColumn<QByteArray>("removeFunc");
    QTest::addColumn<int>("countDelta");
    QTest::addColumn<qreal>("contentHeightDelta");

    QTest::newRow("remove without delayRemove")
            << false
            << QByteArray("takeOne")
            << -1
            << qreal(-1 * 100.0);

    QTest::newRow("remove with delayRemove")
            << true
            << QByteArray("takeOne")
            << -1
            << qreal(-1 * 100.0);

    QTest::newRow("remove with multiple delayRemove")
            << true
            << QByteArray("takeThree")
            << -3
            << qreal(-2 * 100.0);

    QTest::newRow("clear with delayRemove")
            << true
            << QByteArray("takeAll")
            << -5
            << qreal(-3 * 100.0);
}

void tst_QQuickGridView::contentHeightWithDelayRemove()
{
    QFETCH(bool, useDelayRemove);
    QFETCH(QByteArray, removeFunc);
    QFETCH(int, countDelta);
    QFETCH(qreal, contentHeightDelta);

    QQuickView *window = createView();
    window->setSource(testFileUrl("contentHeightWithDelayRemove.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickGridView *gridview = window->rootObject()->findChild<QQuickGridView*>();
    QTRY_VERIFY(gridview != 0);

    const int initialCount(gridview->count());
    const int eventualCount(initialCount + countDelta);

    const qreal initialContentHeight(gridview->contentHeight());
    const int eventualContentHeight(qRound(initialContentHeight + contentHeightDelta));

    gridview->setProperty("useDelayRemove", useDelayRemove);
    QMetaObject::invokeMethod(window->rootObject(), removeFunc.constData());
    QTest::qWait(50);
    QCOMPARE(gridview->count(), eventualCount);

    if (useDelayRemove) {
        QCOMPARE(qRound(gridview->contentHeight()), qRound(initialContentHeight));
        QTRY_COMPARE(qRound(gridview->contentHeight()), eventualContentHeight);
    } else {
        QCOMPARE(qRound(gridview->contentHeight()), eventualContentHeight);
    }

    delete window;
}

void tst_QQuickGridView::QTBUG_45640()
{
    QQuickView *window = createView();
    window->setSource(testFileUrl("qtbug45640.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickGridView *gridview = qobject_cast<QQuickGridView*>(window->rootObject());
    QVERIFY(gridview != 0);

    QCOMPARE(gridview->contentY(), qreal(-50.0));

    gridview->moveCurrentIndexDown();

    QTRY_VERIFY(gridview->contentY() > qreal(-50.0) && gridview->contentY() < qreal(0.0));

    delete window;
}

void tst_QQuickGridView::keyNavigationEnabled()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("gridview4.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));

    QQuickGridView *gridView = qobject_cast<QQuickGridView *>(window->rootObject());
    QVERIFY(gridView);
    QCOMPARE(gridView->isKeyNavigationEnabled(), true);

    gridView->setFocus(true);
    QVERIFY(gridView->hasActiveFocus());

    gridView->setHighlightMoveDuration(0);

    // If keyNavigationEnabled is not explicitly set to true, respect the original behavior
    // of disabling both mouse and keyboard interaction.
    QSignalSpy enabledSpy(gridView, SIGNAL(keyNavigationEnabledChanged()));
    gridView->setInteractive(false);
    QCOMPARE(enabledSpy.count(), 1);
    QCOMPARE(gridView->isKeyNavigationEnabled(), false);

    flick(window.data(), QPoint(200, 175), QPoint(200, 50), 100);
    QVERIFY(!gridView->isMoving());
    QCOMPARE(gridView->contentY(), 0.0);
    QCOMPARE(gridView->currentIndex(), 0);

    QTest::keyClick(window.data(), Qt::Key_Right);
    QCOMPARE(gridView->currentIndex(), 0);

    // Check that isKeyNavigationEnabled implicitly follows the value of interactive.
    gridView->setInteractive(true);
    QCOMPARE(enabledSpy.count(), 2);
    QCOMPARE(gridView->isKeyNavigationEnabled(), true);

    // Change it back again for the next check.
    gridView->setInteractive(false);
    QCOMPARE(enabledSpy.count(), 3);
    QCOMPARE(gridView->isKeyNavigationEnabled(), false);

    // Setting keyNavigationEnabled to true shouldn't enable mouse interaction.
    gridView->setKeyNavigationEnabled(true);
    QCOMPARE(enabledSpy.count(), 4);
    flick(window.data(), QPoint(200, 175), QPoint(200, 50), 100);
    QVERIFY(!gridView->isMoving());
    QCOMPARE(gridView->contentY(), 0.0);
    QCOMPARE(gridView->currentIndex(), 0);

    // Should now work.
    QTest::keyClick(window.data(), Qt::Key_Right);
    QCOMPARE(gridView->currentIndex(), 1);

    // Changing interactive now shouldn't result in keyNavigationEnabled changing,
    // since we broke the "binding".
    gridView->setInteractive(true);
    QCOMPARE(enabledSpy.count(), 4);

    // Keyboard interaction shouldn't work now.
    gridView->setKeyNavigationEnabled(false);
    QTest::keyClick(window.data(), Qt::Key_Right);
    QCOMPARE(gridView->currentIndex(), 1);
}

void tst_QQuickGridView::QTBUG_48870_fastModelUpdates()
{
    StressTestModel model;

    QScopedPointer<QQuickView> window(createView());
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("qtbug48870.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickGridView *view = findItem<QQuickGridView>(window->rootObject(), "view");
    QTRY_VERIFY(view != 0);

    QQuickItemViewPrivate *priv = QQuickItemViewPrivate::get(view);
    bool nonUnique;
    FxViewItem *item = Q_NULLPTR;
    int expectedIdx;
    QVERIFY(testVisibleItems(priv, &nonUnique, &item, &expectedIdx));

    for (int i = 0; i < 10; i++) {
        QTest::qWait(100);
        QVERIFY2(testVisibleItems(priv, &nonUnique, &item, &expectedIdx),
                 qPrintable(!item ? QString("Unexpected null item")
                            : nonUnique ? QString("Non-unique item at %1 and %2").arg(item->index).arg(expectedIdx)
                                        : QString("Found index %1, expected index is %3").arg(item->index).arg(expectedIdx)));
        if (i % 3 != 0) {
            if (i & 1)
                flick(window.data(), QPoint(100, 200), QPoint(100, 0), 100);
            else
                flick(window.data(), QPoint(100, 200), QPoint(100, 400), 100);
        }
    }
}

QTEST_MAIN(tst_QQuickGridView)

#include "tst_qquickgridview.moc"

