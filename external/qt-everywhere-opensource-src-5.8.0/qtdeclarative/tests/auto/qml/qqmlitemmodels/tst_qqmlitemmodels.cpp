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

#include <qtest.h>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QDebug>
#include <QStringListModel>
#include "../../shared/util.h"
#include "testtypes.h"
#include "qtestmodel.h"

#define INIT_TEST_OBJECT(fileName, object) \
    QQmlComponent component_##object(&engine, testFileUrl(fileName)); \
    QScopedPointer<ItemModelsTest>object(qobject_cast<ItemModelsTest *>(component_##object.create())); \


class tst_qqmlitemmodels : public QQmlDataTest
{
    Q_OBJECT

public:
    tst_qqmlitemmodels() {}

private slots:
    void initTestCase();

    void modelIndex();
    void persistentModelIndex();
    void modelIndexConversion();
    void itemSelectionRange();
    void itemSelection();
    void modelIndexList();

private:
    QQmlEngine engine;
};

void tst_qqmlitemmodels::initTestCase()
{
    QQmlDataTest::initTestCase();
    qmlRegisterType<ItemModelsTest>("Test", 1, 0, "ItemModelsTest");
}

void tst_qqmlitemmodels::modelIndex()
{
    INIT_TEST_OBJECT("modelindex.qml", object);
    TestModel model(10, 10);

    QModelIndex index = object->modelIndex();
    for (int i = 0; i < 5; i++) {
        QCOMPARE(object->property("isValid").toBool(), index.isValid());
        QCOMPARE(object->property("row").toInt(), index.row());
        QCOMPARE(object->property("column").toInt(), index.column());
        QCOMPARE(object->property("parent").toModelIndex(), index.parent());
        QCOMPARE(object->property("model").value<QAbstractItemModel *>(), index.model());
        QCOMPARE(object->property("internalId").toULongLong(), index.internalId());

        if (i < 3) {
            index = model.index(2 + i, 4 - i, index);
            object->setModelIndex(index);
        } else if (i < 4) {
            index = model.index(2 + i, 4 - i);
            object->emitSignalWithModelIndex(index);
        }
    }
}

void tst_qqmlitemmodels::persistentModelIndex()
{
    INIT_TEST_OBJECT("persistentmodelindex.qml", object);
    TestModel model(10, 10);

    QPersistentModelIndex index = object->persistentModelIndex();
    for (int i = 0; i < 5; i++) {
        QCOMPARE(object->property("isValid").toBool(), index.isValid());
        QCOMPARE(object->property("row").toInt(), index.row());
        QCOMPARE(object->property("column").toInt(), index.column());
        QCOMPARE(object->property("parent").toModelIndex(), index.parent());
        QCOMPARE(object->property("model").value<QAbstractItemModel *>(), index.model());
        QCOMPARE(object->property("internalId").toULongLong(), index.internalId());

        if (i < 2) {
            index = model.index(2 + i, 4 - i, index);
            object->setPersistentModelIndex(index);
        } else if (i < 3) {
            model.removeRow(2);
            QVERIFY(!index.isValid()); // QPersistentModelIndex should update
            object->emitChanged(); // Help QML get the new values as QPMI doesn't emit anything
        } else if (i < 4) {
            index = model.index(2 + i, 4 - i);
            object->emitSignalWithPersistentModelIndex(index);
        }
    }

    const QVariant &pmiVariant = object->property("pmi");
    QCOMPARE(pmiVariant.userType(), qMetaTypeId<QPersistentModelIndex>());
    QCOMPARE(pmiVariant.value<QPersistentModelIndex>(), QPersistentModelIndex(model.index(0, 0)));
}

void tst_qqmlitemmodels::itemSelectionRange()
{
    INIT_TEST_OBJECT("itemselectionrange.qml", object);
    TestModel model(10, 10);

    for (int i = 0; i < 2; i++) {
        const QVariant &isrVariant = object->property("itemSelectionRange");
        QCOMPARE(isrVariant.userType(), qMetaTypeId<QItemSelectionRange>());
        const QItemSelectionRange &isr = isrVariant.value<QItemSelectionRange>();
        if (i > 0) {
            QModelIndex parentIndex = model.index(0, 0);
            QCOMPARE(QModelIndex(isr.topLeft()), model.index(3, 0, parentIndex));
            QCOMPARE(QModelIndex(isr.bottomRight()), model.index(5, 6, parentIndex));
        } else {
            QCOMPARE(QModelIndex(isr.topLeft()), QModelIndex());
            QCOMPARE(QModelIndex(isr.bottomRight()), QModelIndex());
        }

        QCOMPARE(object->property("top").toInt(), isr.top());
        QCOMPARE(object->property("left").toInt(), isr.left());
        QCOMPARE(object->property("bottom").toInt(), isr.bottom());
        QCOMPARE(object->property("right").toInt(), isr.right());
        QCOMPARE(object->property("width").toInt(), isr.width());
        QCOMPARE(object->property("height").toInt(), isr.height());
        QCOMPARE(object->property("isValid").toBool(), isr.isValid());
        QCOMPARE(object->property("isEmpty").toBool(), isr.isEmpty());
        QCOMPARE(object->property("isrModel").value<QAbstractItemModel *>(), isr.model());

        // Set model for the 2nd iteration and test again
        object->setModel(&model);
    }

    // Check API function calls
    QVERIFY(object->property("contains1").toBool());
    QVERIFY(object->property("contains2").toBool());
    QVERIFY(!object->property("intersects").toBool());
    const QVariant &isrVariant = object->property("intersected");
    QCOMPARE(isrVariant.userType(), qMetaTypeId<QItemSelectionRange>());
}

void tst_qqmlitemmodels::modelIndexConversion()
{
    INIT_TEST_OBJECT("modelindexconversion.qml", object);
    TestModel model(10, 10);
    object->setModel(&model);

    QCOMPARE(object->modelIndex(), model.index(0, 0));
    QCOMPARE(object->persistentModelIndex(), QPersistentModelIndex(model.index(1, 1)));
}

void tst_qqmlitemmodels::itemSelection()
{
    INIT_TEST_OBJECT("itemselection.qml", object);
    TestModel model(10, 10);

    object->setModel(&model);
    QCOMPARE(object->property("count").toInt(), 5);
    QCOMPARE(object->property("contains").toBool(), true);

    const char *propNames[] = { "itemSelectionRead", "itemSelectionBinding", 0 };
    for (const char **name = propNames; *name; name++) {
        QVariant isVariant = object->property(*name);
        QCOMPARE(isVariant.userType(), qMetaTypeId<QItemSelection>());

        const QItemSelection &sel = isVariant.value<QItemSelection>();
        QCOMPARE(sel.count(), object->itemSelection().count());
        QCOMPARE(sel, object->itemSelection());
    }
}

void tst_qqmlitemmodels::modelIndexList()
{
    INIT_TEST_OBJECT("modelindexlist.qml", object);
    TestModel model(10, 10);
    model.fetchMore(QModelIndex());

    object->setModel(&model);
    QVERIFY(object->property("propIsArray").toBool());
    QVERIFY(object->property("varPropIsArray").toBool());
    QVERIFY(object->property("varIsArray").toBool());

    QCOMPARE(object->property("count").toInt(), 10);
    const QModelIndexList &mil = object->modelIndexList();
    QCOMPARE(mil.count(), 4);
    for (int i = 0; i < 3; i++)
        QCOMPARE(mil.at(i), model.index(2 + i, 2 + i));
    QCOMPARE(mil.at(3), QModelIndex()); // The string inserted at the end should result in an invalid index
    QCOMPARE(mil.at(0), object->modelIndex());

    QVariant cppMILVariant = object->property("modelIndexListCopy");
    QCOMPARE(cppMILVariant.userType(), qMetaTypeId<QModelIndexList>());
    QModelIndexList someMIL = object->someModelIndexList();
    QCOMPARE(cppMILVariant.value<QModelIndexList>(), someMIL);

    const char *propNames[] = { "modelIndexListRead", "modelIndexListBinding", 0 };
    for (const char **name = propNames; *name; name++) {
        QVariant milVariant = object->property(*name);
        QCOMPARE(milVariant.userType(), qMetaTypeId<QModelIndexList>());

        const QModelIndexList &milProp = milVariant.value<QModelIndexList>();
        QCOMPARE(milProp.count(), mil.count());
        QCOMPARE(milProp, mil);
    }
}

#undef INIT_TEST_OBJECT

QTEST_MAIN(tst_qqmlitemmodels)

#include "tst_qqmlitemmodels.moc"
