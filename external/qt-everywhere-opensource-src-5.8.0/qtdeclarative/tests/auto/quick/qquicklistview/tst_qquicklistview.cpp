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
#include <QtCore/QStringListModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QStandardItemModel>
#include <QtQuick/qquickview.h>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlexpression.h>
#include <QtQml/qqmlincubator.h>
#include <QtQuick/private/qquickitemview_p_p.h>
#include <QtQuick/private/qquicklistview_p.h>
#include <QtQuick/private/qquicktext_p.h>
#include <QtQml/private/qqmlobjectmodel_p.h>
#include <QtQml/private/qqmllistmodel_p.h>
#include <QtQml/private/qqmldelegatemodel_p.h>
#include "../../shared/util.h"
#include "../shared/viewtestutil.h"
#include "../shared/visualtestutil.h"
#include "incrementalmodel.h"
#include "proxytestinnermodel.h"
#include "randomsortmodel.h"
#include <math.h>

Q_DECLARE_METATYPE(Qt::LayoutDirection)
Q_DECLARE_METATYPE(QQuickItemView::VerticalLayoutDirection)
Q_DECLARE_METATYPE(QQuickItemView::PositionMode)
Q_DECLARE_METATYPE(QQuickListView::Orientation)
Q_DECLARE_METATYPE(Qt::Key)

using namespace QQuickViewTestUtil;
using namespace QQuickVisualTestUtil;

#define SHARE_VIEWS

class tst_QQuickListView : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_QQuickListView();

private slots:
    void init();
    void cleanupTestCase();
    // Test QAbstractItemModel model types
    void qAbstractItemModel_package_items();
    void qAbstractItemModel_items();

    void qAbstractItemModel_package_changed();
    void qAbstractItemModel_changed();

    void qAbstractItemModel_package_inserted();
    void qAbstractItemModel_inserted();
    void qAbstractItemModel_inserted_more();
    void qAbstractItemModel_inserted_more_data();
    void qAbstractItemModel_inserted_more_bottomToTop();
    void qAbstractItemModel_inserted_more_bottomToTop_data();

    void qAbstractItemModel_package_removed();
    void qAbstractItemModel_removed();
    void qAbstractItemModel_removed_more();
    void qAbstractItemModel_removed_more_data();
    void qAbstractItemModel_removed_more_bottomToTop();
    void qAbstractItemModel_removed_more_bottomToTop_data();

    void qAbstractItemModel_package_moved();
    void qAbstractItemModel_package_moved_data();
    void qAbstractItemModel_moved();
    void qAbstractItemModel_moved_data();
    void qAbstractItemModel_moved_bottomToTop();
    void qAbstractItemModel_moved_bottomToTop_data();

    void multipleChanges_condensed() { multipleChanges(true); }
    void multipleChanges_condensed_data() { multipleChanges_data(); }
    void multipleChanges_uncondensed() { multipleChanges(false); }
    void multipleChanges_uncondensed_data() { multipleChanges_data(); }

    void qAbstractItemModel_package_clear();
    void qAbstractItemModel_clear();
    void qAbstractItemModel_clear_bottomToTop();

    void insertBeforeVisible();
    void insertBeforeVisible_data();
    void swapWithFirstItem();
    void itemList();
    void itemListFlicker();
    void currentIndex_delayedItemCreation();
    void currentIndex_delayedItemCreation_data();
    void currentIndex();
    void noCurrentIndex();
    void keyNavigation();
    void keyNavigation_data();
    void enforceRange();
    void enforceRange_withoutHighlight();
    void spacing();
    void qAbstractItemModel_package_sections();
    void qAbstractItemModel_sections();
    void sectionsPositioning();
    void sectionsDelegate();
    void sectionsDragOutsideBounds_data();
    void sectionsDragOutsideBounds();
    void sectionsDelegate_headerVisibility();
    void sectionPropertyChange();
    void sectionDelegateChange();
    void sectionsItemInsertion();
    void cacheBuffer();
    void positionViewAtBeginningEnd();
    void positionViewAtIndex();
    void positionViewAtIndex_data();
    void resetModel();
    void propertyChanges();
    void componentChanges();
    void modelChanges();
    void manualHighlight();
    void initialZValues();
    void initialZValues_data();
    void header();
    void header_data();
    void header_delayItemCreation();
    void headerChangesViewport();
    void footer();
    void footer_data();
    void extents();
    void extents_data();
    void resetModel_headerFooter();
    void resizeView();
    void resizeViewAndRepaint();
    void sizeLessThan1();
    void QTBUG_14821();
    void resizeDelegate();
    void resizeFirstDelegate();
    void repositionResizedDelegate();
    void repositionResizedDelegate_data();
    void QTBUG_16037();
    void indexAt_itemAt_data();
    void indexAt_itemAt();
    void incrementalModel();
    void onAdd();
    void onAdd_data();
    void onRemove();
    void onRemove_data();
    void attachedProperties_QTBUG_32836();
    void rightToLeft();
    void test_mirroring();
    void margins();
    void marginsResize();
    void marginsResize_data();
    void creationContext();
    void snapToItem_data();
    void snapToItem();
    void snapOneItemResize_QTBUG_43555();
    void snapOneItem_data();
    void snapOneItem();
    void snapOneItemCurrentIndexRemoveAnimation();

    void QTBUG_9791();
    void QTBUG_11105();
    void QTBUG_21742();

    void asynchronous();
    void unrequestedVisibility();

    void populateTransitions();
    void populateTransitions_data();
    void sizeTransitions();
    void sizeTransitions_data();

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

    void flickBeyondBounds();
    void destroyItemOnCreation();

    void parentBinding();
    void defaultHighlightMoveDuration();
    void accessEmptyCurrentItem_QTBUG_30227();
    void delayedChanges_QTBUG_30555();
    void outsideViewportChangeNotAffectingView();
    void testProxyModelChangedAfterMove();

    void typedModel();
    void displayMargin();
    void negativeDisplayMargin();

    void highlightItemGeometryChanges();

    void QTBUG_36481();
    void QTBUG_35920();

    void stickyPositioning();
    void stickyPositioning_data();

    void roundingErrors();
    void roundingErrors_data();

    void QTBUG_38209();
    void programmaticFlickAtBounds();
    void programmaticFlickAtBounds2();

    void layoutChange();

    void QTBUG_39492_data();
    void QTBUG_39492();

    void jsArrayChange();
    void objectModel();

    void contentHeightWithDelayRemove();
    void contentHeightWithDelayRemove_data();

    void QTBUG_48044_currentItemNotVisibleAfterTransition();
    void QTBUG_48870_fastModelUpdates();

    void QTBUG_50105();
    void keyNavigationEnabled();
    void QTBUG_50097_stickyHeader_positionViewAtIndex();
    void itemFiltered();

private:
    template <class T> void items(const QUrl &source);
    template <class T> void changed(const QUrl &source);
    template <class T> void inserted(const QUrl &source);
    template <class T> void inserted_more(QQuickItemView::VerticalLayoutDirection verticalLayoutDirection = QQuickItemView::TopToBottom);
    template <class T> void removed(const QUrl &source, bool animated);
    template <class T> void removed_more(const QUrl &source, QQuickItemView::VerticalLayoutDirection verticalLayoutDirection = QQuickItemView::TopToBottom);
    template <class T> void moved(const QUrl &source, QQuickItemView::VerticalLayoutDirection verticalLayoutDirection = QQuickItemView::TopToBottom);
    template <class T> void clear(const QUrl &source, QQuickItemView::VerticalLayoutDirection verticalLayoutDirection = QQuickItemView::TopToBottom);
    template <class T> void sections(const QUrl &source);

    void multipleChanges(bool condensed);
    void multipleChanges_data();

    QList<int> toIntList(const QVariantList &list);
    void matchIndexLists(const QVariantList &indexLists, const QList<int> &expectedIndexes);
    void matchItemsAndIndexes(const QVariantMap &items, const QaimModel &model, const QList<int> &expectedIndexes);
    void matchItemLists(const QVariantList &itemLists, const QList<QQuickItem *> &expectedItems);

    void inserted_more_data();
    void removed_more_data();
    void moved_data();

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

class TestObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool error READ error WRITE setError NOTIFY changedError)
    Q_PROPERTY(bool animate READ animate NOTIFY changedAnim)
    Q_PROPERTY(bool invalidHighlight READ invalidHighlight NOTIFY changedHl)
    Q_PROPERTY(int cacheBuffer READ cacheBuffer NOTIFY changedCacheBuffer)

public:
    TestObject(QObject *parent = 0)
        : QObject(parent), mError(true), mAnimate(false), mInvalidHighlight(false)
        , mCacheBuffer(0) {}

    bool error() const { return mError; }
    void setError(bool err) { mError = err; emit changedError(); }

    bool animate() const { return mAnimate; }
    void setAnimate(bool anim) { mAnimate = anim; emit changedAnim(); }

    bool invalidHighlight() const { return mInvalidHighlight; }
    void setInvalidHighlight(bool invalid) { mInvalidHighlight = invalid; emit changedHl(); }

    int cacheBuffer() const { return mCacheBuffer; }
    void setCacheBuffer(int buffer) { mCacheBuffer = buffer; emit changedCacheBuffer(); }

signals:
    void changedError();
    void changedAnim();
    void changedHl();
    void changedCacheBuffer();

public:
    bool mError;
    bool mAnimate;
    bool mInvalidHighlight;
    int mCacheBuffer;
};

tst_QQuickListView::tst_QQuickListView() : m_view(0)
{
}

void tst_QQuickListView::init()
{
#ifdef SHARE_VIEWS
    if (m_view && QString(QTest::currentTestFunction()) != testForView) {
        testForView = QString();
        delete m_view;
        m_view = 0;
    }
#endif
    qmlRegisterType<QAbstractItemModel>();
    qmlRegisterType<ProxyTestInnerModel>("Proxy", 1, 0, "ProxyTestInnerModel");
    qmlRegisterType<QSortFilterProxyModel>("Proxy", 1, 0, "QSortFilterProxyModel");
}

void tst_QQuickListView::cleanupTestCase()
{
#ifdef SHARE_VIEWS
    testForView = QString();
    delete m_view;
    m_view = 0;
#endif
}

template <class T>
void tst_QQuickListView::items(const QUrl &source)
{
    QScopedPointer<QQuickView> window(createView());

    T model;
    model.addItem("Fred", "12345");
    model.addItem("John", "2345");
    model.addItem("Bob", "54321");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(source);
    qApp->processEvents();

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    listview->forceLayout();

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QMetaObject::invokeMethod(window->rootObject(), "checkProperties");
    QTRY_VERIFY(!testObject->error());

    QTRY_VERIFY(listview->highlightItem() != 0);
    QTRY_COMPARE(listview->count(), model.count());
    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());
    listview->forceLayout();
    QTRY_COMPARE(contentItem->childItems().count(), model.count()+1); // assumes all are visible, +1 for the (default) highlight item

    // current item should be first item
    QTRY_COMPARE(listview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 0));

    for (int i = 0; i < model.count(); ++i) {
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QTRY_VERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
        QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", i);
        QTRY_VERIFY(number != 0);
        QTRY_COMPARE(number->text(), model.number(i));
    }

    // switch to other delegate
    testObject->setAnimate(true);
    QMetaObject::invokeMethod(window->rootObject(), "checkProperties");
    QTRY_VERIFY(!testObject->error());
    QTRY_VERIFY(listview->currentItem());

    // set invalid highlight
    testObject->setInvalidHighlight(true);
    QMetaObject::invokeMethod(window->rootObject(), "checkProperties");
    QTRY_VERIFY(!testObject->error());
    QTRY_VERIFY(listview->currentItem());
    QTRY_VERIFY(!listview->highlightItem());

    // back to normal highlight
    testObject->setInvalidHighlight(false);
    QMetaObject::invokeMethod(window->rootObject(), "checkProperties");
    QTRY_VERIFY(!testObject->error());
    QTRY_VERIFY(listview->currentItem());
    QTRY_VERIFY(listview->highlightItem() != 0);

    // set an empty model and confirm that items are destroyed
    T model2;
    ctxt->setContextProperty("testModel", &model2);

    // Force a layout, necessary if ListView is completed before VisualDataModel.
    listview->forceLayout();

    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    QTRY_COMPARE(itemCount, 0);

    QTRY_COMPARE(listview->highlightResizeVelocity(), 1000.0);
    QTRY_COMPARE(listview->highlightMoveVelocity(), 100000.0);

    delete testObject;
}


template <class T>
void tst_QQuickListView::changed(const QUrl &source)
{
    QScopedPointer<QQuickView> window(createView());

    T model;
    model.addItem("Fred", "12345");
    model.addItem("John", "2345");
    model.addItem("Bob", "54321");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(source);
    qApp->processEvents();

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    listview->forceLayout();

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    // Force a layout, necessary if ListView is completed before VisualDataModel.
    listview->forceLayout();

    model.modifyItem(1, "Will", "9876");
    QQuickText *name = findItem<QQuickText>(contentItem, "textName", 1);
    QTRY_VERIFY(name != 0);
    QTRY_COMPARE(name->text(), model.name(1));
    QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", 1);
    QTRY_VERIFY(number != 0);
    QTRY_COMPARE(number->text(), model.number(1));

    delete testObject;
}

template <class T>
void tst_QQuickListView::inserted(const QUrl &source)
{
    QScopedPointer<QQuickView> window(createView());
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    T model;
    model.addItem("Fred", "12345");
    model.addItem("John", "2345");
    model.addItem("Bob", "54321");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(source);
    qApp->processEvents();

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
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

    // Confirm items positioned correctly
    for (int i = 0; i < model.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_COMPARE(item->y(), i*20.0);
    }

    model.insertItem(0, "Foo", "1111"); // zero index, and current item

    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());
    QTRY_COMPARE(contentItem->childItems().count(), model.count()+1); // assumes all are visible, +1 for the (default) highlight item

    name = findItem<QQuickText>(contentItem, "textName", 0);
    QTRY_VERIFY(name != 0);
    QTRY_COMPARE(name->text(), model.name(0));
    number = findItem<QQuickText>(contentItem, "textNumber", 0);
    QTRY_VERIFY(number != 0);
    QTRY_COMPARE(number->text(), model.number(0));

    QTRY_COMPARE(listview->currentIndex(), 1);

    // Confirm items positioned correctly
    for (int i = 0; i < model.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_COMPARE(item->y(), i*20.0);
    }

    for (int i = model.count(); i < 30; ++i)
        model.insertItem(i, "Hello", QString::number(i));

    listview->setContentY(80);

    // Insert item outside visible area
    model.insertItem(1, "Hello", "1324");

    QTRY_COMPARE(listview->contentY(), qreal(80));

    // Confirm items positioned correctly
    for (int i = 5; i < 5+15; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), i*20.0 - 20.0);
    }

//    QTRY_COMPARE(listview->contentItemHeight(), model.count() * 20.0);

    // QTBUG-19675
    model.clear();
    model.insertItem(0, "Hello", "1234");
    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());

    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item);
    QTRY_COMPARE(item->y() - listview->contentY(), 0.);

    delete testObject;
}

template <class T>
void tst_QQuickListView::inserted_more(QQuickItemView::VerticalLayoutDirection verticalLayoutDirection)
{
    QFETCH(qreal, contentY);
    QFETCH(int, insertIndex);
    QFETCH(int, insertCount);
    QFETCH(qreal, itemsOffsetAfterMove);

    T model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("listviewtest.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    if (verticalLayoutDirection == QQuickItemView::BottomToTop) {
        listview->setVerticalLayoutDirection(verticalLayoutDirection);
        QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
        contentY = -listview->height() - contentY;
    }
    listview->setContentY(contentY);

    QQuickItemViewPrivate::get(listview)->layout();

    QList<QPair<QString, QString> > newData;
    for (int i=0; i<insertCount; i++)
        newData << qMakePair(QString("value %1").arg(i), QString::number(i));
    model.insertItems(insertIndex, newData);

    //Wait for polish (updates list to the model changes)
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QTRY_COMPARE(listview->property("count").toInt(), model.count());

    // FIXME This is NOT checking anything about visibleItems.first()
#if 0
    // check visibleItems.first() is in correct position
    QQuickItem *item0 = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item0);
    if (verticalLayoutDirection == QQuickItemView::BottomToTop)
        QCOMPARE(item0->y(), -item0->height() - itemsOffsetAfterMove);
    else
        QCOMPARE(item0->y(), itemsOffsetAfterMove);
#endif

    QList<FxViewItem *> visibleItems = QQuickItemViewPrivate::get(listview)->visibleItems;
    for (QList<FxViewItem *>::const_iterator itemIt = visibleItems.begin(); itemIt != visibleItems.end(); ++itemIt) {
        FxViewItem *item = *itemIt;
        if (item->item->position().y() >= 0 && item->item->position().y() < listview->height()) {
            QVERIFY(!QQuickItemPrivate::get(item->item)->culled);
        }
    }

    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    int firstVisibleIndex = -1;
    for (int i=0; i<items.count(); i++) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (item && !QQuickItemPrivate::get(item)->culled) {
            firstVisibleIndex = i;
            break;
        }
    }
    QVERIFY2(firstVisibleIndex >= 0, QTest::toString(firstVisibleIndex));

    // Confirm items positioned correctly and indexes correct
    QQuickText *name;
    QQuickText *number;
    const qreal visibleFromPos = listview->contentY() - listview->displayMarginBeginning() - listview->cacheBuffer();
    const qreal visibleToPos = listview->contentY() + listview->height() + listview->displayMarginEnd() + listview->cacheBuffer();
    for (int i = firstVisibleIndex; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        qreal pos = i*20.0 + itemsOffsetAfterMove;
        if (verticalLayoutDirection == QQuickItemView::BottomToTop)
            pos = -item->height() - pos;
        // Items outside the visible area (including cache buffer) should be skipped
        if (pos > visibleToPos || pos < visibleFromPos) {
            QTRY_VERIFY2(QQuickItemPrivate::get(item)->culled || item->y() < visibleFromPos || item->y() > visibleToPos,
                     QTest::toString(QString("index %5, y %1, from %2, to %3, expected pos %4, culled %6").
                                     arg(item->y()).arg(visibleFromPos).arg(visibleToPos).arg(pos).arg(i).arg(bool(QQuickItemPrivate::get(item)->culled))));
            continue;
        }
        QTRY_COMPARE(item->y(), pos);
        name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
        number = findItem<QQuickText>(contentItem, "textNumber", i);
        QVERIFY(number != 0);
        QTRY_COMPARE(number->text(), model.number(i));
    }

    releaseView(window);
    delete testObject;
}

void tst_QQuickListView::inserted_more_data()
{
    QTest::addColumn<qreal>("contentY");
    QTest::addColumn<int>("insertIndex");
    QTest::addColumn<int>("insertCount");
    QTest::addColumn<qreal>("itemsOffsetAfterMove");

    QTest::newRow("add 1, before visible items")
            << 80.0     // show 4-19
            << 3 << 1
            << -20.0;   // insert above first visible i.e. 0 is at -20, first visible should not move

    QTest::newRow("add multiple, before visible")
            << 80.0     // show 4-19
            << 3 << 3
            << -20.0 * 3;   // again first visible should not move

    QTest::newRow("add 1, at start of visible, content at start")
            << 0.0
            << 0 << 1
            << 0.0;

    QTest::newRow("add multiple, start of visible, content at start")
            << 0.0
            << 0 << 3
            << 0.0;

    QTest::newRow("add 1, at start of visible, content not at start")
            << 80.0     // show 4-19
            << 4 << 1
            << 0.0;

    QTest::newRow("add multiple, at start of visible, content not at start")
            << 80.0     // show 4-19
            << 4 << 3
            << 0.0;


    QTest::newRow("add 1, at end of visible, content at start")
            << 0.0
            << 15 << 1
            << 0.0;

    QTest::newRow("add multiple, at end of visible, content at start")
            << 0.0
            << 15 << 3
            << 0.0;

    QTest::newRow("add 1, at end of visible, content not at start")
            << 80.0     // show 4-19
            << 19 << 1
            << 0.0;

    QTest::newRow("add multiple, at end of visible, content not at start")
            << 80.0     // show 4-19
            << 19 << 3
            << 0.0;


    QTest::newRow("add 1, after visible, content at start")
            << 0.0
            << 16 << 1
            << 0.0;

    QTest::newRow("add multiple, after visible, content at start")
            << 0.0
            << 16 << 3
            << 0.0;

    QTest::newRow("add 1, after visible, content not at start")
            << 80.0     // show 4-19
            << 20 << 1
            << 0.0;

    QTest::newRow("add multiple, after visible, content not at start")
            << 80.0     // show 4-19
            << 20 << 3
            << 0.0;

    QTest::newRow("add multiple, within visible, content at start")
            << 0.0
            << 2 << 50
            << 0.0;
}

void tst_QQuickListView::insertBeforeVisible()
{
    QFETCH(int, insertIndex);
    QFETCH(int, insertCount);
    QFETCH(int, removeIndex);
    QFETCH(int, removeCount);
    QFETCH(int, cacheBuffer);

    QQuickText *name;
    QQuickView *window = getView();

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("listviewtest.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    listview->setCacheBuffer(cacheBuffer);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // trigger a refill (not just setting contentY) so that the visibleItems grid is updated
    int firstVisibleIndex = 20;     // move to an index where the top item is not visible
    listview->setContentY(firstVisibleIndex * 20.0);
    listview->setCurrentIndex(firstVisibleIndex);

    qApp->processEvents();
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    QTRY_COMPARE(listview->currentIndex(), firstVisibleIndex);
    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", firstVisibleIndex);
    QVERIFY(item);
    QCOMPARE(item->y(), listview->contentY());

    if (removeCount > 0)
        model.removeItems(removeIndex, removeCount);

    if (insertCount > 0) {
        QList<QPair<QString, QString> > newData;
        for (int i=0; i<insertCount; i++)
            newData << qMakePair(QString("value %1").arg(i), QString::number(i));
        model.insertItems(insertIndex, newData);
        QTRY_COMPARE(listview->property("count").toInt(), model.count());
    }

    // now, moving to the top of the view should position the inserted items correctly
    int itemsOffsetAfterMove = (removeCount - insertCount) * 20;
    listview->setCurrentIndex(0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    QTRY_COMPARE(listview->currentIndex(), 0);
    QTRY_COMPARE(listview->contentY(), 0.0 + itemsOffsetAfterMove);

    // Confirm items positioned correctly and indexes correct
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->y(), i*20.0 + itemsOffsetAfterMove);
        name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
    delete testObject;
}

void tst_QQuickListView::insertBeforeVisible_data()
{
    QTest::addColumn<int>("insertIndex");
    QTest::addColumn<int>("insertCount");
    QTest::addColumn<int>("removeIndex");
    QTest::addColumn<int>("removeCount");
    QTest::addColumn<int>("cacheBuffer");

    QTest::newRow("insert 1 at 0, 0 buffer") << 0 << 1 << 0 << 0 << 0;
    QTest::newRow("insert 1 at 0, 100 buffer") << 0 << 1 << 0 << 0 << 100;
    QTest::newRow("insert 1 at 0, 500 buffer") << 0 << 1 << 0 << 0 << 500;

    QTest::newRow("insert 1 at 1, 0 buffer") << 1 << 1 << 0 << 0 << 0;
    QTest::newRow("insert 1 at 1, 100 buffer") << 1 << 1 << 0 << 0 << 100;
    QTest::newRow("insert 1 at 1, 500 buffer") << 1 << 1 << 0 << 0 << 500;

    QTest::newRow("insert multiple at 0, 0 buffer") << 0 << 3 << 0 << 0 << 0;
    QTest::newRow("insert multiple at 0, 100 buffer") << 0 << 3 << 0 << 0 << 100;
    QTest::newRow("insert multiple at 0, 500 buffer") << 0 << 3 << 0 << 0 << 500;

    QTest::newRow("insert multiple at 1, 0 buffer") << 1 << 3 << 0 << 0 << 0;
    QTest::newRow("insert multiple at 1, 100 buffer") << 1 << 3 << 0 << 0 << 100;
    QTest::newRow("insert multiple at 1, 500 buffer") << 1 << 3 << 0 << 0 << 500;

    QTest::newRow("remove 1 at 0, 0 buffer") << 0 << 0 << 0 << 1 << 0;
    QTest::newRow("remove 1 at 0, 100 buffer") << 0 << 0 << 0 << 1 << 100;
    QTest::newRow("remove 1 at 0, 500 buffer") << 0 << 0 << 0 << 1 << 500;

    QTest::newRow("remove 1 at 1, 0 buffer") << 0 << 0 << 1 << 1 << 0;
    QTest::newRow("remove 1 at 1, 100 buffer") << 0 << 0 << 1 << 1 << 100;
    QTest::newRow("remove 1 at 1, 500 buffer") << 0 << 0 << 1 << 1 << 500;

    QTest::newRow("remove multiple at 0, 0 buffer") << 0 << 0 << 0 << 3 << 0;
    QTest::newRow("remove multiple at 0, 100 buffer") << 0 << 0 << 0 << 3 << 100;
    QTest::newRow("remove multiple at 0, 500 buffer") << 0 << 0 << 0 << 3 << 500;

    QTest::newRow("remove multiple at 1, 0 buffer") << 0 << 0 << 1 << 3 << 0;
    QTest::newRow("remove multiple at 1, 100 buffer") << 0 << 0 << 1 << 3 << 100;
    QTest::newRow("remove multiple at 1, 500 buffer") << 0 << 0 << 1 << 3 << 500;
}

template <class T>
void tst_QQuickListView::removed(const QUrl &source, bool /* animated */)
{
    QScopedPointer<QQuickView> window(createView());

    T model;
    for (int i = 0; i < 50; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(source);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    model.removeItem(1);
    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());

    QQuickText *name = findItem<QQuickText>(contentItem, "textName", 1);
    QTRY_VERIFY(name != 0);
    QTRY_COMPARE(name->text(), model.name(1));
    QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", 1);
    QTRY_VERIFY(number != 0);
    QTRY_COMPARE(number->text(), model.number(1));

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20));
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
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(),i*20.0);
    }

    // Remove items not visible
    model.removeItem(18);
    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());

    // Confirm items positioned correctly
    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(),i*20.0);
    }

    // Remove items before visible
    listview->setContentY(80);
    listview->setCurrentIndex(10);

    model.removeItem(1); // post: top item will be at 20
    QTRY_COMPARE(window->rootObject()->property("count").toInt(), model.count());

    // Confirm items positioned correctly
    for (int i = 2; i < 18; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(),20+i*20.0);
    }

    // Remove current index
    QTRY_COMPARE(listview->currentIndex(), 9);
    QQuickItem *oldCurrent = listview->currentItem();
    model.removeItem(9);

    QTRY_COMPARE(listview->currentIndex(), 9);
    QTRY_VERIFY(listview->currentItem() != oldCurrent);

    listview->setContentY(20); // That's the top now
    // let transitions settle.
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(),20+i*20.0);
    }

    // remove current item beyond visible items.
    listview->setCurrentIndex(20);
    listview->setContentY(40);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    model.removeItem(20);
    QTRY_COMPARE(listview->currentIndex(), 20);
    QTRY_VERIFY(listview->currentItem() != 0);

    // remove item before current, but visible
    listview->setCurrentIndex(8);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    oldCurrent = listview->currentItem();
    model.removeItem(6);

    QTRY_COMPARE(listview->currentIndex(), 7);
    QTRY_COMPARE(listview->currentItem(), oldCurrent);

    listview->setContentY(80);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // remove all visible items
    model.removeItems(1, 18);
    QTRY_COMPARE(listview->count() , model.count());

    // Confirm items positioned correctly
    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount-1; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i+1);
        if (!item) qWarning() << "Item" << i+1 << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(),80+i*20.0);
    }

    model.removeItems(1, 17);
    QTRY_COMPARE(listview->count() , model.count());

    model.removeItems(2, 1);
    QTRY_COMPARE(listview->count() , model.count());

    model.addItem("New", "1");
    QTRY_COMPARE(listview->count() , model.count());

    QTRY_VERIFY(name = findItem<QQuickText>(contentItem, "textName", model.count()-1));
    QCOMPARE(name->text(), QString("New"));

    // Add some more items so that we don't run out
    model.clear();
    for (int i = 0; i < 50; i++)
        model.addItem("Item" + QString::number(i), "");

    // QTBUG-QTBUG-20575
    listview->setCurrentIndex(0);
    listview->setContentY(30);
    model.removeItem(0);
    QTRY_VERIFY(name = findItem<QQuickText>(contentItem, "textName", 0));

    // QTBUG-19198 move to end and remove all visible items one at a time.
    listview->positionViewAtEnd();
    for (int i = 0; i < 18; ++i)
        model.removeItems(model.count() - 1, 1);
    QTRY_VERIFY(findItems<QQuickItem>(contentItem, "wrapper").count() > 16);

    delete testObject;
}

