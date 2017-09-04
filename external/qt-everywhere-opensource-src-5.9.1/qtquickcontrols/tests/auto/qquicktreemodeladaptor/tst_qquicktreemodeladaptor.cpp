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
#include <QtCore>
#include <qquicktreemodeladaptor_p.h>
#include "../shared/testmodel.h"

class tst_QQuickTreeModelAdaptor : public QObject
{
    Q_OBJECT

public:
    void compareData(int row, QQuickTreeModelAdaptor1 &tma, const QModelIndex &idx, TestModel &model, bool expanded = false);
    void compareModels(QQuickTreeModelAdaptor1 &tma, TestModel &model);
    void expandAndTest(const QModelIndex &idx, QQuickTreeModelAdaptor1 &tma, bool expandable, int expectedRowCountDifference);
    void collapseAndTest(const QModelIndex &idx, QQuickTreeModelAdaptor1 &tma, bool expandable, int expectedRowCountDifference);

private slots:
    void initTestCase();
    void cleanup();

    void setModel();
    void modelDestroyed();
    void modelReset();

    void rootIndex();

    void dataAccess();
    void dataChange();
    void groupedDataChange();

    void expandAndCollapse_data();
    void expandAndCollapse();
    void expandAndCollapse2ndLevel();

    void layoutChange();

    void removeRows_data();
    void removeRows();

    void removeRowsChildrenAndParent();
    void removeChildrenMoveParent();
    void removeChildrenRelayoutParent();

    void insertRows_data();
    void insertRows();

    void moveRows_data();
    void moveRows();
    void reparentOnSameRow();

    void selectionForRowRange();

    void hasChildrenEmit();
};

void tst_QQuickTreeModelAdaptor::initTestCase()
{
}

void tst_QQuickTreeModelAdaptor::cleanup()
{
}

void tst_QQuickTreeModelAdaptor::compareData(int row, QQuickTreeModelAdaptor1 &tma, const QModelIndex &modelIdx, TestModel &model, bool expanded)
{
    const QModelIndex &tmaIdx = tma.index(row);
    const int indexDepth = model.level(modelIdx) - model.level(tma.rootIndex()) - 1;
    QCOMPARE(tma.mapToModel(tmaIdx), modelIdx);
    QCOMPARE(tma.data(tmaIdx, Qt::DisplayRole).toString(), model.displayData(modelIdx));
    QCOMPARE(tma.data(tmaIdx, QQuickTreeModelAdaptor1::DepthRole).toInt(), indexDepth);
    QCOMPARE(tma.data(tmaIdx, QQuickTreeModelAdaptor1::ExpandedRole).toBool(), expanded);
    QCOMPARE(tma.data(tmaIdx, QQuickTreeModelAdaptor1::HasChildrenRole).toBool(), model.hasChildren(modelIdx));
}

void tst_QQuickTreeModelAdaptor::expandAndTest(const QModelIndex &idx, QQuickTreeModelAdaptor1 &tma, bool expandable,
                                                  int expectedRowCountDifference)
{
    QSignalSpy rowsAboutToBeInsertedSpy(&tma, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)));
    QSignalSpy rowsInsertedSpy(&tma, SIGNAL(rowsInserted(QModelIndex,int,int)));
    QSignalSpy dataChangedSpy(&tma, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));

    int oldRowCount = tma.rowCount();
    tma.expand(idx);
    QCOMPARE(tma.isExpanded(idx), expandable);

    const QModelIndex &tmaIdx = tma.index(tma.itemIndex(idx));
    QCOMPARE(tma.data(tmaIdx, QQuickTreeModelAdaptor1::ExpandedRole).toBool(), expandable);

    if (expandable && expectedRowCountDifference != 0) {
        // Rows were added below the parent
        QCOMPARE(tma.rowCount(), oldRowCount + expectedRowCountDifference);
        QCOMPARE(rowsAboutToBeInsertedSpy.count(), rowsInsertedSpy.count());
        QVERIFY(rowsInsertedSpy.count() > 0);
        if (rowsInsertedSpy.count() == 1) {
            const QVariantList &rowsAboutToBeInsertedArgs = rowsAboutToBeInsertedSpy.takeFirst();
            const QVariantList &rowsInsertedArgs = rowsInsertedSpy.takeFirst();
            for (int i = 0; i < rowsInsertedArgs.count(); i++)
                QCOMPARE(rowsAboutToBeInsertedArgs.at(i), rowsInsertedArgs.at(i));
            QCOMPARE(rowsInsertedArgs.at(0).toModelIndex(), QModelIndex());
            QCOMPARE(rowsInsertedArgs.at(1).toInt(), tma.itemIndex(idx) + 1);
            QCOMPARE(rowsInsertedArgs.at(2).toInt(), tma.itemIndex(idx) + expectedRowCountDifference);
        }

        // Data changed for the parent's ExpandedRole (value checked above)
        QCOMPARE(dataChangedSpy.count(), 1);
        const QVariantList &dataChangedArgs = dataChangedSpy.first();
        QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tmaIdx);
        QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tmaIdx);
        QCOMPARE(dataChangedArgs.at(2).value<QVector<int> >(), QVector<int>(1, QQuickTreeModelAdaptor1::ExpandedRole));
    } else {
        QCOMPARE(tma.rowCount(), oldRowCount);
        QCOMPARE(rowsAboutToBeInsertedSpy.count(), 0);
        QCOMPARE(rowsInsertedSpy.count(), 0);
    }
}

void tst_QQuickTreeModelAdaptor::collapseAndTest(const QModelIndex &idx, QQuickTreeModelAdaptor1 &tma,
                                                 bool expandable, int expectedRowCountDifference)
{
    QSignalSpy rowsAboutToBeRemovedSpy(&tma, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)));
    QSignalSpy rowsRemovedSpy(&tma, SIGNAL(rowsRemoved(QModelIndex,int,int)));
    QSignalSpy dataChangedSpy(&tma, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));

    int oldRowCount = tma.rowCount();
    tma.collapse(idx);
    QVERIFY(!tma.isExpanded(idx));

    const QModelIndex &tmaIdx = tma.index(tma.itemIndex(idx));
    if (tmaIdx.isValid())
        QCOMPARE(tma.data(tmaIdx, QQuickTreeModelAdaptor1::ExpandedRole).toBool(), false);

    if (expandable && expectedRowCountDifference != 0) {
        // Rows were removed below the parent
        QCOMPARE(tma.rowCount(), oldRowCount - expectedRowCountDifference);
        QCOMPARE(rowsAboutToBeRemovedSpy.count(), 1);
        QCOMPARE(rowsRemovedSpy.count(), 1);
        const QVariantList &rowsAboutToBeRemovedArgs = rowsAboutToBeRemovedSpy.takeFirst();
        const QVariantList &rowsRemovedArgs = rowsRemovedSpy.takeFirst();
        for (int i = 0; i < rowsRemovedArgs.count(); i++)
            QCOMPARE(rowsAboutToBeRemovedArgs.at(i), rowsRemovedArgs.at(i));
        QCOMPARE(rowsRemovedArgs.at(0).toModelIndex(), QModelIndex());
        QCOMPARE(rowsRemovedArgs.at(1).toInt(), tma.itemIndex(idx) + 1);
        QCOMPARE(rowsRemovedArgs.at(2).toInt(), tma.itemIndex(idx) + expectedRowCountDifference);

        // Data changed for the parent's ExpandedRole (value checked above)
        QCOMPARE(dataChangedSpy.count(), 1);
        const QVariantList &dataChangedArgs = dataChangedSpy.first();
        QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tmaIdx);
        QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tmaIdx);
        QCOMPARE(dataChangedArgs.at(2).value<QVector<int> >(), QVector<int>(1, QQuickTreeModelAdaptor1::ExpandedRole));
    } else {
        QCOMPARE(tma.rowCount(), oldRowCount);
        QCOMPARE(rowsAboutToBeRemovedSpy.count(), 0);
        QCOMPARE(rowsRemovedSpy.count(), 0);
    }
}

void tst_QQuickTreeModelAdaptor::compareModels(QQuickTreeModelAdaptor1 &tma, TestModel &model)
{
    QModelIndex parent = tma.rootIndex();
    QStack<QModelIndex> parents;
    QModelIndex idx = model.index(0, 0, parent);
    int modelVisibleRows = model.rowCount(parent);
    for (int i = 0; i < tma.rowCount(); i++) {
        bool expanded = tma.isExpanded(i);
        compareData(i, tma, idx, model, expanded);
        if (expanded) {
            parents.push(parent);
            parent = idx;
            modelVisibleRows += model.rowCount(parent);
            idx = model.index(0, 0, parent);
        } else {
            while (idx.row() == model.rowCount(parent) - 1) {
                if (parents.isEmpty())
                    break;
                idx = parent;
                parent = parents.pop();
            }
            idx = model.index(idx.row() + 1, 0, parent);
        }
    }
    QCOMPARE(tma.rowCount(), modelVisibleRows);

    // Duplicates the model inspection above, but provides extra tests
    QVERIFY(tma.testConsistency());
}

