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
#include <QStandardItemModel>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/qdeclarativecontext.h>
#include <QtDeclarative/qdeclarativeexpression.h>
#include <QtDeclarative/qdeclarativeview.h>
#include <private/qdeclarativelistview_p.h>
#include <private/qdeclarativetext_p.h>
#include <private/qdeclarativevisualitemmodel_p.h>
#include <private/qdeclarativevaluetype_p.h>
#include <math.h>

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

class SingleRoleModel : public QAbstractListModel
{
    Q_OBJECT

public:
    SingleRoleModel(QObject * /* parent */ = 0) {
        QHash<int, QByteArray> roles;
        roles.insert(Qt::DisplayRole , "name");
        setRoleNames(roles);
        list << "one" << "two" << "three" << "four";
    }

    void emitMove(int sourceFirst, int sourceLast, int destinationChild) {
        emit beginMoveRows(QModelIndex(), sourceFirst, sourceLast, QModelIndex(), destinationChild);
        emit endMoveRows();
    }

    QStringList list;

public slots:
    void set(int idx, QString string) {
        list[idx] = string;
        emit dataChanged(index(idx,0), index(idx,0));
    }

protected:
    int rowCount(const QModelIndex & /* parent */ = QModelIndex()) const {
        return list.count();
    }
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const {
        if (role == Qt::DisplayRole)
            return list.at(index.row());
        return QVariant();
    }
};


class tst_qdeclarativevisualdatamodel : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativevisualdatamodel();

private slots:
    void rootIndex();
    void updateLayout();
    void childChanged();
    void objectListModel();
    void singleRole();
    void modelProperties();
    void noDelegate();
    void qaimRowsMoved();
    void qaimRowsMoved_data();

private:
    QDeclarativeEngine engine;
    template<typename T>
    T *findItem(QGraphicsObject *parent, const QString &objectName, int index);
};

class DataObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString color READ color WRITE setColor NOTIFY colorChanged)

public:
    DataObject(QObject *parent=0) : QObject(parent) {}
    DataObject(const QString &name, const QString &color, QObject *parent=0)
        : QObject(parent), m_name(name), m_color(color) { }


    QString name() const { return m_name; }
    void setName(const QString &name) {
        if (name != m_name) {
            m_name = name;
            emit nameChanged();
        }
    }

    QString color() const { return m_color; }
    void setColor(const QString &color) {
        if (color != m_color) {
            m_color = color;
            emit colorChanged();
        }
    }

signals:
    void nameChanged();
    void colorChanged();

private:
    QString m_name;
    QString m_color;
};

tst_qdeclarativevisualdatamodel::tst_qdeclarativevisualdatamodel()
{
}

void tst_qdeclarativevisualdatamodel::rootIndex()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/visualdatamodel.qml"));

    QStandardItemModel model;
    initStandardTreeModel(&model);

    engine.rootContext()->setContextProperty("myModel", &model);

    QDeclarativeVisualDataModel *obj = qobject_cast<QDeclarativeVisualDataModel*>(c.create());
    QVERIFY(obj != 0);

    QMetaObject::invokeMethod(obj, "setRoot");
    QVERIFY(qvariant_cast<QModelIndex>(obj->rootIndex()) == model.index(0,0));

    QMetaObject::invokeMethod(obj, "setRootToParent");
    QVERIFY(qvariant_cast<QModelIndex>(obj->rootIndex()) == QModelIndex());

    QMetaObject::invokeMethod(obj, "setRoot");
    QVERIFY(qvariant_cast<QModelIndex>(obj->rootIndex()) == model.index(0,0));
    model.clear(); // will emit modelReset()
    QVERIFY(qvariant_cast<QModelIndex>(obj->rootIndex()) == QModelIndex());

    delete obj;
}

void tst_qdeclarativevisualdatamodel::updateLayout()
{
    QDeclarativeView view;

    QStandardItemModel model;
    initStandardTreeModel(&model);

    view.rootContext()->setContextProperty("myModel", &model);

    view.setSource(QUrl::fromLocalFile(SRCDIR "/data/datalist.qml"));
    qDebug() << "setSource";

    QDeclarativeListView *listview = qobject_cast<QDeclarativeListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QDeclarativeItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QDeclarativeText *name = findItem<QDeclarativeText>(contentItem, "display", 0);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 1 Item"));
    name = findItem<QDeclarativeText>(contentItem, "display", 1);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 2 Item"));
    name = findItem<QDeclarativeText>(contentItem, "display", 2);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 3 Item"));

    model.invisibleRootItem()->sortChildren(0, Qt::DescendingOrder);

    name = findItem<QDeclarativeText>(contentItem, "display", 0);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 3 Item"));
    name = findItem<QDeclarativeText>(contentItem, "display", 1);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 2 Item"));
    name = findItem<QDeclarativeText>(contentItem, "display", 2);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 1 Item"));
}