template <class T>
void tst_QQuickListView::removed_more(const QUrl &source, QQuickItemView::VerticalLayoutDirection verticalLayoutDirection)
{
    QFETCH(qreal, contentY);
    QFETCH(int, removeIndex);
    QFETCH(int, removeCount);
    QFETCH(qreal, itemsOffsetAfterMove);

    QQuickView *window = getView();

    T model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(source);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    if (verticalLayoutDirection == QQuickItemView::BottomToTop) {
        listview->setVerticalLayoutDirection(verticalLayoutDirection);
        QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
        contentY = -listview->height() - contentY;
    }
    listview->setContentY(contentY);

    model.removeItems(removeIndex, removeCount);
    //Wait for polish (updates list to the model changes)
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QTRY_COMPARE(listview->property("count").toInt(), model.count());

    // check visibleItems.first() is in correct position
    QQuickItem *item0 = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item0);
    QVERIFY(item0);
    if (verticalLayoutDirection == QQuickItemView::BottomToTop)
        QCOMPARE(item0->y(), -item0->height() - itemsOffsetAfterMove);
    else
        QCOMPARE(item0->y(), itemsOffsetAfterMove);

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
    QQuickText *name;
    QQuickText *number;
    for (int i = firstVisibleIndex; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        qreal pos = i*20.0 + itemsOffsetAfterMove;
        if (verticalLayoutDirection == QQuickItemView::BottomToTop)
            pos = -item0->height() - pos;
        QTRY_COMPARE(item->y(), pos);
        name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
        number = findItem<QQuickText>(contentItem, "textNumber", i);
        QVERIFY(number != 0);
        QTRY_COMPARE(number->text(), model.number(i));
    }

    releaseView(window);
    delete testObject;
}

void tst_QQuickListView::removed_more_data()
{
    QTest::addColumn<qreal>("contentY");
    QTest::addColumn<int>("removeIndex");
    QTest::addColumn<int>("removeCount");
    QTest::addColumn<qreal>("itemsOffsetAfterMove");

    QTest::newRow("remove 1, before visible items")
            << 80.0     // show 4-19
            << 3 << 1
            << 20.0;   // visible items slide down by 1 item so that first visible does not move

    QTest::newRow("remove multiple, all before visible items")
            << 80.0
            << 1 << 3
            << 20.0 * 3;

    QTest::newRow("remove multiple, all before visible items, remove item 0")
            << 80.0
            << 0 << 4
            << 20.0 * 4;

    // remove 1,2,3 before the visible pos, 0 moves down to just before the visible pos,
    // items 4,5 are removed from view, item 6 slides up to original pos of item 4 (80px)
    QTest::newRow("remove multiple, mix of items from before and within visible items")
            << 80.0
            << 1 << 5
            << 20.0 * 3;    // adjust for the 3 items removed before the visible

    QTest::newRow("remove multiple, mix of items from before and within visible items, remove item 0")
            << 80.0
            << 0 << 6
            << 20.0 * 4;    // adjust for the 3 items removed before the visible


    QTest::newRow("remove 1, from start of visible, content at start")
            << 0.0
            << 0 << 1
            << 0.0;

    QTest::newRow("remove multiple, from start of visible, content at start")
            << 0.0
            << 0 << 3
            << 0.0;

    QTest::newRow("remove 1, from start of visible, content not at start")
            << 80.0     // show 4-19
            << 4 << 1
            << 0.0;

    QTest::newRow("remove multiple, from start of visible, content not at start")
            << 80.0     // show 4-19
            << 4 << 3
            << 0.0;


    QTest::newRow("remove 1, from middle of visible, content at start")
            << 0.0
            << 10 << 1
            << 0.0;

    QTest::newRow("remove multiple, from middle of visible, content at start")
            << 0.0
            << 10 << 5
            << 0.0;

    QTest::newRow("remove 1, from middle of visible, content not at start")
            << 80.0     // show 4-19
            << 10 << 1
            << 0.0;

    QTest::newRow("remove multiple, from middle of visible, content not at start")
            << 80.0     // show 4-19
            << 10 << 5
            << 0.0;


    QTest::newRow("remove 1, after visible, content at start")
            << 0.0
            << 16 << 1
            << 0.0;

    QTest::newRow("remove multiple, after visible, content at start")
            << 0.0
            << 16 << 5
            << 0.0;

    QTest::newRow("remove 1, after visible, content not at middle")
            << 80.0     // show 4-19
            << 16+4 << 1
            << 0.0;

    QTest::newRow("remove multiple, after visible, content not at start")
            << 80.0     // show 4-19
            << 16+4 << 5
            << 0.0;

    QTest::newRow("remove multiple, mix of items from within and after visible items")
            << 80.0
            << 18 << 5
            << 0.0;
}

template <class T>
void tst_QQuickListView::clear(const QUrl &source, QQuickItemView::VerticalLayoutDirection verticalLayoutDirection)
{
    QScopedPointer<QQuickView> window(createView());

    T model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(source);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    listview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    model.clear();

    QTRY_COMPARE(findItems<QQuickListView>(contentItem, "wrapper").count(), 0);
    QTRY_COMPARE(listview->count(), 0);
    QTRY_VERIFY(!listview->currentItem());
    if (verticalLayoutDirection == QQuickItemView::TopToBottom)
        QTRY_COMPARE(listview->contentY(), 0.0);
    else
        QTRY_COMPARE(listview->contentY(), -listview->height());
    QCOMPARE(listview->currentIndex(), -1);

    QCOMPARE(listview->contentHeight(), 0.0);

    // confirm sanity when adding an item to cleared list
    model.addItem("New", "1");
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), 1);
    QVERIFY(listview->currentItem() != 0);
    QCOMPARE(listview->currentIndex(), 0);

    delete testObject;
}

template <class T>
void tst_QQuickListView::moved(const QUrl &source, QQuickItemView::VerticalLayoutDirection verticalLayoutDirection)
{
    QFETCH(qreal, contentY);
    QFETCH(int, from);
    QFETCH(int, to);
    QFETCH(int, count);
    QFETCH(qreal, itemsOffsetAfterMove);

    QQuickText *name;
    QQuickText *number;
    QQuickView *window = getView();

    T model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(source);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    // always need to wait for view to be painted before the first move()
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    bool waitForPolish = (contentY != 0);
    if (verticalLayoutDirection == QQuickItemView::BottomToTop) {
        listview->setVerticalLayoutDirection(verticalLayoutDirection);
        QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
        contentY = -listview->height() - contentY;
    }
    listview->setContentY(contentY);
    if (waitForPolish)
        QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    model.moveItems(from, to, count);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

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
        qreal pos = i*20.0 + itemsOffsetAfterMove;
        if (verticalLayoutDirection == QQuickItemView::BottomToTop)
            pos = -item->height() - pos;
        QTRY_COMPARE(item->y(), pos);
        name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
        number = findItem<QQuickText>(contentItem, "textNumber", i);
        QVERIFY(number != 0);
        QTRY_COMPARE(number->text(), model.number(i));

        // current index should have been updated
        if (item == listview->currentItem())
            QTRY_COMPARE(listview->currentIndex(), i);
    }

    releaseView(window);
    delete testObject;
}

void tst_QQuickListView::moved_data()
{
    QTest::addColumn<qreal>("contentY");
    QTest::addColumn<int>("from");
    QTest::addColumn<int>("to");
    QTest::addColumn<int>("count");
    QTest::addColumn<qreal>("itemsOffsetAfterMove");

    // model starts with 30 items, each 20px high, in area 320px high
    // 16 items should be visible at a time
    // itemsOffsetAfterMove should be > 0 whenever items above the visible pos have moved

    QTest::newRow("move 1 forwards, within visible items")
            << 0.0
            << 1 << 4 << 1
            << 0.0;

    QTest::newRow("move 1 forwards, from non-visible -> visible")
            << 80.0     // show 4-19
            << 1 << 18 << 1
            << 20.0;    // removed 1 item above the first visible, so item 0 should drop down by 1 to minimize movement

    QTest::newRow("move 1 forwards, from non-visible -> visible (move first item)")
            << 80.0     // show 4-19
            << 0 << 4 << 1
            << 20.0;    // first item has moved to below item4, everything drops down by size of 1 item

    QTest::newRow("move 1 forwards, from visible -> non-visible")
            << 0.0
            << 1 << 16 << 1
            << 0.0;

    QTest::newRow("move 1 forwards, from visible -> non-visible (move first item)")
            << 0.0
            << 0 << 16 << 1
            << 0.0;


    QTest::newRow("move 1 backwards, within visible items")
            << 0.0
            << 4 << 1 << 1
            << 0.0;

    QTest::newRow("move 1 backwards, within visible items (to first index)")
            << 0.0
            << 4 << 0 << 1
            << 0.0;

    QTest::newRow("move 1 backwards, from non-visible -> visible")
            << 0.0
            << 20 << 4 << 1
            << 0.0;

    QTest::newRow("move 1 backwards, from non-visible -> visible (move last item)")
            << 0.0
            << 29 << 15 << 1
            << 0.0;

    QTest::newRow("move 1 backwards, from visible -> non-visible")
            << 80.0     // show 4-19
            << 16 << 1 << 1
            << -20.0;   // to minimize movement, item 0 moves to -20, and other items do not move

    QTest::newRow("move 1 backwards, from visible -> non-visible (move first item)")
            << 80.0     // show 4-19
            << 16 << 0 << 1
            << -20.0;   // to minimize movement, item 16 (now at 0) moves to -20, and other items do not move


    QTest::newRow("move multiple forwards, within visible items")
            << 0.0
            << 0 << 5 << 3
            << 0.0;

    QTest::newRow("move multiple forwards, before visible items")
            << 140.0     // show 7-22
            << 4 << 5 << 3      // 4,5,6 move to below 7
            << 20.0 * 3;      // 4,5,6 moved down

    QTest::newRow("move multiple forwards, from non-visible -> visible")
            << 80.0     // show 4-19
            << 1 << 5 << 3
            << 20.0 * 3;    // moving 3 from above the content y should adjust y positions accordingly

    QTest::newRow("move multiple forwards, from non-visible -> visible (move first item)")
            << 80.0     // show 4-19
            << 0 << 5 << 3
            << 20.0 * 3;        // moving 3 from above the content y should adjust y positions accordingly

    QTest::newRow("move multiple forwards, mix of non-visible/visible")
            << 40.0
            << 1 << 16 << 2
            << 20.0;    // item 1,2 are removed, item 3 is now first visible

    QTest::newRow("move multiple forwards, to bottom of view")
            << 0.0
            << 5 << 13 << 3
            << 0.0;

    QTest::newRow("move multiple forwards, to bottom of view, first->last")
            << 0.0
            << 0 << 13 << 3
            << 0.0;

    QTest::newRow("move multiple forwards, to bottom of view, content y not 0")
            << 80.0
            << 5+4 << 13+4 << 3
            << 0.0;

    QTest::newRow("move multiple forwards, from visible -> non-visible")
            << 0.0
            << 1 << 16 << 3
            << 0.0;

    QTest::newRow("move multiple forwards, from visible -> non-visible (move first item)")
            << 0.0
            << 0 << 16 << 3
            << 0.0;


    QTest::newRow("move multiple backwards, within visible items")
            << 0.0
            << 4 << 1 << 3
            << 0.0;

    QTest::newRow("move multiple backwards, within visible items (move first item)")
            << 0.0
            << 10 << 0 << 3
            << 0.0;

    QTest::newRow("move multiple backwards, from non-visible -> visible")
            << 0.0
            << 20 << 4 << 3
            << 0.0;

    QTest::newRow("move multiple backwards, from non-visible -> visible (move last item)")
            << 0.0
            << 27 << 10 << 3
            << 0.0;

    QTest::newRow("move multiple backwards, from visible -> non-visible")
            << 80.0     // show 4-19
            << 16 << 1 << 3
            << -20.0 * 3;   // to minimize movement, 0 moves by -60, and other items do not move

    QTest::newRow("move multiple backwards, from visible -> non-visible (move first item)")
            << 80.0     // show 4-19
            << 16 << 0 << 3
            << -20.0 * 3;   // to minimize movement, 16,17,18 move to above item 0, and other items do not move
}

void tst_QQuickListView::multipleChanges(bool condensed)
{
    QFETCH(int, startCount);
    QFETCH(QList<ListChange>, changes);
    QFETCH(int, newCount);
    QFETCH(int, newCurrentIndex);

    QQuickView *window = getView();

    QaimModel model;
    for (int i = 0; i < startCount; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("listviewtest.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

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
                listview->setCurrentIndex(changes[i].index);
                break;
            case ListChange::SetContentY:
                listview->setContentY(changes[i].pos);
                break;
            default:
                continue;
        }
        if (!condensed) {
            QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
        }
    }
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QCOMPARE(listview->count(), newCount);
    QCOMPARE(listview->count(), model.count());
    QCOMPARE(listview->currentIndex(), newCurrentIndex);

    QQuickText *name;
    QQuickText *number;
    QQuickItem *contentItem = listview->contentItem();
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

    delete testObject;
    releaseView(window);
}

void tst_QQuickListView::multipleChanges_data()
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

void tst_QQuickListView::swapWithFirstItem()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("listviewtest.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // ensure content position is stable
    listview->setContentY(0);
    model.moveItem(1, 0);
    QTRY_COMPARE(listview->contentY(), qreal(0));

    delete testObject;
}

void tst_QQuickListView::enforceRange()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("listview-enforcerange.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QTRY_COMPARE(listview->preferredHighlightBegin(), 100.0);
    QTRY_COMPARE(listview->preferredHighlightEnd(), 100.0);
    QTRY_COMPARE(listview->highlightRangeMode(), QQuickListView::StrictlyEnforceRange);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    // view should be positioned at the top of the range.
    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QTRY_VERIFY(item);
    QTRY_COMPARE(listview->contentY(), -100.0);

    QQuickText *name = findItem<QQuickText>(contentItem, "textName", 0);
    QTRY_VERIFY(name != 0);
    QTRY_COMPARE(name->text(), model.name(0));
    QQuickText *number = findItem<QQuickText>(contentItem, "textNumber", 0);
    QTRY_VERIFY(number != 0);
    QTRY_COMPARE(number->text(), model.number(0));

    // Check currentIndex is updated when contentItem moves
    listview->setContentY(20);

    QTRY_COMPARE(listview->currentIndex(), 6);

    // change model
    QaimModel model2;
    for (int i = 0; i < 5; i++)
        model2.addItem("Item" + QString::number(i), "");

    ctxt->setContextProperty("testModel", &model2);
    QCOMPARE(listview->count(), 5);
}

void tst_QQuickListView::enforceRange_withoutHighlight()
{
    // QTBUG-20287
    // If no highlight is set but StrictlyEnforceRange is used, the content should still move
    // to the correct position (i.e. to the next/previous item, not next/previous section)
    // when moving up/down via incrementCurrentIndex() and decrementCurrentIndex()

    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    model.addItem("Item 0", "a");
    model.addItem("Item 1", "b");
    model.addItem("Item 2", "b");
    model.addItem("Item 3", "c");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("listview-enforcerange-nohighlight.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    qreal expectedPos = -100.0;

    expectedPos += 10.0;    // scroll past 1st section's delegate (10px height)
    QTRY_COMPARE(listview->contentY(), expectedPos);

    expectedPos += 20 + 10;     // scroll past 1st section and section delegate of 2nd section
    QTest::keyClick(window.data(), Qt::Key_Down);

    QTRY_COMPARE(listview->contentY(), expectedPos);

    expectedPos += 20;     // scroll past 1st item of 2nd section
    QTest::keyClick(window.data(), Qt::Key_Down);
    QTRY_COMPARE(listview->contentY(), expectedPos);

    expectedPos += 20 + 10;     // scroll past 2nd item of 2nd section and section delegate of 3rd section
    QTest::keyClick(window.data(), Qt::Key_Down);
    QTRY_COMPARE(listview->contentY(), expectedPos);
}

void tst_QQuickListView::spacing()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("listviewtest.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20));
    }

    listview->setSpacing(10);
    QTRY_COMPARE(listview->spacing(), qreal(10));

    // Confirm items positioned correctly
    QTRY_VERIFY(findItems<QQuickItem>(contentItem, "wrapper").count() == 11);
    for (int i = 0; i < 11; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*30));
    }

    listview->setSpacing(0);

    // Confirm items positioned correctly
    QTRY_VERIFY(findItems<QQuickItem>(contentItem, "wrapper").count() >= 16);
    for (int i = 0; i < 16; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), i*20.0);
    }

    delete testObject;
}

template <typename T>
void tst_QQuickListView::sections(const QUrl &source)
{
    QScopedPointer<QQuickView> window(createView());

    T model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), QString::number(i/5));

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(source);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20 + ((i+4)/5) * 20));
        QQuickText *next = findItem<QQuickText>(item, "nextSection");
        QCOMPARE(next->text().toInt(), (i+1)/5);
    }

    QVERIFY(!listview->property("sectionsInvalidOnCompletion").toBool());

    QSignalSpy currentSectionChangedSpy(listview, SIGNAL(currentSectionChanged()));

    // Remove section boundary
    model.removeItem(5);
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), model.count());

    // New section header created
    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", 5);
    QTRY_VERIFY(item);
    QTRY_COMPARE(item->height(), 40.0);

    model.insertItem(3, "New Item", "0");
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), model.count());

    // Section header moved
    item = findItem<QQuickItem>(contentItem, "wrapper", 5);
    QTRY_VERIFY(item);
    QTRY_COMPARE(item->height(), 20.0);

    item = findItem<QQuickItem>(contentItem, "wrapper", 6);
    QTRY_VERIFY(item);
    QTRY_COMPARE(item->height(), 40.0);

    // insert item which will become a section header
    model.insertItem(6, "Replace header", "1");
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), model.count());

    item = findItem<QQuickItem>(contentItem, "wrapper", 6);
    QTRY_VERIFY(item);
    QTRY_COMPARE(item->height(), 40.0);

    item = findItem<QQuickItem>(contentItem, "wrapper", 7);
    QTRY_VERIFY(item);
    QTRY_COMPARE(item->height(), 20.0);

    QTRY_COMPARE(listview->currentSection(), QString("0"));

    listview->setContentY(140);
    QTRY_COMPARE(listview->currentSection(), QString("1"));

    QTRY_COMPARE(currentSectionChangedSpy.count(), 1);

    listview->setContentY(20);
    QTRY_COMPARE(listview->currentSection(), QString("0"));

    QTRY_COMPARE(currentSectionChangedSpy.count(), 2);

    item = findItem<QQuickItem>(contentItem, "wrapper", 1);
    QTRY_VERIFY(item);
    QTRY_COMPARE(item->height(), 20.0);

    // check that headers change when item changes
    listview->setContentY(0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    model.modifyItem(0, "changed", "2");
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    item = findItem<QQuickItem>(contentItem, "wrapper", 1);
    QTRY_VERIFY(item);
    QTRY_COMPARE(item->height(), 40.0);
}

void tst_QQuickListView::sectionsDelegate()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), QString::number(i/5));

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("listview-sections_delegate.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20 + ((i+5)/5) * 20));
        QQuickText *next = findItem<QQuickText>(item, "nextSection");
        QCOMPARE(next->text().toInt(), (i+1)/5);
    }

    for (int i = 0; i < 3; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "sect_" + QString::number(i));
        QVERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20*6));
    }

    // change section
    model.modifyItem(0, "One", "aaa");
    model.modifyItem(1, "Two", "aaa");
    model.modifyItem(2, "Three", "aaa");
    model.modifyItem(3, "Four", "aaa");
    model.modifyItem(4, "Five", "aaa");
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    for (int i = 0; i < 3; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem,
                "sect_" + (i == 0 ? QString("aaa") : QString::number(i)));
        QVERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20*6));
    }

    // remove section boundary
    model.removeItem(5);
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), model.count());
    for (int i = 0; i < 3; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem,
                "sect_" + (i == 0 ? QString("aaa") : QString::number(i)));
        QVERIFY(item);
    }

    // QTBUG-17606
    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "sect_1");
    QCOMPARE(items.count(), 1);

    // QTBUG-17759
    model.modifyItem(0, "One", "aaa");
    model.modifyItem(1, "One", "aaa");
    model.modifyItem(2, "One", "aaa");
    model.modifyItem(3, "Four", "aaa");
    model.modifyItem(4, "Four", "aaa");
    model.modifyItem(5, "Four", "aaa");
    model.modifyItem(6, "Five", "aaa");
    model.modifyItem(7, "Five", "aaa");
    model.modifyItem(8, "Five", "aaa");
    model.modifyItem(9, "Two", "aaa");
    model.modifyItem(10, "Two", "aaa");
    model.modifyItem(11, "Two", "aaa");
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "sect_aaa").count(), 1);
    window->rootObject()->setProperty("sectionProperty", "name");
    // ensure view has settled.
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "sect_Four").count(), 1);
    for (int i = 0; i < 4; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem,
                "sect_" + model.name(i*3));
        QVERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20*4));
    }
}

void tst_QQuickListView::sectionsDragOutsideBounds_data()
{
    QTest::addColumn<int>("distance");
    QTest::addColumn<int>("cacheBuffer");

    QTest::newRow("500, no cache buffer") << 500 << 0;
    QTest::newRow("1000, no cache buffer") << 1000 << 0;
    QTest::newRow("500, cache buffer") << 500 << 320;
    QTest::newRow("1000, cache buffer") << 1000 << 320;
}

void tst_QQuickListView::sectionsDragOutsideBounds()
{
    QFETCH(int, distance);
    QFETCH(int, cacheBuffer);

    QQuickView *window = getView();
    QQuickViewTestUtil::moveMouseAway(window);

    QaimModel model;
    for (int i = 0; i < 10; i++)
        model.addItem("Item" + QString::number(i), QString::number(i/5));

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("listview-sections_delegate.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    listview->setCacheBuffer(cacheBuffer);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // QTBUG-17769
    // Drag view up beyond bounds
    QTest::mousePress(window, Qt::LeftButton, 0, QPoint(20,20));
    QTest::mouseMove(window, QPoint(20,0));
    QTest::mouseMove(window, QPoint(20,-50));
    QTest::mouseMove(window, QPoint(20,-distance));
    QTest::mouseRelease(window, Qt::LeftButton, 0, QPoint(20,-distance));
    // view should settle back at 0
    QTRY_COMPARE(listview->contentY(), 0.0);

    QTest::mousePress(window, Qt::LeftButton, 0, QPoint(20,0));
    QTest::mouseMove(window, QPoint(20,20));
    QTest::mouseMove(window, QPoint(20,70));
    QTest::mouseMove(window, QPoint(20,distance));

    QTest::mouseRelease(window, Qt::LeftButton, 0, QPoint(20,distance));
    // view should settle back at 0
    QTRY_COMPARE(listview->contentY(), 0.0);

    releaseView(window);
}

void tst_QQuickListView::sectionsDelegate_headerVisibility()
{
    QSKIP("QTBUG-24395");

    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), QString::number(i/5));

    window->rootContext()->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("listview-sections_delegate.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    window->requestActivate();
    QTest::qWaitForWindowActive(window.data());

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // ensure section header is maintained in view
    listview->setCurrentIndex(20);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    QTRY_VERIFY(qFuzzyCompare(listview->contentY(), 200.0));
    QTRY_VERIFY(!listview->isMoving());
    listview->setCurrentIndex(0);
    QTRY_VERIFY(qFuzzyIsNull(listview->contentY()));
}

void tst_QQuickListView::sectionsPositioning()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), QString::number(i/5));

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("listview-sections_delegate.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
    window->rootObject()->setProperty("sectionPositioning", QVariant(int(QQuickViewSection::InlineLabels | QQuickViewSection::CurrentLabelAtStart | QQuickViewSection::NextLabelAtEnd)));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    for (int i = 0; i < 3; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "sect_" + QString::number(i));
        QVERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20*6));
    }

    QQuickItem *topItem = findVisibleChild(contentItem, "sect_0"); // section header
    QVERIFY(topItem);
    QCOMPARE(topItem->y(), 0.);

    QQuickItem *bottomItem = findVisibleChild(contentItem, "sect_3"); // section footer
    QVERIFY(bottomItem);
    QCOMPARE(bottomItem->y(), 300.);

    // move down a little and check that section header is at top
    listview->setContentY(10);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    QCOMPARE(topItem->y(), 0.);

    // push the top header up
    listview->setContentY(110);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    topItem = findVisibleChild(contentItem, "sect_0"); // section header
    QVERIFY(topItem);
    QCOMPARE(topItem->y(), 100.);

    QQuickItem *item = findVisibleChild(contentItem, "sect_1");
    QVERIFY(item);
    QCOMPARE(item->y(), 120.);

    bottomItem = findVisibleChild(contentItem, "sect_4"); // section footer
    QVERIFY(bottomItem);
    QCOMPARE(bottomItem->y(), 410.);

    // Move past section 0
    listview->setContentY(120);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    topItem = findVisibleChild(contentItem, "sect_0"); // section header
    QVERIFY(!topItem);

    // Push section footer down
    listview->setContentY(70);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    bottomItem = findVisibleChild(contentItem, "sect_4"); // section footer
    QVERIFY(bottomItem);
    QCOMPARE(bottomItem->y(), 380.);

    // Change current section, and verify case insensitive comparison
    listview->setContentY(10);
    model.modifyItem(0, "One", "aaa");
    model.modifyItem(1, "Two", "AAA");
    model.modifyItem(2, "Three", "aAa");
    model.modifyItem(3, "Four", "aaA");
    model.modifyItem(4, "Five", "Aaa");
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QTRY_COMPARE(listview->currentSection(), QString("aaa"));

    for (int i = 0; i < 3; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem,
                "sect_" + (i == 0 ? QString("aaa") : QString::number(i)));
        QVERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20*6));
    }

    QTRY_VERIFY(topItem = findVisibleChild(contentItem, "sect_aaa")); // section header
    QCOMPARE(topItem->y(), 10.);

    // remove section boundary
    listview->setContentY(120);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    model.removeItem(5);
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), model.count());
    for (int i = 1; i < 3; ++i) {
        QQuickItem *item = findVisibleChild(contentItem,
                "sect_" + QString::number(i));
        QVERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20*6));
    }

    QVERIFY(topItem = findVisibleChild(contentItem, "sect_1"));
    QTRY_COMPARE(topItem->y(), 120.);

    // Change the next section
    listview->setContentY(0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    bottomItem = findVisibleChild(contentItem, "sect_3"); // section footer
    QVERIFY(bottomItem);
    QTRY_COMPARE(bottomItem->y(), 300.);

    model.modifyItem(14, "New", "new");
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QTRY_VERIFY(bottomItem = findVisibleChild(contentItem, "sect_new")); // section footer
    QTRY_COMPARE(bottomItem->y(), 300.);

    // delegate size increase should push section footer down
    listview->setContentY(70);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    QTRY_VERIFY(bottomItem = findVisibleChild(contentItem, "sect_3")); // section footer
    QTRY_COMPARE(bottomItem->y(), 370.);
    QQuickItem *inlineSection = findVisibleChild(contentItem, "sect_new");
    item = findItem<QQuickItem>(contentItem, "wrapper", 13);
    QVERIFY(item);
    item->setHeight(40.);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    QTRY_COMPARE(bottomItem->y(), 380.);
    QCOMPARE(inlineSection->y(), 360.);
    item->setHeight(20.);

    // Turn sticky footer off
    listview->setContentY(20);
    window->rootObject()->setProperty("sectionPositioning", QVariant(int(QQuickViewSection::InlineLabels | QQuickViewSection::CurrentLabelAtStart)));
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    QTRY_VERIFY(item = findVisibleChild(contentItem, "sect_new")); // inline label restored
    QCOMPARE(item->y(), 340.);

    // Turn sticky header off
    listview->setContentY(30);
    window->rootObject()->setProperty("sectionPositioning", QVariant(int(QQuickViewSection::InlineLabels)));
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    QTRY_VERIFY(item = findVisibleChild(contentItem, "sect_aaa")); // inline label restored
    QCOMPARE(item->y(), 0.);

    // if an empty model is set the header/footer should be cleaned up
    window->rootObject()->setProperty("sectionPositioning", QVariant(int(QQuickViewSection::InlineLabels | QQuickViewSection::CurrentLabelAtStart | QQuickViewSection::NextLabelAtEnd)));
    QTRY_VERIFY(findVisibleChild(contentItem, "sect_aaa")); // section header
    QTRY_VERIFY(findVisibleChild(contentItem, "sect_new")); // section footer
    QaimModel model1;
    ctxt->setContextProperty("testModel", &model1);
    QTRY_VERIFY(!findVisibleChild(contentItem, "sect_aaa")); // section header
    QTRY_VERIFY(!findVisibleChild(contentItem, "sect_new")); // section footer

    // clear model - header/footer should be cleaned up
    ctxt->setContextProperty("testModel", &model);
    QTRY_VERIFY(findVisibleChild(contentItem, "sect_aaa")); // section header
    QTRY_VERIFY(findVisibleChild(contentItem, "sect_new")); // section footer
    model.clear();
    QTRY_VERIFY(!findVisibleChild(contentItem, "sect_aaa")); // section header
    QTRY_VERIFY(!findVisibleChild(contentItem, "sect_new")); // section footer
}

void tst_QQuickListView::sectionPropertyChange()
{
    QScopedPointer<QQuickView> window(createView());

    window->setSource(testFileUrl("sectionpropertychange.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    for (int i = 0; i < 2; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(25. + i*75.));
    }

    QMetaObject::invokeMethod(window->rootObject(), "switchGroups");
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    for (int i = 0; i < 2; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(25. + i*75.));
    }

    QMetaObject::invokeMethod(window->rootObject(), "switchGroups");
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    for (int i = 0; i < 2; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(25. + i*75.));
    }

    QMetaObject::invokeMethod(window->rootObject(), "switchGrouped");
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    for (int i = 0; i < 2; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(25. + i*50.));
    }

    QMetaObject::invokeMethod(window->rootObject(), "switchGrouped");
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    for (int i = 0; i < 2; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(25. + i*75.));
    }
}