void tst_QQuickTreeModelAdaptor::setModel()
{
    TestModel model(5, 1);
    QQuickTreeModelAdaptor1 tma;

    QSignalSpy modelChangedSpy(&tma, SIGNAL(modelChanged(QAbstractItemModel*)));
    tma.setModel(&model);
    QCOMPARE(modelChangedSpy.count(), 1);
    QCOMPARE(tma.model(), &model);

    // Set same model twice
    tma.setModel(&model);
    QCOMPARE(modelChangedSpy.count(), 1);

    modelChangedSpy.clear();
    tma.setModel(0);
    QCOMPARE(modelChangedSpy.count(), 1);
    QCOMPARE(tma.model(), static_cast<QAbstractItemModel *>(0));
}

void tst_QQuickTreeModelAdaptor::modelDestroyed()
{
    TestModel *model = new TestModel(5, 1);
    QQuickTreeModelAdaptor1 tma;

    QSignalSpy modelChangedSpy(&tma, SIGNAL(modelChanged(QAbstractItemModel*)));
    tma.setModel(model);
    QCOMPARE(modelChangedSpy.count(), 1);
    QCOMPARE(tma.model(), model);

    QModelIndex idx = model->index(0, 0);
    modelChangedSpy.clear();
    delete model;
    QCOMPARE(modelChangedSpy.count(), 1);
    QCOMPARE(tma.model(), (QAbstractItemModel *)0);
    tma.expand(idx); // No crash, all fine
}

void tst_QQuickTreeModelAdaptor::modelReset()
{
    TestModel model(5, 1);
    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    QSignalSpy modelAboutToBeResetSpy(&tma, SIGNAL(modelAboutToBeReset()));
    QSignalSpy modelResetSpy(&tma, SIGNAL(modelReset()));

    // Nothing expanded
    model.resetModel();
    QCOMPARE(modelAboutToBeResetSpy.count(), 1);
    QCOMPARE(modelResetSpy.count(), 1);
    QCOMPARE(tma.rowCount(), model.rowCount());
    compareModels(tma, model);

    // Expanded items should not be anymore
    tma.expand(model.index(0, 0));
    tma.expand(model.index(2, 0));
    tma.expand(model.index(2, 0, model.index(2, 0)));
    modelAboutToBeResetSpy.clear();
    modelResetSpy.clear();
    model.resetModel();
    QCOMPARE(modelAboutToBeResetSpy.count(), 1);
    QCOMPARE(modelResetSpy.count(), 1);
    QCOMPARE(tma.rowCount(), model.rowCount());
    compareModels(tma, model);
}

void tst_QQuickTreeModelAdaptor::rootIndex()
{
    TestModel model(5, 1);

    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    QVERIFY(!tma.rootIndex().isValid());
    compareModels(tma, model);

    QSignalSpy rootIndexSpy(&tma, SIGNAL(rootIndexChanged()));
    QModelIndex rootIndex = model.index(0, 0);
    tma.setRootIndex(rootIndex);
    QCOMPARE(tma.rootIndex(), rootIndex);
    QCOMPARE(rootIndexSpy.count(), 1);
    compareModels(tma, model);

    rootIndexSpy.clear();
    rootIndex = model.index(2, 2, tma.rootIndex());
    tma.setRootIndex(rootIndex);
    QCOMPARE(tma.rootIndex(), rootIndex);
    QCOMPARE(rootIndexSpy.count(), 1);
    compareModels(tma, model);

    // Expand 1st visible item, business as usual
    expandAndTest(model.index(0, 0, rootIndex), tma, true /*expandable*/, 5);
    // Expand non root item descendant item, nothing should happen
    expandAndTest(model.index(0, 0), tma, true /*expandable*/, 0);
    // Collapse 1st visible item, business as usual
    collapseAndTest(model.index(0, 0, rootIndex), tma, true /*expandable*/, 5);
    // Collapse non root item descendant item, nothing should happen
    collapseAndTest(model.index(0, 0), tma, true /*expandable*/, 0);
}

void tst_QQuickTreeModelAdaptor::dataAccess()
{
    TestModel model(5, 1);

    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    QCOMPARE(tma.rowCount(), model.rowCount());
    compareModels(tma, model);

    QModelIndex parentIdx = model.index(2, 0);
    QVERIFY(model.hasChildren(parentIdx));
    tma.expand(parentIdx);
    QVERIFY(tma.isExpanded(parentIdx));
    QCOMPARE(tma.rowCount(), model.rowCount() + model.rowCount(parentIdx));
    compareModels(tma, model);

    tma.collapse(parentIdx);
    QCOMPARE(tma.rowCount(), model.rowCount());
    compareModels(tma, model);
}

void tst_QQuickTreeModelAdaptor::dataChange()
{
    TestModel model(5, 1);

    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    QSignalSpy dataChangedSpy(&tma, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    const QModelIndex &idx = model.index(2, 0);
    model.setData(idx, QVariant(), Qt::DisplayRole);
    QCOMPARE(dataChangedSpy.count(), 1);
    const QVariantList &dataChangedArgs = dataChangedSpy.first();
    const QModelIndex &tmaIdx = tma.index(tma.itemIndex(idx));
    QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tmaIdx);
    QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tmaIdx);
    QCOMPARE(dataChangedArgs.at(2).value<QVector<int> >(), QVector<int>(1, Qt::DisplayRole));
    compareModels(tma, model);

    {
        // Non expanded children shouldn't emit any signal
        dataChangedSpy.clear();
        const QModelIndex &childIdx = model.index(4, 0, idx);
        model.setData(childIdx, QVariant(), Qt::DisplayRole);
        QCOMPARE(dataChangedSpy.count(), 0);
        compareModels(tma, model);

        // But expanded children should
        tma.expand(idx);
        QVERIFY(tma.isExpanded(idx));
        dataChangedSpy.clear(); // expand() emits dataChanged() with ExpandedRole
        model.setData(childIdx, QVariant(), Qt::DisplayRole);
        QCOMPARE(dataChangedSpy.count(), 1);
        const QVariantList &dataChangedArgs = dataChangedSpy.first();
        const QModelIndex &tmaIdx = tma.index(tma.itemIndex(childIdx));
        QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tmaIdx);
        QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tmaIdx);
        QCOMPARE(dataChangedArgs.at(2).value<QVector<int> >(), QVector<int>(1, Qt::DisplayRole));
        compareModels(tma, model);
    }
}