void tst_qdeclarativevisualdatamodel::childChanged()
{
    QDeclarativeView view;

    QStandardItemModel model;
    initStandardTreeModel(&model);

    view.rootContext()->setContextProperty("myModel", &model);

    view.setSource(QUrl::fromLocalFile(SRCDIR "/data/datalist.qml"));

    QDeclarativeListView *listview = qobject_cast<QDeclarativeListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QDeclarativeItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QDeclarativeVisualDataModel *vdm = listview->findChild<QDeclarativeVisualDataModel*>("visualModel");
    vdm->setRootIndex(QVariant::fromValue(model.indexFromItem(model.item(1,0))));

    QDeclarativeText *name = findItem<QDeclarativeText>(contentItem, "display", 0);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 2 Child Item"));

    model.item(1,0)->child(0,0)->setText("Row 2 updated child");

    name = findItem<QDeclarativeText>(contentItem, "display", 0);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 2 updated child"));

    model.item(1,0)->appendRow(new QStandardItem(QLatin1String("Row 2 Child Item 2")));
    QTest::qWait(300);

    name = findItem<QDeclarativeText>(contentItem, "display", 1);
    QVERIFY(name != 0);
    QCOMPARE(name->text(), QString("Row 2 Child Item 2"));

    model.item(1,0)->takeRow(1);
    name = findItem<QDeclarativeText>(contentItem, "display", 1);
    QVERIFY(name == 0);

    vdm->setRootIndex(QVariant::fromValue(QModelIndex()));
    QTest::qWait(300);
    name = findItem<QDeclarativeText>(contentItem, "display", 0);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 1 Item"));
    name = findItem<QDeclarativeText>(contentItem, "display", 1);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 2 Item"));
    name = findItem<QDeclarativeText>(contentItem, "display", 2);
    QVERIFY(name);
    QCOMPARE(name->text(), QString("Row 3 Item"));
}

void tst_qdeclarativevisualdatamodel::objectListModel()
{
    QDeclarativeView view;

    QList<QObject*> dataList;
    dataList.append(new DataObject("Item 1", "red"));
    dataList.append(new DataObject("Item 2", "green"));
    dataList.append(new DataObject("Item 3", "blue"));
    dataList.append(new DataObject("Item 4", "yellow"));

    QDeclarativeContext *ctxt = view.rootContext();
    ctxt->setContextProperty("myModel", QVariant::fromValue(dataList));

    view.setSource(QUrl::fromLocalFile(SRCDIR "/data/objectlist.qml"));

    QDeclarativeListView *listview = qobject_cast<QDeclarativeListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QDeclarativeItem *contentItem = listview->contentItem();
    QVERIFY(contentItem != 0);

    QDeclarativeText *name = findItem<QDeclarativeText>(contentItem, "name", 0);
    QCOMPARE(name->text(), QString("Item 1"));

    QDeclarativeText *section = findItem<QDeclarativeText>(contentItem, "section", 0);
    QCOMPARE(section->text(), QString("Item 1"));

    dataList[0]->setProperty("name", QLatin1String("Changed"));
    QCOMPARE(name->text(), QString("Changed"));
}

void tst_qdeclarativevisualdatamodel::singleRole()
{
    {
        QDeclarativeView view;

        SingleRoleModel model;

        QDeclarativeContext *ctxt = view.rootContext();
        ctxt->setContextProperty("myModel", &model);

        view.setSource(QUrl::fromLocalFile(SRCDIR "/data/singlerole1.qml"));

        QDeclarativeListView *listview = qobject_cast<QDeclarativeListView*>(view.rootObject());
        QVERIFY(listview != 0);

        QDeclarativeItem *contentItem = listview->contentItem();
        QVERIFY(contentItem != 0);

        QDeclarativeText *name = findItem<QDeclarativeText>(contentItem, "name", 1);
        QCOMPARE(name->text(), QString("two"));

        model.set(1, "Changed");
        QCOMPARE(name->text(), QString("Changed"));
    }
    {
        QDeclarativeView view;

        SingleRoleModel model;

        QDeclarativeContext *ctxt = view.rootContext();
        ctxt->setContextProperty("myModel", &model);

        view.setSource(QUrl::fromLocalFile(SRCDIR "/data/singlerole2.qml"));

        QDeclarativeListView *listview = qobject_cast<QDeclarativeListView*>(view.rootObject());
        QVERIFY(listview != 0);

        QDeclarativeItem *contentItem = listview->contentItem();
        QVERIFY(contentItem != 0);

        QDeclarativeText *name = findItem<QDeclarativeText>(contentItem, "name", 1);
        QCOMPARE(name->text(), QString("two"));

        model.set(1, "Changed");
        QCOMPARE(name->text(), QString("Changed"));
    }
}