void tst_QQuickListView::sectionDelegateChange()
{
    QScopedPointer<QQuickView> window(createView());

    window->setSource(testFileUrl("sectiondelegatechange.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = qobject_cast<QQuickListView *>(window->rootObject());
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QQUICK_VERIFY_POLISH(listview);

    QVERIFY(findItems<QQuickItem>(contentItem, "section1").count() > 0);
    QCOMPARE(findItems<QQuickItem>(contentItem, "section2").count(), 0);

    for (int i = 0; i < 3; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "item", i);
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(25. + i*50.));
    }

    QMetaObject::invokeMethod(window->rootObject(), "switchDelegates");
    QQUICK_VERIFY_POLISH(listview);

    QCOMPARE(findItems<QQuickItem>(contentItem, "section1").count(), 0);
    QVERIFY(findItems<QQuickItem>(contentItem, "section2").count() > 0);

    for (int i = 0; i < 3; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "item", i);
        QVERIFY(item);
        QTRY_COMPARE(item->y(), qreal(50. + i*75.));
    }
}

// QTBUG-43873
void tst_QQuickListView::sectionsItemInsertion()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), QString::number(i/5));

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("listview-sections_delegate.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    for (int i = 0; i < 3; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "sect_" + QString::number(i));
        QVERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20*6));
    }

    QQuickItem *topItem = findVisibleChild(contentItem, "sect_0"); // section header
    QVERIFY(topItem);
    QCOMPARE(topItem->y(), 0.);

    // Insert a full screen of items at the beginning.
    for (int i = 0; i < 10; i++)
        model.insertItem(i, "Item" + QString::number(i), QLatin1String("A"));

    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    QVERIFY(itemCount > 10);

    // Verify that the new items are postioned correctly, and have the correct attached section properties
    for (int i = 0; i < 10 && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY(item);
        QTRY_COMPARE(item->y(), 20+i*20.0);
        QCOMPARE(item->property("section").toString(), QLatin1String("A"));
        QCOMPARE(item->property("nextSection").toString(), i < 9 ? QLatin1String("A") : QLatin1String("0"));
        QCOMPARE(item->property("prevSection").toString(), i > 0 ? QLatin1String("A") : QLatin1String(""));
    }
    // Verify that the exiting items are postioned correctly, and have the correct attached section properties
    for (int i = 10; i < 15 && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY(item);
        QTRY_COMPARE(item->y(), 40+i*20.0);
        QCOMPARE(item->property("section").toString(), QLatin1String("0"));
        QCOMPARE(item->property("nextSection").toString(), i < 14 ? QLatin1String("0") : QLatin1String("1"));
        QCOMPARE(item->property("prevSection").toString(), i > 10 ? QLatin1String("0") : QLatin1String("A"));
    }
}

void tst_QQuickListView::currentIndex_delayedItemCreation()
{
    QFETCH(bool, setCurrentToZero);

    QQuickView *window = getView();

    // test currentIndexChanged() is emitted even if currentIndex = 0 on start up
    // (since the currentItem will have changed and that shares the same index)
    window->rootContext()->setContextProperty("setCurrentToZero", setCurrentToZero);

    window->setSource(testFileUrl("fillModelOnComponentCompleted.qml"));
    qApp->processEvents();

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QSignalSpy spy(listview, SIGNAL(currentItemChanged()));
    //QCOMPARE(listview->currentIndex(), 0);
    listview->forceLayout();
    QTRY_COMPARE(spy.count(), 1);

    releaseView(window);
}

void tst_QQuickListView::currentIndex_delayedItemCreation_data()
{
    QTest::addColumn<bool>("setCurrentToZero");

    QTest::newRow("set to 0") << true;
    QTest::newRow("don't set to 0") << false;
}

void tst_QQuickListView::currentIndex()
{
    QaimModel initModel;

    for (int i = 0; i < 30; i++)
        initModel.addItem("Item" + QString::number(i), QString::number(i));

    QQuickView *window = new QQuickView(0);
    window->setGeometry(0,0,240,320);

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &initModel);
    ctxt->setContextProperty("testWrap", QVariant(false));

    QString filename(testFile("listview-initCurrent.qml"));
    window->setSource(QUrl::fromLocalFile(filename));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // currentIndex is initialized to 20
    // currentItem should be in view
    QCOMPARE(listview->currentIndex(), 20);
    QCOMPARE(listview->contentY(), 100.0);
    QCOMPARE(listview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 20));
    QCOMPARE(listview->highlightItem()->y(), listview->currentItem()->y());

    // changing model should reset currentIndex to 0
    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), QString::number(i));
    ctxt->setContextProperty("testModel", &model);
    listview->forceLayout();

    QCOMPARE(listview->currentIndex(), 0);
    QCOMPARE(listview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 0));

    // confirm that the velocity is updated
    listview->setCurrentIndex(20);
    QTRY_VERIFY(listview->verticalVelocity() != 0.0);
    listview->setCurrentIndex(0);
    QTRY_COMPARE(listview->verticalVelocity(), 0.0);

    // footer should become visible if it is out of view, and then current index is set to count-1
    window->rootObject()->setProperty("showFooter", true);
    QTRY_VERIFY(listview->footerItem());
    listview->setCurrentIndex(model.count()-2);
    QTRY_VERIFY(listview->footerItem()->y() > listview->contentY() + listview->height());
    listview->setCurrentIndex(model.count()-1);
    QTRY_COMPARE(listview->contentY() + listview->height(), (20.0 * model.count()) + listview->footerItem()->height());
    window->rootObject()->setProperty("showFooter", false);

    // header should become visible if it is out of view, and then current index is set to 0
    window->rootObject()->setProperty("showHeader", true);
    QTRY_VERIFY(listview->headerItem());
    listview->setCurrentIndex(1);
    QTRY_VERIFY(listview->headerItem()->y() + listview->headerItem()->height() < listview->contentY());
    listview->setCurrentIndex(0);
    QTRY_COMPARE(listview->contentY(), -listview->headerItem()->height());
    window->rootObject()->setProperty("showHeader", false);

    // turn off auto highlight
    listview->setHighlightFollowsCurrentItem(false);
    QVERIFY(!listview->highlightFollowsCurrentItem());

    QVERIFY(listview->highlightItem());
    qreal hlPos = listview->highlightItem()->y();

    listview->setCurrentIndex(4);
    QTRY_COMPARE(listview->highlightItem()->y(), hlPos);

    // insert item before currentIndex
    window->rootObject()->setProperty("currentItemChangedCount", QVariant(0));
    listview->setCurrentIndex(28);
    QTRY_COMPARE(window->rootObject()->property("currentItemChangedCount").toInt(), 1);
    model.insertItem(0, "Foo", "1111");
    QTRY_COMPARE(window->rootObject()->property("current").toInt(), 29);
    QCOMPARE(window->rootObject()->property("currentItemChangedCount").toInt(), 1);

    // check removing highlight by setting currentIndex to -1;
    listview->setCurrentIndex(-1);

    QCOMPARE(listview->currentIndex(), -1);
    QVERIFY(!listview->highlightItem());
    QVERIFY(!listview->currentItem());

    // moving currentItem out of view should make it invisible
    listview->setCurrentIndex(0);
    QTRY_VERIFY(delegateVisible(listview->currentItem()));
    listview->setContentY(200);
    QTRY_VERIFY(!delegateVisible(listview->currentItem()));

    // empty model should reset currentIndex to -1
    QaimModel emptyModel;
    ctxt->setContextProperty("testModel", &emptyModel);
    QCOMPARE(listview->currentIndex(), -1);

    delete window;
}

void tst_QQuickListView::noCurrentIndex()
{
    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), QString::number(i));

    QQuickView *window = new QQuickView(0);
    window->setGeometry(0,0,240,320);

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    QString filename(testFile("listview-noCurrent.qml"));
    window->setSource(QUrl::fromLocalFile(filename));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // current index should be -1 at startup
    // and we should not have a currentItem or highlightItem
    QCOMPARE(listview->currentIndex(), -1);
    QCOMPARE(listview->contentY(), 0.0);
    QVERIFY(!listview->highlightItem());
    QVERIFY(!listview->currentItem());

    listview->setCurrentIndex(2);
    QCOMPARE(listview->currentIndex(), 2);
    QVERIFY(listview->highlightItem());
    QVERIFY(listview->currentItem());

    delete window;
}

void tst_QQuickListView::keyNavigation()
{
    QFETCH(QQuickListView::Orientation, orientation);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(Qt::Key, forwardsKey);
    QFETCH(Qt::Key, backwardsKey);
    QFETCH(QPointF, contentPosAtFirstItem);
    QFETCH(QPointF, contentPosAtLastItem);

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQuickView *window = getView();
    TestObject *testObject = new TestObject;
    window->rootContext()->setContextProperty("testModel", &model);
    window->rootContext()->setContextProperty("testObject", testObject);
    window->setSource(testFileUrl("listviewtest.qml"));
    window->show();
    QTest::qWaitForWindowActive(window);

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    listview->setOrientation(orientation);
    listview->setLayoutDirection(layoutDirection);
    listview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    window->requestActivate();
    QTest::qWaitForWindowActive(window);
    QTRY_COMPARE(qGuiApp->focusWindow(), window);

    QTest::keyClick(window, forwardsKey);
    QCOMPARE(listview->currentIndex(), 1);

    QTest::keyClick(window, backwardsKey);
    QCOMPARE(listview->currentIndex(), 0);

    // hold down a key to go forwards
    for (int i=0; i<model.count()-1; i++) {
        QTest::simulateEvent(window, true, forwardsKey, Qt::NoModifier, "", true);
        QCOMPARE(listview->currentIndex(), i+1);
    }
    QTest::keyRelease(window, forwardsKey);
    listview->forceLayout();
    QTRY_COMPARE(listview->currentIndex(), model.count()-1);
    QTRY_COMPARE(listview->contentX(), contentPosAtLastItem.x());
    QTRY_COMPARE(listview->contentY(), contentPosAtLastItem.y());

    // hold down a key to go backwards
    for (int i=model.count()-1; i > 0; i--) {
        QTest::simulateEvent(window, true, backwardsKey, Qt::NoModifier, "", true);
        QTRY_COMPARE(listview->currentIndex(), i-1);
    }
    QTest::keyRelease(window, backwardsKey);
    listview->forceLayout();
    QTRY_COMPARE(listview->currentIndex(), 0);
    QTRY_COMPARE(listview->contentX(), contentPosAtFirstItem.x());
    QTRY_COMPARE(listview->contentY(), contentPosAtFirstItem.y());

    // no wrap
    QVERIFY(!listview->isWrapEnabled());
    listview->incrementCurrentIndex();
    QCOMPARE(listview->currentIndex(), 1);
    listview->decrementCurrentIndex();
    QCOMPARE(listview->currentIndex(), 0);

    listview->decrementCurrentIndex();
    QCOMPARE(listview->currentIndex(), 0);

    // with wrap
    listview->setWrapEnabled(true);
    QVERIFY(listview->isWrapEnabled());

    listview->decrementCurrentIndex();
    QCOMPARE(listview->currentIndex(), model.count()-1);
    QTRY_COMPARE(listview->contentX(), contentPosAtLastItem.x());
    QTRY_COMPARE(listview->contentY(), contentPosAtLastItem.y());

    listview->incrementCurrentIndex();
    QCOMPARE(listview->currentIndex(), 0);
    QTRY_COMPARE(listview->contentX(), contentPosAtFirstItem.x());
    QTRY_COMPARE(listview->contentY(), contentPosAtFirstItem.y());

    releaseView(window);
    delete testObject;
}

void tst_QQuickListView::keyNavigation_data()
{
    QTest::addColumn<QQuickListView::Orientation>("orientation");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickItemView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<Qt::Key>("forwardsKey");
    QTest::addColumn<Qt::Key>("backwardsKey");
    QTest::addColumn<QPointF>("contentPosAtFirstItem");
    QTest::addColumn<QPointF>("contentPosAtLastItem");

    QTest::newRow("Vertical, TopToBottom")
            << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::TopToBottom
            << Qt::Key_Down << Qt::Key_Up
            << QPointF(0, 0)
            << QPointF(0, 30*20 - 320);

    QTest::newRow("Vertical, BottomToTop")
            << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::BottomToTop
            << Qt::Key_Up << Qt::Key_Down
            << QPointF(0, -320)
            << QPointF(0, -(30 * 20));

    QTest::newRow("Horizontal, LeftToRight")
            << QQuickListView::Horizontal << Qt::LeftToRight << QQuickItemView::TopToBottom
            << Qt::Key_Right << Qt::Key_Left
            << QPointF(0, 0)
            << QPointF(30*240 - 240, 0);

    QTest::newRow("Horizontal, RightToLeft")
            << QQuickListView::Horizontal << Qt::RightToLeft << QQuickItemView::TopToBottom
            << Qt::Key_Left << Qt::Key_Right
            << QPointF(-240, 0)
            << QPointF(-(30 * 240), 0);
}

void tst_QQuickListView::itemList()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("itemlist.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "view");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQmlObjectModel *model = window->rootObject()->findChild<QQmlObjectModel*>("itemModel");
    QTRY_VERIFY(model != 0);

    QTRY_COMPARE(model->count(), 3);
    QTRY_COMPARE(listview->currentIndex(), 0);

    QQuickItem *item = findItem<QQuickItem>(contentItem, "item1");
    QTRY_VERIFY(item);
    QTRY_COMPARE(item->x(), 0.0);
    QCOMPARE(item->height(), listview->height());

    QQuickText *text = findItem<QQuickText>(contentItem, "text1");
    QTRY_VERIFY(text);
    QTRY_COMPARE(text->text(), QLatin1String("index: 0"));

    listview->setCurrentIndex(2);

    item = findItem<QQuickItem>(contentItem, "item3");
    QTRY_VERIFY(item);
    QTRY_COMPARE(item->x(), 480.0);

    text = findItem<QQuickText>(contentItem, "text3");
    QTRY_VERIFY(text);
    QTRY_COMPARE(text->text(), QLatin1String("index: 2"));
}

void tst_QQuickListView::itemListFlicker()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("itemlist-flicker.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "view");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQmlObjectModel *model = window->rootObject()->findChild<QQmlObjectModel*>("itemModel");
    QTRY_VERIFY(model != 0);

    QTRY_COMPARE(model->count(), 3);
    QTRY_COMPARE(listview->currentIndex(), 0);

    QQuickItem *item;

    QVERIFY(item = findItem<QQuickItem>(contentItem, "item1"));
    QVERIFY(delegateVisible(item));
    QVERIFY(item = findItem<QQuickItem>(contentItem, "item2"));
    QVERIFY(delegateVisible(item));
    QVERIFY(item = findItem<QQuickItem>(contentItem, "item3"));
    QVERIFY(delegateVisible(item));

    listview->setCurrentIndex(1);

    QVERIFY(item = findItem<QQuickItem>(contentItem, "item1"));
    QVERIFY(delegateVisible(item));
    QVERIFY(item = findItem<QQuickItem>(contentItem, "item2"));
    QVERIFY(delegateVisible(item));
    QVERIFY(item = findItem<QQuickItem>(contentItem, "item3"));
    QVERIFY(delegateVisible(item));

    listview->setCurrentIndex(2);

    QVERIFY(item = findItem<QQuickItem>(contentItem, "item1"));
    QVERIFY(delegateVisible(item));
    QVERIFY(item = findItem<QQuickItem>(contentItem, "item2"));
    QVERIFY(delegateVisible(item));
    QVERIFY(item = findItem<QQuickItem>(contentItem, "item3"));
    QVERIFY(delegateVisible(item));
}

void tst_QQuickListView::cacheBuffer()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 90; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("listviewtest.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_VERIFY(listview->delegate() != 0);
    QTRY_VERIFY(listview->model() != 0);
    QTRY_VERIFY(listview->highlight() != 0);

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20));
    }

    QQmlIncubationController controller;
    window->engine()->setIncubationController(&controller);

    testObject->setCacheBuffer(200);
    QTRY_COMPARE(listview->cacheBuffer(), 200);

    // items will be created one at a time
    for (int i = itemCount; i < qMin(itemCount+10,model.count()); ++i) {
        QVERIFY(findItem<QQuickItem>(listview, "wrapper", i) == 0);
        QQuickItem *item = 0;
        while (!item) {
            bool b = false;
            controller.incubateWhile(&b);
            item = findItem<QQuickItem>(listview, "wrapper", i);
        }
    }

    {
        bool b = true;
        controller.incubateWhile(&b);
    }

    int newItemCount = 0;
    newItemCount = findItems<QQuickItem>(contentItem, "wrapper").count();

    // Confirm items positioned correctly
    for (int i = 0; i < model.count() && i < newItemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20));
    }

    // move view and confirm items in view are visible immediately and outside are created async
    listview->setContentY(300);

    for (int i = 15; i < 32; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QVERIFY(item);
        QCOMPARE(item->y(), qreal(i*20));
    }

    QVERIFY(findItem<QQuickItem>(listview, "wrapper", 32) == 0);

    // ensure buffered items are created
    for (int i = 32; i < qMin(41,model.count()); ++i) {
        QQuickItem *item = 0;
        while (!item) {
            qGuiApp->processEvents(); // allow refill to happen
            bool b = false;
            controller.incubateWhile(&b);
            item = findItem<QQuickItem>(listview, "wrapper", i);
        }
    }

    {
        bool b = true;
        controller.incubateWhile(&b);
    }

    // negative cache buffer is ignored
    listview->setCacheBuffer(-1);
    QCOMPARE(listview->cacheBuffer(), 200);

    delete testObject;
}

void tst_QQuickListView::positionViewAtBeginningEnd()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 40; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);
    window->show();
    window->setSource(testFileUrl("listviewtest.qml"));
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    listview->setContentY(100);

    // positionAtBeginnging
    listview->positionViewAtBeginning();
    QTRY_COMPARE(listview->contentY(), 0.);

    listview->setContentY(80);
    window->rootObject()->setProperty("showHeader", true);
    listview->positionViewAtBeginning();
    QTRY_COMPARE(listview->contentY(), -30.);

    // positionAtEnd
    listview->positionViewAtEnd();
    QTRY_COMPARE(listview->contentY(), 480.); // 40*20 - 320

    listview->setContentY(80);
    window->rootObject()->setProperty("showFooter", true);
    listview->positionViewAtEnd();
    QTRY_COMPARE(listview->contentY(), 510.);

    // set current item to outside visible view, position at beginning
    // and ensure highlight moves to current item
    listview->setCurrentIndex(1);
    listview->positionViewAtBeginning();
    QTRY_COMPARE(listview->contentY(), -30.);
    QVERIFY(listview->highlightItem());
    QCOMPARE(listview->highlightItem()->y(), 20.);

    delete testObject;
}

void tst_QQuickListView::positionViewAtIndex()
{
    QFETCH(bool, enforceRange);
    QFETCH(qreal, initContentY);
    QFETCH(int, index);
    QFETCH(QQuickListView::PositionMode, mode);
    QFETCH(qreal, contentY);

    QQuickView *window = getView();

    QaimModel model;
    for (int i = 0; i < 40; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);
    window->show();
    window->setSource(testFileUrl("listviewtest.qml"));
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    window->rootObject()->setProperty("enforceRange", enforceRange);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    listview->setContentY(initContentY);

    listview->positionViewAtIndex(index, mode);
    QTRY_COMPARE(listview->contentY(), contentY);

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = index; i < model.count() && i < itemCount-index-1; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), i*20.);
    }

    releaseView(window);
}

void tst_QQuickListView::positionViewAtIndex_data()
{
    QTest::addColumn<bool>("enforceRange");
    QTest::addColumn<qreal>("initContentY");
    QTest::addColumn<int>("index");
    QTest::addColumn<QQuickListView::PositionMode>("mode");
    QTest::addColumn<qreal>("contentY");

    QTest::newRow("no range, 3 at Beginning") << false << 0. << 3 << QQuickListView::Beginning << 60.;
    QTest::newRow("no range, 3 at End") << false << 0. << 3 << QQuickListView::End << 0.;
    QTest::newRow("no range, 22 at Beginning") << false << 0. << 22 << QQuickListView::Beginning << 440.;
    // Position on an item that would leave empty space if positioned at the top
    QTest::newRow("no range, 28 at Beginning") << false << 0. << 28 << QQuickListView::Beginning << 480.;
    // Position at End using last index
    QTest::newRow("no range, last at End") << false << 0. << 39 << QQuickListView::End << 480.;
    // Position at End
    QTest::newRow("no range, 20 at End") << false << 0. << 20 << QQuickListView::End << 100.;
    // Position in Center
    QTest::newRow("no range, 15 at Center") << false << 0. << 15 << QQuickListView::Center << 150.;
    // Ensure at least partially visible
    QTest::newRow("no range, 15 visible => Visible") << false << 150. << 15 << QQuickListView::Visible << 150.;
    QTest::newRow("no range, 15 partially visible => Visible") << false << 302. << 15 << QQuickListView::Visible << 302.;
    QTest::newRow("no range, 15 before visible => Visible") << false << 320. << 15 << QQuickListView::Visible << 300.;
    QTest::newRow("no range, 20 visible => Visible") << false << 85. << 20 << QQuickListView::Visible << 85.;
    QTest::newRow("no range, 20 before visible => Visible") << false << 75. << 20 << QQuickListView::Visible << 100.;
    QTest::newRow("no range, 20 after visible => Visible") << false << 480. << 20 << QQuickListView::Visible << 400.;
    // Ensure completely visible
    QTest::newRow("no range, 20 visible => Contain") << false << 120. << 20 << QQuickListView::Contain << 120.;
    QTest::newRow("no range, 15 partially visible => Contain") << false << 302. << 15 << QQuickListView::Contain << 300.;
    QTest::newRow("no range, 20 partially visible => Contain") << false << 85. << 20 << QQuickListView::Contain << 100.;

    QTest::newRow("strict range, 3 at End") << true << 0. << 3 << QQuickListView::End << -120.;
    QTest::newRow("strict range, 38 at Beginning") << true << 0. << 38 << QQuickListView::Beginning << 660.;
    QTest::newRow("strict range, 15 at Center") << true << 0. << 15 << QQuickListView::Center << 140.;
    QTest::newRow("strict range, 3 at SnapPosition") << true << 0. << 3 << QQuickListView::SnapPosition << -60.;
    QTest::newRow("strict range, 10 at SnapPosition") << true << 0. << 10 << QQuickListView::SnapPosition << 80.;
    QTest::newRow("strict range, 38 at SnapPosition") << true << 0. << 38 << QQuickListView::SnapPosition << 640.;
}

void tst_QQuickListView::resetModel()
{
    QScopedPointer<QQuickView> window(createView());

    QStringList strings;
    strings << "one" << "two" << "three";
    QStringListModel model(strings);

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("displaylist.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QTRY_COMPARE(listview->count(), model.rowCount());

    for (int i = 0; i < model.rowCount(); ++i) {
        QQuickText *display = findItem<QQuickText>(contentItem, "displayText", i);
        QTRY_VERIFY(display != 0);
        QTRY_COMPARE(display->text(), strings.at(i));
    }

    strings.clear();
    strings << "four" << "five" << "six" << "seven";
    model.setStringList(strings);

    listview->forceLayout();
    QTRY_COMPARE(listview->count(), model.rowCount());

    for (int i = 0; i < model.rowCount(); ++i) {
        QQuickText *display = findItem<QQuickText>(contentItem, "displayText", i);
        QTRY_VERIFY(display != 0);
        QTRY_COMPARE(display->text(), strings.at(i));
    }
}

void tst_QQuickListView::propertyChanges()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("propertychangestest.qml"));

    QQuickListView *listView = window->rootObject()->findChild<QQuickListView*>("listView");
    QTRY_VERIFY(listView);

    QSignalSpy highlightFollowsCurrentItemSpy(listView, SIGNAL(highlightFollowsCurrentItemChanged()));
    QSignalSpy preferredHighlightBeginSpy(listView, SIGNAL(preferredHighlightBeginChanged()));
    QSignalSpy preferredHighlightEndSpy(listView, SIGNAL(preferredHighlightEndChanged()));
    QSignalSpy highlightRangeModeSpy(listView, SIGNAL(highlightRangeModeChanged()));
    QSignalSpy keyNavigationWrapsSpy(listView, SIGNAL(keyNavigationWrapsChanged()));
    QSignalSpy cacheBufferSpy(listView, SIGNAL(cacheBufferChanged()));
    QSignalSpy snapModeSpy(listView, SIGNAL(snapModeChanged()));

    QTRY_COMPARE(listView->highlightFollowsCurrentItem(), true);
    QTRY_COMPARE(listView->preferredHighlightBegin(), 0.0);
    QTRY_COMPARE(listView->preferredHighlightEnd(), 0.0);
    QTRY_COMPARE(listView->highlightRangeMode(), QQuickListView::ApplyRange);
    QTRY_COMPARE(listView->isWrapEnabled(), true);
    QTRY_COMPARE(listView->cacheBuffer(), 10);
    QTRY_COMPARE(listView->snapMode(), QQuickListView::SnapToItem);

    listView->setHighlightFollowsCurrentItem(false);
    listView->setPreferredHighlightBegin(1.0);
    listView->setPreferredHighlightEnd(1.0);
    listView->setHighlightRangeMode(QQuickListView::StrictlyEnforceRange);
    listView->setWrapEnabled(false);
    listView->setCacheBuffer(3);
    listView->setSnapMode(QQuickListView::SnapOneItem);

    QTRY_COMPARE(listView->highlightFollowsCurrentItem(), false);
    QTRY_COMPARE(listView->preferredHighlightBegin(), 1.0);
    QTRY_COMPARE(listView->preferredHighlightEnd(), 1.0);
    QTRY_COMPARE(listView->highlightRangeMode(), QQuickListView::StrictlyEnforceRange);
    QTRY_COMPARE(listView->isWrapEnabled(), false);
    QTRY_COMPARE(listView->cacheBuffer(), 3);
    QTRY_COMPARE(listView->snapMode(), QQuickListView::SnapOneItem);

    QTRY_COMPARE(highlightFollowsCurrentItemSpy.count(),1);
    QTRY_COMPARE(preferredHighlightBeginSpy.count(),1);
    QTRY_COMPARE(preferredHighlightEndSpy.count(),1);
    QTRY_COMPARE(highlightRangeModeSpy.count(),1);
    QTRY_COMPARE(keyNavigationWrapsSpy.count(),1);
    QTRY_COMPARE(cacheBufferSpy.count(),1);
    QTRY_COMPARE(snapModeSpy.count(),1);

    listView->setHighlightFollowsCurrentItem(false);
    listView->setPreferredHighlightBegin(1.0);
    listView->setPreferredHighlightEnd(1.0);
    listView->setHighlightRangeMode(QQuickListView::StrictlyEnforceRange);
    listView->setWrapEnabled(false);
    listView->setCacheBuffer(3);
    listView->setSnapMode(QQuickListView::SnapOneItem);

    QTRY_COMPARE(highlightFollowsCurrentItemSpy.count(),1);
    QTRY_COMPARE(preferredHighlightBeginSpy.count(),1);
    QTRY_COMPARE(preferredHighlightEndSpy.count(),1);
    QTRY_COMPARE(highlightRangeModeSpy.count(),1);
    QTRY_COMPARE(keyNavigationWrapsSpy.count(),1);
    QTRY_COMPARE(cacheBufferSpy.count(),1);
    QTRY_COMPARE(snapModeSpy.count(),1);
}

void tst_QQuickListView::componentChanges()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("propertychangestest.qml"));

    QQuickListView *listView = window->rootObject()->findChild<QQuickListView*>("listView");
    QTRY_VERIFY(listView);

    QQmlComponent component(window->engine());
    component.setData("import QtQuick 2.0; Rectangle { color: \"blue\"; }", QUrl::fromLocalFile(""));

    QQmlComponent delegateComponent(window->engine());
    delegateComponent.setData("import QtQuick 2.0; Text { text: '<b>Name:</b> ' + name }", QUrl::fromLocalFile(""));

    QSignalSpy highlightSpy(listView, SIGNAL(highlightChanged()));
    QSignalSpy delegateSpy(listView, SIGNAL(delegateChanged()));
    QSignalSpy headerSpy(listView, SIGNAL(headerChanged()));
    QSignalSpy footerSpy(listView, SIGNAL(footerChanged()));

    listView->setHighlight(&component);
    listView->setHeader(&component);
    listView->setFooter(&component);
    listView->setDelegate(&delegateComponent);

    QTRY_COMPARE(listView->highlight(), &component);
    QTRY_COMPARE(listView->header(), &component);
    QTRY_COMPARE(listView->footer(), &component);
    QTRY_COMPARE(listView->delegate(), &delegateComponent);

    QTRY_COMPARE(highlightSpy.count(),1);
    QTRY_COMPARE(delegateSpy.count(),1);
    QTRY_COMPARE(headerSpy.count(),1);
    QTRY_COMPARE(footerSpy.count(),1);

    listView->setHighlight(&component);
    listView->setHeader(&component);
    listView->setFooter(&component);
    listView->setDelegate(&delegateComponent);

    QTRY_COMPARE(highlightSpy.count(),1);
    QTRY_COMPARE(delegateSpy.count(),1);
    QTRY_COMPARE(headerSpy.count(),1);
    QTRY_COMPARE(footerSpy.count(),1);
}