void tst_QQuickTreeModelAdaptor::groupedDataChange()
{
    TestModel model(10, 1);
    const QModelIndex &topLeftIdx = model.index(1, 0);
    const QModelIndex &bottomRightIdx = model.index(7, 0);

    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    QSignalSpy dataChangedSpy(&tma, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    const QVector<int> roles(1, Qt::DisplayRole);

    {
        // No expanded items
        model.groupedSetData(topLeftIdx, bottomRightIdx, roles);
        QCOMPARE(dataChangedSpy.count(), 1);
        compareModels(tma, model);

        const QModelIndex &tmaTLIdx = tma.index(tma.itemIndex(topLeftIdx));
        const QModelIndex &tmaBRIdx = tma.index(tma.itemIndex(bottomRightIdx));
        const QVariantList &dataChangedArgs = dataChangedSpy.first();
        QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tmaTLIdx);
        QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tmaBRIdx);
        QCOMPARE(dataChangedArgs.at(2).value<QVector<int> >(), roles);
    }

    // One item expanded in the group range
    const QModelIndex &expandedIdx = model.index(4, 0);
    tma.expand(expandedIdx);
    QVERIFY(tma.isExpanded(expandedIdx));

    for (int i = 0; i < 2; i++) {
        const QModelIndex &tmaTLIdx = tma.index(tma.itemIndex(topLeftIdx));
        const QModelIndex &tmaExpandedIdx = tma.index(tma.itemIndex(expandedIdx));
        const QModelIndex &tmaExpandedSiblingIdx = tma.index(tma.itemIndex(expandedIdx.sibling(expandedIdx.row() + 1, 0)));
        const QModelIndex &tmaBRIdx = tma.index(tma.itemIndex(bottomRightIdx));

        dataChangedSpy.clear(); // expand() sends a dataChaned() signal
        model.groupedSetData(topLeftIdx, bottomRightIdx, roles);
        QCOMPARE(dataChangedSpy.count(), 2);
        compareModels(tma, model);

        QVariantList dataChangedArgs = dataChangedSpy.takeFirst();
        QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tmaTLIdx);
        QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tmaExpandedIdx);
        QCOMPARE(dataChangedArgs.at(2).value<QVector<int> >(), roles);

        dataChangedArgs = dataChangedSpy.takeFirst();
        QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tmaExpandedSiblingIdx);
        QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tmaBRIdx);
        QCOMPARE(dataChangedArgs.at(2).value<QVector<int> >(), roles);

        // Further expanded descendants should not change grouping
        tma.expand(model.index(0, 0, expandedIdx));
        QVERIFY(tma.isExpanded(expandedIdx));
    }
    tma.collapse(model.index(0, 0, expandedIdx));

    // Let's expand one more and see what happens...
    const QModelIndex &otherExpandedIdx = model.index(6, 0);
    tma.expand(otherExpandedIdx);
    QVERIFY(tma.isExpanded(otherExpandedIdx));

    for (int i = 0; i < 3; i++) {
        const QModelIndex &tmaTLIdx = tma.index(tma.itemIndex(topLeftIdx));
        const QModelIndex &tmaExpandedIdx = tma.index(tma.itemIndex(expandedIdx));
        const QModelIndex &tmaExpandedSiblingIdx = tma.index(tma.itemIndex(expandedIdx.sibling(expandedIdx.row() + 1, 0)));
        const QModelIndex &tmaOtherExpandedIdx = tma.index(tma.itemIndex(otherExpandedIdx));
        const QModelIndex &tmaOtherExpandedSiblingIdx = tma.index(tma.itemIndex(otherExpandedIdx.sibling(otherExpandedIdx.row() + 1, 0)));
        const QModelIndex &tmaBRIdx = tma.index(tma.itemIndex(bottomRightIdx));

        dataChangedSpy.clear(); // expand() sends a dataChaned() signal
        model.groupedSetData(topLeftIdx, bottomRightIdx, roles);
        QCOMPARE(dataChangedSpy.count(), 3);
        compareModels(tma, model);

        QVariantList dataChangedArgs = dataChangedSpy.takeFirst();
        QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tmaTLIdx);
        QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tmaExpandedIdx);
        QCOMPARE(dataChangedArgs.at(2).value<QVector<int> >(), roles);

        dataChangedArgs = dataChangedSpy.takeFirst();
        QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tmaExpandedSiblingIdx);
        QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tmaOtherExpandedIdx);
        QCOMPARE(dataChangedArgs.at(2).value<QVector<int> >(), roles);

        dataChangedArgs = dataChangedSpy.takeFirst();
        QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tmaOtherExpandedSiblingIdx);
        QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tmaBRIdx);
        QCOMPARE(dataChangedArgs.at(2).value<QVector<int> >(), roles);

        // Further expanded descendants should not change grouping
        if (i == 0) {
            tma.expand(model.index(0, 0, expandedIdx));
            QVERIFY(tma.isExpanded(expandedIdx));
        } else {
            tma.expand(model.index(0, 0, otherExpandedIdx));
            QVERIFY(tma.isExpanded(expandedIdx));
        }
    }
}

void tst_QQuickTreeModelAdaptor::expandAndCollapse_data()
{
    QTest::addColumn<int>("parentRow");
    QTest::newRow("First") << 0;
    QTest::newRow("Middle") << 2;
    QTest::newRow("Last") << 4;
    QTest::newRow("Non expandable") << 3;
}

void tst_QQuickTreeModelAdaptor::expandAndCollapse()
{
    QFETCH(int, parentRow);
    TestModel model(5, 1);
    const QModelIndex &parentIdx = model.index(parentRow, 0);
    bool expandable = model.hasChildren(parentIdx);

    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    expandAndTest(parentIdx, tma, expandable, model.rowCount(parentIdx));
    compareModels(tma, model);

    collapseAndTest(parentIdx, tma, expandable, model.rowCount(parentIdx));
    compareModels(tma, model);
}

void tst_QQuickTreeModelAdaptor::expandAndCollapse2ndLevel()
{
    const int expandRows[] = { 0, 2, 4, 3 };
    const int expandRowsCount = sizeof(expandRows) / sizeof(expandRows[0]);
    for (int i = 0; i < expandRowsCount - 1; i++) { // Skip last non-expandable row
        TestModel model(5, 1);
        const QModelIndex &parentIdx = model.index(expandRows[i], 0);
        QVERIFY(model.hasChildren(parentIdx));

        QQuickTreeModelAdaptor1 tma;
        tma.setModel(&model);

        tma.expand(parentIdx);
        QVERIFY(tma.isExpanded(parentIdx));
        QCOMPARE(tma.rowCount(), model.rowCount() + model.rowCount(parentIdx));

        for (int j = 0; j < expandRowsCount; j++) {
            const QModelIndex &childIdx = model.index(expandRows[j], 0, parentIdx);
            bool expandable = model.hasChildren(childIdx);

            // Expand child
            expandAndTest(childIdx, tma, expandable, model.rowCount(childIdx));
            compareModels(tma, model);
            // Collapse child
            collapseAndTest(childIdx, tma, expandable, model.rowCount(childIdx));
            compareModels(tma, model);

            // Expand child again
            expandAndTest(childIdx, tma, expandable, model.rowCount(childIdx));
            compareModels(tma, model);
            // Collapse parent -> child node invisible, but expanded
            collapseAndTest(parentIdx, tma, true, model.rowCount(parentIdx) + model.rowCount(childIdx));
            compareModels(tma, model);
            QCOMPARE(tma.isExpanded(childIdx), expandable);
            // Expand parent again
            expandAndTest(parentIdx, tma, true, model.rowCount(parentIdx) + model.rowCount(childIdx));
            compareModels(tma, model);

            // Collapse parent -> child node invisible, but expanded
            collapseAndTest(parentIdx, tma, true, model.rowCount(parentIdx) + model.rowCount(childIdx));
            compareModels(tma, model);
            QCOMPARE(tma.isExpanded(childIdx), expandable);
            // Collapse child -> nothing should change
            collapseAndTest(childIdx, tma, false, 0);
            compareModels(tma, model);
            // Expand parent again
            expandAndTest(parentIdx, tma, true, model.rowCount(parentIdx));
            compareModels(tma, model);

            // Expand child, one last time
            expandAndTest(childIdx, tma, expandable, model.rowCount(childIdx));
            compareModels(tma, model);
            // Collapse child, and done
            collapseAndTest(childIdx, tma, expandable, model.rowCount(childIdx));
            compareModels(tma, model);
        }
    }
}

void tst_QQuickTreeModelAdaptor::layoutChange()
{
    TestModel model(5, 1);
    const QModelIndex &idx = model.index(0, 0);
    const QModelIndex &idx2 = model.index(2, 0);

    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    // Nothing expanded
    QSignalSpy dataChangedSpy(&tma, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    model.changeLayout();
    QCOMPARE(dataChangedSpy.count(), 1);
    QVariantList dataChangedArgs = dataChangedSpy.takeFirst();
    QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tma.index(0));
    QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tma.index(tma.rowCount() - 1));
    QVERIFY(dataChangedArgs.at(2).value<QVector<int> >().isEmpty());
    compareModels(tma, model);

    // One item expanded
    tma.expand(idx);
    QVERIFY(tma.isExpanded(idx));
    dataChangedSpy.clear();
    model.changeLayout();
    QCOMPARE(dataChangedSpy.count(), 1);
    dataChangedArgs = dataChangedSpy.takeFirst();
    QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tma.index(0));
    QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tma.index(tma.rowCount() - 1));
    QVERIFY(dataChangedArgs.at(2).value<QVector<int> >().isEmpty());
    compareModels(tma, model);

    // One parent layout change, expanded
    dataChangedSpy.clear();
    QList<QPersistentModelIndex> parents;
    parents << idx;
    model.changeLayout(parents);
    QCOMPARE(dataChangedSpy.count(), 1);
    dataChangedArgs = dataChangedSpy.takeFirst();
    QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tma.index(tma.itemIndex(model.index(0, 0, idx))));
    QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tma.index(tma.itemIndex(model.index(model.rowCount(idx) - 1, 0, idx))));
    QVERIFY(dataChangedArgs.at(2).value<QVector<int> >().isEmpty());
    compareModels(tma, model);

    // One parent layout change, collapsed
    tma.collapse(idx);
    dataChangedSpy.clear();
    model.changeLayout(parents);
    QCOMPARE(dataChangedSpy.count(), 0);
    compareModels(tma, model);

    // Two-parent layout change, both collapsed
    parents << idx2;
    dataChangedSpy.clear();
    model.changeLayout(parents);
    QCOMPARE(dataChangedSpy.count(), 0);
    compareModels(tma, model);

    // Two-parent layout change, only one expanded
    tma.expand(idx2);
    QVERIFY(tma.isExpanded(idx2));
    dataChangedSpy.clear();
    model.changeLayout(parents);
    QCOMPARE(dataChangedSpy.count(), 1);
    dataChangedArgs = dataChangedSpy.takeFirst();
    QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tma.index(tma.itemIndex(model.index(0, 0, idx2))));
    QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tma.index(tma.itemIndex(model.index(model.rowCount(idx2) - 1, 0, idx2))));
    QVERIFY(dataChangedArgs.at(2).value<QVector<int> >().isEmpty());
    compareModels(tma, model);

    // Two-parent layout change, both expanded
    tma.expand(idx);
    QVERIFY(tma.isExpanded(idx));
    dataChangedSpy.clear();
    model.changeLayout(parents);
    QCOMPARE(dataChangedSpy.count(), 2);
    dataChangedArgs = dataChangedSpy.takeFirst();
    QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tma.index(tma.itemIndex(model.index(0, 0, idx))));
    QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tma.index(tma.itemIndex(model.index(model.rowCount(idx) - 1, 0, idx))));
    QVERIFY(dataChangedArgs.at(2).value<QVector<int> >().isEmpty());
    dataChangedArgs = dataChangedSpy.takeFirst();
    QCOMPARE(dataChangedArgs.at(0).toModelIndex(), tma.index(tma.itemIndex(model.index(0, 0, idx2))));
    QCOMPARE(dataChangedArgs.at(1).toModelIndex(), tma.index(tma.itemIndex(model.index(model.rowCount(idx2) - 1, 0, idx2))));
    QVERIFY(dataChangedArgs.at(2).value<QVector<int> >().isEmpty());
    compareModels(tma, model);
}