void tst_qdeclarativevisualdatamodel::modelProperties()
{
    {
        QDeclarativeView view;

        SingleRoleModel model;

        QDeclarativeContext *ctxt = view.rootContext();
        ctxt->setContextProperty("myModel", &model);

        view.setSource(QUrl::fromLocalFile(SRCDIR "/data/modelproperties.qml"));

        QDeclarativeListView *listview = qobject_cast<QDeclarativeListView*>(view.rootObject());
        QVERIFY(listview != 0);

        QDeclarativeItem *contentItem = listview->contentItem();
        QVERIFY(contentItem != 0);

        QDeclarativeItem *delegate = findItem<QDeclarativeItem>(contentItem, "delegate", 1);
        QCOMPARE(delegate->property("test1").toString(),QString("two"));
        QCOMPARE(delegate->property("test2").toString(),QString("two"));
        QCOMPARE(delegate->property("test3").toString(),QString("two"));
        QCOMPARE(delegate->property("test4").toString(),QString("two"));
        QVERIFY(!delegate->property("test9").isValid());
        QCOMPARE(delegate->property("test5").toString(),QString(""));
        QVERIFY(delegate->property("test6").value<QObject*>() != 0);
        QCOMPARE(delegate->property("test7").toInt(),1);
        QCOMPARE(delegate->property("test8").toInt(),1);
    }

    {
        QDeclarativeView view;

        QList<QObject*> dataList;
        dataList.append(new DataObject("Item 1", "red"));
        dataList.append(new DataObject("Item 2", "green"));
        dataList.append(new DataObject("Item 3", "blue"));
        dataList.append(new DataObject("Item 4", "yellow"));

        QDeclarativeContext *ctxt = view.rootContext();
        ctxt->setContextProperty("myModel", QVariant::fromValue(dataList));

        view.setSource(QUrl::fromLocalFile(SRCDIR "/data/modelproperties.qml"));

        QDeclarativeListView *listview = qobject_cast<QDeclarativeListView*>(view.rootObject());
        QVERIFY(listview != 0);

        QDeclarativeItem *contentItem = listview->contentItem();
        QVERIFY(contentItem != 0);

        QDeclarativeItem *delegate = findItem<QDeclarativeItem>(contentItem, "delegate", 1);
        QCOMPARE(delegate->property("test1").toString(),QString("Item 2"));
        QEXPECT_FAIL("", "QTBUG-13576", Continue);
        QCOMPARE(delegate->property("test2").toString(),QString("Item 2"));
        QVERIFY(qobject_cast<DataObject*>(delegate->property("test3").value<QObject*>()) != 0);
        QVERIFY(qobject_cast<DataObject*>(delegate->property("test4").value<QObject*>()) != 0);
        QCOMPARE(delegate->property("test5").toString(),QString("Item 2"));
        QCOMPARE(delegate->property("test9").toString(),QString("Item 2"));
        QVERIFY(delegate->property("test6").value<QObject*>() != 0);
        QCOMPARE(delegate->property("test7").toInt(),1);
        QCOMPARE(delegate->property("test8").toInt(),1);
    }

    {
        QDeclarativeView view;

        QStandardItemModel model;
        initStandardTreeModel(&model);

        view.rootContext()->setContextProperty("myModel", &model);

        QUrl source(QUrl::fromLocalFile(SRCDIR "/data/modelproperties2.qml"));

        //3 items, 3 warnings each
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":11: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":11: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":11: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":9: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":9: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":9: ReferenceError: Can't find variable: modelData");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":15: TypeError: Result of expression 'model.modelData' [undefined] is not an object.");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":15: TypeError: Result of expression 'model.modelData' [undefined] is not an object.");
        QTest::ignoreMessage(QtWarningMsg, source.toString().toLatin1() + ":15: TypeError: Result of expression 'model.modelData' [undefined] is not an object.");

        view.setSource(source);

        QDeclarativeListView *listview = qobject_cast<QDeclarativeListView*>(view.rootObject());
        QVERIFY(listview != 0);

        QDeclarativeItem *contentItem = listview->contentItem();
        QVERIFY(contentItem != 0);

        QDeclarativeItem *delegate = findItem<QDeclarativeItem>(contentItem, "delegate", 1);
        QCOMPARE(delegate->property("test1").toString(),QString("Row 2 Item"));
        QCOMPARE(delegate->property("test2").toString(),QString("Row 2 Item"));
        QVERIFY(!delegate->property("test3").isValid());
        QVERIFY(!delegate->property("test4").isValid());
        QVERIFY(!delegate->property("test5").isValid());
        QVERIFY(!delegate->property("test9").isValid());
        QVERIFY(delegate->property("test6").value<QObject*>() != 0);
        QCOMPARE(delegate->property("test7").toInt(),1);
        QCOMPARE(delegate->property("test8").toInt(),1);
    }

    //### should also test QStringList and QVariantList
}