void tst_QQuickListView::modelChanges()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("propertychangestest.qml"));

    QQuickListView *listView = window->rootObject()->findChild<QQuickListView*>("listView");
    QTRY_VERIFY(listView);

    QQmlListModel *alternateModel = window->rootObject()->findChild<QQmlListModel*>("alternateModel");
    QTRY_VERIFY(alternateModel);
    QVariant modelVariant = QVariant::fromValue<QObject *>(alternateModel);
    QSignalSpy modelSpy(listView, SIGNAL(modelChanged()));

    listView->setModel(modelVariant);
    QTRY_COMPARE(listView->model(), modelVariant);
    QTRY_COMPARE(modelSpy.count(),1);

    listView->setModel(modelVariant);
    QTRY_COMPARE(modelSpy.count(),1);

    listView->setModel(QVariant());
    QTRY_COMPARE(modelSpy.count(),2);
}

void tst_QQuickListView::QTBUG_9791()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("strictlyenforcerange.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = qobject_cast<QQuickListView*>(window->rootObject());
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_VERIFY(listview->delegate() != 0);
    QTRY_VERIFY(listview->model() != 0);

    QMetaObject::invokeMethod(listview, "fillModel");
    qApp->processEvents();

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper", false).count();
    QCOMPARE(itemCount, 3);

    for (int i = 0; i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->x(), i*300.0);
    }

    // check that view is positioned correctly
    QTRY_COMPARE(listview->contentX(), 590.0);
}

void tst_QQuickListView::manualHighlight()
{
    QQuickView *window = new QQuickView(0);
    window->setGeometry(0,0,240,320);

    QString filename(testFile("manual-highlight.qml"));
    window->setSource(QUrl::fromLocalFile(filename));

    qApp->processEvents();

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QTRY_COMPARE(listview->currentIndex(), 0);
    QTRY_COMPARE(listview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 0));
    QTRY_COMPARE(listview->highlightItem()->y() - 5, listview->currentItem()->y());

    listview->setCurrentIndex(2);

    QTRY_COMPARE(listview->currentIndex(), 2);
    QTRY_COMPARE(listview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 2));
    QTRY_COMPARE(listview->highlightItem()->y() - 5, listview->currentItem()->y());

    // QTBUG-15972
    listview->positionViewAtIndex(3, QQuickListView::Contain);

    QTRY_COMPARE(listview->currentIndex(), 2);
    QTRY_COMPARE(listview->currentItem(), findItem<QQuickItem>(contentItem, "wrapper", 2));
    QTRY_COMPARE(listview->highlightItem()->y() - 5, listview->currentItem()->y());

    delete window;
}

void tst_QQuickListView::QTBUG_11105()
{
    QScopedPointer<QQuickView> window(createView());
    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("listviewtest.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*20));
    }

    listview->positionViewAtIndex(20, QQuickListView::Beginning);
    QCOMPARE(listview->contentY(), 280.);

    QaimModel model2;
    for (int i = 0; i < 5; i++)
        model2.addItem("Item" + QString::number(i), "");

    ctxt->setContextProperty("testModel", &model2);

    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    QCOMPARE(itemCount, 5);

    delete testObject;
}

void tst_QQuickListView::initialZValues()
{
    QFETCH(QString, fileName);
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl(fileName));
    qApp->processEvents();

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QVERIFY(listview->currentItem());
    QTRY_COMPARE(listview->currentItem()->z(), listview->property("itemZ").toReal());

    QVERIFY(listview->headerItem());
    QTRY_COMPARE(listview->headerItem()->z(), listview->property("headerZ").toReal());

    QVERIFY(listview->footerItem());
    QTRY_COMPARE(listview->footerItem()->z(), listview->property("footerZ").toReal());

    QVERIFY(listview->highlightItem());
    QTRY_COMPARE(listview->highlightItem()->z(), listview->property("highlightZ").toReal());

    QQuickText *sectionItem = 0;
    QTRY_VERIFY(sectionItem = findItem<QQuickText>(contentItem, "section"));
    QTRY_COMPARE(sectionItem->z(), listview->property("sectionZ").toReal());
}

void tst_QQuickListView::initialZValues_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::newRow("defaults") << "defaultZValues.qml";
    QTest::newRow("constants") << "constantZValues.qml";
    QTest::newRow("bindings") << "boundZValues.qml";
}

void tst_QQuickListView::header()
{
    QFETCH(QQuickListView::Orientation, orientation);
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
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    listview->setOrientation(orientation);
    listview->setLayoutDirection(layoutDirection);
    listview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQuickText *header = 0;
    QTRY_VERIFY(header = findItem<QQuickText>(contentItem, "header"));
    QCOMPARE(header, listview->headerItem());

    QCOMPARE(header->width(), 100.);
    QCOMPARE(header->height(), 30.);
    QCOMPARE(header->position(), initialHeaderPos);
    QCOMPARE(QPointF(listview->contentX(), listview->contentY()), initialContentPos);

    if (orientation == QQuickListView::Vertical)
        QCOMPARE(listview->contentHeight(), model.count() * 30. + header->height());
    else
        QCOMPARE(listview->contentWidth(), model.count() * 240. + header->width());

    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->position(), firstDelegatePos);

    model.clear();
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), model.count());
    QCOMPARE(header->position(), initialHeaderPos); // header should stay where it is
    if (orientation == QQuickListView::Vertical)
        QCOMPARE(listview->contentHeight(), header->height());
    else
        QCOMPARE(listview->contentWidth(), header->width());

    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QSignalSpy headerItemSpy(listview, SIGNAL(headerItemChanged()));
    QMetaObject::invokeMethod(window->rootObject(), "changeHeader");

    QCOMPARE(headerItemSpy.count(), 1);

    header = findItem<QQuickText>(contentItem, "header");
    QVERIFY(!header);
    header = findItem<QQuickText>(contentItem, "header2");
    QVERIFY(header);

    QCOMPARE(header, listview->headerItem());

    QCOMPARE(header->position(), changedHeaderPos);
    QCOMPARE(header->width(), 50.);
    QCOMPARE(header->height(), 20.);
    QTRY_COMPARE(QPointF(listview->contentX(), listview->contentY()), changedContentPos);

    item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->position(), firstDelegatePos);

    listview->positionViewAtBeginning();
    header->setHeight(10);
    header->setWidth(40);
    QTRY_COMPARE(QPointF(listview->contentX(), listview->contentY()), resizeContentPos);

    releaseView(window);


    // QTBUG-21207 header should become visible if view resizes from initial empty size

    window = getView();
    window->rootContext()->setContextProperty("testModel", &model);
    window->rootContext()->setContextProperty("initialViewWidth", 0.0);
    window->rootContext()->setContextProperty("initialViewHeight", 0.0);
    window->setSource(testFileUrl("header.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    listview->setOrientation(orientation);
    listview->setLayoutDirection(layoutDirection);
    listview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    listview->setWidth(240);
    listview->setHeight(320);
    QTRY_COMPARE(listview->headerItem()->position(), initialHeaderPos);
    QCOMPARE(QPointF(listview->contentX(), listview->contentY()), initialContentPos);

    releaseView(window);
}

void tst_QQuickListView::header_data()
{
    QTest::addColumn<QQuickListView::Orientation>("orientation");
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
    // delegates = 240 x 30
    // view width = 240

    // header above items, top left
    QTest::newRow("vertical, left to right") << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::TopToBottom
        << QPointF(0, -30)
        << QPointF(0, -20)
        << QPointF(0, -30)
        << QPointF(0, -20)
        << QPointF(0, 0)
        << QPointF(0, -10);

    // header above items, top right
    QTest::newRow("vertical, layout right to left") << QQuickListView::Vertical << Qt::RightToLeft << QQuickItemView::TopToBottom
        << QPointF(0, -30)
        << QPointF(0, -20)
        << QPointF(0, -30)
        << QPointF(0, -20)
        << QPointF(0, 0)
        << QPointF(0, -10);

    // header to left of items
    QTest::newRow("horizontal, layout left to right") << QQuickListView::Horizontal << Qt::LeftToRight << QQuickItemView::TopToBottom
        << QPointF(-100, 0)
        << QPointF(-50, 0)
        << QPointF(-100, 0)
        << QPointF(-50, 0)
        << QPointF(0, 0)
        << QPointF(-40, 0);

    // header to right of items
    QTest::newRow("horizontal, layout right to left") << QQuickListView::Horizontal << Qt::RightToLeft << QQuickItemView::TopToBottom
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF(-240 + 100, 0)
        << QPointF(-240 + 50, 0)
        << QPointF(-240, 0)
        << QPointF(-240 + 40, 0);

    // header below items
    QTest::newRow("vertical, bottom to top") << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::BottomToTop
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF(0, -320 + 30)
        << QPointF(0, -320 + 20)
        << QPointF(0, -30)
        << QPointF(0, -320 + 10);
}

void tst_QQuickListView::header_delayItemCreation()
{
    QScopedPointer<QQuickView> window(createView());
    QaimModel model;

    window->rootContext()->setContextProperty("setCurrentToZero", QVariant(false));
    window->setSource(testFileUrl("fillModelOnComponentCompleted.qml"));
    qApp->processEvents();

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQuickText *header = findItem<QQuickText>(contentItem, "header");
    QVERIFY(header);
    QCOMPARE(header->y(), -header->height());

    QCOMPARE(listview->contentY(), -header->height());

    model.clear();
    QTRY_COMPARE(header->y(), -header->height());
}

void tst_QQuickListView::headerChangesViewport()
{
    QQuickView *window = getView();
    window->rootContext()->setContextProperty("headerHeight", 20);
    window->rootContext()->setContextProperty("headerWidth", 240);
    window->setSource(testFileUrl("headerchangesviewport.qml"));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQuickText *header = 0;
    QTRY_VERIFY(header = findItem<QQuickText>(contentItem, "header"));
    QCOMPARE(header, listview->headerItem());

    QCOMPARE(header->height(), 20.);
    QCOMPARE(listview->contentHeight(), 20.);

    // change height
    window->rootContext()->setContextProperty("headerHeight", 50);

    // verify that list content height updates also
    QCOMPARE(header->height(), 50.);
    QCOMPARE(listview->contentHeight(), 50.);
}

void tst_QQuickListView::footer()
{
    QFETCH(QQuickListView::Orientation, orientation);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(QPointF, initialFooterPos);
    QFETCH(QPointF, firstDelegatePos);
    QFETCH(QPointF, initialContentPos);
    QFETCH(QPointF, changedFooterPos);
    QFETCH(QPointF, changedContentPos);
    QFETCH(QPointF, resizeContentPos);

    QQuickView *window = getView();

    QaimModel model;
    for (int i = 0; i < 3; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("footer.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    listview->setOrientation(orientation);
    listview->setLayoutDirection(layoutDirection);
    listview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQuickText *footer = findItem<QQuickText>(contentItem, "footer");
    QVERIFY(footer);

    QCOMPARE(footer, listview->footerItem());

    QCOMPARE(footer->position(), initialFooterPos);
    QCOMPARE(footer->width(), 100.);
    QCOMPARE(footer->height(), 30.);
    QCOMPARE(QPointF(listview->contentX(), listview->contentY()), initialContentPos);

    if (orientation == QQuickListView::Vertical)
        QCOMPARE(listview->contentHeight(), model.count() * 20. + footer->height());
    else
        QCOMPARE(listview->contentWidth(), model.count() * 40. + footer->width());

    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->position(), firstDelegatePos);

    // remove one item
    model.removeItem(1);

    if (orientation == QQuickListView::Vertical) {
        QTRY_COMPARE(footer->y(), verticalLayoutDirection == QQuickItemView::TopToBottom ?
                initialFooterPos.y() - 20 : initialFooterPos.y() + 20);  // delegate width = 40
    } else {
        QTRY_COMPARE(footer->x(), layoutDirection == Qt::LeftToRight ?
                initialFooterPos.x() - 40 : initialFooterPos.x() + 40);  // delegate width = 40
    }

    // remove all items
    model.clear();
    if (orientation == QQuickListView::Vertical)
        QTRY_COMPARE(listview->contentHeight(), footer->height());
    else
        QTRY_COMPARE(listview->contentWidth(), footer->width());

    QPointF posWhenNoItems(0, 0);
    if (orientation == QQuickListView::Horizontal && layoutDirection == Qt::RightToLeft)
        posWhenNoItems.setX(-100);
    else if (orientation == QQuickListView::Vertical && verticalLayoutDirection == QQuickItemView::BottomToTop)
        posWhenNoItems.setY(-30);
    QTRY_COMPARE(footer->position(), posWhenNoItems);

    // if header is present, it's at a negative pos, so the footer should not move
    window->rootObject()->setProperty("showHeader", true);
    QTRY_COMPARE(footer->position(), posWhenNoItems);
    window->rootObject()->setProperty("showHeader", false);

    // add 30 items
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QSignalSpy footerItemSpy(listview, SIGNAL(footerItemChanged()));
    QMetaObject::invokeMethod(window->rootObject(), "changeFooter");

    QCOMPARE(footerItemSpy.count(), 1);

    footer = findItem<QQuickText>(contentItem, "footer");
    QVERIFY(!footer);
    footer = findItem<QQuickText>(contentItem, "footer2");
    QVERIFY(footer);

    QCOMPARE(footer, listview->footerItem());

    QCOMPARE(footer->position(), changedFooterPos);
    QCOMPARE(footer->width(), 50.);
    QCOMPARE(footer->height(), 20.);
    QTRY_COMPARE(QPointF(listview->contentX(), listview->contentY()), changedContentPos);

    item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->position(), firstDelegatePos);

    listview->positionViewAtEnd();
    footer->setHeight(10);
    footer->setWidth(40);
    QTRY_COMPARE(QPointF(listview->contentX(), listview->contentY()), resizeContentPos);

    releaseView(window);
}

void tst_QQuickListView::footer_data()
{
    QTest::addColumn<QQuickListView::Orientation>("orientation");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickItemView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<QPointF>("initialFooterPos");
    QTest::addColumn<QPointF>("changedFooterPos");
    QTest::addColumn<QPointF>("initialContentPos");
    QTest::addColumn<QPointF>("changedContentPos");
    QTest::addColumn<QPointF>("firstDelegatePos");
    QTest::addColumn<QPointF>("resizeContentPos");

    // footer1 = 100 x 30
    // footer2 = 50 x 20
    // delegates = 40 x 20
    // view width = 240
    // view height = 320

    // footer below items, bottom left
    QTest::newRow("vertical, layout left to right") << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::TopToBottom
        << QPointF(0, 3 * 20)
        << QPointF(0, 30 * 20)  // added 30 items
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF(0, 30 * 20 - 320 + 10);

    // footer below items, bottom right
    QTest::newRow("vertical, layout right to left") << QQuickListView::Vertical << Qt::RightToLeft << QQuickItemView::TopToBottom
        << QPointF(0, 3 * 20)
        << QPointF(0, 30 * 20)
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF(0, 30 * 20 - 320 + 10);

    // footer to right of items
    QTest::newRow("horizontal, layout left to right") << QQuickListView::Horizontal << Qt::LeftToRight << QQuickItemView::TopToBottom
        << QPointF(40 * 3, 0)
        << QPointF(40 * 30, 0)
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF(0, 0)
        << QPointF(40 * 30 - 240 + 40, 0);

    // footer to left of items
    QTest::newRow("horizontal, layout right to left") << QQuickListView::Horizontal << Qt::RightToLeft << QQuickItemView::TopToBottom
        << QPointF(-(40 * 3) - 100, 0)
        << QPointF(-(40 * 30) - 50, 0)     // 50 = new footer width
        << QPointF(-240, 0)
        << QPointF(-240, 0)
        << QPointF(-40, 0)
        << QPointF(-(40 * 30) - 40, 0);

    // footer above items
    QTest::newRow("vertical, layout left to right") << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::BottomToTop
        << QPointF(0, -(3 * 20) - 30)
        << QPointF(0, -(30 * 20) - 20)
        << QPointF(0, -320)
        << QPointF(0, -320)
        << QPointF(0, -20)
        << QPointF(0, -(30 * 20) - 10);
}

class LVAccessor : public QQuickListView
{
public:
    qreal minY() const { return minYExtent(); }
    qreal maxY() const { return maxYExtent(); }
    qreal minX() const { return minXExtent(); }
    qreal maxX() const { return maxXExtent(); }
};


void tst_QQuickListView::extents()
{
    QFETCH(QQuickListView::Orientation, orientation);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(QPointF, headerPos);
    QFETCH(QPointF, footerPos);
    QFETCH(QPointF, minPos);
    QFETCH(QPointF, maxPos);
    QFETCH(QPointF, origin_empty);
    QFETCH(QPointF, origin_short);
    QFETCH(QPointF, origin_long);

    QQuickView *window = getView();

    QaimModel model;
    QQmlContext *ctxt = window->rootContext();

    ctxt->setContextProperty("testModel", &model);
    window->setSource(testFileUrl("headerfooter.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = qobject_cast<QQuickListView*>(window->rootObject());
    QTRY_VERIFY(listview != 0);
    listview->setOrientation(orientation);
    listview->setLayoutDirection(layoutDirection);
    listview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQuickItem *header = findItem<QQuickItem>(contentItem, "header");
    QVERIFY(header);
    QCOMPARE(header->position(), headerPos);

    QQuickItem *footer = findItem<QQuickItem>(contentItem, "footer");
    QVERIFY(footer);
    QCOMPARE(footer->position(), footerPos);

    QCOMPARE(static_cast<LVAccessor*>(listview)->minX(), minPos.x());
    QCOMPARE(static_cast<LVAccessor*>(listview)->minY(), minPos.y());
    QCOMPARE(static_cast<LVAccessor*>(listview)->maxX(), maxPos.x());
    QCOMPARE(static_cast<LVAccessor*>(listview)->maxY(), maxPos.y());

    QCOMPARE(listview->originX(), origin_empty.x());
    QCOMPARE(listview->originY(), origin_empty.y());

    for (int i=0; i<3; i++)
        model.addItem("Item" + QString::number(i), "");
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), model.count());
    QCOMPARE(listview->originX(), origin_short.x());
    QCOMPARE(listview->originY(), origin_short.y());

    for (int i=3; i<30; i++)
        model.addItem("Item" + QString::number(i), "");
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), model.count());
    QCOMPARE(listview->originX(), origin_long.x());
    QCOMPARE(listview->originY(), origin_long.y());

    releaseView(window);
}

void tst_QQuickListView::extents_data()
{
    QTest::addColumn<QQuickListView::Orientation>("orientation");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickItemView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<QPointF>("headerPos");
    QTest::addColumn<QPointF>("footerPos");
    QTest::addColumn<QPointF>("minPos");
    QTest::addColumn<QPointF>("maxPos");
    QTest::addColumn<QPointF>("origin_empty");
    QTest::addColumn<QPointF>("origin_short");
    QTest::addColumn<QPointF>("origin_long");

    // header is 240x20 (or 20x320 in Horizontal orientation)
    // footer is 240x30 (or 30x320 in Horizontal orientation)

    QTest::newRow("Vertical, TopToBottom")
            << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::TopToBottom
            << QPointF(0, -20) << QPointF(0, 0)
            << QPointF(0, 20) << QPointF(240, 20)
            << QPointF(0, -20) << QPointF(0, -20) << QPointF(0, -20);

    QTest::newRow("Vertical, BottomToTop")
            << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::BottomToTop
            << QPointF(0, 0) << QPointF(0, -30)
            << QPointF(0, 320 - 20) << QPointF(240, 320 - 20)  // content flow is reversed
            << QPointF(0, -30) << QPointF(0, (-30.0 * 3) - 30) << QPointF(0, (-30.0 * 30) - 30);

    QTest::newRow("Horizontal, LeftToRight")
            << QQuickListView::Horizontal << Qt::LeftToRight << QQuickItemView::TopToBottom
            << QPointF(-20, 0) << QPointF(0, 0)
            << QPointF(20, 0) << QPointF(20, 320)
            << QPointF(-20, 0) << QPointF(-20, 0) << QPointF(-20, 0);

    QTest::newRow("Horizontal, RightToLeft")
            << QQuickListView::Horizontal << Qt::RightToLeft << QQuickItemView::TopToBottom
            << QPointF(0, 0) << QPointF(-30, 0)
            << QPointF(240 - 20, 0) << QPointF(240 - 20, 320)  // content flow is reversed
            << QPointF(-30, 0) << QPointF((-240.0 * 3) - 30, 0) << QPointF((-240.0 * 30) - 30, 0);
}

void tst_QQuickListView::resetModel_headerFooter()
{
    // Resetting a model shouldn't crash in views with header/footer
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 4; i++)
        model.addItem("Item" + QString::number(i), "");
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("headerfooter.qml"));
    qApp->processEvents();

    QQuickListView *listview = qobject_cast<QQuickListView*>(window->rootObject());
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QQuickItem *header = findItem<QQuickItem>(contentItem, "header");
    QVERIFY(header);
    QCOMPARE(header->y(), -header->height());

    QQuickItem *footer = findItem<QQuickItem>(contentItem, "footer");
    QVERIFY(footer);
    QCOMPARE(footer->y(), 30.*4);

    model.reset();

    // A reset should not force a new header or footer to be created.
    QQuickItem *newHeader = findItem<QQuickItem>(contentItem, "header");
    QCOMPARE(newHeader, header);
    QCOMPARE(header->y(), -header->height());

    QQuickItem *newFooter = findItem<QQuickItem>(contentItem, "footer");
    QCOMPARE(newFooter, footer);
    QCOMPARE(footer->y(), 30.*4);
}

void tst_QQuickListView::resizeView()
{
    QScopedPointer<QQuickView> window(createView());
    QaimModel model;
    for (int i = 0; i < 40; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("listviewtest.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), i*20.);
    }

    QVariant heightRatio;
    QMetaObject::invokeMethod(window->rootObject(), "heightRatio", Q_RETURN_ARG(QVariant, heightRatio));
    QCOMPARE(heightRatio.toReal(), 0.4);

    listview->setHeight(200);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QMetaObject::invokeMethod(window->rootObject(), "heightRatio", Q_RETURN_ARG(QVariant, heightRatio));
    QCOMPARE(heightRatio.toReal(), 0.25);

    // Ensure we handle -ve sizes
    listview->setHeight(-100);
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper", false).count(), 1);

    listview->setCacheBuffer(200);
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper", false).count(), 11);

    // ensure items in cache become visible
    listview->setHeight(200);
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper", false).count(), 21);

    itemCount = findItems<QQuickItem>(contentItem, "wrapper", false).count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), i*20.);
        QCOMPARE(delegateVisible(item), i < 11); // inside view visible, outside not visible
    }

    // ensure items outside view become invisible
    listview->setHeight(100);
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper", false).count(), 16);

    itemCount = findItems<QQuickItem>(contentItem, "wrapper", false).count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), i*20.);
        QCOMPARE(delegateVisible(item), i < 6); // inside view visible, outside not visible
    }

    delete testObject;
}

void tst_QQuickListView::resizeViewAndRepaint()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 40; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("initialHeight", 100);

    window->setSource(testFileUrl("resizeview.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // item at index 10 should not be currently visible
    QVERIFY(!findItem<QQuickItem>(contentItem, "wrapper", 10));

    listview->setHeight(320);

    QTRY_VERIFY(findItem<QQuickItem>(contentItem, "wrapper", 10));

    listview->setHeight(100);
    QTRY_VERIFY(!findItem<QQuickItem>(contentItem, "wrapper", 10));
}

void tst_QQuickListView::sizeLessThan1()
{
    QScopedPointer<QQuickView> window(createView());

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("sizelessthan1.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Confirm items positioned correctly
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i = 0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), i*0.5);
    }

    delete testObject;
}

void tst_QQuickListView::QTBUG_14821()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("qtbug14821.qml"));
    qApp->processEvents();

    QQuickListView *listview = qobject_cast<QQuickListView*>(window->rootObject());
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    listview->decrementCurrentIndex();
    QCOMPARE(listview->currentIndex(), 99);

    listview->incrementCurrentIndex();
    QCOMPARE(listview->currentIndex(), 0);
}