static const int ModelRowCount = 9;

void tst_QQuickTreeModelAdaptor::removeRows_data()
{
    QTest::addColumn<int>("removeFromRow");
    QTest::addColumn<int>("removeCount");
    QTest::addColumn<int>("removeParentRow");
    QTest::addColumn<int>("expandRow");
    QTest::addColumn<int>("expandParentRow");
    QTest::addColumn<int>("expectedRemovedCount");

    QTest::newRow("Nothing expanded, remove 1st row") << 0 << 1 << -1 << -1 << -1 << 1;
    QTest::newRow("Expand 1st row, remove 1st row") << 0 << 1 << -1 << 0 << -1 << 1 + ModelRowCount;
    QTest::newRow("Expand last row, remove 1st row") << 0 << 1 << -1 << ModelRowCount - 1 << -1 << 1;
    QTest::newRow("Nothing expanded, remove last row") << ModelRowCount - 1 << 1 << -1 << -1 << -1 << 1;
    QTest::newRow("Expand 1st row, remove last row") << ModelRowCount - 1 << 1 << -1 << 0 << -1 << 1;
    QTest::newRow("Expand last row, remove last row") << ModelRowCount - 1 << 1 << -1 << ModelRowCount - 1 << -1 << 1 + ModelRowCount;
    QTest::newRow("Remove child row, parent collapsed") << 2 << 1 << 0 << -1 << -1 << 0;
    QTest::newRow("Remove child row, parent expanded") << 2 << 1 << 0 << 0 << -1 << 1;
    QTest::newRow("Remove several rows, nothing expanded") << 2 << 5 << -1 << -1 << -1 << 5;
    QTest::newRow("Remove several rows, 1st row expanded") << 2 << 5 << -1 << 0 << -1 << 5;
    QTest::newRow("Remove several rows, last row expanded") << 2 << 5 << -1 << ModelRowCount - 1 << -1 << 5;
    QTest::newRow("Remove several rows, one of them expanded") << 2 << 5 << -1 << 4 << -1 << 5 + ModelRowCount;
    QTest::newRow("Remove all rows, nothing expanded") << 0 << ModelRowCount << -1 << -1 << -1 << ModelRowCount;
    QTest::newRow("Remove all rows, 1st row expanded") << 0 << ModelRowCount << -1 << 0 << -1 << ModelRowCount * 2;
    QTest::newRow("Remove all rows, last row expanded") << 0 << ModelRowCount << -1 << ModelRowCount - 1 << -1 << ModelRowCount * 2;
    QTest::newRow("Remove all rows, random one expanded") << 0 << ModelRowCount << -1 << 4 << -1 << ModelRowCount * 2;
}

void tst_QQuickTreeModelAdaptor::removeRows()
{
    QFETCH(int, removeFromRow);
    QFETCH(int, removeCount);
    QFETCH(int, removeParentRow);
    QFETCH(int, expandRow);
    QFETCH(int, expandParentRow);
    QFETCH(int, expectedRemovedCount);

    TestModel model(ModelRowCount, 1);
    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    const QModelIndex &expandParentIdx = expandParentRow == -1 ? QModelIndex() : model.index(expandParentRow, 0);
    if (expandParentIdx.isValid()) {
        tma.expand(expandParentIdx);
        QVERIFY(tma.isExpanded(expandParentIdx));
    }
    const QModelIndex &expandIdx = model.index(expandRow, 0, expandParentIdx);
    if (expandIdx.isValid()) {
        tma.expand(expandIdx);
        QVERIFY(tma.isExpanded(expandIdx));
    }

    const QModelIndex &removeParentIdx = removeParentRow == -1 ? QModelIndex() : model.index(removeParentRow, 0);
    const QModelIndex &removeIdx = model.index(removeFromRow, 0, removeParentIdx);
    int tmaItemIdx = tma.itemIndex(removeIdx);

    QSignalSpy rowsAboutToBeRemovedSpy(&tma, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)));
    QSignalSpy rowsRemovedSpy(&tma, SIGNAL(rowsRemoved(const QModelIndex&, int, int)));
    model.removeRows(removeFromRow, removeCount, removeParentIdx);
    if (expectedRemovedCount == 0) {
        QCOMPARE(rowsAboutToBeRemovedSpy.count(), 0);
        QCOMPARE(rowsRemovedSpy.count(), 0);
    } else {
        QCOMPARE(rowsAboutToBeRemovedSpy.count(), 1);
        QCOMPARE(rowsRemovedSpy.count(), 1);
        QVariantList rowsAboutToBeRemovedArgs = rowsAboutToBeRemovedSpy.first();
        QVariantList rowsRemovedArgs = rowsRemovedSpy.first();
        QCOMPARE(rowsAboutToBeRemovedArgs, rowsRemovedArgs);
        QCOMPARE(rowsAboutToBeRemovedArgs.at(0).toModelIndex(), QModelIndex());
        QCOMPARE(rowsAboutToBeRemovedArgs.at(1).toInt(), tmaItemIdx);
        QCOMPARE(rowsAboutToBeRemovedArgs.at(2).toInt(), tmaItemIdx + expectedRemovedCount - 1);
    }
}