void tst_qdeclarativevisualdatamodel::noDelegate()
{
    QDeclarativeView view;

    QStandardItemModel model;
    initStandardTreeModel(&model);

    view.rootContext()->setContextProperty("myModel", &model);

    view.setSource(QUrl::fromLocalFile(SRCDIR "/data/datalist.qml"));

    QDeclarativeListView *listview = qobject_cast<QDeclarativeListView*>(view.rootObject());
    QVERIFY(listview != 0);

    QDeclarativeVisualDataModel *vdm = listview->findChild<QDeclarativeVisualDataModel*>("visualModel");
    QVERIFY(vdm != 0);
    QCOMPARE(vdm->count(), 3);

    vdm->setDelegate(0);
    QCOMPARE(vdm->count(), 0);
}


void tst_qdeclarativevisualdatamodel::qaimRowsMoved()
{
    // Test parameters passed in QAIM::rowsMoved() signal are converted correctly
    // when translated and emitted as the QListModelInterface::itemsMoved() signal
    QFETCH(int, sourceFirst);
    QFETCH(int, sourceLast);
    QFETCH(int, destinationChild);
    QFETCH(int, expectFrom);
    QFETCH(int, expectTo);
    QFETCH(int, expectCount);

    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/visualdatamodel.qml"));

    SingleRoleModel model;
    model.list.clear();
    for (int i=0; i<30; i++)
        model.list << (QStringLiteral("item ") + QString::number(i));
    engine.rootContext()->setContextProperty("myModel", &model);

    QDeclarativeVisualDataModel *obj = qobject_cast<QDeclarativeVisualDataModel*>(c.create());
    QVERIFY(obj != 0);

    QSignalSpy spy(obj, SIGNAL(itemsMoved(int,int,int)));
    model.emitMove(sourceFirst, sourceLast, destinationChild);
    QTRY_COMPARE(spy.count(), 1);

    QCOMPARE(spy[0].count(), 3);
    QCOMPARE(spy[0][0].toInt(), expectFrom);
    QCOMPARE(spy[0][1].toInt(), expectTo);
    QCOMPARE(spy[0][2].toInt(), expectCount);

    delete obj;
}

void tst_qdeclarativevisualdatamodel::qaimRowsMoved_data()
{
    QTest::addColumn<int>("sourceFirst");
    QTest::addColumn<int>("sourceLast");
    QTest::addColumn<int>("destinationChild");
    QTest::addColumn<int>("expectFrom");
    QTest::addColumn<int>("expectTo");
    QTest::addColumn<int>("expectCount");

    QTest::newRow("move 1 forward")
        << 1 << 1 << 6
        << 1 << 5 << 1;

    QTest::newRow("move 1 backwards")
        << 4 << 4 << 1
        << 4 << 1 << 1;

    QTest::newRow("move multiple forwards")
        << 0 << 2 << 13
        << 0 << 10 << 3;

    QTest::newRow("move multiple forwards, with same to")
        << 0 << 1 << 3
        << 0 << 1 << 2;

    QTest::newRow("move multiple backwards")
        << 10 << 14 << 1
        << 10 << 1 << 5;
}


template<typename T>
T *tst_qdeclarativevisualdatamodel::findItem(QGraphicsObject *parent, const QString &objectName, int index)
{
    const QMetaObject &mo = T::staticMetaObject;
    //qDebug() << parent->childItems().count() << "children";
    for (int i = 0; i < parent->childItems().count(); ++i) {
        QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(parent->childItems().at(i));
        if(!item)
            continue;
        //qDebug() << "try" << item;
        if (mo.cast(item) && (objectName.isEmpty() || item->objectName() == objectName)) {
            if (index != -1) {
                QDeclarativeExpression e(qmlContext(item), item, "index");
                if (e.evaluate().toInt() == index)
                    return static_cast<T*>(item);
            } else {
                return static_cast<T*>(item);
            }
        }
        item = findItem<T>(item, objectName, index);
        if (item)
        return static_cast<T*>(item);
    }

    return 0;
}

QTEST_MAIN(tst_qdeclarativevisualdatamodel)

#include "tst_qdeclarativevisualdatamodel.moc"