void tst_QQuickListView::resizeDelegate()
{
    QScopedPointer<QQuickView> window(createView());
    QStringList strings;
    for (int i = 0; i < 30; ++i)
        strings << QString::number(i);
    QStringListModel model(strings);

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("displaylist.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QVERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QCOMPARE(listview->count(), model.rowCount());

    listview->setCurrentIndex(25);
    listview->setContentY(0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    for (int i = 0; i < 16; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY(item != 0);
        QCOMPARE(item->y(), i*20.0);
    }

    QCOMPARE(listview->currentItem()->y(), 500.0);
    QTRY_COMPARE(listview->highlightItem()->y(), 500.0);

    window->rootObject()->setProperty("delegateHeight", 30);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    for (int i = 0; i < 11; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY(item != 0);
        QTRY_COMPARE(item->y(), i*30.0);
    }

    QTRY_COMPARE(listview->currentItem()->y(), 750.0);
    QTRY_COMPARE(listview->highlightItem()->y(), 750.0);

    listview->setCurrentIndex(1);
    listview->positionViewAtIndex(25, QQuickListView::Beginning);
    listview->positionViewAtIndex(5, QQuickListView::Beginning);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    for (int i = 5; i < 16; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY(item != 0);
        QCOMPARE(item->y(), i*30.0);
    }

    QTRY_COMPARE(listview->currentItem()->y(), 30.0);
    QTRY_COMPARE(listview->highlightItem()->y(), 30.0);

    window->rootObject()->setProperty("delegateHeight", 20);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    for (int i = 5; i < 11; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY(item != 0);
        QTRY_COMPARE(item->y(), 150 + (i-5)*20.0);
    }

    QTRY_COMPARE(listview->currentItem()->y(), 70.0);
    QTRY_COMPARE(listview->highlightItem()->y(), 70.0);
}

void tst_QQuickListView::resizeFirstDelegate()
{
    // QTBUG-20712: Content Y jumps constantly if first delegate height == 0
    // and other delegates have height > 0
    QScopedPointer<QQuickView> window(createView());

    // bug only occurs when all items in the model are visible
    QaimModel model;
    for (int i = 0; i < 10; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("listviewtest.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QVERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQuickItem *item = 0;
    for (int i = 0; i < model.count(); ++i) {
        item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY(item != 0);
        QCOMPARE(item->y(), i*20.0);
    }

    item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    item->setHeight(0);

    // check the content y has not jumped up and down
    QCOMPARE(listview->contentY(), 0.0);
    QSignalSpy spy(listview, SIGNAL(contentYChanged()));
    QTest::qWait(100);
    QCOMPARE(spy.count(), 0);

    for (int i = 1; i < model.count(); ++i) {
        item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY(item != 0);
        QTRY_COMPARE(item->y(), (i-1)*20.0);
    }


    // QTBUG-22014: refill doesn't clear items scrolling off the top of the
    // list if they follow a zero-sized delegate

    for (int i = 0; i < 10; i++)
        model.addItem("Item" + QString::number(i), "");
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), model.count());

    item = findItem<QQuickItem>(contentItem, "wrapper", 1);
    QVERIFY(item);
    item->setHeight(0);

    listview->setCurrentIndex(19);
    qApp->processEvents();
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // items 0-2 should have been deleted
    for (int i=0; i<3; i++) {
        QTRY_VERIFY(!findItem<QQuickItem>(contentItem, "wrapper", i));
    }

    delete testObject;
}

void tst_QQuickListView::repositionResizedDelegate()
{
    QFETCH(QQuickListView::Orientation, orientation);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(QPointF, contentPos_itemFirstHalfVisible);
    QFETCH(QPointF, contentPos_itemSecondHalfVisible);
    QFETCH(QRectF, origPositionerRect);
    QFETCH(QRectF, resizedPositionerRect);

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testHorizontal", orientation == QQuickListView::Horizontal);
    ctxt->setContextProperty("testRightToLeft", layoutDirection == Qt::RightToLeft);
    ctxt->setContextProperty("testBottomToTop", verticalLayoutDirection == QQuickListView::BottomToTop);
    window->setSource(testFileUrl("repositionResizedDelegate.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = qobject_cast<QQuickListView*>(window->rootObject());
    QTRY_VERIFY(listview != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQuickItem *positioner = findItem<QQuickItem>(window->rootObject(), "positioner");
    QVERIFY(positioner);
    QTRY_COMPARE(positioner->boundingRect().size(), origPositionerRect.size());
    QTRY_COMPARE(positioner->position(), origPositionerRect.topLeft());
    QSignalSpy spy(listview, orientation == QQuickListView::Vertical ? SIGNAL(contentYChanged()) : SIGNAL(contentXChanged()));
    int prevSpyCount = 0;

    // When an item is resized while it is partially visible, it should resize in the
    // direction of the content flow. If a RightToLeft or BottomToTop layout is used,
    // the item should also be re-positioned so its end position stays the same.

    listview->setContentX(contentPos_itemFirstHalfVisible.x());
    listview->setContentY(contentPos_itemFirstHalfVisible.y());
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    prevSpyCount = spy.count();
    QVERIFY(QMetaObject::invokeMethod(window->rootObject(), "incrementRepeater"));
    QTRY_COMPARE(positioner->boundingRect().size(), resizedPositionerRect.size());
    QTRY_COMPARE(positioner->position(), resizedPositionerRect.topLeft());
    QCOMPARE(listview->contentX(), contentPos_itemFirstHalfVisible.x());
    QCOMPARE(listview->contentY(), contentPos_itemFirstHalfVisible.y());
    QCOMPARE(spy.count(), prevSpyCount);

    QVERIFY(QMetaObject::invokeMethod(window->rootObject(), "decrementRepeater"));
    QTRY_COMPARE(positioner->boundingRect().size(), origPositionerRect.size());
    QTRY_COMPARE(positioner->position(), origPositionerRect.topLeft());
    QCOMPARE(listview->contentX(), contentPos_itemFirstHalfVisible.x());
    QCOMPARE(listview->contentY(), contentPos_itemFirstHalfVisible.y());

    listview->setContentX(contentPos_itemSecondHalfVisible.x());
    listview->setContentY(contentPos_itemSecondHalfVisible.y());
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    prevSpyCount = spy.count();

    QVERIFY(QMetaObject::invokeMethod(window->rootObject(), "incrementRepeater"));
    positioner = findItem<QQuickItem>(window->rootObject(), "positioner");
    QTRY_COMPARE(positioner->boundingRect().size(), resizedPositionerRect.size());
    QTRY_COMPARE(positioner->position(), resizedPositionerRect.topLeft());
    QCOMPARE(listview->contentX(), contentPos_itemSecondHalfVisible.x());
    QCOMPARE(listview->contentY(), contentPos_itemSecondHalfVisible.y());
    qApp->processEvents();
    QCOMPARE(spy.count(), prevSpyCount);

    releaseView(window);
}

void tst_QQuickListView::repositionResizedDelegate_data()
{
    QTest::addColumn<QQuickListView::Orientation>("orientation");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickListView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<QPointF>("contentPos_itemFirstHalfVisible");
    QTest::addColumn<QPointF>("contentPos_itemSecondHalfVisible");
    QTest::addColumn<QRectF>("origPositionerRect");
    QTest::addColumn<QRectF>("resizedPositionerRect");

    QTest::newRow("vertical")
            << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::TopToBottom
            << QPointF(0, 60) << QPointF(0, 200 + 60)
            << QRectF(0, 200, 120, 120)
            << QRectF(0, 200, 120, 120 * 2);

    QTest::newRow("vertical, BottomToTop")
            << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::BottomToTop
            << QPointF(0, -200 - 60) << QPointF(0, -200 - 260)
            << QRectF(0, -200 - 120, 120, 120)
            << QRectF(0, -200 - 120*2, 120, 120 * 2);

    QTest::newRow("horizontal")
            << QQuickListView::Horizontal<< Qt::LeftToRight << QQuickItemView::TopToBottom
            << QPointF(60, 0) << QPointF(260, 0)
            << QRectF(200, 0, 120, 120)
            << QRectF(200, 0, 120 * 2, 120);

    QTest::newRow("horizontal, rtl")
            << QQuickListView::Horizontal << Qt::RightToLeft << QQuickItemView::TopToBottom
            << QPointF(-200 - 60, 0) << QPointF(-200 - 260, 0)
            << QRectF(-200 - 120, 0, 120, 120)
            << QRectF(-200 - 120 * 2, 0, 120 * 2, 120);
}

void tst_QQuickListView::QTBUG_16037()
{
    QScopedPointer<QQuickView> window(createView());
    window->show();

    window->setSource(testFileUrl("qtbug16037.qml"));
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "listview");
    QTRY_VERIFY(listview != 0);

    QVERIFY(listview->contentHeight() <= 0.0);

    QMetaObject::invokeMethod(window->rootObject(), "setModel");

    QTRY_COMPARE(listview->contentHeight(), 80.0);
}

void tst_QQuickListView::indexAt_itemAt_data()
{
    QTest::addColumn<qreal>("x");
    QTest::addColumn<qreal>("y");
    QTest::addColumn<int>("index");

    QTest::newRow("Item 0 - 0, 0") << 0. << 0. << 0;
    QTest::newRow("Item 0 - 0, 19") << 0. << 19. << 0;
    QTest::newRow("Item 0 - 239, 19") << 239. << 19. << 0;
    QTest::newRow("Item 1 - 0, 20") << 0. << 20. << 1;
    QTest::newRow("No Item - 240, 20") << 240. << 20. << -1;
}

void tst_QQuickListView::indexAt_itemAt()
{
    QFETCH(qreal, x);
    QFETCH(qreal, y);
    QFETCH(int, index);

    QQuickView *window = getView();

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("listviewtest.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQuickItem *item = 0;
    if (index >= 0) {
        item = findItem<QQuickItem>(contentItem, "wrapper", index);
        QVERIFY(item);
    }
    QCOMPARE(listview->indexAt(x,y), index);
    QCOMPARE(listview->itemAt(x,y), item);

    releaseView(window);
    delete testObject;
}

void tst_QQuickListView::incrementalModel()
{
    QScopedPointer<QQuickView> window(createView());
    QSKIP("QTBUG-30716");

    IncrementalModel model;
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("displaylist.qml"));
    qApp->processEvents();

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    listview->forceLayout();

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    listview->forceLayout();
    QTRY_COMPARE(listview->count(), 20);

    listview->positionViewAtIndex(10, QQuickListView::Beginning);

    listview->forceLayout();
    QTRY_COMPARE(listview->count(), 25);
}

void tst_QQuickListView::onAdd()
{
    QFETCH(int, initialItemCount);
    QFETCH(int, itemsToAdd);

    const int delegateHeight = 10;
    QaimModel model;

    // these initial items should not trigger ListView.onAdd
    for (int i=0; i<initialItemCount; i++)
        model.addItem("dummy value", "dummy value");

    QScopedPointer<QQuickView> window(createView());
    window->setGeometry(0,0,200, delegateHeight * (initialItemCount + itemsToAdd));
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("delegateHeight", delegateHeight);
    window->setSource(testFileUrl("attachedSignals.qml"));

    QQuickListView* listview = qobject_cast<QQuickListView*>(window->rootObject());
    listview->setProperty("width", window->width());
    listview->setProperty("height", window->height());
    qApp->processEvents();

    QList<QPair<QString, QString> > items;
    for (int i=0; i<itemsToAdd; i++)
        items << qMakePair(QString("value %1").arg(i), QString::number(i));
    model.addItems(items);
    listview->forceLayout();
    QTRY_COMPARE(listview->property("count").toInt(), model.count());

    QVariantList result = listview->property("addedDelegates").toList();
    QCOMPARE(result.count(), items.count());
    for (int i=0; i<items.count(); i++)
        QCOMPARE(result[i].toString(), items[i].first);
}

void tst_QQuickListView::onAdd_data()
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

void tst_QQuickListView::onRemove()
{
    QFETCH(int, initialItemCount);
    QFETCH(int, indexToRemove);
    QFETCH(int, removeCount);

    const int delegateHeight = 10;
    QaimModel model;
    for (int i=0; i<initialItemCount; i++)
        model.addItem(QString("value %1").arg(i), "dummy value");

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("delegateHeight", delegateHeight);
    window->setSource(testFileUrl("attachedSignals.qml"));

    QQuickListView *listview = qobject_cast<QQuickListView *>(window->rootObject());

    model.removeItems(indexToRemove, removeCount);
    listview->forceLayout();
    QTRY_COMPARE(listview->property("count").toInt(), model.count());

    QCOMPARE(listview->property("removedDelegateCount"), QVariant(removeCount));

    releaseView(window);
}

void tst_QQuickListView::onRemove_data()
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

void tst_QQuickListView::rightToLeft()
{
    QScopedPointer<QQuickView> window(createView());
    window->setGeometry(0,0,640,320);
    window->setSource(testFileUrl("rightToLeft.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QVERIFY(window->rootObject() != 0);
    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "view");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQmlObjectModel *model = window->rootObject()->findChild<QQmlObjectModel*>("itemModel");
    QTRY_VERIFY(model != 0);

    QTRY_COMPARE(model->count(), 3);
    QTRY_COMPARE(listview->currentIndex(), 0);

    // initial position at first item, right edge aligned
    QCOMPARE(listview->contentX(), -640.);

    QQuickItem *item = findItem<QQuickItem>(contentItem, "item1");
    QTRY_VERIFY(item);
    QTRY_COMPARE(item->x(), -100.0);
    QCOMPARE(item->height(), listview->height());

    QQuickText *text = findItem<QQuickText>(contentItem, "text1");
    QTRY_VERIFY(text);
    QTRY_COMPARE(text->text(), QLatin1String("index: 0"));

    listview->setCurrentIndex(2);

    item = findItem<QQuickItem>(contentItem, "item3");
    QTRY_VERIFY(item);
    QTRY_COMPARE(item->x(), -540.0);

    text = findItem<QQuickText>(contentItem, "text3");
    QTRY_VERIFY(text);
    QTRY_COMPARE(text->text(), QLatin1String("index: 2"));

    QCOMPARE(listview->contentX(), -640.);

    // Ensure resizing maintains position relative to right edge
    qobject_cast<QQuickItem*>(window->rootObject())->setWidth(600);
    QTRY_COMPARE(listview->contentX(), -600.);
}

void tst_QQuickListView::test_mirroring()
{
    QScopedPointer<QQuickView> windowA(createView());
    windowA->setSource(testFileUrl("rightToLeft.qml"));
    QQuickListView *listviewA = findItem<QQuickListView>(windowA->rootObject(), "view");
    QTRY_VERIFY(listviewA != 0);

    QScopedPointer<QQuickView> windowB(createView());
    windowB->setSource(testFileUrl("rightToLeft.qml"));
    QQuickListView *listviewB = findItem<QQuickListView>(windowB->rootObject(), "view");
    QTRY_VERIFY(listviewA != 0);
    qApp->processEvents();

    QList<QString> objectNames;
    objectNames << "item1" << "item2"; // << "item3"

    listviewA->setProperty("layoutDirection", Qt::LeftToRight);
    listviewB->setProperty("layoutDirection", Qt::RightToLeft);
    QCOMPARE(listviewA->layoutDirection(), listviewA->effectiveLayoutDirection());

    // LTR != RTL
    foreach (const QString objectName, objectNames)
        QVERIFY(findItem<QQuickItem>(listviewA, objectName)->x() != findItem<QQuickItem>(listviewB, objectName)->x());

    listviewA->setProperty("layoutDirection", Qt::LeftToRight);
    listviewB->setProperty("layoutDirection", Qt::LeftToRight);

    // LTR == LTR
    foreach (const QString objectName, objectNames)
        QCOMPARE(findItem<QQuickItem>(listviewA, objectName)->x(), findItem<QQuickItem>(listviewB, objectName)->x());

    QCOMPARE(listviewB->layoutDirection(), listviewB->effectiveLayoutDirection());
    QQuickItemPrivate::get(listviewB)->setLayoutMirror(true);
    QVERIFY(listviewB->layoutDirection() != listviewB->effectiveLayoutDirection());

    // LTR != LTR+mirror
    foreach (const QString objectName, objectNames)
        QVERIFY(findItem<QQuickItem>(listviewA, objectName)->x() != findItem<QQuickItem>(listviewB, objectName)->x());

    listviewA->setProperty("layoutDirection", Qt::RightToLeft);

    // RTL == LTR+mirror
    foreach (const QString objectName, objectNames)
        QCOMPARE(findItem<QQuickItem>(listviewA, objectName)->x(), findItem<QQuickItem>(listviewB, objectName)->x());

    listviewB->setProperty("layoutDirection", Qt::RightToLeft);

    // RTL != RTL+mirror
    foreach (const QString objectName, objectNames)
        QVERIFY(findItem<QQuickItem>(listviewA, objectName)->x() != findItem<QQuickItem>(listviewB, objectName)->x());

    listviewA->setProperty("layoutDirection", Qt::LeftToRight);

    // LTR == RTL+mirror
    foreach (const QString objectName, objectNames)
        QCOMPARE(findItem<QQuickItem>(listviewA, objectName)->x(), findItem<QQuickItem>(listviewB, objectName)->x());
}

void tst_QQuickListView::margins()
{
    QScopedPointer<QQuickView> window(createView());
    QaimModel model;
    for (int i = 0; i < 50; i++)
        model.addItem("Item" + QString::number(i), "");

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("margins.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QCOMPARE(listview->contentY(), -30.);
    QCOMPARE(listview->originY(), 0.);

    // check end bound
    listview->positionViewAtEnd();
    qreal pos = listview->contentY();
    listview->setContentY(pos + 80);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    listview->returnToBounds();
    QTRY_COMPARE(listview->contentY(), pos + 50);

    // remove item before visible and check that top margin is maintained
    // and originY is updated
    listview->setContentY(100);
    model.removeItem(1);
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), model.count());
    listview->setContentY(-50);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    listview->returnToBounds();
    QCOMPARE(listview->originY(), 20.);
    QTRY_COMPARE(listview->contentY(), -10.);

    // reduce top margin
    listview->setTopMargin(20);
    QCOMPARE(listview->originY(), 20.);
    QTRY_COMPARE(listview->contentY(), 0.);

    // check end bound
    listview->positionViewAtEnd();
    pos = listview->contentY();
    listview->setContentY(pos + 80);
    listview->returnToBounds();
    QTRY_COMPARE(listview->contentY(), pos + 50);

    // reduce bottom margin
    pos = listview->contentY();
    listview->setBottomMargin(40);
    QCOMPARE(listview->originY(), 20.);
    QTRY_COMPARE(listview->contentY(), pos-10);
}

// QTBUG-24028
void tst_QQuickListView::marginsResize()
{
    QFETCH(QQuickListView::Orientation, orientation);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(qreal, start);
    QFETCH(qreal, end);

    QPoint flickStart(20, 20);
    QPoint flickEnd(20, 20);
    if (orientation == QQuickListView::Vertical)
        flickStart.ry() += (verticalLayoutDirection == QQuickItemView::TopToBottom) ? 180 : -180;
    else
        flickStart.rx() += (layoutDirection == Qt::LeftToRight) ? 180 : -180;

    QQuickView *window = getView();

    window->setSource(testFileUrl("margins2.qml"));
    QQuickViewTestUtil::moveMouseAway(window);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "listview");
    QTRY_VERIFY(listview != 0);

    listview->setOrientation(orientation);
    listview->setLayoutDirection(layoutDirection);
    listview->setVerticalLayoutDirection(verticalLayoutDirection);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // view is resized after componentCompleted - top margin should still be visible
    if (orientation == QQuickListView::Vertical)
        QCOMPARE(listview->contentY(), start);
    else
        QCOMPARE(listview->contentX(), start);

    // move to last index and ensure bottom margin is visible.
    listview->setCurrentIndex(19);
    if (orientation == QQuickListView::Vertical)
        QTRY_COMPARE(listview->contentY(), end);
    else
        QTRY_COMPARE(listview->contentX(), end);

    // flick past the end and check content pos still settles on correct extents
    flick(window, flickStart, flickEnd, 180);
    QTRY_VERIFY(!listview->isMoving());
    if (orientation == QQuickListView::Vertical)
        QTRY_COMPARE(listview->contentY(), end);
    else
        QTRY_COMPARE(listview->contentX(), end);

    // back to top - top margin should be visible.
    listview->setCurrentIndex(0);
    if (orientation == QQuickListView::Vertical)
        QTRY_COMPARE(listview->contentY(), start);
    else
        QTRY_COMPARE(listview->contentX(), start);

    // flick past the beginning and check content pos still settles on correct extents
    flick(window, flickEnd, flickStart, 180);
    QTRY_VERIFY(!listview->isMoving());
    if (orientation == QQuickListView::Vertical)
        QTRY_COMPARE(listview->contentY(), start);
    else
        QTRY_COMPARE(listview->contentX(), start);

    releaseView(window);
}

void tst_QQuickListView::marginsResize_data()
{
    QTest::addColumn<QQuickListView::Orientation>("orientation");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickListView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<qreal>("start");
    QTest::addColumn<qreal>("end");

    // in Right to Left mode, leftMargin still means leftMargin - it doesn't reverse to mean rightMargin

    QTest::newRow("vertical")
            << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::TopToBottom
            << -40.0 << 1020.0;

    QTest::newRow("vertical, BottomToTop")
            << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::BottomToTop
            << -180.0 << -1240.0;

    QTest::newRow("horizontal")
            << QQuickListView::Horizontal<< Qt::LeftToRight << QQuickItemView::TopToBottom
            << -40.0 << 1020.0;

    QTest::newRow("horizontal, rtl")
            << QQuickListView::Horizontal << Qt::RightToLeft << QQuickItemView::TopToBottom
            << -180.0 << -1240.0;
}

void tst_QQuickListView::snapToItem_data()
{
    QTest::addColumn<QQuickListView::Orientation>("orientation");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickItemView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<int>("highlightRangeMode");
    QTest::addColumn<QPoint>("flickStart");
    QTest::addColumn<QPoint>("flickEnd");
    QTest::addColumn<qreal>("snapAlignment");
    QTest::addColumn<qreal>("endExtent");
    QTest::addColumn<qreal>("startExtent");

    QTest::newRow("vertical, top to bottom")
        << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::TopToBottom << int(QQuickItemView::NoHighlightRange)
        << QPoint(20, 200) << QPoint(20, 20) << 60.0 << 560.0 << 0.0;

    QTest::newRow("vertical, bottom to top")
        << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::BottomToTop << int(QQuickItemView::NoHighlightRange)
        << QPoint(20, 20) << QPoint(20, 200) << -60.0 << -560.0 - 240.0 << -240.0;

    QTest::newRow("horizontal, left to right")
        << QQuickListView::Horizontal << Qt::LeftToRight << QQuickItemView::TopToBottom << int(QQuickItemView::NoHighlightRange)
        << QPoint(200, 20) << QPoint(20, 20) << 60.0 << 560.0 << 0.0;

    QTest::newRow("horizontal, right to left")
        << QQuickListView::Horizontal << Qt::RightToLeft << QQuickItemView::TopToBottom << int(QQuickItemView::NoHighlightRange)
        << QPoint(20, 20) << QPoint(200, 20) << -60.0 << -560.0 - 240.0 << -240.0;

    QTest::newRow("vertical, top to bottom, enforce range")
        << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::TopToBottom << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(20, 200) << QPoint(20, 20) << 60.0 << 700.0 << -20.0;

    QTest::newRow("vertical, bottom to top, enforce range")
        << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::BottomToTop << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(20, 20) << QPoint(20, 200) << -60.0 << -560.0 - 240.0 - 140.0 << -220.0;

    QTest::newRow("horizontal, left to right, enforce range")
        << QQuickListView::Horizontal << Qt::LeftToRight << QQuickItemView::TopToBottom << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(200, 20) << QPoint(20, 20) << 60.0 << 700.0 << -20.0;

    QTest::newRow("horizontal, right to left, enforce range")
        << QQuickListView::Horizontal << Qt::RightToLeft << QQuickItemView::TopToBottom << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(20, 20) << QPoint(200, 20) << -60.0 << -560.0 - 240.0 - 140.0 << -220.0;
}

void tst_QQuickListView::snapToItem()
{
    QFETCH(QQuickListView::Orientation, orientation);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(int, highlightRangeMode);
    QFETCH(QPoint, flickStart);
    QFETCH(QPoint, flickEnd);
    QFETCH(qreal, snapAlignment);
    QFETCH(qreal, endExtent);
    QFETCH(qreal, startExtent);

    QQuickView *window = getView();
    QQuickViewTestUtil::moveMouseAway(window);

    window->setSource(testFileUrl("snapToItem.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));


    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    listview->setOrientation(orientation);
    listview->setLayoutDirection(layoutDirection);
    listview->setVerticalLayoutDirection(verticalLayoutDirection);
    listview->setHighlightRangeMode(QQuickItemView::HighlightRangeMode(highlightRangeMode));
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    // confirm that a flick hits an item boundary
    flick(window, flickStart, flickEnd, 180);
    QTRY_VERIFY(listview->isMoving() == false); // wait until it stops
    if (orientation == QQuickListView::Vertical)
        QCOMPARE(qreal(fmod(listview->contentY(),80.0)), snapAlignment);
    else
        QCOMPARE(qreal(fmod(listview->contentX(),80.0)), snapAlignment);

    // flick to end
    do {
        flick(window, flickStart, flickEnd, 180);
        QTRY_VERIFY(listview->isMoving() == false); // wait until it stops
    } while (orientation == QQuickListView::Vertical
           ? verticalLayoutDirection == QQuickItemView::TopToBottom ? !listview->isAtYEnd() : !listview->isAtYBeginning()
           : layoutDirection == Qt::LeftToRight ? !listview->isAtXEnd() : !listview->isAtXBeginning());

    if (orientation == QQuickListView::Vertical)
        QCOMPARE(listview->contentY(), endExtent);
    else
        QCOMPARE(listview->contentX(), endExtent);

    // flick to start
    do {
        flick(window, flickEnd, flickStart, 180);
        QTRY_VERIFY(listview->isMoving() == false); // wait until it stops
    } while (orientation == QQuickListView::Vertical
           ? verticalLayoutDirection == QQuickItemView::TopToBottom ? !listview->isAtYBeginning() : !listview->isAtYEnd()
           : layoutDirection == Qt::LeftToRight ? !listview->isAtXBeginning() : !listview->isAtXEnd());

    if (orientation == QQuickListView::Vertical)
        QCOMPARE(listview->contentY(), startExtent);
    else
        QCOMPARE(listview->contentX(), startExtent);

    releaseView(window);
}

void tst_QQuickListView::snapOneItemResize_QTBUG_43555()
{
    QScopedPointer<QQuickView> window(createView());
    window->resize(QSize(100, 320));
    window->setResizeMode(QQuickView::SizeRootObjectToView);
    QQuickViewTestUtil::moveMouseAway(window.data());

    window->setSource(testFileUrl("snapOneItemResize.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = qobject_cast<QQuickListView*>(window->rootObject());
    QTRY_VERIFY(listview != 0);

    QSignalSpy currentIndexSpy(listview, SIGNAL(currentIndexChanged()));

    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    QTRY_COMPARE(listview->currentIndex(), 5);
    currentIndexSpy.clear();

    window->resize(QSize(400, 320));

    QTRY_COMPARE(int(listview->width()), 400);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QTRY_COMPARE(listview->currentIndex(), 5);
    QCOMPARE(currentIndexSpy.count(), 0);
}

void tst_QQuickListView::qAbstractItemModel_package_items()
{
    items<QaimModel>(testFileUrl("listviewtest-package.qml"));
}

void tst_QQuickListView::qAbstractItemModel_items()
{
    items<QaimModel>(testFileUrl("listviewtest.qml"));
}

void tst_QQuickListView::qAbstractItemModel_package_changed()
{
    changed<QaimModel>(testFileUrl("listviewtest-package.qml"));
}

void tst_QQuickListView::qAbstractItemModel_changed()
{
    changed<QaimModel>(testFileUrl("listviewtest.qml"));
}

void tst_QQuickListView::qAbstractItemModel_package_inserted()
{
    inserted<QaimModel>(testFileUrl("listviewtest-package.qml"));
}

void tst_QQuickListView::qAbstractItemModel_inserted()
{
    inserted<QaimModel>(testFileUrl("listviewtest.qml"));
}

void tst_QQuickListView::qAbstractItemModel_inserted_more()
{
    inserted_more<QaimModel>();
}

void tst_QQuickListView::qAbstractItemModel_inserted_more_data()
{
    inserted_more_data();
}

void tst_QQuickListView::qAbstractItemModel_inserted_more_bottomToTop()
{
    inserted_more<QaimModel>(QQuickItemView::BottomToTop);
}

void tst_QQuickListView::qAbstractItemModel_inserted_more_bottomToTop_data()
{
    inserted_more_data();
}

void tst_QQuickListView::qAbstractItemModel_package_removed()
{
    removed<QaimModel>(testFileUrl("listviewtest-package.qml"), false);
    removed<QaimModel>(testFileUrl("listviewtest-package.qml"), true);
}

void tst_QQuickListView::qAbstractItemModel_removed()
{
    removed<QaimModel>(testFileUrl("listviewtest.qml"), false);
    removed<QaimModel>(testFileUrl("listviewtest.qml"), true);
}

void tst_QQuickListView::qAbstractItemModel_removed_more()
{
    removed_more<QaimModel>(testFileUrl("listviewtest.qml"));
}

void tst_QQuickListView::qAbstractItemModel_removed_more_data()
{
    removed_more_data();
}

void tst_QQuickListView::qAbstractItemModel_removed_more_bottomToTop()
{
    removed_more<QaimModel>(testFileUrl("listviewtest.qml"), QQuickItemView::BottomToTop);
}

void tst_QQuickListView::qAbstractItemModel_removed_more_bottomToTop_data()
{
    removed_more_data();
}

void tst_QQuickListView::qAbstractItemModel_package_moved()
{
    moved<QaimModel>(testFileUrl("listviewtest-package.qml"));
}

void tst_QQuickListView::qAbstractItemModel_package_moved_data()
{
    moved_data();
}

void tst_QQuickListView::qAbstractItemModel_moved()
{
    moved<QaimModel>(testFileUrl("listviewtest.qml"));
}

void tst_QQuickListView::qAbstractItemModel_moved_data()
{
    moved_data();
}

void tst_QQuickListView::qAbstractItemModel_moved_bottomToTop()
{
    moved<QaimModel>(testFileUrl("listviewtest-package.qml"), QQuickItemView::BottomToTop);
}

void tst_QQuickListView::qAbstractItemModel_moved_bottomToTop_data()
{
    moved_data();
}

void tst_QQuickListView::qAbstractItemModel_package_clear()
{
    clear<QaimModel>(testFileUrl("listviewtest-package.qml"));
}

void tst_QQuickListView::qAbstractItemModel_clear()
{
    clear<QaimModel>(testFileUrl("listviewtest.qml"));
}

void tst_QQuickListView::qAbstractItemModel_clear_bottomToTop()
{
    clear<QaimModel>(testFileUrl("listviewtest.qml"), QQuickItemView::BottomToTop);
}

void tst_QQuickListView::qAbstractItemModel_package_sections()
{
    sections<QaimModel>(testFileUrl("listview-sections-package.qml"));
}

void tst_QQuickListView::qAbstractItemModel_sections()
{
    sections<QaimModel>(testFileUrl("listview-sections.qml"));
}

void tst_QQuickListView::creationContext()
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
    QVERIFY(item = rootItem->findChild<QQuickItem *>("section"));
    QCOMPARE(item->property("text").toString(), QString("Hello!"));
}

void tst_QQuickListView::QTBUG_21742()
{
    QQuickView window;
    window.setGeometry(0,0,200,200);
    window.setSource(testFileUrl("qtbug-21742.qml"));
    qApp->processEvents();

    QQuickItem *rootItem = qobject_cast<QQuickItem *>(window.rootObject());
    QVERIFY(rootItem);
    QCOMPARE(rootItem->property("count").toInt(), 1);
}

void tst_QQuickListView::asynchronous()
{
    QScopedPointer<QQuickView> window(createView());
    window->show();
    QQmlIncubationController controller;
    window->engine()->setIncubationController(&controller);

    window->setSource(testFileUrl("asyncloader.qml"));

    QQuickItem *rootObject = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(rootObject);

    QQuickListView *listview = 0;
    while (!listview) {
        bool b = false;
        controller.incubateWhile(&b);
        listview = rootObject->findChild<QQuickListView*>("view");
    }

    // items will be created one at a time
    for (int i = 0; i < 8; ++i) {
        QVERIFY(findItem<QQuickItem>(listview, "wrapper", i) == 0);
        QQuickItem *item = 0;
        while (!item) {
            bool b = false;
            controller.incubateWhile(&b);
            item = findItem<QQuickItem>(listview, "wrapper", i);
        }
    }

    {
        bool b = true;
        controller.incubateWhile(&b);
    }

    // verify positioning
    QQuickItem *contentItem = listview->contentItem();
    for (int i = 0; i < 8; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QTRY_COMPARE(item->y(), i*50.0);
    }
}

void tst_QQuickListView::snapOneItem_data()
{
    QTest::addColumn<QQuickListView::Orientation>("orientation");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickItemView::VerticalLayoutDirection>("verticalLayoutDirection");
    QTest::addColumn<int>("highlightRangeMode");
    QTest::addColumn<QPoint>("flickStart");
    QTest::addColumn<QPoint>("flickEnd");
    QTest::addColumn<qreal>("snapAlignment");
    QTest::addColumn<qreal>("endExtent");
    QTest::addColumn<qreal>("startExtent");

    QTest::newRow("vertical, top to bottom")
        << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::TopToBottom << int(QQuickItemView::NoHighlightRange)
        << QPoint(20, 200) << QPoint(20, 20) << 180.0 << 560.0 << 0.0;

    QTest::newRow("vertical, bottom to top")
        << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::BottomToTop << int(QQuickItemView::NoHighlightRange)
        << QPoint(20, 20) << QPoint(20, 200) << -420.0 << -560.0 - 240.0 << -240.0;

    QTest::newRow("horizontal, left to right")
        << QQuickListView::Horizontal << Qt::LeftToRight << QQuickItemView::TopToBottom << int(QQuickItemView::NoHighlightRange)
        << QPoint(200, 20) << QPoint(20, 20) << 180.0 << 560.0 << 0.0;

    QTest::newRow("horizontal, right to left")
        << QQuickListView::Horizontal << Qt::RightToLeft << QQuickItemView::TopToBottom << int(QQuickItemView::NoHighlightRange)
        << QPoint(20, 20) << QPoint(200, 20) << -420.0 << -560.0 - 240.0 << -240.0;

    QTest::newRow("vertical, top to bottom, enforce range")
        << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::TopToBottom << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(20, 200) << QPoint(20, 20) << 180.0 << 580.0 << -20.0;

    QTest::newRow("vertical, bottom to top, enforce range")
        << QQuickListView::Vertical << Qt::LeftToRight << QQuickItemView::BottomToTop << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(20, 20) << QPoint(20, 200) << -420.0 << -580.0 - 240.0 << -220.0;

    QTest::newRow("horizontal, left to right, enforce range")
        << QQuickListView::Horizontal << Qt::LeftToRight << QQuickItemView::TopToBottom << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(200, 20) << QPoint(20, 20) << 180.0 << 580.0 << -20.0;

    QTest::newRow("horizontal, right to left, enforce range")
        << QQuickListView::Horizontal << Qt::RightToLeft << QQuickItemView::TopToBottom << int(QQuickItemView::StrictlyEnforceRange)
        << QPoint(20, 20) << QPoint(200, 20) << -420.0 << -580.0 - 240.0 << -220.0;
}

void tst_QQuickListView::snapOneItem()
{
    QFETCH(QQuickListView::Orientation, orientation);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);
    QFETCH(int, highlightRangeMode);
    QFETCH(QPoint, flickStart);
    QFETCH(QPoint, flickEnd);
    QFETCH(qreal, snapAlignment);
    QFETCH(qreal, endExtent);
    QFETCH(qreal, startExtent);

    QQuickView *window = getView();
    QQuickViewTestUtil::moveMouseAway(window);

    window->setSource(testFileUrl("snapOneItem.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));


    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    listview->setOrientation(orientation);
    listview->setLayoutDirection(layoutDirection);
    listview->setVerticalLayoutDirection(verticalLayoutDirection);
    listview->setHighlightRangeMode(QQuickItemView::HighlightRangeMode(highlightRangeMode));
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);

    QSignalSpy currentIndexSpy(listview, SIGNAL(currentIndexChanged()));

    // confirm that a flick hits the next item boundary
    flick(window, flickStart, flickEnd, 180);
    QTRY_VERIFY(listview->isMoving() == false); // wait until it stops
    if (orientation == QQuickListView::Vertical)
        QCOMPARE(listview->contentY(), snapAlignment);
    else
        QCOMPARE(listview->contentX(), snapAlignment);

    if (QQuickItemView::HighlightRangeMode(highlightRangeMode) == QQuickItemView::StrictlyEnforceRange) {
        QCOMPARE(listview->currentIndex(), 1);
        QCOMPARE(currentIndexSpy.count(), 1);
    }

    // flick to end
    do {
        flick(window, flickStart, flickEnd, 180);
        QTRY_VERIFY(listview->isMoving() == false); // wait until it stops
    } while (orientation == QQuickListView::Vertical
           ? verticalLayoutDirection == QQuickItemView::TopToBottom ? !listview->isAtYEnd() : !listview->isAtYBeginning()
           : layoutDirection == Qt::LeftToRight ? !listview->isAtXEnd() : !listview->isAtXBeginning());

    if (orientation == QQuickListView::Vertical)
        QCOMPARE(listview->contentY(), endExtent);
    else
        QCOMPARE(listview->contentX(), endExtent);

    if (QQuickItemView::HighlightRangeMode(highlightRangeMode) == QQuickItemView::StrictlyEnforceRange) {
        QCOMPARE(listview->currentIndex(), 3);
        QCOMPARE(currentIndexSpy.count(), 3);
    }

    // flick to start
    do {
        flick(window, flickEnd, flickStart, 180);
        QTRY_VERIFY(listview->isMoving() == false); // wait until it stops
    } while (orientation == QQuickListView::Vertical
           ? verticalLayoutDirection == QQuickItemView::TopToBottom ? !listview->isAtYBeginning() : !listview->isAtYEnd()
           : layoutDirection == Qt::LeftToRight ? !listview->isAtXBeginning() : !listview->isAtXEnd());

    if (orientation == QQuickListView::Vertical)
        QCOMPARE(listview->contentY(), startExtent);
    else
        QCOMPARE(listview->contentX(), startExtent);

    if (QQuickItemView::HighlightRangeMode(highlightRangeMode) == QQuickItemView::StrictlyEnforceRange) {
        QCOMPARE(listview->currentIndex(), 0);
        QCOMPARE(currentIndexSpy.count(), 6);
    }

    releaseView(window);
}

void tst_QQuickListView::snapOneItemCurrentIndexRemoveAnimation()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("snapOneItemCurrentIndexRemoveAnimation.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = qobject_cast<QQuickListView*>(window->rootObject());
    QTRY_VERIFY(listview != 0);

    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    QTRY_COMPARE(listview->currentIndex(), 0);
    QSignalSpy currentIndexSpy(listview, SIGNAL(currentIndexChanged()));

    QMetaObject::invokeMethod(window->rootObject(), "removeItemZero");
    QTRY_COMPARE(listview->property("transitionsRun").toInt(), 1);

    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QCOMPARE(listview->currentIndex(), 0);
    QCOMPARE(currentIndexSpy.count(), 0);
}

void tst_QQuickListView::attachedProperties_QTBUG_32836()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("attachedProperties.qml"));
    window->show();
    qApp->processEvents();

    QQuickListView *listview = qobject_cast<QQuickListView*>(window->rootObject());
    QVERIFY(listview != 0);

    QQuickItem *header = listview->headerItem();
    QVERIFY(header);
    QCOMPARE(header->width(), listview->width());

    QQuickItem *footer = listview->footerItem();
    QVERIFY(footer);
    QCOMPARE(footer->width(), listview->width());

    QQuickItem *highlight = listview->highlightItem();
    QVERIFY(highlight);
    QCOMPARE(highlight->width(), listview->width());

    QQuickItem *currentItem = listview->currentItem();
    QVERIFY(currentItem);
    QCOMPARE(currentItem->width(), listview->width());

    QQuickItem *sectionItem = findItem<QQuickItem>(window->rootObject(), "sectionItem");
    QVERIFY(sectionItem);
    QCOMPARE(sectionItem->width(), listview->width());
}

void tst_QQuickListView::unrequestedVisibility()
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
    QVERIFY(QTest::qWaitForWindowExposed(window));


    QQuickListView *leftview = findItem<QQuickListView>(window->rootObject(), "leftList");
    QTRY_VERIFY(leftview != 0);

    QQuickListView *rightview = findItem<QQuickListView>(window->rootObject(), "rightList");
    QTRY_VERIFY(rightview != 0);

    QQuickItem *leftContent = leftview->contentItem();
    QTRY_VERIFY(leftContent != 0);

    QQuickItem *rightContent = rightview->contentItem();
    QTRY_VERIFY(rightContent != 0);

    rightview->setCurrentIndex(20);

    QTRY_COMPARE(leftview->contentY(), 0.0);
    QTRY_COMPARE(rightview->contentY(), 100.0);

    QQuickItem *item;

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 1));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 1));
    QCOMPARE(delegateVisible(item), false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 19));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 19));
    QCOMPARE(delegateVisible(item), true);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 16));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 17));
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

    QVERIFY(!findItem<QQuickItem>(leftContent, "wrapper", 19));
    QVERIFY(!findItem<QQuickItem>(rightContent, "wrapper", 19));

    leftview->setCurrentIndex(20);

    QTRY_COMPARE(leftview->contentY(), 100.0);
    QTRY_COMPARE(rightview->contentY(), 0.0);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 1));
    QTRY_COMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 1));
    QCOMPARE(delegateVisible(item), true);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 19));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 19));
    QCOMPARE(delegateVisible(item), false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 3));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 4));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 16));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 17));
    QCOMPARE(delegateVisible(item), false);

    model.moveItems(19, 1, 1);
    QTRY_COMPARE(QQuickItemPrivate::get(leftview)->polishScheduled, false);

    QTRY_VERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 1));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 1));
    QCOMPARE(delegateVisible(item), true);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 19));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 19));
    QCOMPARE(delegateVisible(item), false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 4));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 5));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 16));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 17));
    QCOMPARE(delegateVisible(item), false);

    model.moveItems(3, 4, 1);
    QTRY_COMPARE(QQuickItemPrivate::get(leftview)->polishScheduled, false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 4));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 5));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 16));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 17));
    QCOMPARE(delegateVisible(item), false);

    model.moveItems(4, 3, 1);
    QTRY_COMPARE(QQuickItemPrivate::get(leftview)->polishScheduled, false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 4));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 5));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 16));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 17));
    QCOMPARE(delegateVisible(item), false);

    model.moveItems(16, 17, 1);
    QTRY_COMPARE(QQuickItemPrivate::get(leftview)->polishScheduled, false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 4));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 5));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 16));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 17));
    QCOMPARE(delegateVisible(item), false);

    model.moveItems(17, 16, 1);
    QTRY_COMPARE(QQuickItemPrivate::get(leftview)->polishScheduled, false);

    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 4));
    QCOMPARE(delegateVisible(item), false);
    QVERIFY(item = findItem<QQuickItem>(leftContent, "wrapper", 5));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 16));
    QCOMPARE(delegateVisible(item), true);
    QVERIFY(item = findItem<QQuickItem>(rightContent, "wrapper", 17));
    QCOMPARE(delegateVisible(item), false);

    delete window;
}