void tst_QQuickTreeModelAdaptor::removeRowsChildrenAndParent()
{
    TestModel model(ModelRowCount, 1);
    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    // Expand the first node
    const QModelIndex &parent = model.index(0, 0);
    tma.expand(parent);
    QVERIFY(tma.isExpanded(parent));

    QSignalSpy rowsAboutToBeRemovedSpy(&tma, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)));
    QSignalSpy rowsRemovedSpy(&tma, SIGNAL(rowsRemoved(const QModelIndex&, int, int)));

    // Remove the first node children
    int expectedRemovedCount = model.rowCount(parent);
    int tmaItemIdx = tma.itemIndex(model.index(0, 0, parent));
    QCOMPARE(tmaItemIdx, tma.itemIndex(parent) + 1);
    model.removeRows(0, expectedRemovedCount, parent);
    QCOMPARE(rowsAboutToBeRemovedSpy.count(), 1);
    QCOMPARE(rowsRemovedSpy.count(), 1);
    QVariantList rowsAboutToBeRemovedArgs = rowsAboutToBeRemovedSpy.first();
    QVariantList rowsRemovedArgs = rowsRemovedSpy.first();
    QCOMPARE(rowsAboutToBeRemovedArgs, rowsRemovedArgs);
    QCOMPARE(rowsAboutToBeRemovedArgs.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(rowsAboutToBeRemovedArgs.at(1).toInt(), tmaItemIdx);
    QCOMPARE(rowsAboutToBeRemovedArgs.at(2).toInt(), tmaItemIdx + expectedRemovedCount - 1);

    // Remove the first node
    rowsAboutToBeRemovedSpy.clear();
    rowsRemovedSpy.clear();
    model.removeRows(0, 1, QModelIndex());
    QCOMPARE(rowsAboutToBeRemovedSpy.count(), 1);
    QCOMPARE(rowsRemovedSpy.count(), 1);
    rowsAboutToBeRemovedArgs = rowsAboutToBeRemovedSpy.first();
    rowsRemovedArgs = rowsRemovedSpy.first();
    QCOMPARE(rowsAboutToBeRemovedArgs, rowsRemovedArgs);
    QCOMPARE(rowsAboutToBeRemovedArgs.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(rowsAboutToBeRemovedArgs.at(1).toInt(), 0);
    QCOMPARE(rowsAboutToBeRemovedArgs.at(2).toInt(), 0);
}

void tst_QQuickTreeModelAdaptor::removeChildrenMoveParent()
{
    TestModel model(ModelRowCount, 1);
    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    // Expand the first node
    const QModelIndex &parent = model.index(0, 0);
    tma.expand(parent);
    QVERIFY(tma.isExpanded(parent));

    // Remove the first node children
    QSignalSpy rowsAboutToBeRemovedSpy(&tma, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)));
    QSignalSpy rowsRemovedSpy(&tma, SIGNAL(rowsRemoved(const QModelIndex&, int, int)));
    int expectedRemovedCount = model.rowCount(parent);
    int tmaItemIdx = tma.itemIndex(model.index(0, 0, parent));
    QCOMPARE(tmaItemIdx, tma.itemIndex(parent) + 1);
    model.removeRows(0, expectedRemovedCount, parent);
    QCOMPARE(rowsAboutToBeRemovedSpy.count(), 1);
    QCOMPARE(rowsRemovedSpy.count(), 1);
    QVariantList rowsAboutToBeRemovedArgs = rowsAboutToBeRemovedSpy.first();
    QVariantList rowsRemovedArgs = rowsRemovedSpy.first();
    QCOMPARE(rowsAboutToBeRemovedArgs, rowsRemovedArgs);
    QCOMPARE(rowsAboutToBeRemovedArgs.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(rowsAboutToBeRemovedArgs.at(1).toInt(), tmaItemIdx);
    QCOMPARE(rowsAboutToBeRemovedArgs.at(2).toInt(), tmaItemIdx + expectedRemovedCount - 1);

    // Move the first node
    QSignalSpy rowsAboutToBeMovedSpy(&tma, SIGNAL(rowsAboutToBeMoved(QModelIndex,int,int,QModelIndex,int)));
    QSignalSpy rowsMovedSpy(&tma, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
    model.moveRows(QModelIndex(), 0, 1, QModelIndex(), 3);
    QCOMPARE(rowsAboutToBeMovedSpy.count(), 1);
    QCOMPARE(rowsRemovedSpy.count(), 1);
    QVariantList rowsAboutToBeMovedArgs = rowsAboutToBeMovedSpy.first();
    QVariantList rowsMovedArgs = rowsMovedSpy.first();
    QCOMPARE(rowsAboutToBeMovedArgs, rowsMovedArgs);
    QCOMPARE(rowsAboutToBeMovedArgs.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(rowsAboutToBeMovedArgs.at(1).toInt(), 0);
    QCOMPARE(rowsAboutToBeMovedArgs.at(2).toInt(), 0);
    QCOMPARE(rowsAboutToBeMovedArgs.at(3).toModelIndex(), QModelIndex());
    QCOMPARE(rowsAboutToBeMovedArgs.at(4).toInt(), 3);
}

void tst_QQuickTreeModelAdaptor::removeChildrenRelayoutParent()
{
    TestModel model(ModelRowCount, 1);
    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    // Expand the first node
    const QModelIndex &parent = model.index(0, 0);
    tma.expand(parent);
    QVERIFY(tma.isExpanded(parent));

    QSignalSpy rowsAboutToBeRemovedSpy(&tma, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)));
    QSignalSpy rowsRemovedSpy(&tma, SIGNAL(rowsRemoved(const QModelIndex&, int, int)));

    // Remove the first node children
    int expectedRemovedCount = model.rowCount(parent);
    int tmaItemIdx = tma.itemIndex(model.index(0, 0, parent));
    QCOMPARE(tmaItemIdx, tma.itemIndex(parent) + 1);
    model.removeRows(0, expectedRemovedCount, parent);
    QCOMPARE(rowsAboutToBeRemovedSpy.count(), 1);
    QCOMPARE(rowsRemovedSpy.count(), 1);
    QVariantList rowsAboutToBeRemovedArgs = rowsAboutToBeRemovedSpy.first();
    QVariantList rowsRemovedArgs = rowsRemovedSpy.first();
    QCOMPARE(rowsAboutToBeRemovedArgs, rowsRemovedArgs);
    QCOMPARE(rowsAboutToBeRemovedArgs.at(0).toModelIndex(), QModelIndex());
    QCOMPARE(rowsAboutToBeRemovedArgs.at(1).toInt(), tmaItemIdx);
    QCOMPARE(rowsAboutToBeRemovedArgs.at(2).toInt(), tmaItemIdx + expectedRemovedCount - 1);

    // Relayout the first node
    QSignalSpy dataChanged(&tma, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    QList<QPersistentModelIndex> parents;
    parents << parent;
    model.changeLayout(parents);
    QCOMPARE(dataChanged.count(), 0);
}

void tst_QQuickTreeModelAdaptor::insertRows_data()
{
    QTest::addColumn<int>("insertFromRow");
    QTest::addColumn<int>("insertCount");
    QTest::addColumn<int>("insertParentRow");
    QTest::addColumn<int>("expandRow");
    QTest::addColumn<int>("expandParentRow");
    QTest::addColumn<int>("expectedInsertedCount");

    QTest::newRow("Nothing expanded, insert 1st row") << 0 << 1 << -1 << -1 << -1 << 1;
    QTest::newRow("Expand 1st row, insert 1st row") << 0 << 1 << -1 << 0 << -1 << 1;
    QTest::newRow("Expand last row, insert 1st row") << 0 << 1 << -1 << ModelRowCount - 1 << -1 << 1;
    QTest::newRow("Nothing expanded, insert before the last row") << ModelRowCount - 1 << 1 << -1 << -1 << -1 << 1;
    QTest::newRow("Nothing expanded, insert after the last row") << ModelRowCount << 1 << -1 << -1 << -1 << 1;
    QTest::newRow("Expand 1st row, insert before the last row") << ModelRowCount - 1 << 1 << -1 << 0 << -1 << 1;
    QTest::newRow("Expand 1st row, insert after the last row") << ModelRowCount << 1 << -1 << 0 << -1 << 1;
    QTest::newRow("Expand last row, insert before the last row") << ModelRowCount - 1 << 1 << -1 << ModelRowCount - 1 << -1 << 1;
    QTest::newRow("Expand last row, insert after the last row") << ModelRowCount << 1 << -1 << ModelRowCount - 1 << -1 << 1;
    QTest::newRow("Insert child row, parent collapsed") << 2 << 1 << 0 << -1 << -1 << 0;
    QTest::newRow("Insert child row, parent expanded") << 2 << 1 << 0 << 0 << -1 << 1;
    QTest::newRow("Insert several rows, nothing expanded") << 2 << 5 << -1 << -1 << -1 << 5;
    QTest::newRow("Insert several rows, 1st row expanded") << 2 << 5 << -1 << 0 << -1 << 5;
    QTest::newRow("Insert several rows, last row expanded") << 2 << 5 << -1 << ModelRowCount - 1 << -1 << 5;
}

void tst_QQuickTreeModelAdaptor::insertRows()
{
    QFETCH(int, insertFromRow);
    QFETCH(int, insertCount);
    QFETCH(int, insertParentRow);
    QFETCH(int, expandRow);
    QFETCH(int, expandParentRow);
    QFETCH(int, expectedInsertedCount);

    TestModel model(ModelRowCount, 1);
    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    const QModelIndex &expandParentIdx = expandParentRow == -1 ? QModelIndex() : model.index(expandParentRow, 0);
    if (expandParentIdx.isValid()) {
        tma.expand(expandParentIdx);
        QVERIFY(tma.isExpanded(expandParentIdx));
    }
    const QModelIndex &expandIdx = model.index(expandRow, 0, expandParentIdx);
    if (expandIdx.isValid()) {
        tma.expand(expandIdx);
        QVERIFY(tma.isExpanded(expandIdx));
    }

    const QModelIndex &insertParentIdx = insertParentRow == -1 ? QModelIndex() : model.index(insertParentRow, 0);
    const QModelIndex &insertIdx = model.index(insertFromRow, 0, insertParentIdx);
    int tmaItemIdx = insertFromRow == model.rowCount(insertParentIdx) ? tma.rowCount() : tma.itemIndex(insertIdx);

    QSignalSpy rowsAboutToBeInsertedSpy(&tma, SIGNAL(rowsAboutToBeInserted(const QModelIndex&, int, int)));
    QSignalSpy rowsInsertedSpy(&tma, SIGNAL(rowsInserted(const QModelIndex&, int, int)));
    model.insertRows(insertFromRow, insertCount, insertParentIdx);
    if (expectedInsertedCount == 0) {
        QCOMPARE(rowsAboutToBeInsertedSpy.count(), 0);
        QCOMPARE(rowsInsertedSpy.count(), 0);
    } else {
        QCOMPARE(rowsAboutToBeInsertedSpy.count(), 1);
        QCOMPARE(rowsInsertedSpy.count(), 1);
        QVariantList rowsAboutToBeInsertedArgs = rowsAboutToBeInsertedSpy.first();
        QVariantList rowsInsertedArgs = rowsInsertedSpy.first();
        QCOMPARE(rowsAboutToBeInsertedArgs, rowsInsertedArgs);
        QCOMPARE(rowsAboutToBeInsertedArgs.at(0).toModelIndex(), QModelIndex());
        QCOMPARE(rowsAboutToBeInsertedArgs.at(1).toInt(), tmaItemIdx);
        QCOMPARE(rowsAboutToBeInsertedArgs.at(2).toInt(), tmaItemIdx + expectedInsertedCount - 1);
        QCOMPARE(tma.itemIndex(model.index(insertFromRow, 0, insertParentIdx)), tmaItemIdx);
    }
}

enum MoveSignalType {
    RowsMoved = 0, RowsInserted, RowsRemoved
};

void tst_QQuickTreeModelAdaptor::moveRows_data()
{
    QTest::addColumn<int>("sourceRow");
    QTest::addColumn<bool>("expandSource");
    QTest::addColumn<int>("moveCount");
    QTest::addColumn<int>("sourceParentRow");
    QTest::addColumn<bool>("expandSourceParent");
    QTest::addColumn<int>("destRow");
    QTest::addColumn<bool>("expandDest");
    QTest::addColumn<int>("destParentRow");
    QTest::addColumn<bool>("expandDestParent");
    QTest::addColumn<int>("expandRow");
    QTest::addColumn<int>("expandParentRow");
    QTest::addColumn<int>("signalType");
    QTest::addColumn<int>("expectedMovedCount");

    QTest::newRow("From and to top-level parent")
            << 0 << false << 1 << -1 << false
            << 3 << false << -1 << false
            << -1 << -1 << (int)RowsMoved << 1;
    QTest::newRow("From and to top-level parent, expanded")
            << 0 << true << 1 << -1 << false
            << 3 << false << -1 << false
            << -1 << -1 << (int)RowsMoved << ModelRowCount + 1;
    QTest::newRow("From and to top-level parent, backwards")
            << 4 << false << 1 << -1 << false
            << 0 << false << -1 << false
            << -1 << -1 << (int)RowsMoved << 1;
    QTest::newRow("From and to top-level parent, expanded, backwards")
            << 4 << true << 1 << -1 << false
            << 0 << false << -1 << false
            << -1 << -1 << (int)RowsMoved << ModelRowCount + 1;
    QTest::newRow("Moving between collapsed parents")
            << 0 << false << 1 << 0 << false
            << 0 << false << 2 << false
            << -1 << -1 << (int)RowsMoved << 0;
    QTest::newRow("From expanded parent to collapsed parent")
            << 0 << false << 1 << 0 << true
            << 0 << false << 2 << false
            << -1 << -1 << (int)RowsRemoved << 1;
    QTest::newRow("From collapsed parent to expanded parent")
            << 0 << false << 1 << 0 << false
            << 0 << false << 2 << true
            << -1 << -1 << (int)RowsInserted << 1;
    QTest::newRow("From and to same expanded parent")
            << 0 << false << 1 << 0 << true
            << 2 << false << 0 << false
            << -1 << -1 << (int)RowsMoved << 1;
    QTest::newRow("From expanded parent to collapsed parent, expanded row")
            << 0 << true << 1 << 0 << true
            << 0 << false << 2 << false
            << -1 << -1 << (int)RowsRemoved << ModelRowCount + 1;
    QTest::newRow("From collapsed parent to expanded parent, expanded row")
            << 0 << true << 1 << 0 << false
            << 0 << false << 2 << true
            << -1 << -1 << (int)RowsInserted << ModelRowCount + 1;
    QTest::newRow("From and to same expanded parent, expanded row, forward")
            << 0 << true << 1 << 0 << true
            << 5 << false << 0 << false
            << -1 << -1 << (int)RowsMoved << ModelRowCount + 1;
    QTest::newRow("From and to same expanded parent, expanded row, last row")
            << 0 << true << 1 << 0 << true
            << ModelRowCount << false << 0 << false
            << -1 << -1 << (int)RowsMoved << ModelRowCount + 1;
    QTest::newRow("From and to same expanded parent, expanded row, several")
            << 0 << true << 3 << 0 << true
            << 5 << false << 0 << false
            << -1 << -1 << (int)RowsMoved << ModelRowCount + 3;
    QTest::newRow("From and to same expanded parent, expanded row, backward")
            << 6 << true << 1 << 0 << true
            << 0 << false << 0 << false
            << -1 << -1 << (int)RowsMoved << ModelRowCount + 1;
    QTest::newRow("From and to same expanded parent, expanded row, several, backward")
            << 6 << true << 2 << 0 << true
            << 0 << false << 0 << false
            << -1 << -1 << (int)RowsMoved << ModelRowCount + 2;
    QTest::newRow("From and to different expanded parents")
            << 0 << false << 1 << 0 << true
            << 1 << false << 4 << true
            << -1 << -1 << (int)RowsMoved << 1;
    QTest::newRow("From and to different expanded parents, backward")
            << 0 << false << 1 << 4 << true
            << 2 << false << 0 << true
            << -1 << -1 << (int)RowsMoved << 1;
    QTest::newRow("From and to different expanded parents, up in level")
            << 0 << false << 1 << 0 << true
            << 5 << true << -1 << true
            << -1 << -1 << (int)RowsMoved << 1;
    QTest::newRow("From and to different expanded parents, up in level, backwards")
            << 0 << false << 1 << 4 << true
            << 1 << false << -1 << true
            << -1 << -1 << (int)RowsMoved << 1;
    QTest::newRow("From and to different expanded parents, up in level, as 1st item")
            << 0 << false << 1 << 0 << true
            << 0 << false << -1 << true
            << -1 << -1 << (int)RowsMoved << 1;
    QTest::newRow("From and to different expanded parents, backward, up in level")
            << 0 << false << 1 << 4 << true
            << 2 << false << 0 << true
            << -1 << -1 << (int)RowsMoved << 1;
}

void tst_QQuickTreeModelAdaptor::moveRows()
{
    QFETCH(int, sourceRow);
    QFETCH(bool, expandSource);
    QFETCH(int, moveCount);
    QFETCH(int, sourceParentRow);
    QFETCH(bool, expandSourceParent);
    QFETCH(int, destRow);
    QFETCH(bool, expandDest);
    QFETCH(int, destParentRow);
    QFETCH(bool, expandDestParent);
    QFETCH(int, expandRow);
    QFETCH(int, expandParentRow);
    QFETCH(int, signalType);
    QFETCH(int, expectedMovedCount);

    TestModel model(ModelRowCount, 1);
    model.alternateChildlessRows = false;
    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    const QModelIndex &expandParentIdx = expandParentRow == -1 ? QModelIndex() : model.index(expandParentRow, 0);
    if (expandParentIdx.isValid()) {
        tma.expand(expandParentIdx);
        QVERIFY(tma.isExpanded(expandParentIdx));
    }
    const QModelIndex &expandIdx = model.index(expandRow, 0, expandParentIdx);
    if (expandIdx.isValid()) {
        tma.expand(expandIdx);
        QVERIFY(tma.isExpanded(expandIdx));
    }

    const QModelIndex &sourceParentIdx = sourceParentRow == -1 ? QModelIndex() : model.index(sourceParentRow, 0);
    if (expandSourceParent && sourceParentIdx.isValid()) {
        tma.expand(sourceParentIdx);
        QVERIFY(tma.isExpanded(sourceParentIdx));
    }
    const QModelIndex &sourceIdx = model.index(sourceRow, 0, sourceParentIdx);
    if (expandSource) {
        tma.expand(sourceIdx);
        QVERIFY(tma.isExpanded(sourceIdx));
    }

    const QModelIndex &destParentIdx = destParentRow == -1 ? QModelIndex() : model.index(destParentRow, 0);
    if (expandDestParent && destParentIdx.isValid()) {
        tma.expand(destParentIdx);
        QVERIFY(tma.isExpanded(destParentIdx));
    }
    const QModelIndex &destIdx = model.index(destRow, 0, destParentIdx);
    if (expandDest) {
        tma.expand(destIdx);
        QVERIFY(tma.isExpanded(destIdx));
    }

    int tmaSourceItemIdx = signalType == RowsInserted ? -1 // Not tested if RowsInserted
                           : tma.itemIndex(sourceIdx);
    int tmaDestItemIdx = signalType == RowsRemoved ? -1 : // Not tested if RowsRemoved
                         destRow == model.rowCount(destParentIdx) ? -1  /* FIXME */ : tma.itemIndex(destIdx);

    QSignalSpy rowsAboutToBeMovedSpy(&tma, SIGNAL(rowsAboutToBeMoved(QModelIndex,int,int,QModelIndex,int)));
    QSignalSpy rowsMovedSpy(&tma, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
    QSignalSpy rowsAboutToBeInsertedSpy(&tma, SIGNAL(rowsAboutToBeInserted(const QModelIndex&, int, int)));
    QSignalSpy rowsInsertedSpy(&tma, SIGNAL(rowsInserted(const QModelIndex&, int, int)));
    QSignalSpy rowsAboutToBeRemovedSpy(&tma, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)));
    QSignalSpy rowsRemovedSpy(&tma, SIGNAL(rowsRemoved(const QModelIndex&, int, int)));

    QVERIFY(model.moveRows(sourceParentIdx, sourceRow, moveCount, destParentIdx, destRow));

    if (signalType != RowsMoved || expectedMovedCount == 0) {
        QCOMPARE(rowsAboutToBeMovedSpy.count(), 0);
        QCOMPARE(rowsMovedSpy.count(), 0);
    }
    if (signalType != RowsInserted || expectedMovedCount == 0) {
        QCOMPARE(rowsAboutToBeInsertedSpy.count(), 0);
        QCOMPARE(rowsInsertedSpy.count(), 0);
    }
    if (signalType != RowsRemoved || expectedMovedCount == 0) {
        QCOMPARE(rowsAboutToBeRemovedSpy.count(), 0);
        QCOMPARE(rowsRemovedSpy.count(), 0);
    }

    if (expectedMovedCount != 0) {
        if (signalType == RowsMoved) {
            QCOMPARE(rowsAboutToBeMovedSpy.count(), 1);
            QCOMPARE(rowsMovedSpy.count(), 1);
            QVariantList rowsAboutToBeMovedArgs = rowsAboutToBeMovedSpy.first();
            QVariantList rowsMovedArgs = rowsMovedSpy.first();
            QCOMPARE(rowsAboutToBeMovedArgs, rowsMovedArgs);
            QCOMPARE(rowsAboutToBeMovedArgs.at(0).toModelIndex(), QModelIndex());
            QCOMPARE(rowsAboutToBeMovedArgs.at(1).toInt(), tmaSourceItemIdx);
            QCOMPARE(rowsAboutToBeMovedArgs.at(2).toInt(), tmaSourceItemIdx + expectedMovedCount - 1);
            QCOMPARE(rowsAboutToBeMovedArgs.at(3).toModelIndex(), QModelIndex());
            if (tmaDestItemIdx != -1)
                QCOMPARE(rowsAboutToBeMovedArgs.at(4).toInt(), tmaDestItemIdx);
        } else if (signalType == RowsInserted) {
            // We only test with one level of expanded children here, so we can do
            // exhaustive testing depending on whether the moved row is expanded.
            int signalCount = expandSource ? 2 : 1;
            QCOMPARE(rowsAboutToBeInsertedSpy.count(), signalCount);
            QCOMPARE(rowsInsertedSpy.count(), signalCount);
            QVariantList rowsAboutToBeInsertedArgs = rowsAboutToBeInsertedSpy.takeFirst();
            QVariantList rowsInsertedArgs = rowsInsertedSpy.takeFirst();
            QCOMPARE(rowsAboutToBeInsertedArgs, rowsInsertedArgs);
            QCOMPARE(rowsAboutToBeInsertedArgs.at(0).toModelIndex(), QModelIndex());
            QCOMPARE(rowsAboutToBeInsertedArgs.at(1).toInt(), tmaDestItemIdx);
            if (expandSource) {
                QCOMPARE(rowsAboutToBeInsertedArgs.at(2).toInt(), tmaDestItemIdx);
                rowsAboutToBeInsertedArgs = rowsAboutToBeInsertedSpy.first();
                rowsInsertedArgs = rowsInsertedSpy.first();
                QCOMPARE(rowsAboutToBeInsertedArgs, rowsInsertedArgs);
                QCOMPARE(rowsAboutToBeInsertedArgs.at(0).toModelIndex(), QModelIndex());
                QCOMPARE(rowsAboutToBeInsertedArgs.at(1).toInt(), tmaDestItemIdx + 1);
            }
            QCOMPARE(rowsAboutToBeInsertedArgs.at(2).toInt(), tmaDestItemIdx + expectedMovedCount - 1);
            QCOMPARE(tma.itemIndex(model.index(destRow, 0, destParentIdx)), tmaDestItemIdx);
        } else if (signalType == RowsRemoved) {
            QCOMPARE(rowsAboutToBeRemovedSpy.count(), 1);
            QCOMPARE(rowsRemovedSpy.count(), 1);
            QVariantList rowsAboutToBeRemovedArgs = rowsAboutToBeRemovedSpy.first();
            QVariantList rowsRemovedArgs = rowsRemovedSpy.first();
            QCOMPARE(rowsAboutToBeRemovedArgs, rowsRemovedArgs);
            QCOMPARE(rowsAboutToBeRemovedArgs.at(0).toModelIndex(), QModelIndex());
            QCOMPARE(rowsAboutToBeRemovedArgs.at(1).toInt(), tmaSourceItemIdx);
            QCOMPARE(rowsAboutToBeRemovedArgs.at(2).toInt(), tmaSourceItemIdx + expectedMovedCount - 1);
        }
    }
    QVERIFY(tma.testConsistency());
    compareModels(tma, model);
}