void tst_QQuickListView::populateTransitions()
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
    window->rootContext()->setContextProperty("testObject", new TestObject(window->rootContext()));
    window->rootContext()->setContextProperty("usePopulateTransition", usePopulateTransition);
    window->rootContext()->setContextProperty("dynamicallyPopulate", dynamicallyPopulate);
    window->rootContext()->setContextProperty("transitionFrom", transitionFrom);
    window->rootContext()->setContextProperty("transitionVia", transitionVia);
    window->rootContext()->setContextProperty("model_transitionFrom", &model_transitionFrom);
    window->rootContext()->setContextProperty("model_transitionVia", &model_transitionVia);
    window->setSource(testFileUrl("populateTransitions.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QVERIFY(listview);
    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem);

    if (staticallyPopulate && usePopulateTransition) {
        QTRY_COMPARE(listview->property("countPopulateTransitions").toInt(), 16);
        QTRY_COMPARE(listview->property("countAddTransitions").toInt(), 0);
    } else if (dynamicallyPopulate) {
        QTRY_COMPARE(listview->property("countPopulateTransitions").toInt(), 0);
        QTRY_COMPARE(listview->property("countAddTransitions").toInt(), 16);
    } else {
        QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
        QCOMPARE(listview->property("countPopulateTransitions").toInt(), 0);
        QCOMPARE(listview->property("countAddTransitions").toInt(), 0);
    }

    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i=0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->x(), 0.0);
        QTRY_COMPARE(item->y(), i*20.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    listview->setProperty("countPopulateTransitions", 0);
    listview->setProperty("countAddTransitions", 0);

    // add an item and check this is done with add transition, not populate
    model.insertItem(0, "another item", "");
    QTRY_COMPARE(listview->property("countAddTransitions").toInt(), 1);
    QTRY_COMPARE(listview->property("countPopulateTransitions").toInt(), 0);

    // clear the model
    window->rootContext()->setContextProperty("testModel", QVariant());
    listview->forceLayout();
    QTRY_COMPARE(listview->count(), 0);
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper").count(), 0);
    listview->setProperty("countPopulateTransitions", 0);
    listview->setProperty("countAddTransitions", 0);

    // set to a valid model and check populate transition is run a second time
    model.clear();
    for (int i = 0; i < 30; i++)
        model.addItem("item" + QString::number(i), "");
    window->rootContext()->setContextProperty("testModel", &model);
    QTRY_COMPARE(listview->property("countPopulateTransitions").toInt(), usePopulateTransition ? 16 : 0);
    QTRY_COMPARE(listview->property("countAddTransitions").toInt(), 0);

    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i=0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->x(), 0.0);
        QTRY_COMPARE(item->y(), i*20.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    // reset model and check populate transition is run again
    listview->setProperty("countPopulateTransitions", 0);
    listview->setProperty("countAddTransitions", 0);
    model.reset();
    QTRY_COMPARE(listview->property("countPopulateTransitions").toInt(), usePopulateTransition ? 16 : 0);
    QTRY_COMPARE(listview->property("countAddTransitions").toInt(), 0);

    itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i=0; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->x(), 0.0);
        QTRY_COMPARE(item->y(), i*20.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickListView::populateTransitions_data()
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


/*
 * Tests if the first visible item is not repositioned if the same item
 * resized + changes position during a transition. The test does not test the
 * actual position while it is transitioning (since its timing sensitive), but
 * rather tests if the transition has reached its target state properly.
 **/
void tst_QQuickListView::sizeTransitions()
{
    QFETCH(bool, topToBottom);
    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    QaimModel model;
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("topToBottom", topToBottom);
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", &model);
    window->setSource(testFileUrl("sizeTransitions.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // the following will start the transition
    model.addItem(QLatin1String("Test"), "");

    // This ensures early failure in case of failure (in which case
    // transitionFinished == true and scriptActionExecuted == false)
    QTRY_COMPARE(listview->property("scriptActionExecuted").toBool() ||
                 listview->property("transitionFinished").toBool(), true);
    QCOMPARE(listview->property("scriptActionExecuted").toBool(), true);
    QCOMPARE(listview->property("transitionFinished").toBool(), true);

    releaseView(window);
    delete testObject;
}

void tst_QQuickListView::sizeTransitions_data()
{
    QTest::addColumn<bool>("topToBottom");

    QTest::newRow("TopToBottom")
            << true;

    QTest::newRow("LeftToRight")
            << false;
}

void tst_QQuickListView::addTransitions()
{
    QFETCH(int, initialItemCount);
    QFETCH(bool, shouldAnimateTargets);
    QFETCH(qreal, contentY);
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
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("model_targetItems_transitionFrom", &model_targetItems_transitionFrom);
    ctxt->setContextProperty("model_displacedItems_transitionVia", &model_displacedItems_transitionVia);
    ctxt->setContextProperty("targetItems_transitionFrom", targetItems_transitionFrom);
    ctxt->setContextProperty("displacedItems_transitionVia", displacedItems_transitionVia);
    ctxt->setContextProperty("testObject", testObject);
    window->setSource(testFileUrl("addTransitions.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    if (contentY != 0) {
        listview->setContentY(contentY);
        QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    }

    QList<QPair<QString,QString> > expectedDisplacedValues = expectedDisplacedIndexes.getModelDataValues(model);

    // only target items that will become visible should be animated
    QList<QPair<QString, QString> > newData;
    QList<QPair<QString, QString> > expectedTargetData;
    QList<int> targetIndexes;
    if (shouldAnimateTargets) {
        for (int i=insertionIndex; i<insertionIndex+insertionCount; i++) {
            newData << qMakePair(QString("New item %1").arg(i), QString(""));

            if (i >= contentY / 20 && i < (contentY + listview->height()) / 20) {  // only grab visible items
                expectedTargetData << newData.last();
                targetIndexes << i;
            }
        }
        QVERIFY(expectedTargetData.count() > 0);
    }

    // start animation
    if (!newData.isEmpty()) {
        model.insertItems(insertionIndex, newData);
        QTRY_COMPARE(model.count(), listview->count());
        listview->forceLayout();
    }

    QList<QQuickItem *> targetItems = findItems<QQuickItem>(contentItem, "wrapper", targetIndexes);

    if (shouldAnimateTargets) {
        QTRY_COMPARE(listview->property("targetTransitionsDone").toInt(), expectedTargetData.count());
        QTRY_COMPARE(listview->property("displaceTransitionsDone").toInt(),
                     expectedDisplacedIndexes.isValid() ? expectedDisplacedIndexes.count() : 0);

        // check the target and displaced items were animated
        model_targetItems_transitionFrom.matchAgainst(expectedTargetData, "wasn't animated from target 'from' pos", "shouldn't have been animated from target 'from' pos");
        model_displacedItems_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with displaced anim", "shouldn't have been animated with displaced anim");

        // check attached properties
        matchItemsAndIndexes(listview->property("targetTrans_items").toMap(), model, targetIndexes);
        matchIndexLists(listview->property("targetTrans_targetIndexes").toList(), targetIndexes);
        matchItemLists(listview->property("targetTrans_targetItems").toList(), targetItems);
        if (expectedDisplacedIndexes.isValid()) {
            // adjust expectedDisplacedIndexes to their final values after the move
            QList<int> displacedIndexes = adjustIndexesForAddDisplaced(expectedDisplacedIndexes.indexes, insertionIndex, insertionCount);
            matchItemsAndIndexes(listview->property("displacedTrans_items").toMap(), model, displacedIndexes);
            matchIndexLists(listview->property("displacedTrans_targetIndexes").toList(), targetIndexes);
            matchItemLists(listview->property("displacedTrans_targetItems").toList(), targetItems);
        }

    } else {
        QTRY_COMPARE(model_targetItems_transitionFrom.count(), 0);
        QTRY_COMPARE(model_displacedItems_transitionVia.count(), 0);
    }

    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    int firstVisibleIndex = -1;
    int itemCount = items.count();
    for (int i=0; i<items.count(); i++) {
        if (items[i]->y() >= contentY) {
            QQmlExpression e(qmlContext(items[i]), items[i], "index");
            firstVisibleIndex = e.evaluate().toInt();
            break;
        }
    }
    QVERIFY2(firstVisibleIndex >= 0, QTest::toString(firstVisibleIndex));

    // verify all items moved to the correct final positions
    for (int i=firstVisibleIndex; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->y(), i*20.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
    delete testObject;
}

void tst_QQuickListView::addTransitions_data()
{
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<qreal>("contentY");
    QTest::addColumn<bool>("shouldAnimateTargets");
    QTest::addColumn<int>("insertionIndex");
    QTest::addColumn<int>("insertionCount");
    QTest::addColumn<ListRange>("expectedDisplacedIndexes");

    // if inserting before visible index, items should not appear or animate in, even if there are > 1 new items
    QTest::newRow("insert 1, just before start")
            << 30 << 20.0 << false
            << 0 << 1 << ListRange();
    QTest::newRow("insert 1, way before start")
            << 30 << 20.0 << false
            << 0 << 1 << ListRange();
    QTest::newRow("insert multiple, just before start")
            << 30 << 100.0 << false
            << 0 << 3 << ListRange();
    QTest::newRow("insert multiple, way before start")
            << 30 << 100.0 << false
            << 0 << 3 << ListRange();

    QTest::newRow("insert 1 at start")
            << 30 << 0.0 << true
            << 0 << 1 << ListRange(0, 15);
    QTest::newRow("insert multiple at start")
            << 30 << 0.0 << true
            << 0 << 3 << ListRange(0, 15);
    QTest::newRow("insert 1 at start, content y not 0")
            << 30 << 40.0 << true  // first visible is index 2, so translate the displaced indexes by 2
            << 2 << 1 << ListRange(0 + 2, 15 + 2);
    QTest::newRow("insert multiple at start, content y not 0")
            << 30 << 40.0 << true    // first visible is index 2
            << 2 << 3 << ListRange(0 + 2, 15 + 2);

    QTest::newRow("insert 1 at start, to empty list")
            << 0 << 0.0 << true
            << 0 << 1 << ListRange();
    QTest::newRow("insert multiple at start, to empty list")
            << 0 << 0.0 << true
            << 0 << 3 << ListRange();

    QTest::newRow("insert 1 at middle")
            << 30 << 0.0 << true
            << 5 << 1 << ListRange(5, 15);
    QTest::newRow("insert multiple at middle")
            << 30 << 0.0 << true
            << 5 << 3 << ListRange(5, 15);

    QTest::newRow("insert 1 at bottom")
            << 30 << 0.0 << true
            << 15 << 1 << ListRange(15, 15);
    QTest::newRow("insert multiple at bottom")
            << 30 << 0.0 << true
            << 15 << 3 << ListRange(15, 15);
    QTest::newRow("insert 1 at bottom, content y not 0")
            << 30 << 20.0 * 3 << true
            << 15 + 3 << 1 << ListRange(15 + 3, 15 + 3);
    QTest::newRow("insert multiple at bottom, content y not 0")
            << 30 << 20.0 * 3 << true
            << 15 + 3 << 3 << ListRange(15 + 3, 15 + 3);

    // items added after the last visible will not be animated in, since they
    // do not appear in the final view
    QTest::newRow("insert 1 after end")
            << 30 << 0.0 << false
            << 17 << 1 << ListRange();
    QTest::newRow("insert multiple after end")
            << 30 << 0.0 << false
            << 17 << 3 << ListRange();
}

void tst_QQuickListView::moveTransitions()
{
    QFETCH(int, initialItemCount);
    QFETCH(qreal, contentY);
    QFETCH(qreal, itemsOffsetAfterMove);
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
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("model_targetItems_transitionVia", &model_targetItems_transitionVia);
    ctxt->setContextProperty("model_displacedItems_transitionVia", &model_displacedItems_transitionVia);
    ctxt->setContextProperty("targetItems_transitionVia", targetItems_transitionVia);
    ctxt->setContextProperty("displacedItems_transitionVia", displacedItems_transitionVia);
    ctxt->setContextProperty("testObject", testObject);
    window->setSource(testFileUrl("moveTransitions.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);
    QQuickText *name;

    if (contentY != 0) {
        listview->setContentY(contentY);
        QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    }

    QList<QPair<QString,QString> > expectedDisplacedValues = expectedDisplacedIndexes.getModelDataValues(model);

    // Items moving to *or* from visible positions should be animated.
    // Otherwise, they should not be animated.
    QList<QPair<QString, QString> > expectedTargetData;
    QList<int> targetIndexes;
    for (int i=moveFrom; i<moveFrom+moveCount; i++) {
        int toIndex = moveTo + (i - moveFrom);
        if (i <= (contentY + listview->height()) / 20
                || toIndex < (contentY + listview->height()) / 20) {
            expectedTargetData << qMakePair(model.name(i), model.number(i));
            targetIndexes << i;
        }
    }
    // ViewTransition.index provides the indices that items are moving to, not from
    targetIndexes = adjustIndexesForMove(targetIndexes, moveFrom, moveTo, moveCount);

    // start animation
    model.moveItems(moveFrom, moveTo, moveCount);

    QTRY_COMPARE(listview->property("targetTransitionsDone").toInt(), expectedTargetData.count());
    QTRY_COMPARE(listview->property("displaceTransitionsDone").toInt(),
                 expectedDisplacedIndexes.isValid() ? expectedDisplacedIndexes.count() : 0);

    QList<QQuickItem *> targetItems = findItems<QQuickItem>(contentItem, "wrapper", targetIndexes);

    // check the target and displaced items were animated
    model_targetItems_transitionVia.matchAgainst(expectedTargetData, "wasn't animated from target 'from' pos", "shouldn't have been animated from target 'from' pos");
    model_displacedItems_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with displaced anim", "shouldn't have been animated with displaced anim");

    // check attached properties
    matchItemsAndIndexes(listview->property("targetTrans_items").toMap(), model, targetIndexes);
    matchIndexLists(listview->property("targetTrans_targetIndexes").toList(), targetIndexes);
    matchItemLists(listview->property("targetTrans_targetItems").toList(), targetItems);
    if (expectedDisplacedIndexes.isValid()) {
        // adjust expectedDisplacedIndexes to their final values after the move
        QList<int> displacedIndexes = adjustIndexesForMove(expectedDisplacedIndexes.indexes, moveFrom, moveTo, moveCount);
        matchItemsAndIndexes(listview->property("displacedTrans_items").toMap(), model, displacedIndexes);
        matchIndexLists(listview->property("displacedTrans_targetIndexes").toList(), targetIndexes);
        matchItemLists(listview->property("displacedTrans_targetItems").toList(), targetItems);
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
    QVERIFY2(firstVisibleIndex >= 0, QTest::toString(firstVisibleIndex));

    // verify all items moved to the correct final positions
    int itemCount = findItems<QQuickItem>(contentItem, "wrapper").count();
    for (int i=firstVisibleIndex; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->y(), i*20.0 + itemsOffsetAfterMove);
        name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
    delete testObject;
}

void tst_QQuickListView::moveTransitions_data()
{
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<qreal>("contentY");
    QTest::addColumn<qreal>("itemsOffsetAfterMove");
    QTest::addColumn<int>("moveFrom");
    QTest::addColumn<int>("moveTo");
    QTest::addColumn<int>("moveCount");
    QTest::addColumn<ListRange>("expectedDisplacedIndexes");

    // when removing from above the visible, all items shift down depending on how many
    // items have been removed from above the visible
    QTest::newRow("move from above view, outside visible items, move 1") << 30 << 4*20.0 << 20.0
            << 1 << 10 << 1 << ListRange(11, 15+4);
    QTest::newRow("move from above view, outside visible items, move 1 (first item)") << 30 << 4*20.0 << 20.0
            << 0 << 10 << 1 << ListRange(11, 15+4);
    QTest::newRow("move from above view, outside visible items, move multiple") << 30 << 4*20.0 << 2*20.0
            << 1 << 10 << 2 << ListRange(12, 15+4);
    QTest::newRow("move from above view, outside visible items, move multiple (first item)") << 30 << 4*20.0 << 3*20.0
            << 0 << 10 << 3 << ListRange(13, 15+4);
    QTest::newRow("move from above view, mix of visible/non-visible") << 30 << 4*20.0 << 3*20.0
            << 1 << 10 << 5 << ListRange(6, 14) + ListRange(15, 15+4);
    QTest::newRow("move from above view, mix of visible/non-visible (move first)") << 30 << 4*20.0 << 4*20.0
            << 0 << 10 << 5 << ListRange(5, 14) + ListRange(15, 15+4);

    QTest::newRow("move within view, move 1 down") << 30 << 0.0 << 0.0
            << 1 << 10 << 1 << ListRange(2, 10);
    QTest::newRow("move within view, move 1 down, move first item") << 30 << 0.0 << 0.0
            << 0 << 10 << 1 << ListRange(1, 10);
    QTest::newRow("move within view, move 1 down, move first item, contentY not 0") << 30 << 4*20.0 << 0.0
            << 0+4 << 10+4 << 1 << ListRange(1+4, 10+4);
    QTest::newRow("move within view, move 1 down, to last item") << 30 << 0.0 << 0.0
            << 10 << 15 << 1 << ListRange(11, 15);
    QTest::newRow("move within view, move first->last") << 30 << 0.0 << 0.0
            << 0 << 15 << 1 << ListRange(1, 15);

    QTest::newRow("move within view, move multiple down") << 30 << 0.0 << 0.0
            << 1 << 10 << 3 << ListRange(4, 12);
    QTest::newRow("move within view, move multiple down, move first item") << 30 << 0.0 << 0.0
            << 0 << 10 << 3 << ListRange(3, 12);
    QTest::newRow("move within view, move multiple down, move first item, contentY not 0") << 30 << 4*20.0 << 0.0
            << 0+4 << 10+4 << 3 << ListRange(3+4, 12+4);
    QTest::newRow("move within view, move multiple down, displace last item") << 30 << 0.0 << 0.0
            << 5 << 13 << 3 << ListRange(8, 15);
    QTest::newRow("move within view, move multiple down, move first->last") << 30 << 0.0 << 0.0
            << 0 << 13 << 3 << ListRange(3, 15);

    QTest::newRow("move within view, move 1 up") << 30 << 0.0 << 0.0
            << 10 << 1 << 1 << ListRange(1, 9);
    QTest::newRow("move within view, move 1 up, move to first index") << 30 << 0.0 << 0.0
            << 10 << 0 << 1 << ListRange(0, 9);
    QTest::newRow("move within view, move 1 up, move to first index, contentY not 0") << 30 << 4*20.0 << 0.0
            << 10+4 << 0+4 << 1 << ListRange(0+4, 9+4);
    QTest::newRow("move within view, move 1 up, move to first index, contentY not on item border") << 30 << 4*20.0 - 10 << 0.0
            << 10+4 << 0+4 << 1 << ListRange(0+4, 9+4);
    QTest::newRow("move within view, move 1 up, move last item") << 30 << 0.0 << 0.0
            << 15 << 10 << 1 << ListRange(10, 14);
    QTest::newRow("move within view, move 1 up, move last->first") << 30 << 0.0 << 0.0
            << 15 << 0 << 1 << ListRange(0, 14);

    QTest::newRow("move within view, move multiple up") << 30 << 0.0 << 0.0
            << 10 << 1 << 3 << ListRange(1, 9);
    QTest::newRow("move within view, move multiple up, move to first index") << 30 << 0.0 << 0.0
            << 10 << 0 << 3 << ListRange(0, 9);
    QTest::newRow("move within view, move multiple up, move to first index, contentY not 0") << 30 << 4*20.0 << 0.0
            << 10+4 << 0+4 << 3 << ListRange(0+4, 9+4);
    QTest::newRow("move within view, move multiple up, move last item") << 30 << 0.0 << 0.0
            << 13 << 5 << 3 << ListRange(5, 12);
    QTest::newRow("move within view, move multiple up, move last->first") << 30 << 0.0 << 0.0
            << 13 << 0 << 3 << ListRange(0, 12);

    QTest::newRow("move from below view, move 1 up, move to top") << 30 << 0.0 << 0.0
            << 20 << 0 << 1 << ListRange(0, 15);
    QTest::newRow("move from below view, move 1 up, move to top, contentY not 0") << 30 << 4*20.0 << 0.0
            << 25 << 4 << 1 << ListRange(0+4, 15+4);
    QTest::newRow("move from below view, move multiple up, move to top") << 30 << 0.0 << 0.0
            << 20 << 0 << 3 << ListRange(0, 15);
    QTest::newRow("move from below view, move multiple up, move to top, contentY not 0") << 30 << 4*20.0 << 0.0
            << 25 << 4 << 3 << ListRange(0+4, 15+4);

    QTest::newRow("move from below view, move 1 up, move to bottom") << 30 << 0.0 << 0.0
            << 20 << 15 << 1 << ListRange(15, 15);
    QTest::newRow("move from below view, move 1 up, move to bottom, contentY not 0") << 30 << 4*20.0 << 0.0
            << 25 << 15+4 << 1 << ListRange(15+4, 15+4);
    QTest::newRow("move from below view, move multiple up, move to bottom") << 30 << 0.0 << 0.0
            << 20 << 15 << 3 << ListRange(15, 15);
    QTest::newRow("move from below view, move multiple up, move to bottom, contentY not 0") << 30 << 4*20.0 << 0.0
            << 25 << 15+4 << 3 << ListRange(15+4, 15+4);
}

void tst_QQuickListView::removeTransitions()
{
    QFETCH(int, initialItemCount);
    QFETCH(bool, shouldAnimateTargets);
    QFETCH(qreal, contentY);
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
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("model_targetItems_transitionTo", &model_targetItems_transitionTo);
    ctxt->setContextProperty("model_displacedItems_transitionVia", &model_displacedItems_transitionVia);
    ctxt->setContextProperty("targetItems_transitionTo", targetItems_transitionTo);
    ctxt->setContextProperty("displacedItems_transitionVia", displacedItems_transitionVia);
    ctxt->setContextProperty("testObject", testObject);
    window->setSource(testFileUrl("removeTransitions.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    if (contentY != 0) {
        listview->setContentY(contentY);
        QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
    }

    QList<QPair<QString,QString> > expectedDisplacedValues = expectedDisplacedIndexes.getModelDataValues(model);

    // only target items that are visible should be animated
    QList<QPair<QString, QString> > expectedTargetData;
    QList<int> targetIndexes;
    if (shouldAnimateTargets) {
        for (int i=removalIndex; i<removalIndex+removalCount; i++) {
            if (i >= contentY / 20 && i < (contentY + listview->height()) / 20) {
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
    QTRY_COMPARE(model.count(), listview->count());

    if (shouldAnimateTargets) {
        QTRY_COMPARE(listview->property("targetTransitionsDone").toInt(), expectedTargetData.count());
        QTRY_COMPARE(listview->property("displaceTransitionsDone").toInt(),
                     expectedDisplacedIndexes.isValid() ? expectedDisplacedIndexes.count() : 0);

        // check the target and displaced items were animated
        model_targetItems_transitionTo.matchAgainst(expectedTargetData, "wasn't animated to target 'to' pos", "shouldn't have been animated to target 'to' pos");
        model_displacedItems_transitionVia.matchAgainst(expectedDisplacedValues, "wasn't animated with displaced anim", "shouldn't have been animated with displaced anim");

        // check attached properties
        QCOMPARE(listview->property("targetTrans_items").toMap(), expectedTargets);
        matchIndexLists(listview->property("targetTrans_targetIndexes").toList(), targetIndexes);
        matchItemLists(listview->property("targetTrans_targetItems").toList(), targetItems);
        if (expectedDisplacedIndexes.isValid()) {
            // adjust expectedDisplacedIndexes to their final values after the move
            QList<int> displacedIndexes = adjustIndexesForRemoveDisplaced(expectedDisplacedIndexes.indexes, removalIndex, removalCount);
            matchItemsAndIndexes(listview->property("displacedTrans_items").toMap(), model, displacedIndexes);
            matchIndexLists(listview->property("displacedTrans_targetIndexes").toList(), targetIndexes);
            matchItemLists(listview->property("displacedTrans_targetItems").toList(), targetItems);
        }
    } else {
        QTRY_COMPARE(model_targetItems_transitionTo.count(), 0);
        QTRY_COMPARE(model_displacedItems_transitionVia.count(), 0);
    }

    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    int firstVisibleIndex = -1;
    int itemCount = items.count();

    for (int i=0; i<items.count(); i++) {
        QQmlExpression e(qmlContext(items[i]), items[i], "index");
        int index = e.evaluate().toInt();
        if (firstVisibleIndex < 0 && items[i]->y() >= contentY)
            firstVisibleIndex = index;
        if (index < 0)
            itemCount--;    // exclude deleted items
    }
    QVERIFY2(firstVisibleIndex >= 0, QTest::toString(firstVisibleIndex));

    // verify all items moved to the correct final positions
    for (int i=firstVisibleIndex; i < model.count() && i < itemCount; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QCOMPARE(item->x(), 0.0);
        QCOMPARE(item->y(), contentY + (i-firstVisibleIndex) * 20.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
    delete testObject;
}

void tst_QQuickListView::removeTransitions_data()
{
    QTest::addColumn<int>("initialItemCount");
    QTest::addColumn<qreal>("contentY");
    QTest::addColumn<bool>("shouldAnimateTargets");
    QTest::addColumn<int>("removalIndex");
    QTest::addColumn<int>("removalCount");
    QTest::addColumn<ListRange>("expectedDisplacedIndexes");

    // All items that are visible following the remove operation should be animated.
    // Remove targets that are outside of the view should not be animated.

    QTest::newRow("remove 1 before start")
            << 30 << 20.0 * 3 << false
            << 2 << 1 << ListRange();
    QTest::newRow("remove multiple, all before start")
            << 30 << 20.0 * 3 << false
            << 0 << 3 << ListRange();
    QTest::newRow("remove mix of before and after start")
            << 30 << 20.0 * 3 << true
            << 2 << 3 << ListRange(5, 20);  // 5-20 are visible after the remove

    QTest::newRow("remove 1 from start")
            << 30 << 0.0 << true
            << 0 << 1 << ListRange(1, 16);  // 1-16 are visible after the remove
    QTest::newRow("remove multiple from start")
            << 30 << 0.0 << true
            << 0 << 3 << ListRange(3, 18);  // 3-18 are visible after the remove
    QTest::newRow("remove 1 from start, content y not 0")
            << 30 << 20.0 * 2 << true  // first visible is index 2, so translate the displaced indexes by 2
            << 2 << 1 << ListRange(1 + 2, 16 + 2);
    QTest::newRow("remove multiple from start, content y not 0")
            << 30 << 20.0 * 2 << true    // first visible is index 2
            << 2 << 3 << ListRange(3 + 2, 18 + 2);

    QTest::newRow("remove 1 from middle")
            << 30 << 0.0 << true
            << 5 << 1 << ListRange(6, 16);
    QTest::newRow("remove multiple from middle")
            << 30 << 0.0 << true
            << 5 << 3 << ListRange(8, 18);


    QTest::newRow("remove 1 from bottom")
            << 30 << 0.0 << true
            << 15 << 1 << ListRange(16, 16);

    // remove 15, 16, 17
    // 15 will animate as the target item, 16 & 17 won't be animated since they are outside
    // the view, and 18 will be animated as the displaced item to replace the last item
    QTest::newRow("remove multiple from bottom")
            << 30 << 0.0 << true
            << 15 << 3 << ListRange(18, 18);

    QTest::newRow("remove 1 from bottom, content y not 0")
            << 30 << 20.0 * 2 << true
            << 15 + 2 << 1 << ListRange(16 + 2, 16 + 2);
    QTest::newRow("remove multiple from bottom, content y not 0")
            << 30 << 20.0 * 2 << true
            << 15 + 2 << 3 << ListRange(18 + 2, 18 + 2);


    QTest::newRow("remove 1 after end")
            << 30 << 0.0 << false
            << 17 << 1 << ListRange();
    QTest::newRow("remove multiple after end")
            << 30 << 0.0 << false
            << 17 << 3 << ListRange();
}

void tst_QQuickListView::displacedTransitions()
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
    TestObject *testObject = new TestObject(window);
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testObject", testObject);
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
    QVERIFY(QTest::qWaitForWindowExposed(window));


    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QList<QPair<QString,QString> > expectedDisplacedValues = expectedDisplacedIndexes.getModelDataValues(model);
    listview->setProperty("displaceTransitionsDone", false);

    switch (change.type) {
        case ListChange::Inserted:
        {
            QList<QPair<QString, QString> > targetItemData;
            for (int i=change.index; i<change.index + change.count; ++i)
                targetItemData << qMakePair(QString("new item %1").arg(i), QString::number(i));
            model.insertItems(change.index, targetItemData);
            QTRY_COMPARE(model.count(), listview->count());
            break;
        }
        case ListChange::Removed:
            model.removeItems(change.index, change.count);
            QTRY_COMPARE(model.count(), listview->count());
            break;
        case ListChange::Moved:
            model.moveItems(change.index, change.to, change.count);
            QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
            break;
        case ListChange::SetCurrent:
        case ListChange::SetContentY:
        case ListChange::Polish:
            break;
    }
    listview->forceLayout();

    QVariantList resultTargetIndexes = listview->property("displacedTargetIndexes").toList();
    QVariantList resultTargetItems = listview->property("displacedTargetItems").toList();

    if ((useDisplaced && displacedEnabled)
            || (useAddDisplaced && addDisplacedEnabled)
            || (useMoveDisplaced && moveDisplacedEnabled)
            || (useRemoveDisplaced && removeDisplacedEnabled)) {
        QTRY_VERIFY(listview->property("displaceTransitionsDone").toBool());

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
        QCOMPARE(item->x(), 0.0);
        QCOMPARE(item->y(), i * 20.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

void tst_QQuickListView::displacedTransitions_data()
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
            << ListChange::insert(0, 1) << ListRange(0, 15);

    QTest::newRow("just displaced")
            << true << true
            << false << false
            << false << false
            << false << false
            << ListChange::insert(0, 1) << ListRange(0, 15);

    QTest::newRow("just displaced (not enabled)")
            << true << false
            << false << false
            << false << false
            << false << false
            << ListChange::insert(0, 1) << ListRange(0, 15);

    QTest::newRow("displaced + addDisplaced")
            << true << true
            << true << true
            << false << false
            << false << false
            << ListChange::insert(0, 1) << ListRange(0, 15);

    QTest::newRow("displaced + addDisplaced (not enabled)")
            << true << true
            << true << false
            << false << false
            << false << false
            << ListChange::insert(0, 1) << ListRange(0, 15);

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
            << ListChange::remove(0, 1) << ListRange(1, 16);

    QTest::newRow("displaced + removeDisplaced (not enabled)")
            << true << true
            << false << false
            << false << false
            << true << false
            << ListChange::remove(0, 1) << ListRange(1, 16);


    QTest::newRow("displaced + add, should use generic displaced for a remove")
            << true << true
            << true << true
            << false << false
            << true << false
            << ListChange::remove(0, 1) << ListRange(1, 16);
}

void tst_QQuickListView::multipleTransitions()
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
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testObject", testObject);
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

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    if (contentY != 0) {
        listview->setContentY(contentY);
        QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
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
                QTRY_COMPARE(model.count(), listview->count());
                if (i == changes.count() - 1) {
                    QTRY_VERIFY(!listview->property("runningAddTargets").toBool());
                    QTRY_VERIFY(!listview->property("runningAddDisplaced").toBool());
                } else {
                    QTest::qWait(timeBetweenActions);
                }
                break;
            }
            case ListChange::Removed:
                model.removeItems(changes[i].index, changes[i].count);
                QTRY_COMPARE(model.count(), listview->count());
                if (i == changes.count() - 1) {
                    QTRY_VERIFY(!listview->property("runningRemoveTargets").toBool());
                    QTRY_VERIFY(!listview->property("runningRemoveDisplaced").toBool());
                } else {
                    QTest::qWait(timeBetweenActions);
                }
                break;
            case ListChange::Moved:
                model.moveItems(changes[i].index, changes[i].to, changes[i].count);
                QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
                if (i == changes.count() - 1) {
                    QTRY_VERIFY(!listview->property("runningMoveTargets").toBool());
                    QTRY_VERIFY(!listview->property("runningMoveDisplaced").toBool());
                } else {
                    QTest::qWait(timeBetweenActions);
                }
                break;
            case ListChange::SetCurrent:
                listview->setCurrentIndex(changes[i].index);
                break;
            case ListChange::SetContentY:
                listview->setContentY(changes[i].pos);
                QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);
                break;
            case ListChange::Polish:
                break;
        }
    }
    listview->forceLayout();
    QTest::qWait(200);
    QCOMPARE(listview->count(), model.count());

    // verify all items moved to the correct final positions
    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    for (int i=0; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->x(), 0.0);
        QTRY_COMPARE(item->y(), i*20.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
    delete testObject;
}

void tst_QQuickListView::multipleTransitions_data()
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
            << ListChange::setContentY(80.0)
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

void tst_QQuickListView::multipleDisplaced()
{
    // multiple move() operations should only restart displace transitions for items that
    // moved from previously set positions, and not those that have moved from their current
    // item positions (which may e.g. still be changing from easing bounces in the last transition)

    QaimModel model;
    for (int i = 0; i < 30; i++)
        model.addItem("Original item" + QString::number(i), "");

    QQuickView *window = getView();
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testObject", new TestObject(window));
    window->setSource(testFileUrl("multipleDisplaced.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);
    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    model.moveItems(12, 8, 1);
    QTest::qWait(window->rootObject()->property("duration").toInt() / 2);
    model.moveItems(8, 3, 1);
    QTRY_VERIFY(listview->property("displaceTransitionsDone").toBool());

    QVariantMap transitionsStarted = listview->property("displaceTransitionsStarted").toMap();
    foreach (const QString &name, transitionsStarted.keys()) {
        QVERIFY2(transitionsStarted[name] == 1,
                 QTest::toString(QString("%1 was displaced %2 times").arg(name).arg(transitionsStarted[name].toInt())));
    }

    // verify all items moved to the correct final positions
    QList<QQuickItem*> items = findItems<QQuickItem>(contentItem, "wrapper");
    for (int i=0; i < model.count() && i < items.count(); ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        QVERIFY2(item, QTest::toString(QString("Item %1 not found").arg(i)));
        QTRY_COMPARE(item->x(), 0.0);
        QTRY_COMPARE(item->y(), i*20.0);
        QQuickText *name = findItem<QQuickText>(contentItem, "textName", i);
        QVERIFY(name != 0);
        QTRY_COMPARE(name->text(), model.name(i));
    }

    releaseView(window);
}

QList<int> tst_QQuickListView::toIntList(const QVariantList &list)
{
    QList<int> ret;
    bool ok = true;
    for (int i=0; i<list.count(); i++) {
        ret << list[i].toInt(&ok);
        if (!ok)
            qWarning() << "tst_QQuickListView::toIntList(): not a number:" << list[i];
    }

    return ret;
}

void tst_QQuickListView::matchIndexLists(const QVariantList &indexLists, const QList<int> &expectedIndexes)
{
    for (int i=0; i<indexLists.count(); i++) {
        QSet<int> current = indexLists[i].value<QList<int> >().toSet();
        if (current != expectedIndexes.toSet())
            qDebug() << "Cannot match actual targets" << current << "with expected" << expectedIndexes;
        QCOMPARE(current, expectedIndexes.toSet());
    }
}

void tst_QQuickListView::matchItemsAndIndexes(const QVariantMap &items, const QaimModel &model, const QList<int> &expectedIndexes)
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

void tst_QQuickListView::matchItemLists(const QVariantList &itemLists, const QList<QQuickItem *> &expectedItems)
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

void tst_QQuickListView::flickBeyondBounds()
{
    QScopedPointer<QQuickView> window(createView());
    QQuickViewTestUtil::moveMouseAway(window.data());

    window->setSource(testFileUrl("flickBeyondBoundsBug.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));


    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QTRY_VERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    // Flick view up beyond bounds
    flick(window.data(), QPoint(10, 10), QPoint(10, -2000), 180);
#ifdef Q_OS_MAC
    QSKIP("Disabled due to flaky behavior on CI system (QTBUG-44493)");
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper").count(), 0);
#endif

    // We're really testing that we don't get stuck in a loop,
    // but also confirm items positioned correctly.
    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper").count(), 2);
    for (int i = 0; i < 2; ++i) {
        QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", i);
        if (!item) qWarning() << "Item" << i << "not found";
        QTRY_VERIFY(item);
        QTRY_COMPARE(item->y(), qreal(i*45));
    }
}

void tst_QQuickListView::destroyItemOnCreation()
{
    QaimModel model;
    QScopedPointer<QQuickView> window(createView());
    window->rootContext()->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("destroyItemOnCreation.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));


    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QCOMPARE(window->rootObject()->property("createdIndex").toInt(), -1);
    model.addItem("new item", "");
    QTRY_COMPARE(window->rootObject()->property("createdIndex").toInt(), 0);

    QTRY_COMPARE(findItems<QQuickItem>(contentItem, "wrapper").count(), 0);
    QCOMPARE(model.count(), 0);
}

void tst_QQuickListView::parentBinding()
{
    QScopedPointer<QQuickView> window(createView());
    QQmlTestMessageHandler messageHandler;

    window->setSource(testFileUrl("parentBinding.qml"));
    window->show();
    QTest::qWaitForWindowExposed(window.data());

    QQuickListView *listview = qobject_cast<QQuickListView*>(window->rootObject());
    QVERIFY(listview != 0);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);
    QTRY_COMPARE(QQuickItemPrivate::get(listview)->polishScheduled, false);

    QQuickItem *item = findItem<QQuickItem>(contentItem, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->width(), listview->width());
    QCOMPARE(item->height(), listview->height()/12);

    // there should be no transient binding error
    QVERIFY2(messageHandler.messages().isEmpty(), qPrintable(messageHandler.messageString()));
}

void tst_QQuickListView::defaultHighlightMoveDuration()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; ListView {}", QUrl::fromLocalFile(""));

    QObject *obj = component.create();
    QVERIFY(obj);

    QCOMPARE(obj->property("highlightMoveDuration").toInt(), -1);
}

void tst_QQuickListView::accessEmptyCurrentItem_QTBUG_30227()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("emptymodel.qml"));

    QQuickListView *listview = window->rootObject()->findChild<QQuickListView*>();
    QTRY_VERIFY(listview != 0);
    listview->forceLayout();

    QMetaObject::invokeMethod(window->rootObject(), "remove");
    QVERIFY(window->rootObject()->property("isCurrentItemNull").toBool());

    QMetaObject::invokeMethod(window->rootObject(), "add");
    QVERIFY(!window->rootObject()->property("isCurrentItemNull").toBool());
}

void tst_QQuickListView::delayedChanges_QTBUG_30555()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("delayedChanges.qml"));

    QQuickListView *listview = window->rootObject()->findChild<QQuickListView*>();
    QTRY_VERIFY(listview != 0);

    QCOMPARE(listview->count(), 10);

    //Takes two just like in the bug report
    QMetaObject::invokeMethod(window->rootObject(), "takeTwo");
    QTRY_COMPARE(listview->count(), 8);

    QMetaObject::invokeMethod(window->rootObject(), "takeTwo_sync");
    QCOMPARE(listview->count(), 6);
}

void tst_QQuickListView::outsideViewportChangeNotAffectingView()
{
    QScopedPointer<QQuickView> window(createView());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->setSource(testFileUrl("outsideViewportChangeNotAffectingView.qml"));

    QQuickListView *listview = window->rootObject()->findChild<QQuickListView*>();
    QTRY_VERIFY(listview != 0);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    listview->setContentY(1250);

    QTRY_COMPARE(listview->indexAt(0, listview->contentY()), 4);
    QTRY_COMPARE(listview->itemAt(0, listview->contentY())->y(), 1200.);

    QMetaObject::invokeMethod(window->rootObject(), "resizeThirdItem", Q_ARG(QVariant, 290));
    QTRY_COMPARE(listview->indexAt(0, listview->contentY()), 4);
    QTRY_COMPARE(listview->itemAt(0, listview->contentY())->y(), 1200.);

    QMetaObject::invokeMethod(window->rootObject(), "resizeThirdItem", Q_ARG(QVariant, 300));
    QTRY_COMPARE(listview->indexAt(0, listview->contentY()), 4);
    QTRY_COMPARE(listview->itemAt(0, listview->contentY())->y(), 1200.);

    QMetaObject::invokeMethod(window->rootObject(), "resizeThirdItem", Q_ARG(QVariant, 310));
    QTRY_COMPARE(listview->indexAt(0, listview->contentY()), 4);
    QTRY_COMPARE(listview->itemAt(0, listview->contentY())->y(), 1200.);

    QMetaObject::invokeMethod(window->rootObject(), "resizeThirdItem", Q_ARG(QVariant, 400));
    QTRY_COMPARE(listview->indexAt(0, listview->contentY()), 4);
    QTRY_COMPARE(listview->itemAt(0, listview->contentY())->y(), 1200.);
}

void tst_QQuickListView::testProxyModelChangedAfterMove()
{
    QScopedPointer<QQuickView> window(createView());
    QQuickViewTestUtil::moveMouseAway(window.data());
    window->setSource(testFileUrl("proxytest.qml"));

    QQuickListView *listview = window->rootObject()->findChild<QQuickListView*>();
    QTRY_VERIFY(listview != 0);

    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QTRY_COMPARE(listview->count(), 3);
}

void tst_QQuickListView::typedModel()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("typedModel.qml"));

    QScopedPointer<QObject> object(component.create());

    QQuickListView *listview = qobject_cast<QQuickListView *>(object.data());
    QVERIFY(listview);

    QCOMPARE(listview->count(), 6);

    QQmlListModel *listModel = 0;

    listview->setModel(QVariant::fromValue(listModel));
    QCOMPARE(listview->count(), 0);
}

void tst_QQuickListView::displayMargin()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("displayMargin.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = window->rootObject()->findChild<QQuickListView*>();
    QVERIFY(listview != 0);

    QQuickItem *content = listview->contentItem();
    QVERIFY(content != 0);

    QQuickItem *item0;
    QQuickItem *item14;

    QVERIFY(item0 = findItem<QQuickItem>(content, "delegate", 0));
    QCOMPARE(delegateVisible(item0), true);

    // the 14th item should be within the end margin
    QVERIFY(item14 = findItem<QQuickItem>(content, "delegate", 13));
    QCOMPARE(delegateVisible(item14), true);

    // the 15th item should be outside the end margin
    QVERIFY(findItem<QQuickItem>(content, "delegate", 14) == 0);

    // the first delegate should still be within the begin margin
    listview->positionViewAtIndex(3, QQuickListView::Beginning);
    QCOMPARE(delegateVisible(item0), true);

    // the first delegate should now be outside the begin margin
    listview->positionViewAtIndex(4, QQuickListView::Beginning);
    QCOMPARE(delegateVisible(item0), false);
}

void tst_QQuickListView::negativeDisplayMargin()
{
    QQuickItem *item;
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("negativeDisplayMargin.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickItem *listview = window->rootObject();
    QQuickListView *innerList = findItem<QQuickListView>(window->rootObject(), "innerList");
    QVERIFY(innerList != 0);

    QTRY_COMPARE(innerList->property("createdItems").toInt(), 11);
    QCOMPARE(innerList->property("destroyedItem").toInt(), 0);

    QQuickItem *content = innerList->contentItem();
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
    QTRY_VERIFY(item = findItem<QQuickItem>(content, "delegate", 4));
    QTRY_COMPARE(delegateVisible(item), true);
}

void tst_QQuickListView::highlightItemGeometryChanges()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("HighlightResize.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = qobject_cast<QQuickListView *>(window->rootObject());
    QVERIFY(listview);

    QCOMPARE(listview->count(), 5);

    for (int i = 0; i < listview->count(); ++i) {
        listview->setCurrentIndex(i);
        QTRY_COMPARE(listview->highlightItem()->width(), qreal(100 + i * 20));
        QTRY_COMPARE(listview->highlightItem()->height(), qreal(100 + i * 10));
    }
}

void tst_QQuickListView::QTBUG_36481()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("headerCrash.qml"));

    // just testing that we don't crash when creating
    QScopedPointer<QObject> object(component.create());
}

void tst_QQuickListView::QTBUG_35920()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("qtbug35920.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = qobject_cast<QQuickListView *>(window->rootObject());
    QVERIFY(listview);

    QTest::mousePress(window.data(), Qt::LeftButton, 0, QPoint(10,0));
    for (int i = 0; i < 100; ++i) {
        QTest::mouseMove(window.data(), QPoint(10,i));
        if (listview->isMoving()) {
            // do not fixup() the position while in movement to avoid flicker
            const qreal contentY = listview->contentY();
            listview->setPreferredHighlightBegin(i);
            QCOMPARE(listview->contentY(), contentY);
            listview->resetPreferredHighlightBegin();
            QCOMPARE(listview->contentY(), contentY);

            listview->setPreferredHighlightEnd(i+10);
            QCOMPARE(listview->contentY(), contentY);
            listview->resetPreferredHighlightEnd();
            QCOMPARE(listview->contentY(), contentY);
        }
    }
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(10,100));
}

Q_DECLARE_METATYPE(Qt::Orientation)

void tst_QQuickListView::stickyPositioning()
{
    QFETCH(QString, fileName);

    QFETCH(Qt::Orientation, orientation);
    QFETCH(Qt::LayoutDirection, layoutDirection);
    QFETCH(QQuickItemView::VerticalLayoutDirection, verticalLayoutDirection);

    QFETCH(int, positionIndex);
    QFETCH(QQuickItemView::PositionMode, positionMode);
    QFETCH(QList<QPointF>, movement);

    QFETCH(QPointF, headerPos);
    QFETCH(QPointF, footerPos);

    QQuickView *window = getView();

    QaimModel model;
    for (int i = 0; i < 20; i++)
        model.addItem(QString::number(i), QString::number(i/10));

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);
    ctxt->setContextProperty("testOrientation", orientation);
    ctxt->setContextProperty("testLayoutDirection", layoutDirection);
    ctxt->setContextProperty("testVerticalLayoutDirection", verticalLayoutDirection);

    window->setSource(testFileUrl(fileName));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QVERIFY(listview);

    QQuickItem *contentItem = listview->contentItem();
    QVERIFY(contentItem);

    listview->positionViewAtIndex(positionIndex, positionMode);

    foreach (const QPointF &offset, movement) {
        listview->setContentX(listview->contentX() + offset.x());
        listview->setContentY(listview->contentY() + offset.y());
    }

    if (listview->header()) {
        QQuickItem *headerItem = listview->headerItem();
        QVERIFY(headerItem);
        QPointF actualPos = listview->mapFromItem(contentItem, headerItem->position());
        QCOMPARE(actualPos, headerPos);
    }

    if (listview->footer()) {
        QQuickItem *footerItem = listview->footerItem();
        QVERIFY(footerItem);
        QPointF actualPos = listview->mapFromItem(contentItem, footerItem->position());
        QCOMPARE(actualPos, footerPos);
    }

    releaseView(window);
}

void tst_QQuickListView::stickyPositioning_data()
{
    qRegisterMetaType<Qt::Orientation>();
    qRegisterMetaType<Qt::LayoutDirection>();
    qRegisterMetaType<QQuickItemView::VerticalLayoutDirection>();
    qRegisterMetaType<QQuickItemView::PositionMode>();

    QTest::addColumn<QString>("fileName");

    QTest::addColumn<Qt::Orientation>("orientation");
    QTest::addColumn<Qt::LayoutDirection>("layoutDirection");
    QTest::addColumn<QQuickItemView::VerticalLayoutDirection>("verticalLayoutDirection");

    QTest::addColumn<int>("positionIndex");
    QTest::addColumn<QQuickItemView::PositionMode>("positionMode");
    QTest::addColumn<QList<QPointF> >("movement");

    QTest::addColumn<QPointF>("headerPos");
    QTest::addColumn<QPointF>("footerPos");

    // header at the top
    QTest::newRow("top header") << "stickyPositioning-header.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 0 << QQuickItemView::Beginning << QList<QPointF>()
            << QPointF(0,0) << QPointF();

    QTest::newRow("top header: 1/2 up") << "stickyPositioning-header.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 1 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,-5))
            << QPointF(0,-5) << QPointF();

    QTest::newRow("top header: up") << "stickyPositioning-header.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 2 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,-15))
            << QPointF(0,0) << QPointF();

    QTest::newRow("top header: 1/2 down") << "stickyPositioning-header.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 3 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,-15) << QPointF(0,5))
            << QPointF(0,-5) << QPointF();

    QTest::newRow("top header: down") << "stickyPositioning-header.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 4 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,-15) << QPointF(0,10))
            << QPointF(0,-10) << QPointF();


    // footer at the top
    QTest::newRow("top footer") << "stickyPositioning-footer.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::BottomToTop
            << 19 << QQuickItemView::End << QList<QPointF>()
            << QPointF() << QPointF(0,0);

    QTest::newRow("top footer: 1/2 up") << "stickyPositioning-footer.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::BottomToTop
            << 18 << QQuickItemView::End << (QList<QPointF>() << QPointF(0,-5))
            << QPointF() << QPointF(0,-5);

    QTest::newRow("top footer: up") << "stickyPositioning-footer.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::BottomToTop
            << 17 << QQuickItemView::End << (QList<QPointF>() << QPointF(0,-15))
            << QPointF() << QPointF(0,0);

    QTest::newRow("top footer: 1/2 down") << "stickyPositioning-footer.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::BottomToTop
            << 16 << QQuickItemView::End << (QList<QPointF>() << QPointF(0,-15) << QPointF(0,5))
            << QPointF() << QPointF(0,-5);

    QTest::newRow("top footer: down") << "stickyPositioning-footer.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::BottomToTop
            << 15 << QQuickItemView::End << (QList<QPointF>() << QPointF(0,-15) << QPointF(0,10))
            << QPointF() << QPointF(0,-10);


    // header at the bottom
    QTest::newRow("bottom header") << "stickyPositioning-header.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::BottomToTop
            << 0 << QQuickItemView::Beginning << QList<QPointF>()
            << QPointF(0,90) << QPointF();

    QTest::newRow("bottom header: 1/2 down") << "stickyPositioning-header.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::BottomToTop
            << 1 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,5))
            << QPointF(0,95) << QPointF();

    QTest::newRow("bottom header: down") << "stickyPositioning-header.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::BottomToTop
            << 2 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,15))
            << QPointF(0,90) << QPointF();

    QTest::newRow("bottom header: 1/2 up") << "stickyPositioning-header.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::BottomToTop
            << 3 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,15) << QPointF(0,-5))
            << QPointF(0,95) << QPointF();

    QTest::newRow("bottom header: up") << "stickyPositioning-header.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::BottomToTop
            << 4 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,15) << QPointF(0,-10))
            << QPointF(0,100) << QPointF();


    // footer at the bottom
    QTest::newRow("bottom footer") << "stickyPositioning-footer.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 19 << QQuickItemView::End << QList<QPointF>()
            << QPointF() << QPointF(0,90);

    QTest::newRow("bottom footer: 1/2 down") << "stickyPositioning-footer.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 18 << QQuickItemView::End << (QList<QPointF>() << QPointF(0,5))
            << QPointF() << QPointF(0,95);

    QTest::newRow("bottom footer: down") << "stickyPositioning-footer.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 17 << QQuickItemView::End << (QList<QPointF>() << QPointF(0,15))
            << QPointF() << QPointF(0,90);

    QTest::newRow("bottom footer: 1/2 up") << "stickyPositioning-footer.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 16 << QQuickItemView::End << (QList<QPointF>() << QPointF(0,15) << QPointF(0,-5))
            << QPointF() << QPointF(0,95);

    QTest::newRow("bottom footer: up") << "stickyPositioning-footer.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 15 << QQuickItemView::End << (QList<QPointF>() << QPointF(0,15) << QPointF(0,-10))
            << QPointF() << QPointF(0,100);


    // header at the top (& footer at the bottom)
    QTest::newRow("top header & bottom footer") << "stickyPositioning-both.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 0 << QQuickItemView::Beginning << QList<QPointF>()
            << QPointF(0,0) << QPointF(0,100);

    QTest::newRow("top header & bottom footer: 1/2 up") << "stickyPositioning-both.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 1 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,-5))
            << QPointF(0,-5) << QPointF(0,95);

    QTest::newRow("top header & bottom footer: up") << "stickyPositioning-both.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 2 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,-15))
            << QPointF(0,0) << QPointF(0,100);

    QTest::newRow("top header & bottom footer: 1/2 down") << "stickyPositioning-both.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 3 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,-15) << QPointF(0,5))
            << QPointF(0,-5) << QPointF(0,95);

    QTest::newRow("top header & bottom footer: down") << "stickyPositioning-both.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 4 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,-15) << QPointF(0,10))
            << QPointF(0,-10) << QPointF(0,90);


    // footer at the bottom (& header at the top)
    QTest::newRow("bottom footer & top header") << "stickyPositioning-both.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 1 << QQuickItemView::Beginning << QList<QPointF>()
            << QPointF(0,-10) << QPointF(0,90);

    QTest::newRow("bottom footer & top header: 1/2 down") << "stickyPositioning-both.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 1 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,5))
            << QPointF(0,-10) << QPointF(0,90);

    QTest::newRow("bottom footer & top header: down") << "stickyPositioning-both.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 2 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,15))
            << QPointF(0,-10) << QPointF(0,90);

    QTest::newRow("bottom footer & top header: 1/2 up") << "stickyPositioning-both.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 3 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,15) << QPointF(0,-5))
            << QPointF(0,-5) << QPointF(0,95);

    QTest::newRow("bottom footer & top header: up") << "stickyPositioning-both.qml"
            << Qt::Vertical << Qt::LeftToRight << QQuickListView::TopToBottom
            << 4 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(0,15) << QPointF(0,-10))
            << QPointF(0,0) << QPointF(0,100);

    // header on the left
    QTest::newRow("left header") << "stickyPositioning-header.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 0 << QQuickItemView::Beginning << QList<QPointF>()
            << QPointF(0,0) << QPointF();

    QTest::newRow("left header: 1/2 left") << "stickyPositioning-header.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 1 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(-5,0))
            << QPointF(-5,0) << QPointF();

    QTest::newRow("left header: left") << "stickyPositioning-header.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 2 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(-15,0))
            << QPointF(0,0) << QPointF();

    QTest::newRow("left header: 1/2 right") << "stickyPositioning-header.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 3 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(-15,0) << QPointF(5,0))
            << QPointF(-5,0) << QPointF();

    QTest::newRow("left header: right") << "stickyPositioning-header.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 4 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(-15,0) << QPointF(10,0))
            << QPointF(-10,0) << QPointF();


    // footer on the left
    QTest::newRow("left footer") << "stickyPositioning-footer.qml"
            << Qt::Horizontal << Qt::RightToLeft << QQuickListView::TopToBottom
            << 19 << QQuickItemView::End << QList<QPointF>()
            << QPointF() << QPointF(0,0);

    QTest::newRow("left footer: 1/2 left") << "stickyPositioning-footer.qml"
            << Qt::Horizontal << Qt::RightToLeft << QQuickListView::TopToBottom
            << 18 << QQuickItemView::End << (QList<QPointF>() << QPointF(-5,0))
            << QPointF() << QPointF(-5,0);

    QTest::newRow("left footer: left") << "stickyPositioning-footer.qml"
            << Qt::Horizontal << Qt::RightToLeft << QQuickListView::TopToBottom
            << 17 << QQuickItemView::End << (QList<QPointF>() << QPointF(-15,0))
            << QPointF() << QPointF(0,0);

    QTest::newRow("left footer: 1/2 right") << "stickyPositioning-footer.qml"
            << Qt::Horizontal << Qt::RightToLeft << QQuickListView::TopToBottom
            << 16 << QQuickItemView::End << (QList<QPointF>() << QPointF(-15,0) << QPointF(5,0))
            << QPointF() << QPointF(-5,0);

    QTest::newRow("left footer: right") << "stickyPositioning-footer.qml"
            << Qt::Horizontal << Qt::RightToLeft << QQuickListView::TopToBottom
            << 15 << QQuickItemView::End << (QList<QPointF>() << QPointF(-15,0) << QPointF(10,0))
            << QPointF() << QPointF(-10,0);


    // header on the right
    QTest::newRow("right header") << "stickyPositioning-header.qml"
            << Qt::Horizontal << Qt::RightToLeft << QQuickListView::TopToBottom
            << 0 << QQuickItemView::Beginning << QList<QPointF>()
            << QPointF(90,0) << QPointF();

    QTest::newRow("right header: 1/2 right") << "stickyPositioning-header.qml"
            << Qt::Horizontal << Qt::RightToLeft << QQuickListView::TopToBottom
            << 1 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(5,0))
            << QPointF(95,0) << QPointF();

    QTest::newRow("right header: right") << "stickyPositioning-header.qml"
            << Qt::Horizontal << Qt::RightToLeft << QQuickListView::TopToBottom
            << 2 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(15,0))
            << QPointF(90,0) << QPointF();

    QTest::newRow("right header: 1/2 left") << "stickyPositioning-header.qml"
            << Qt::Horizontal << Qt::RightToLeft << QQuickListView::TopToBottom
            << 3 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(15,0) << QPointF(-5,0))
            << QPointF(95,0) << QPointF();

    QTest::newRow("right header: left") << "stickyPositioning-header.qml"
            << Qt::Horizontal << Qt::RightToLeft << QQuickListView::TopToBottom
            << 4 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(15,0) << QPointF(-10,0))
            << QPointF(100,0) << QPointF();


    // footer on the right
    QTest::newRow("right footer") << "stickyPositioning-footer.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 19 << QQuickItemView::End << QList<QPointF>()
            << QPointF() << QPointF(90,0);

    QTest::newRow("right footer: 1/2 right") << "stickyPositioning-footer.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 18 << QQuickItemView::End << (QList<QPointF>() << QPointF(5,0))
            << QPointF() << QPointF(95,0);

    QTest::newRow("right footer: right") << "stickyPositioning-footer.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 17 << QQuickItemView::End << (QList<QPointF>() << QPointF(15,0))
            << QPointF() << QPointF(90,0);

    QTest::newRow("right footer: 1/2 left") << "stickyPositioning-footer.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 16 << QQuickItemView::End << (QList<QPointF>() << QPointF(15,0) << QPointF(-5,0))
            << QPointF() << QPointF(95,0);

    QTest::newRow("right footer: left") << "stickyPositioning-footer.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 15 << QQuickItemView::End << (QList<QPointF>() << QPointF(15,0) << QPointF(-10,0))
            << QPointF() << QPointF(100,0);


    // header on the left (& footer on the right)
    QTest::newRow("left header & right footer") << "stickyPositioning-both.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 0 << QQuickItemView::Beginning << QList<QPointF>()
            << QPointF(0,0) << QPointF(100,0);

    QTest::newRow("left header & right footer: 1/2 left") << "stickyPositioning-both.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 1 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(-5,0))
            << QPointF(-5,0) << QPointF(95,0);

    QTest::newRow("left header & right footer: left") << "stickyPositioning-both.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 2 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(-15,0))
            << QPointF(0,0) << QPointF(100,0);

    QTest::newRow("left header & right footer: 1/2 right") << "stickyPositioning-both.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 3 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(-15,0) << QPointF(5,0))
            << QPointF(-5,0) << QPointF(95,0);

    QTest::newRow("left header & right footer: right") << "stickyPositioning-both.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 4 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(-15,0) << QPointF(10,0))
            << QPointF(-10,0) << QPointF(90,0);


    // footer on the right (& header on the left)
    QTest::newRow("right footer & left header") << "stickyPositioning-both.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 1 << QQuickItemView::Beginning << QList<QPointF>()
            << QPointF(-10,0) << QPointF(90,0);

    QTest::newRow("right footer & left header: 1/2 right") << "stickyPositioning-both.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 1 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(5,0))
            << QPointF(-10,0) << QPointF(90,0);

    QTest::newRow("right footer & left header: right") << "stickyPositioning-both.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 2 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(15,0))
            << QPointF(-10,0) << QPointF(90,0);

    QTest::newRow("right footer & left header: 1/2 left") << "stickyPositioning-both.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 3 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(15,0) << QPointF(-5,0))
            << QPointF(-5,0) << QPointF(95,0);

    QTest::newRow("right footer & left header: left") << "stickyPositioning-both.qml"
            << Qt::Horizontal << Qt::LeftToRight << QQuickListView::TopToBottom
            << 4 << QQuickItemView::Beginning << (QList<QPointF>() << QPointF(15,0) << QPointF(-10,0))
            << QPointF(0,0) << QPointF(100,0);
}