void tst_QQuickTreeModelAdaptor::reparentOnSameRow()
{
    TestModel model(2, 1);
    model.alternateChildlessRows = false;
    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    const QModelIndex &destParent = model.index(0, 0);
    const QModelIndex &sourceParent = QModelIndex();
    QVERIFY(destParent.isValid());
    tma.expand(destParent);
    QVERIFY(tma.isExpanded(destParent));

    QSignalSpy dataChangedSpy(&tma, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    QSignalSpy rowsMovedSpy(&tma, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
    QVERIFY(rowsMovedSpy.isValid());
    QVERIFY(dataChangedSpy.isValid());

    QVERIFY(model.moveRows(sourceParent, 1, 1, destParent, 2));

    QModelIndex movedIndex = tma.index(3, 0, QModelIndex());
    QVERIFY(movedIndex.isValid());
    QCOMPARE(movedIndex.data(QQuickTreeModelAdaptor1::DepthRole).toInt(), 1);
    QCOMPARE(tma.data(movedIndex, QQuickTreeModelAdaptor1::ModelIndexRole).toModelIndex(), model.index(2, 0, destParent));

    // at least DepthRole and ModeIndexRole changes should have happened for the affected row
    bool depthChanged = false;
    bool modelIndexChanged = false;
    QList<QList<QVariant> > &changes = dataChangedSpy;
    foreach (QList<QVariant> change, changes) {
        if (change.at(0) == movedIndex) {
            if (change.at(2).value<QVector<int> >().contains(QQuickTreeModelAdaptor1::DepthRole))
                depthChanged = true;
            if (change.at(2).value<QVector<int> >().contains(QQuickTreeModelAdaptor1::ModelIndexRole))
                modelIndexChanged = true;
        }
    }

    QCOMPARE(depthChanged, true);
    QCOMPARE(modelIndexChanged, true);

    QCOMPARE(rowsMovedSpy.count(), 0);

    model.moveRow(destParent, 2, QModelIndex(), 1);

    QCOMPARE(rowsMovedSpy.count(), 0);
    QVERIFY(tma.testConsistency());
    compareModels(tma, model);
}

void tst_QQuickTreeModelAdaptor::selectionForRowRange()
{
    const int ModelRowCount = 9;
    const int ModelRowCountLoopStep = 4;

    TestModel model(ModelRowCount, 1);
    model.alternateChildlessRows = false;
    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    // NOTE: Some selections may look a bit cryptic. Insert a call to
    // tma.dump() before each block if you need to see what's going on.

    for (int i = 0; i < ModelRowCount; i += ModelRowCountLoopStep) {
        // Single row selection
        const QModelIndex &idx = model.index(i, 0);
        const QItemSelection &sel = tma.selectionForRowRange(idx, idx);
        QCOMPARE(sel.count(), 1);
        const QItemSelectionRange &range = sel.first();
        QCOMPARE(QModelIndex(range.topLeft()), model.index(i, 0));
        QCOMPARE(QModelIndex(range.bottomRight()), model.index(i, 0));
    }

    for (int i = 0; i < ModelRowCount - ModelRowCountLoopStep; i += ModelRowCountLoopStep) {
        // Single range selection
        const QModelIndex &from = model.index(i, 0);
        const QModelIndex &to = model.index(i + ModelRowCountLoopStep, 0);
        const QItemSelection &sel = tma.selectionForRowRange(from, to);
        QCOMPARE(sel.count(), 1);
        const QItemSelectionRange &range = sel.first();
        QCOMPARE(QModelIndex(range.topLeft()), model.index(i, 0));
        QCOMPARE(QModelIndex(range.bottomRight()), model.index(i + ModelRowCountLoopStep, 0));
    }

    {   // Select all, no branch expanded
        const QModelIndex &from = model.index(0, 0);
        const QModelIndex &to = model.index(ModelRowCount - 1, 0);
        const QItemSelection &sel = tma.selectionForRowRange(from, to);
        QCOMPARE(sel.count(), 1);
        const QItemSelectionRange &range = sel.first();
        QCOMPARE(QModelIndex(range.topLeft()), model.index(0, 0));
        QCOMPARE(QModelIndex(range.bottomRight()), model.index(ModelRowCount - 1, 0));
    }

    // Expand 1st top-level item
    const QModelIndex &parent = model.index(0, 0);
    tma.expand(parent);

    {   // 1st item expanded, select first 5 rows
        const QModelIndex &from = tma.mapRowToModelIndex(0);
        const QModelIndex &to = tma.mapRowToModelIndex(4);
        const QItemSelection &sel = tma.selectionForRowRange(from, to);
        QCOMPARE(sel.count(), 2);
        // We don't know in which order the selection ranges are
        // being added, so we iterate and try to find what we expect.
        foreach (const QItemSelectionRange &range, sel) {
            if (range.topLeft() ==  model.index(0, 0))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(0, 0));
            else if (range.topLeft() ==  model.index(0, 0, parent))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(3, 0, parent));
            else
                QFAIL("Unexpected selection range");
        }
    }

    {   // 1st item expanded, select first 5 top-level items
        const QModelIndex &from = tma.mapRowToModelIndex(0);
        const QModelIndex &to = tma.mapRowToModelIndex(4 + ModelRowCount);
        const QItemSelection &sel = tma.selectionForRowRange(from, to);
        QCOMPARE(sel.count(), 2);
        // We don't know in which order the selection ranges are
        // being added, so we iterate and try to find what we expect.
        foreach (const QItemSelectionRange &range, sel) {
            if (range.topLeft() ==  model.index(0, 0))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(4, 0));
            else if (range.topLeft() ==  model.index(0, 0, parent))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(ModelRowCount - 1, 0, parent));
            else
                QFAIL("Unexpected selection range");
        }
    }

    // Expand 2nd top-level item
    const QModelIndex &parent2 = model.index(1, 0);
    tma.expand(parent2);

    {   // 1st two items expanded, select first 5 top-level items
        const QModelIndex &from = tma.mapRowToModelIndex(0);
        const QModelIndex &to = tma.mapRowToModelIndex(4 + 2 * ModelRowCount);
        const QItemSelection &sel = tma.selectionForRowRange(from, to);
        QCOMPARE(sel.count(), 3);
        // We don't know in which order the selection ranges are
        // being added, so we iterate and try to find what we expect.
        foreach (const QItemSelectionRange &range, sel) {
            if (range.topLeft() ==  model.index(0, 0))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(4, 0));
            else if (range.topLeft() ==  model.index(0, 0, parent))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(ModelRowCount - 1, 0, parent));
            else if (range.topLeft() ==  model.index(0, 0, parent2))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(ModelRowCount - 1, 0, parent2));
            else
                QFAIL("Unexpected selection range");
        }
    }

    // Expand 1st child of 1st top-level item
    const QModelIndex &parent3 = model.index(0, 0, parent);
    tma.expand(parent3);

    {   // 1st two items, and 1st child of 1st item expanded, select first 5 rows
        const QModelIndex &from = tma.mapRowToModelIndex(0);
        const QModelIndex &to = tma.mapRowToModelIndex(4);
        const QItemSelection &sel = tma.selectionForRowRange(from, to);
        QCOMPARE(sel.count(), 3);
        // We don't know in which order the selection ranges are
        // being added, so we iterate and try to find what we expect.
        foreach (const QItemSelectionRange &range, sel) {
            if (range.topLeft() ==  model.index(0, 0))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(0, 0));
            else if (range.topLeft() ==  model.index(0, 0, parent))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(0, 0, parent));
            else if (range.topLeft() ==  model.index(0, 0, parent3))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(2, 0, parent3));
            else
                QFAIL("Unexpected selection range");
        }
    }

    {   // 1st two items, and 1st child of 1st item expanded, select all
        const QModelIndex &from = tma.mapRowToModelIndex(0);
        const QModelIndex &to = tma.mapRowToModelIndex(4 * ModelRowCount - 1);
        const QItemSelection &sel = tma.selectionForRowRange(from, to);
        QCOMPARE(sel.count(), 4);
        // We don't know in which order the selection ranges are
        // being added, so we iterate and try to find what we expect.
        foreach (const QItemSelectionRange &range, sel) {
            if (range.topLeft() ==  model.index(0, 0))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(ModelRowCount - 1, 0));
            else if (range.topLeft() ==  model.index(0, 0, parent))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(ModelRowCount - 1, 0, parent));
            else if (range.topLeft() ==  model.index(0, 0, parent2))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(ModelRowCount - 1, 0, parent2));
            else if (range.topLeft() ==  model.index(0, 0, parent3))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(ModelRowCount - 1, 0, parent3));
            else
                QFAIL("Unexpected selection range");
        }
    }

    {   // 1st two items, and 1st child of 1st item expanded, select rows across branches
        const QModelIndex &from = tma.mapRowToModelIndex(8);
        const QModelIndex &to = tma.mapRowToModelIndex(23);
        const QItemSelection &sel = tma.selectionForRowRange(from, to);
        QCOMPARE(sel.count(), 4);
        // We don't know in which order the selection ranges are
        // being added, so we iterate and try to find what we expect.
        foreach (const QItemSelectionRange &range, sel) {
            if (range.topLeft() ==  model.index(1, 0))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(1, 0));
            else if (range.topLeft() ==  model.index(1, 0, parent))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(ModelRowCount - 1, 0, parent));
            else if (range.topLeft() ==  model.index(0, 0, parent2))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(3, 0, parent2));
            else if (range.topLeft() ==  model.index(6, 0, parent3))
                QCOMPARE(QModelIndex(range.bottomRight()), model.index(ModelRowCount - 1, 0, parent3));
            else
                QFAIL("Unexpected selection range");
        }
    }
}