void tst_QQuickListView::roundingErrors()
{
    QFETCH(bool, pixelAligned);

    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("roundingErrors.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = qobject_cast<QQuickListView *>(window->rootObject());
    QVERIFY(listview);
    listview->setPixelAligned(pixelAligned);
    listview->positionViewAtIndex(20, QQuickListView::Beginning);

    QQuickItem *content = listview->contentItem();
    QVERIFY(content);

    const QPoint viewPos(150, 36);
    const QPointF contentPos = content->mapFromItem(listview, viewPos);

    QPointer<QQuickItem> item = listview->itemAt(contentPos.x(), contentPos.y());
    QVERIFY(item);

    // QTBUG-37339: drag an item and verify that it doesn't
    // get prematurely released due to rounding errors
    QTest::mousePress(window.data(), Qt::LeftButton, 0, viewPos);
    for (int i = 0; i < 150; i += 5) {
        QTest::mouseMove(window.data(), viewPos - QPoint(i, 0));
        QVERIFY(item);
    }
    QTest::mouseRelease(window.data(), Qt::LeftButton, 0, QPoint(0, 36));

    // maintain position relative to the right edge
    listview->setLayoutDirection(Qt::RightToLeft);
    const qreal contentX = listview->contentX();
    listview->setContentX(contentX + 0.2);
    QCOMPARE(listview->contentX(), pixelAligned ? qRound(contentX + 0.2) : contentX + 0.2);
    listview->setWidth(listview->width() - 0.2);
    QCOMPARE(listview->contentX(), pixelAligned ? qRound(contentX + 0.2) : contentX + 0.2);

    // maintain position relative to the bottom edge
    listview->setOrientation(QQuickListView::Vertical);
    listview->setVerticalLayoutDirection(QQuickListView::BottomToTop);
    const qreal contentY = listview->contentY();
    listview->setContentY(contentY + 0.2);
    QCOMPARE(listview->contentY(), pixelAligned ? qRound(contentY + 0.2) : contentY + 0.2);
    listview->setHeight(listview->height() - 0.2);
    QCOMPARE(listview->contentY(), pixelAligned ? qRound(contentY + 0.2) : contentY + 0.2);
}

void tst_QQuickListView::roundingErrors_data()
{
    QTest::addColumn<bool>("pixelAligned");
    QTest::newRow("pixelAligned=true") << true;
    QTest::newRow("pixelAligned=false") << false;
}

void tst_QQuickListView::QTBUG_38209()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("simplelistview.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = qobject_cast<QQuickListView *>(window->rootObject());
    QVERIFY(listview);

    // simulate mouse flick
    flick(window.data(), QPoint(200, 200), QPoint(200, 50), 100);
    QTRY_VERIFY(!listview->isMoving());
    qreal contentY = listview->contentY();

    // flick down
    listview->flick(0, 1000);

    // ensure we move more than just a couple pixels
    QTRY_VERIFY(contentY - listview->contentY() > qreal(100.0));
}

void tst_QQuickListView::programmaticFlickAtBounds()
{
    QSKIP("Disabled due to false negatives (QTBUG-41228)");

    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("simplelistview.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = qobject_cast<QQuickListView *>(window->rootObject());
    QVERIFY(listview);
    QSignalSpy spy(listview, SIGNAL(contentYChanged()));

    // flick down
    listview->flick(0, 1000);

    // verify that there is movement beyond bounds
    QVERIFY(spy.wait(100));

    // reset, and test with StopAtBounds
    listview->cancelFlick();
    listview->returnToBounds();
    QTRY_COMPARE(listview->contentY(), qreal(0.0));
    listview->setBoundsBehavior(QQuickFlickable::StopAtBounds);

    // flick down
    listview->flick(0, 1000);

    // verify that there is no movement beyond bounds
    QVERIFY(!spy.wait(100));
}

void tst_QQuickListView::programmaticFlickAtBounds2()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("programmaticFlickAtBounds2.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = qobject_cast<QQuickListView *>(window->rootObject());
    QVERIFY(listview);

    // move exactly one item
    qreal velocity = -qSqrt(2 * 50 * listview->flickDeceleration());

    // flick down
    listview->flick(0, velocity);

    QTRY_COMPARE(listview->contentY(), qreal(50.0));

    // flick down
    listview->flick(0, velocity);

    QTRY_COMPARE(listview->contentY(), qreal(100.0));
}

void tst_QQuickListView::layoutChange()
{
    RandomSortModel *model = new RandomSortModel;
    QSortFilterProxyModel *sortModel = new QSortFilterProxyModel;
    sortModel->setSourceModel(model);
    sortModel->setSortRole(Qt::UserRole);
    sortModel->setDynamicSortFilter(true);
    sortModel->sort(0);

    QScopedPointer<QQuickView> window(createView());
    window->rootContext()->setContextProperty("testModel", QVariant::fromValue(sortModel));
    window->setSource(testFileUrl("layoutChangeSort.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = window->rootObject()->findChild<QQuickListView *>("listView");
    QVERIFY(listview);

    for (int iter = 0; iter < 100; iter++) {
        for (int i = 0; i < sortModel->rowCount(); ++i) {
            QQuickItem *delegateItem = listview->itemAt(10, 10 + 2 * i * 20 + 20); // item + group
            QVERIFY(delegateItem);
            QQuickItem *delegateText = delegateItem->findChild<QQuickItem *>("delegateText");
            QVERIFY(delegateText);

            QCOMPARE(delegateText->property("text").toString(),
                     sortModel->index(i, 0, QModelIndex()).data().toString());
        }

        model->randomize();
        listview->forceLayout();
        QTest::qWait(5); // give view a chance to update
    }
}

void tst_QQuickListView::QTBUG_39492_data()
{
    QStandardItemModel *sourceModel = new QStandardItemModel(this);
    for (int i = 0; i < 5; ++i) {
        QStandardItem *item = new QStandardItem(QString::number(i));
        for (int j = 0; j < 5; ++j) {
            QStandardItem *subItem = new QStandardItem(QString("%1-%2").arg(i).arg(j));
            item->appendRow(subItem);
        }
        sourceModel->appendRow(item);
    }

    QSortFilterProxyModel *sortModel = new QSortFilterProxyModel(this);
    sortModel->setSourceModel(sourceModel);

    QTest::addColumn<QSortFilterProxyModel*>("model");
    QTest::addColumn<QPersistentModelIndex>("rootIndex");

    QTest::newRow("invalid rootIndex")
        << sortModel
        << QPersistentModelIndex();

    QTest::newRow("rootIndex 1")
        << sortModel
        << QPersistentModelIndex(sortModel->index(1, 0));

    QTest::newRow("rootIndex 3")
        << sortModel
        << QPersistentModelIndex(sortModel->index(3, 0));

    const QModelIndex rootIndex2 = sortModel->index(2, 0);
    QTest::newRow("rootIndex 2-1")
        << sortModel
        << QPersistentModelIndex(sortModel->index(1, 0, rootIndex2));
}

void tst_QQuickListView::QTBUG_39492()
{
    QFETCH(QSortFilterProxyModel*, model);
    QFETCH(QPersistentModelIndex, rootIndex);

    QQuickView *window = getView();
    window->rootContext()->setContextProperty("testModel", QVariant::fromValue(model));
    window->setSource(testFileUrl("qtbug39492.qml"));

    QQuickListView *listview = window->rootObject()->findChild<QQuickListView *>("listView");
    QVERIFY(listview);

    QQmlDelegateModel *delegateModel = window->rootObject()->findChild<QQmlDelegateModel *>("delegateModel");
    QVERIFY(delegateModel);

    delegateModel->setRootIndex(QVariant::fromValue(QModelIndex(rootIndex)));
    model->sort(0, Qt::AscendingOrder);
    listview->forceLayout();

    for (int i = 0; i < model->rowCount(rootIndex); ++i) {
        QQuickItem *delegateItem = listview->itemAt(10, 10 + i * 20);
        QVERIFY(delegateItem);
        QQuickItem *delegateText = delegateItem->findChild<QQuickItem *>("delegateText");
        QVERIFY(delegateText);
        QCOMPARE(delegateText->property("text").toString(),
                 model->index(i, 0, rootIndex).data().toString());
    }

    model->sort(0, Qt::DescendingOrder);
    listview->forceLayout();

    for (int i = 0; i < model->rowCount(rootIndex); ++i) {
        QQuickItem *delegateItem = listview->itemAt(10, 10 + i * 20);
        QVERIFY(delegateItem);
        QQuickItem *delegateText = delegateItem->findChild<QQuickItem *>("delegateText");
        QVERIFY(delegateText);
        QCOMPARE(delegateText->property("text").toString(),
                 model->index(i, 0, rootIndex).data().toString());
    }

    releaseView(window);
}

void tst_QQuickListView::jsArrayChange()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.4; ListView {}", QUrl());

    QScopedPointer<QQuickListView> view(qobject_cast<QQuickListView *>(component.create()));
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

static bool compareObjectModel(QQuickListView *listview, QQmlObjectModel *model)
{
    if (listview->count() != model->count())
        return false;
    for (int i = 0; i < listview->count(); ++i) {
        listview->setCurrentIndex(i);
        if (listview->currentItem() != model->get(i))
            return false;
    }
    return true;
}

void tst_QQuickListView::objectModel()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("objectmodel.qml"));

    QQuickListView *listview = qobject_cast<QQuickListView *>(component.create());
    QVERIFY(listview);

    QQmlObjectModel *model = listview->model().value<QQmlObjectModel *>();
    QVERIFY(model);

    listview->setCurrentIndex(0);
    QVERIFY(listview->currentItem());
    QCOMPARE(listview->currentItem()->property("color").toString(), QColor("red").name());

    listview->setCurrentIndex(1);
    QVERIFY(listview->currentItem());
    QCOMPARE(listview->currentItem()->property("color").toString(), QColor("green").name());

    listview->setCurrentIndex(2);
    QVERIFY(listview->currentItem());
    QCOMPARE(listview->currentItem()->property("color").toString(), QColor("blue").name());

    QQuickItem *item0 = new QQuickItem(listview);
    item0->setSize(QSizeF(20, 20));
    model->append(item0);
    QCOMPARE(model->count(), 4);
    QVERIFY(compareObjectModel(listview, model));

    QQuickItem *item1 = new QQuickItem(listview);
    item1->setSize(QSizeF(20, 20));
    model->insert(0, item1);
    QCOMPARE(model->count(), 5);
    QVERIFY(compareObjectModel(listview, model));

    model->move(1, 2, 3);
    QVERIFY(compareObjectModel(listview, model));

    model->remove(2, 2);
    QCOMPARE(model->count(), 3);
    QVERIFY(compareObjectModel(listview, model));

    model->clear();
    QCOMPARE(model->count(), 0);
    QCOMPARE(listview->count(), 0);

    delete listview;
}

void tst_QQuickListView::contentHeightWithDelayRemove_data()
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
            << qreal(-3 * 100.0);

    QTest::newRow("clear with delayRemove")
            << true
            << QByteArray("takeAll")
            << -5
            << qreal(-5 * 100.0);
}

void tst_QQuickListView::contentHeightWithDelayRemove()
{
    QFETCH(bool, useDelayRemove);
    QFETCH(QByteArray, removeFunc);
    QFETCH(int, countDelta);
    QFETCH(qreal, contentHeightDelta);

    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("contentHeightWithDelayRemove.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = window->rootObject()->findChild<QQuickListView*>();
    QTRY_VERIFY(listview != 0);

    const int initialCount(listview->count());
    const int eventualCount(initialCount + countDelta);

    const qreal initialContentHeight(listview->contentHeight());
    const int eventualContentHeight(qRound(initialContentHeight + contentHeightDelta));

    listview->setProperty("useDelayRemove", useDelayRemove);
    QMetaObject::invokeMethod(window->rootObject(), removeFunc.constData());
    QTest::qWait(50);
    QCOMPARE(listview->count(), eventualCount);

    if (useDelayRemove) {
        QCOMPARE(qRound(listview->contentHeight()), qRound(initialContentHeight));
        QTRY_COMPARE(qRound(listview->contentHeight()), eventualContentHeight);
    } else {
        QCOMPARE(qRound(listview->contentHeight()), eventualContentHeight);
    }
}

void tst_QQuickListView::QTBUG_48044_currentItemNotVisibleAfterTransition()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("qtbug48044.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = window->rootObject()->findChild<QQuickListView*>();
    QTRY_VERIFY(listview != 0);

    // Expand 2nd header
    listview->setProperty("transitionsDone", QVariant(false));
    QTest::mouseClick(window.data(), Qt::LeftButton, Qt::NoModifier, QPoint(window->width() / 2, 75));
    QTRY_VERIFY(listview->property("transitionsDone").toBool());

    // Flick listview to the bottom
    flick(window.data(), QPoint(window->width() / 2, 400), QPoint(window->width() / 2, 0), 100);
    QTRY_VERIFY(!listview->isMoving());

    // Expand 3rd header
    listview->setProperty("transitionsDone", QVariant(false));
    QTest::mouseClick(window.data(), Qt::LeftButton, Qt::NoModifier, QPoint(window->width() / 2, window->height() - 25));
    QTRY_VERIFY(listview->property("transitionsDone").toBool());

    // Check current item is what we expect
    QCOMPARE(listview->currentIndex(), 2);
    QQuickItem *currentItem = listview->currentItem();
    QVERIFY(currentItem);
    QVERIFY(currentItem->isVisible());

    // This is the actual test
    QQuickItemPrivate *currentPriv = QQuickItemPrivate::get(currentItem);
    QVERIFY(!currentPriv->culled);
}

void tst_QQuickListView::keyNavigationEnabled()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("simplelistview.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));

    QQuickListView *listView = qobject_cast<QQuickListView *>(window->rootObject());
    QVERIFY(listView);
    QCOMPARE(listView->isKeyNavigationEnabled(), true);

    listView->setFocus(true);
    QVERIFY(listView->hasActiveFocus());

    listView->setHighlightMoveDuration(0);

    // If keyNavigationEnabled is not explicitly set to true, respect the original behavior
    // of disabling both mouse and keyboard interaction.
    QSignalSpy enabledSpy(listView, SIGNAL(keyNavigationEnabledChanged()));
    listView->setInteractive(false);
    QCOMPARE(enabledSpy.count(), 1);
    QCOMPARE(listView->isKeyNavigationEnabled(), false);

    flick(window.data(), QPoint(200, 200), QPoint(200, 50), 100);
    QVERIFY(!listView->isMoving());
    QCOMPARE(listView->contentY(), 0.0);
    QCOMPARE(listView->currentIndex(), 0);

    QTest::keyClick(window.data(), Qt::Key_Down);
    QCOMPARE(listView->currentIndex(), 0);

    // Check that isKeyNavigationEnabled implicitly follows the value of interactive.
    listView->setInteractive(true);
    QCOMPARE(enabledSpy.count(), 2);
    QCOMPARE(listView->isKeyNavigationEnabled(), true);

    // Change it back again for the next check.
    listView->setInteractive(false);
    QCOMPARE(enabledSpy.count(), 3);
    QCOMPARE(listView->isKeyNavigationEnabled(), false);

    // Setting keyNavigationEnabled to true shouldn't enable mouse interaction.
    listView->setKeyNavigationEnabled(true);
    QCOMPARE(enabledSpy.count(), 4);
    flick(window.data(), QPoint(200, 200), QPoint(200, 50), 100);
    QVERIFY(!listView->isMoving());
    QCOMPARE(listView->contentY(), 0.0);
    QCOMPARE(listView->currentIndex(), 0);

    // Should now work.
    QTest::keyClick(window.data(), Qt::Key_Down);
    QCOMPARE(listView->currentIndex(), 1);
    // contentY won't change for one index change in a view this high.

    // Changing interactive now shouldn't result in keyNavigationEnabled changing,
    // since we broke the "binding".
    listView->setInteractive(true);
    QCOMPARE(enabledSpy.count(), 4);

    // Keyboard interaction shouldn't work now.
    listView->setKeyNavigationEnabled(false);
    QTest::keyClick(window.data(), Qt::Key_Down);
    QCOMPARE(listView->currentIndex(), 1);
}

void tst_QQuickListView::QTBUG_48870_fastModelUpdates()
{
    StressTestModel model;

    QScopedPointer<QQuickView> window(createView());
    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(testFileUrl("qtbug48870.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = findItem<QQuickListView>(window->rootObject(), "list");
    QTRY_VERIFY(listview != 0);

    QQuickItemViewPrivate *priv = QQuickItemViewPrivate::get(listview);
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

// infinite loop in overlay header positioning due to undesired rounding in QQuickFlickablePrivate::fixup()
void tst_QQuickListView::QTBUG_50105()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("qtbug50105.qml"));

    QScopedPointer<QQuickWindow> window(qobject_cast<QQuickWindow *>(component.create()));
    QVERIFY(window.data());
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));
}

void tst_QQuickListView::QTBUG_50097_stickyHeader_positionViewAtIndex()
{
    QScopedPointer<QQuickView> window(createView());
    window->setSource(testFileUrl("qtbug50097.qml"));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.data()));

    QQuickListView *listview = qobject_cast<QQuickListView*>(window->rootObject());
    QVERIFY(listview != 0);
    QTRY_COMPARE(listview->contentY(), -100.0); // the header size, since the header is overlaid
    listview->setProperty("currentPage", 2);
    QTRY_COMPARE(listview->contentY(), 400.0); // a full page of items down, sans the original negative header position
    listview->setProperty("currentPage", 1);
    QTRY_COMPARE(listview->contentY(), -100.0); // back to the same position: header visible, items not under the header.
}

void tst_QQuickListView::itemFiltered()
{
    QStringListModel model(QStringList() << "one" << "two" << "three" << "four" << "five" << "six");
    QSortFilterProxyModel proxy1;
    proxy1.setSourceModel(&model);
    proxy1.setSortRole(Qt::DisplayRole);
    proxy1.setDynamicSortFilter(true);
    proxy1.sort(0);

    QSortFilterProxyModel proxy2;
    proxy2.setSourceModel(&proxy1);
    proxy2.setFilterRole(Qt::DisplayRole);
    proxy2.setFilterRegExp("^[^ ]*$");
    proxy2.setDynamicSortFilter(true);

    QScopedPointer<QQuickView> window(createView());
    window->engine()->rootContext()->setContextProperty("_model", &proxy2);
    QQmlComponent component(window->engine());
    component.setData("import QtQuick 2.4; ListView { "
                      "anchors.fill: parent; model: _model; delegate: Text { width: parent.width;"
                      "text: model.display; } }",
                      QUrl());
    window->setContent(QUrl(), &component, component.create());

    window->show();
    QTest::qWaitForWindowExposed(window.data());

    // this should not crash
    model.setData(model.index(2), QStringLiteral("modified three"), Qt::DisplayRole);
}

QTEST_MAIN(tst_QQuickListView)

#include "tst_qquicklistview.moc"