void tst_QQuickTreeModelAdaptor::hasChildrenEmit()
{
    TestModel model(1, 1);
    model.alternateChildlessRows = false;
    QQuickTreeModelAdaptor1 tma;
    tma.setModel(&model);

    QModelIndex root = model.index(0,0);
    QVERIFY(root.isValid());

    QModelIndex child = root.child(0,0);
    QVERIFY(child.isValid());

    // Root not expanded , child not expanded, insert in child, expect no datachanged
    {
        QVERIFY(!tma.isExpanded(root));
        QVERIFY(!tma.isExpanded(child));
        QSignalSpy dataChangedSpy(&tma, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QCOMPARE(dataChangedSpy.count(), 0);
        QCOMPARE(model.rowCount(child), 1);
        model.insertRow(1, child);
        QCOMPARE(model.rowCount(child), 2);
        QCOMPARE(dataChangedSpy.count(), 0);
    }

    // Root not expanded, insert in root, expect datachanged
    {
        QVERIFY(!tma.isExpanded(root));
        QSignalSpy dataChangedSpy(&tma, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QCOMPARE(dataChangedSpy.count(), 0);
        QCOMPARE(model.rowCount(root), 1);
        model.insertRow(1, root);
        QCOMPARE(model.rowCount(root), 2);
        QCOMPARE(dataChangedSpy.count(), 1);
    }

    // Root not expanded, child not expanded, remove in child, expect no datachanged
    {
        QVERIFY(!tma.isExpanded(root));
        QVERIFY(!tma.isExpanded(child));
        QSignalSpy dataChangedSpy(&tma, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QCOMPARE(dataChangedSpy.count(), 0);
        QCOMPARE(model.rowCount(child), 2);
        model.removeRow(1, child);
        QCOMPARE(model.rowCount(child), 1);
        QCOMPARE(dataChangedSpy.count(), 0);
    }

    // Root not expanded, remove in root, expected datachanged on hasChildren
    {
        QVERIFY(!tma.isExpanded(root));
        QSignalSpy dataChangedSpy(&tma, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        QCOMPARE(dataChangedSpy.count(), 0);
        QCOMPARE(model.rowCount(root), 2);
        model.removeRow(1, root);
        QCOMPARE(model.rowCount(root), 1);
        QCOMPARE(dataChangedSpy.count(), 1);
    }
}

QTEST_MAIN(tst_QQuickTreeModelAdaptor)
#include "tst_qquicktreemodeladaptor.moc"
