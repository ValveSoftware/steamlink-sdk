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
#include <QtDeclarative/qdeclarativeview.h>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/qdeclarativecontext.h>
#include <QtDeclarative/qdeclarativeexpression.h>
#include <QtDeclarative/private/qdeclarativepathview_p.h>
#include <QtDeclarative/private/qdeclarativepath_p.h>
#include <QtDeclarative/private/qdeclarativetext_p.h>
#include <QtDeclarative/private/qdeclarativerectangle_p.h>
#include <QtDeclarative/private/qdeclarativelistmodel_p.h>
#include <QtDeclarative/private/qdeclarativevaluetype_p.h>
#include <QAbstractListModel>
#include <QStringListModel>
#include <QStandardItemModel>
#include <QFile>

template<typename T>
static void qdeclarativepathview_move(int from, int to, int n, T *items)
{
    if (from > to) {
        // Only move forwards - flip if backwards moving
        int tfrom = from;
        int tto = to;
        from = tto;
        to = tto+n;
        n = tfrom-tto;
    }

    T replaced;
    int i=0;
    typename T::ConstIterator it=items->begin(); it += from+n;
    for (; i<to-from; ++i,++it)
        replaced.append(*it);
    i=0;
    it=items->begin(); it += from;
    for (; i<n; ++i,++it)
        replaced.append(*it);
    typename T::ConstIterator f=replaced.begin();
    typename T::Iterator t=items->begin(); t += from;
    for (; f != replaced.end(); ++f, ++t)
        *t = *f;
}

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


class tst_QDeclarativePathView : public QObject
{
    Q_OBJECT
public:
    tst_QDeclarativePathView();

private slots:
    void initValues();
    void items();
    void dataModel();
    void pathview2();
    void pathview3();
    void initialCurrentIndex();
    void insertModel_data();
    void insertModel();
    void removeModel_data();
    void removeModel();
    void moveModel_data();
    void moveModel();
    void path();
    void pathMoved();
    void setCurrentIndex();
    void resetModel();
    void propertyChanges();
    void pathChanges();
    void componentChanges();
    void modelChanges();
    void pathUpdateOnStartChanged();
    void package();
    void emptyModel();
    void closed();
    void pathUpdate();
    void visualDataModel();
    void undefinedPath();
    void mouseDrag();
    void treeModel();
    void changePreferredHighlight();

private:
    QDeclarativeView *createView();
    template<typename T>
    T *findItem(QGraphicsObject *parent, const QString &objectName, int index=-1);
    template<typename T>
    QList<T*> findItems(QGraphicsObject *parent, const QString &objectName);
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

class TestModel : public QAbstractListModel
{
public:
    enum Roles { Name = Qt::UserRole+1, Number = Qt::UserRole+2 };

    TestModel(QObject *parent=0) : QAbstractListModel(parent) {
        QHash<int, QByteArray> roles;
        roles[Name] = "name";
        roles[Number] = "number";
        setRoleNames(roles);
    }

    int rowCount(const QModelIndex &parent=QModelIndex()) const { Q_UNUSED(parent); return list.count(); }
    QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const {
        QVariant rv;
        if (role == Name)
            rv = list.at(index.row()).first;
        else if (role == Number)
            rv = list.at(index.row()).second;

        return rv;
    }

    int count() const { return rowCount(); }
    QString name(int index) const { return list.at(index).first; }
    QString number(int index) const { return list.at(index).second; }

    void addItem(const QString &name, const QString &number) {
        beginInsertRows(QModelIndex(), list.count(), list.count());
        list.append(QPair<QString,QString>(name, number));
        endInsertRows();
    }

    void insertItem(int index, const QString &name, const QString &number) {
        beginInsertRows(QModelIndex(), index, index);
        list.insert(index, QPair<QString,QString>(name, number));
        endInsertRows();
    }

    void insertItems(int index, const QList<QPair<QString, QString> > &items)
    {
        emit beginInsertRows(QModelIndex(), index, index+items.count()-1);
        for (int i=0; i<items.count(); i++)
            list.insert(index + i, QPair<QString,QString>(items[i].first, items[i].second));
        emit endInsertRows();
    }

    void removeItem(int index) {
        beginRemoveRows(QModelIndex(), index, index);
        list.removeAt(index);
        endRemoveRows();
    }

    void removeItems(int index, int count)
    {
        emit beginRemoveRows(QModelIndex(), index, index+count-1);
        while (count--)
            list.removeAt(index);
        emit endRemoveRows();
    }

    void moveItem(int from, int to) {
        beginMoveRows(QModelIndex(), from, from, QModelIndex(), to);
        list.move(from, to);
        endMoveRows();
    }

    void moveItems(int from, int to, int count)
    {
        emit beginMoveRows(QModelIndex(), from, from+count-1, QModelIndex(), to > from ? to+count : to);
        qdeclarativepathview_move(from, to, count, &list);
        emit endMoveRows();
    }

    void modifyItem(int idx, const QString &name, const QString &number) {
        list[idx] = QPair<QString,QString>(name, number);
        emit dataChanged(index(idx,0), index(idx,0));
    }

private:
    QList<QPair<QString,QString> > list;
};


tst_QDeclarativePathView::tst_QDeclarativePathView()
{
}

void tst_QDeclarativePathView::initValues()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/pathview1.qml"));
    QDeclarativePathView *obj = qobject_cast<QDeclarativePathView*>(c.create());

    QVERIFY(obj != 0);
    QVERIFY(obj->path() == 0);
    QVERIFY(obj->delegate() == 0);
    QCOMPARE(obj->model(), QVariant());
    QCOMPARE(obj->currentIndex(), 0);
    QCOMPARE(obj->offset(), 0.);
    QCOMPARE(obj->preferredHighlightBegin(), 0.);
    QCOMPARE(obj->dragMargin(), 0.);
    QCOMPARE(obj->count(), 0);
    QCOMPARE(obj->pathItemCount(), -1);
}

void tst_QDeclarativePathView::items()
{
    QDeclarativeView *canvas = createView();

    TestModel model;
    model.addItem("Fred", "12345");
    model.addItem("John", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");

    QDeclarativeContext *ctxt = canvas->rootContext();
    ctxt->setContextProperty("testModel", &model);

    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/pathview0.qml"));
    qApp->processEvents();

    QDeclarativePathView *pathview = findItem<QDeclarativePathView>(canvas->rootObject(), "view");
    QVERIFY(pathview != 0);

    QCOMPARE(pathview->count(), model.count());
    QCOMPARE(canvas->rootObject()->property("count").toInt(), model.count());
    QCOMPARE(pathview->childItems().count(), model.count()+1); // assumes all are visible, including highlight

    for (int i = 0; i < model.count(); ++i) {
        QDeclarativeText *name = findItem<QDeclarativeText>(pathview, "textName", i);
        QVERIFY(name != 0);
        QCOMPARE(name->text(), model.name(i));
        QDeclarativeText *number = findItem<QDeclarativeText>(pathview, "textNumber", i);
        QVERIFY(number != 0);
        QCOMPARE(number->text(), model.number(i));
    }

    QDeclarativePath *path = qobject_cast<QDeclarativePath*>(pathview->path());
    QVERIFY(path);

    QVERIFY(pathview->highlightItem());
    QPointF start = path->pointAt(0.0);
    QPointF offset;
    offset.setX(pathview->highlightItem()->width()/2);
    offset.setY(pathview->highlightItem()->height()/2);
    QCOMPARE(pathview->highlightItem()->pos() + offset, start);

    delete canvas;
}

void tst_QDeclarativePathView::pathview2()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/pathview2.qml"));
    QDeclarativePathView *obj = qobject_cast<QDeclarativePathView*>(c.create());

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
}

void tst_QDeclarativePathView::pathview3()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/pathview3.qml"));
    QDeclarativePathView *obj = qobject_cast<QDeclarativePathView*>(c.create());

    QVERIFY(obj != 0);
    QVERIFY(obj->path() != 0);
    QVERIFY(obj->delegate() != 0);
    QVERIFY(obj->model() != QVariant());
    QCOMPARE(obj->currentIndex(), 0);
    QCOMPARE(obj->offset(), 1.0);
    QCOMPARE(obj->preferredHighlightBegin(), 0.5);
    QCOMPARE(obj->dragMargin(), 24.);
    QCOMPARE(obj->count(), 8);
    QCOMPARE(obj->pathItemCount(), 4);
}

void tst_QDeclarativePathView::initialCurrentIndex()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/initialCurrentIndex.qml"));
    QDeclarativePathView *obj = qobject_cast<QDeclarativePathView*>(c.create());

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

void tst_QDeclarativePathView::insertModel_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("idx");
    QTest::addColumn<int>("count");
    QTest::addColumn<qreal>("offset");
    QTest::addColumn<int>("currentIndex");

    // We have 8 items, with currentIndex == 4
    QTest::newRow("insert after current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 6 << 1 << qreal(5.) << 4;
    QTest::newRow("insert before current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 2 << 1 << qreal(4.) << 5;
    QTest::newRow("insert multiple after current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 5 << 2 << qreal(6.) << 4;
    QTest::newRow("insert multiple before current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 1 << 2 << qreal(4.) << 6;
    QTest::newRow("insert at end")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 8 << 1 << qreal(5.) << 4;
    QTest::newRow("insert at beginning")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 0 << 1 << qreal(4.) << 5;
    QTest::newRow("insert at current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 4 << 1 << qreal(4.) << 5;

    QTest::newRow("no range - insert after current")
        << int(QDeclarativePathView::NoHighlightRange) << 6 << 1 << qreal(5.) << 4;
    QTest::newRow("no range - insert before current")
        << int(QDeclarativePathView::NoHighlightRange) << 2 << 1 << qreal(4.) << 5;
    QTest::newRow("no range - insert multiple after current")
        << int(QDeclarativePathView::NoHighlightRange) << 5 << 2 << qreal(6.) << 4;
    QTest::newRow("no range - insert multiple before current")
        << int(QDeclarativePathView::NoHighlightRange) << 1 << 2 << qreal(4.) << 6;
    QTest::newRow("no range - insert at end")
        << int(QDeclarativePathView::NoHighlightRange) << 8 << 1 << qreal(5.) << 4;
    QTest::newRow("no range - insert at beginning")
        << int(QDeclarativePathView::NoHighlightRange) << 0 << 1 << qreal(4.) << 5;
    QTest::newRow("no range - insert at current")
        << int(QDeclarativePathView::NoHighlightRange) << 4 << 1 << qreal(4.) << 5;
}

void tst_QDeclarativePathView::insertModel()
{
    QFETCH(int, mode);
    QFETCH(int, idx);
    QFETCH(int, count);
    QFETCH(qreal, offset);
    QFETCH(int, currentIndex);

    QDeclarativeView *window = createView();
    window->show();

    TestModel model;
    model.addItem("Ben", "12345");
    model.addItem("Bohn", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");
    model.addItem("Jinny", "679");
    model.addItem("Milly", "73378");
    model.addItem("Jimmy", "3535");
    model.addItem("Barb", "9039");

    QDeclarativeContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(QUrl::fromLocalFile(SRCDIR "/data/pathview0.qml"));
    qApp->processEvents();

    QDeclarativePathView *pathview = findItem<QDeclarativePathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    pathview->setHighlightRangeMode((QDeclarativePathView::HighlightRangeMode)mode);

    pathview->setCurrentIndex(4);
    if (mode == QDeclarativePathView::StrictlyEnforceRange)
        QTRY_COMPARE(pathview->offset(), 4.0);
    else
        pathview->setOffset(4);

    QList<QPair<QString, QString> > items;
    for (int i = 0; i < count; ++i)
        items.append(qMakePair(QString("New"), QString::number(i)));

    model.insertItems(idx, items);
    QTRY_COMPARE(pathview->offset(), offset);

    QCOMPARE(pathview->currentIndex(), currentIndex);

    delete window;
}

void tst_QDeclarativePathView::path()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/pathtest.qml"));
    QDeclarativePath *obj = qobject_cast<QDeclarativePath*>(c.create());

    QVERIFY(obj != 0);
    QCOMPARE(obj->startX(), 120.);
    QCOMPARE(obj->startY(), 100.);
    QVERIFY(obj->path() != QPainterPath());

    QDeclarativeListReference list(obj, "pathElements");
    QCOMPARE(list.count(), 5);

    QDeclarativePathAttribute* attr = qobject_cast<QDeclarativePathAttribute*>(list.at(0));
    QVERIFY(attr != 0);
    QCOMPARE(attr->name(), QString("scale"));
    QCOMPARE(attr->value(), 1.0);

    QDeclarativePathQuad* quad = qobject_cast<QDeclarativePathQuad*>(list.at(1));
    QVERIFY(quad != 0);
    QCOMPARE(quad->x(), 120.);
    QCOMPARE(quad->y(), 25.);
    QCOMPARE(quad->controlX(), 260.);
    QCOMPARE(quad->controlY(), 75.);

    QDeclarativePathPercent* perc = qobject_cast<QDeclarativePathPercent*>(list.at(2));
    QVERIFY(perc != 0);
    QCOMPARE(perc->value(), 0.3);

    QDeclarativePathLine* line = qobject_cast<QDeclarativePathLine*>(list.at(3));
    QVERIFY(line != 0);
    QCOMPARE(line->x(), 120.);
    QCOMPARE(line->y(), 100.);

    QDeclarativePathCubic* cubic = qobject_cast<QDeclarativePathCubic*>(list.at(4));
    QVERIFY(cubic != 0);
    QCOMPARE(cubic->x(), 180.);
    QCOMPARE(cubic->y(), 0.);
    QCOMPARE(cubic->control1X(), -10.);
    QCOMPARE(cubic->control1Y(), 90.);
    QCOMPARE(cubic->control2X(), 210.);
    QCOMPARE(cubic->control2Y(), 90.);
}

void tst_QDeclarativePathView::removeModel_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("idx");
    QTest::addColumn<int>("count");
    QTest::addColumn<qreal>("offset");
    QTest::addColumn<int>("currentIndex");

    // We have 8 items, with currentIndex == 4
    QTest::newRow("remove after current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 6 << 1 << qreal(3.) << 4;
    QTest::newRow("remove before current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 2 << 1 << qreal(4.) << 3;
    QTest::newRow("remove multiple after current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 5 << 2 << qreal(2.) << 4;
    QTest::newRow("remove multiple before current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 1 << 2 << qreal(4.) << 2;
    QTest::newRow("remove last")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 7 << 1 << qreal(3.) << 4;
    QTest::newRow("remove first")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 0 << 1 << qreal(4.) << 3;
    QTest::newRow("remove current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 4 << 1 << qreal(3.) << 4;
    QTest::newRow("remove all")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 0 << 8 << qreal(0.) << 0;

    QTest::newRow("no range - remove after current")
        << int(QDeclarativePathView::NoHighlightRange) << 6 << 1 << qreal(3.) << 4;
    QTest::newRow("no range - remove before current")
        << int(QDeclarativePathView::NoHighlightRange) << 2 << 1 << qreal(4.) << 3;
    QTest::newRow("no range - remove multiple after current")
        << int(QDeclarativePathView::NoHighlightRange) << 5 << 2 << qreal(2.) << 4;
    QTest::newRow("no range - remove multiple before current")
        << int(QDeclarativePathView::NoHighlightRange) << 1 << 2 << qreal(4.) << 2;
    QTest::newRow("no range - remove last")
        << int(QDeclarativePathView::NoHighlightRange) << 7 << 1 << qreal(3.) << 4;
    QTest::newRow("no range - remove first")
        << int(QDeclarativePathView::NoHighlightRange) << 0 << 1 << qreal(4.) << 3;
    QTest::newRow("no range - remove current offset")
        << int(QDeclarativePathView::NoHighlightRange) << 4 << 1 << qreal(4.) << 4;
    QTest::newRow("no range - remove all")
        << int(QDeclarativePathView::NoHighlightRange) << 0 << 8 << qreal(0.) << 0;
}

void tst_QDeclarativePathView::removeModel()
{
    QFETCH(int, mode);
    QFETCH(int, idx);
    QFETCH(int, count);
    QFETCH(qreal, offset);
    QFETCH(int, currentIndex);

    QDeclarativeView *window = createView();
    window->show();

    TestModel model;
    model.addItem("Ben", "12345");
    model.addItem("Bohn", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");
    model.addItem("Jinny", "679");
    model.addItem("Milly", "73378");
    model.addItem("Jimmy", "3535");
    model.addItem("Barb", "9039");

    QDeclarativeContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(QUrl::fromLocalFile(SRCDIR "/data/pathview0.qml"));
    qApp->processEvents();

    QDeclarativePathView *pathview = findItem<QDeclarativePathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    pathview->setHighlightRangeMode((QDeclarativePathView::HighlightRangeMode)mode);

    pathview->setCurrentIndex(4);
    if (mode == QDeclarativePathView::StrictlyEnforceRange)
        QTRY_COMPARE(pathview->offset(), 4.0);
    else
        pathview->setOffset(4);

    model.removeItems(idx, count);
    QTRY_COMPARE(pathview->offset(), offset);

    QCOMPARE(pathview->currentIndex(), currentIndex);

    delete window;
}
void tst_QDeclarativePathView::moveModel_data()
{
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("from");
    QTest::addColumn<int>("to");
    QTest::addColumn<int>("count");
    QTest::addColumn<int>("currentIndex");

    // We have 8 items, with currentIndex == 4
    QTest::newRow("move after current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 5 << 6 << 1 << 4;
    QTest::newRow("move before current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 2 << 3 << 1 << 4;
    QTest::newRow("move before current to after")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 2 << 6 << 1 << 3;
    QTest::newRow("move multiple after current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 5 << 6 << 2 << 4;
    QTest::newRow("move multiple before current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 0 << 1 << 2 << 4;
    QTest::newRow("move before current to end")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 2 << 7 << 1 << 3;
    QTest::newRow("move last to beginning")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 7 << 0 << 1 << 5;
    QTest::newRow("move current")
        << int(QDeclarativePathView::StrictlyEnforceRange) << 4 << 6 << 1 << 6;

    QTest::newRow("no range - move after current")
        << int(QDeclarativePathView::NoHighlightRange) << 5 << 6 << 1 << 4;
    QTest::newRow("no range - move before current")
        << int(QDeclarativePathView::NoHighlightRange) << 2 << 3 << 1 << 4;
    QTest::newRow("no range - move before current to after")
        << int(QDeclarativePathView::NoHighlightRange) << 2 << 6 << 1 << 3;
    QTest::newRow("no range - move multiple after current")
        << int(QDeclarativePathView::NoHighlightRange) << 5 << 6 << 2 << 4;
    QTest::newRow("no range - move multiple before current")
        << int(QDeclarativePathView::NoHighlightRange) << 0 << 1 << 2 << 4;
    QTest::newRow("no range - move before current to end")
        << int(QDeclarativePathView::NoHighlightRange) << 2 << 7 << 1 << 3;
    QTest::newRow("no range - move last to beginning")
        << int(QDeclarativePathView::NoHighlightRange) << 7 << 0 << 1 << 5;
    QTest::newRow("no range - move current")
        << int(QDeclarativePathView::NoHighlightRange) << 4 << 6 << 1 << 6;
    QTest::newRow("no range - move multiple incl. current")
        << int(QDeclarativePathView::NoHighlightRange) << 0 << 1 << 5 << 5;
}

void tst_QDeclarativePathView::moveModel()
{
    QFETCH(int, mode);
    QFETCH(int, from);
    QFETCH(int, to);
    QFETCH(int, count);
    QFETCH(int, currentIndex);

    QDeclarativeView *window = createView();
    window->show();

    TestModel model;
    model.addItem("Ben", "12345");
    model.addItem("Bohn", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");
    model.addItem("Jinny", "679");
    model.addItem("Milly", "73378");
    model.addItem("Jimmy", "3535");
    model.addItem("Barb", "9039");

    QDeclarativeContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testModel", &model);

    window->setSource(QUrl::fromLocalFile(SRCDIR "/data/pathview0.qml"));
    qApp->processEvents();

    QDeclarativePathView *pathview = findItem<QDeclarativePathView>(window->rootObject(), "view");
    QVERIFY(pathview != 0);

    pathview->setHighlightRangeMode((QDeclarativePathView::HighlightRangeMode)mode);

    pathview->setCurrentIndex(4);
    if (mode == QDeclarativePathView::StrictlyEnforceRange)
        QTRY_COMPARE(pathview->offset(), 4.0);
    else
        pathview->setOffset(4);

    model.moveItems(from, to, count);

    // don't enable this test as 371b2f6947779272494b34ec44445aaad0613756 has
    // not been backported to QtQuick1 so offset is still buggy
//    QTRY_COMPARE(pathview->offset(), offset);

    QCOMPARE(pathview->currentIndex(), currentIndex);

    delete window;
}

void tst_QDeclarativePathView::dataModel()
{
    QDeclarativeView *canvas = createView();

    QDeclarativeContext *ctxt = canvas->rootContext();
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    TestModel model;
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

    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/datamodel.qml"));
    qApp->processEvents();

    QDeclarativePathView *pathview = qobject_cast<QDeclarativePathView*>(canvas->rootObject());
    QVERIFY(pathview != 0);

    QMetaObject::invokeMethod(canvas->rootObject(), "checkProperties");
    QVERIFY(testObject->error() == false);

    QDeclarativeItem *item = findItem<QDeclarativeItem>(pathview, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->x(), 110.0);
    QCOMPARE(item->y(), 10.0);

    model.insertItem(4, "orange", "10");
    QTest::qWait(100);

    QCOMPARE(canvas->rootObject()->property("viewCount").toInt(), model.count());
    QTRY_COMPARE(findItems<QDeclarativeItem>(pathview, "wrapper").count(), 14);

    QVERIFY(pathview->currentIndex() == 0);

    QDeclarativeText *text = findItem<QDeclarativeText>(pathview, "myText", 4);
    QVERIFY(text);
    QCOMPARE(text->text(), model.name(4));

    model.removeItem(2);
    QCOMPARE(canvas->rootObject()->property("viewCount").toInt(), model.count());
    text = findItem<QDeclarativeText>(pathview, "myText", 2);
    QVERIFY(text);
    QCOMPARE(text->text(), model.name(2));

    testObject->setPathItemCount(5);
    QMetaObject::invokeMethod(canvas->rootObject(), "checkProperties");
    QVERIFY(testObject->error() == false);

    QTRY_COMPARE(findItems<QDeclarativeItem>(pathview, "wrapper").count(), 5);

    QDeclarativeRectangle *testItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 4);
    QVERIFY(testItem != 0);
    testItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 5);
    QVERIFY(testItem == 0);

    pathview->setCurrentIndex(1);

    model.insertItem(2, "pink", "2");
    QTest::qWait(100);

    QTRY_COMPARE(findItems<QDeclarativeItem>(pathview, "wrapper").count(), 5);
    QVERIFY(pathview->currentIndex() == 1);

    text = findItem<QDeclarativeText>(pathview, "myText", 2);
    QVERIFY(text);
    QCOMPARE(text->text(), model.name(2));

    model.removeItem(3);
    QTRY_COMPARE(findItems<QDeclarativeItem>(pathview, "wrapper").count(), 5);
    text = findItem<QDeclarativeText>(pathview, "myText", 3);
    QVERIFY(text);
    QCOMPARE(text->text(), model.name(3));

    model.moveItem(3, 5);
    QTRY_COMPARE(findItems<QDeclarativeItem>(pathview, "wrapper").count(), 5);
    QList<QDeclarativeItem*> items = findItems<QDeclarativeItem>(pathview, "wrapper");
    foreach (QDeclarativeItem *item, items) {
        QVERIFY(item->property("onPath").toBool());
    }

    // QTBUG-14199
    pathview->setOffset(7);
    pathview->setOffset(0);
    QCOMPARE(findItems<QDeclarativeItem>(pathview, "wrapper").count(), 5);

    pathview->setCurrentIndex(model.count()-1);
    model.removeItem(model.count()-1);
    QCOMPARE(pathview->currentIndex(), model.count()-1);

    // QTBUG-18825
    // Confirm that the target offset is adjusted when removing items
    pathview->setCurrentIndex(model.count()-1);
    QTRY_COMPARE(pathview->offset(), 1.);
    pathview->setCurrentIndex(model.count()-5);
    model.removeItem(model.count()-1);
    model.removeItem(model.count()-1);
    model.removeItem(model.count()-1);
    QTRY_COMPARE(pathview->offset(), 2.);

    delete canvas;
}

void tst_QDeclarativePathView::pathMoved()
{
    QDeclarativeView *canvas = createView();

    TestModel model;
    model.addItem("Ben", "12345");
    model.addItem("Bohn", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");

    QDeclarativeContext *ctxt = canvas->rootContext();
    ctxt->setContextProperty("testModel", &model);

    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/pathview0.qml"));
    qApp->processEvents();

    QDeclarativePathView *pathview = findItem<QDeclarativePathView>(canvas->rootObject(), "view");
    QVERIFY(pathview != 0);

    QDeclarativeRectangle *firstItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QDeclarativePath *path = qobject_cast<QDeclarativePath*>(pathview->path());
    QVERIFY(path);
    QPointF start = path->pointAt(0.0);
    QPointF offset;//Center of item is at point, but pos is from corner
    offset.setX(firstItem->width()/2);
    offset.setY(firstItem->height()/2);
    QCOMPARE(firstItem->pos() + offset, start);
    pathview->setOffset(1.0);

    for(int i=0; i<model.count(); i++){
        QDeclarativeRectangle *curItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", i);
        QPointF itemPos(path->pointAt(0.25 + i*0.25));
        QCOMPARE(curItem->pos() + offset, QPointF(qRound(itemPos.x()), qRound(itemPos.y())));
    }

    pathview->setOffset(0.0);
    QCOMPARE(firstItem->pos() + offset, start);

    // Change delegate size
    pathview->setOffset(0.1);
    pathview->setOffset(0.0);
    canvas->rootObject()->setProperty("delegateWidth", 30);
    QCOMPARE(firstItem->width(), 30.0);
    offset.setX(firstItem->width()/2);
    QTRY_COMPARE(firstItem->pos() + offset, start);

    // Change delegate scale
    pathview->setOffset(0.1);
    pathview->setOffset(0.0);
    canvas->rootObject()->setProperty("delegateScale", 1.2);
    QTRY_COMPARE(firstItem->pos() + offset, start);

    delete canvas;
}

void tst_QDeclarativePathView::setCurrentIndex()
{
    QDeclarativeView *canvas = createView();

    TestModel model;
    model.addItem("Ben", "12345");
    model.addItem("Bohn", "2345");
    model.addItem("Bob", "54321");
    model.addItem("Bill", "4321");

    QDeclarativeContext *ctxt = canvas->rootContext();
    ctxt->setContextProperty("testModel", &model);

    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/pathview0.qml"));
    qApp->processEvents();

    QDeclarativePathView *pathview = findItem<QDeclarativePathView>(canvas->rootObject(), "view");
    QVERIFY(pathview != 0);

    QDeclarativeRectangle *firstItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QDeclarativePath *path = qobject_cast<QDeclarativePath*>(pathview->path());
    QVERIFY(path);
    QPointF start = path->pointAt(0.0);
    QPointF offset;//Center of item is at point, but pos is from corner
    offset.setX(firstItem->width()/2);
    offset.setY(firstItem->height()/2);
    QCOMPARE(firstItem->pos() + offset, start);
    QCOMPARE(canvas->rootObject()->property("currentA").toInt(), 0);
    QCOMPARE(canvas->rootObject()->property("currentB").toInt(), 0);

    pathview->setCurrentIndex(2);

    firstItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 2);
    QTRY_COMPARE(firstItem->pos() + offset, start);
    QCOMPARE(canvas->rootObject()->property("currentA").toInt(), 2);
    QCOMPARE(canvas->rootObject()->property("currentB").toInt(), 2);

    pathview->decrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 1);
    firstItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 1);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->pos() + offset, start);

    pathview->decrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 0);
    firstItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->pos() + offset, start);

    pathview->decrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 3);
    firstItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 3);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->pos() + offset, start);

    pathview->incrementCurrentIndex();
    QTRY_COMPARE(pathview->currentIndex(), 0);
    firstItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->pos() + offset, start);

    // Test positive indexes are wrapped.
    pathview->setCurrentIndex(6);
    QTRY_COMPARE(pathview->currentIndex(), 2);
    firstItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 2);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->pos() + offset, start);

    // Test negative indexes are wrapped.
    pathview->setCurrentIndex(-3);
    QTRY_COMPARE(pathview->currentIndex(), 1);
    firstItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 1);
    QVERIFY(firstItem);
    QTRY_COMPARE(firstItem->pos() + offset, start);

    delete canvas;
}

void tst_QDeclarativePathView::resetModel()
{
    QDeclarativeView *canvas = createView();

    QStringList strings;
    strings << "one" << "two" << "three";
    QStringListModel model(strings);

    QDeclarativeContext *ctxt = canvas->rootContext();
    ctxt->setContextProperty("testModel", &model);

    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/displaypath.qml"));
    qApp->processEvents();

    QDeclarativePathView *pathview = findItem<QDeclarativePathView>(canvas->rootObject(), "view");
    QVERIFY(pathview != 0);

    QCOMPARE(pathview->count(), model.rowCount());

    for (int i = 0; i < model.rowCount(); ++i) {
        QDeclarativeText *display = findItem<QDeclarativeText>(pathview, "displayText", i);
        QVERIFY(display != 0);
        QCOMPARE(display->text(), strings.at(i));
    }

    strings.clear();
    strings << "four" << "five" << "six" << "seven";
    model.setStringList(strings);

    QCOMPARE(pathview->count(), model.rowCount());

    for (int i = 0; i < model.rowCount(); ++i) {
        QDeclarativeText *display = findItem<QDeclarativeText>(pathview, "displayText", i);
        QVERIFY(display != 0);
        QCOMPARE(display->text(), strings.at(i));
    }
}

void tst_QDeclarativePathView::propertyChanges()
{
    QDeclarativeView *canvas = createView();
    QVERIFY(canvas);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/propertychanges.qml"));

    QDeclarativePathView *pathView = canvas->rootObject()->findChild<QDeclarativePathView*>("pathView");
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
    delete canvas;
}

void tst_QDeclarativePathView::pathChanges()
{
    QDeclarativeView *canvas = createView();
    QVERIFY(canvas);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/propertychanges.qml"));

    QDeclarativePathView *pathView = canvas->rootObject()->findChild<QDeclarativePathView*>("pathView");
    QVERIFY(pathView);

    QDeclarativePath *path = canvas->rootObject()->findChild<QDeclarativePath*>("path");
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

    QDeclarativePath *alternatePath = canvas->rootObject()->findChild<QDeclarativePath*>("alternatePath");
    QVERIFY(alternatePath);

    QSignalSpy pathSpy(pathView, SIGNAL(pathChanged()));

    QCOMPARE(pathView->path(), path);

    pathView->setPath(alternatePath);
    QCOMPARE(pathView->path(), alternatePath);
    QCOMPARE(pathSpy.count(),1);

    pathView->setPath(alternatePath);
    QCOMPARE(pathSpy.count(),1);

    QDeclarativePathAttribute *pathAttribute = canvas->rootObject()->findChild<QDeclarativePathAttribute*>("pathAttribute");
    QVERIFY(pathAttribute);

    QSignalSpy nameSpy(pathAttribute, SIGNAL(nameChanged()));
    QCOMPARE(pathAttribute->name(), QString("opacity"));

    pathAttribute->setName("scale");
    QCOMPARE(pathAttribute->name(), QString("scale"));
    QCOMPARE(nameSpy.count(),1);

    pathAttribute->setName("scale");
    QCOMPARE(nameSpy.count(),1);
    delete canvas;
}

void tst_QDeclarativePathView::componentChanges()
{
    QDeclarativeView *canvas = createView();
    QVERIFY(canvas);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/propertychanges.qml"));

    QDeclarativePathView *pathView = canvas->rootObject()->findChild<QDeclarativePathView*>("pathView");
    QVERIFY(pathView);

    QDeclarativeComponent delegateComponent(canvas->engine());
    delegateComponent.setData("import QtQuick 1.0; Text { text: '<b>Name:</b> ' + name }", QUrl::fromLocalFile(""));

    QSignalSpy delegateSpy(pathView, SIGNAL(delegateChanged()));

    pathView->setDelegate(&delegateComponent);
    QCOMPARE(pathView->delegate(), &delegateComponent);
    QCOMPARE(delegateSpy.count(),1);

    pathView->setDelegate(&delegateComponent);
    QCOMPARE(delegateSpy.count(),1);
    delete canvas;
}

void tst_QDeclarativePathView::modelChanges()
{
    QDeclarativeView *canvas = createView();
    QVERIFY(canvas);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/propertychanges.qml"));

    QDeclarativePathView *pathView = canvas->rootObject()->findChild<QDeclarativePathView*>("pathView");
    QVERIFY(pathView);

    QDeclarativeListModel *alternateModel = canvas->rootObject()->findChild<QDeclarativeListModel*>("alternateModel");
    QVERIFY(alternateModel);
    QVariant modelVariant = QVariant::fromValue(alternateModel);
    QSignalSpy modelSpy(pathView, SIGNAL(modelChanged()));

    pathView->setModel(modelVariant);
    QCOMPARE(pathView->model(), modelVariant);
    QCOMPARE(modelSpy.count(),1);

    pathView->setModel(modelVariant);
    QCOMPARE(modelSpy.count(),1);

    pathView->setModel(QVariant());
    QCOMPARE(modelSpy.count(),2);

    delete canvas;
}

void tst_QDeclarativePathView::pathUpdateOnStartChanged()
{
    QDeclarativeView *canvas = createView();
    QVERIFY(canvas);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/pathUpdateOnStartChanged.qml"));

    QDeclarativePathView *pathView = canvas->rootObject()->findChild<QDeclarativePathView*>("pathView");
    QVERIFY(pathView);

    QDeclarativePath *path = canvas->rootObject()->findChild<QDeclarativePath*>("path");
    QVERIFY(path);
    QCOMPARE(path->startX(), 400.0);
    QCOMPARE(path->startY(), 300.0);

    QDeclarativeItem *item = findItem<QDeclarativeItem>(pathView, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->x(), path->startX() - item->width() / 2.0);
    QCOMPARE(item->y(), path->startY() - item->height() / 2.0);

    delete canvas;
}

void tst_QDeclarativePathView::package()
{
    QDeclarativeView *canvas = createView();
    QVERIFY(canvas);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/pathview_package.qml"));

    QDeclarativePathView *pathView = canvas->rootObject()->findChild<QDeclarativePathView*>("photoPathView");
    QVERIFY(pathView);

    QDeclarativeItem *item = findItem<QDeclarativeItem>(pathView, "pathItem");
    QVERIFY(item);
    QVERIFY(item->scale() != 1.0);

    delete canvas;
}

//QTBUG-13017
void tst_QDeclarativePathView::emptyModel()
{
    QDeclarativeView *canvas = createView();

    QStringListModel model;

    QDeclarativeContext *ctxt = canvas->rootContext();
    ctxt->setContextProperty("emptyModel", &model);

    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/emptymodel.qml"));
    qApp->processEvents();

    QDeclarativePathView *pathview = qobject_cast<QDeclarativePathView*>(canvas->rootObject());
    QVERIFY(pathview != 0);

    QCOMPARE(pathview->offset(), qreal(0.0));

    delete canvas;
}

void tst_QDeclarativePathView::closed()
{
    QDeclarativeEngine engine;

    {
        QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/openPath.qml"));
        QDeclarativePath *obj = qobject_cast<QDeclarativePath*>(c.create());
        QVERIFY(obj);
        QCOMPARE(obj->isClosed(), false);
        delete obj;
    }

    {
        QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/closedPath.qml"));
        QDeclarativePath *obj = qobject_cast<QDeclarativePath*>(c.create());
        QVERIFY(obj);
        QCOMPARE(obj->isClosed(), true);
        delete obj;
    }
}

// QTBUG-14239
void tst_QDeclarativePathView::pathUpdate()
{
    QDeclarativeView *canvas = createView();
    QVERIFY(canvas);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/pathUpdate.qml"));

    QDeclarativePathView *pathView = canvas->rootObject()->findChild<QDeclarativePathView*>("pathView");
    QVERIFY(pathView);

    QDeclarativeItem *item = findItem<QDeclarativeItem>(pathView, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->x(), 150.0);

    delete canvas;
}

void tst_QDeclarativePathView::visualDataModel()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/vdm.qml"));

    QDeclarativePathView *obj = qobject_cast<QDeclarativePathView*>(c.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->count(), 3);

    delete obj;
}

void tst_QDeclarativePathView::undefinedPath()
{
    QDeclarativeEngine engine;
    QDeclarativeComponent c(&engine, QUrl::fromLocalFile(SRCDIR "/data/undefinedpath.qml"));

    QDeclarativePathView *obj = qobject_cast<QDeclarativePathView*>(c.create());
    QVERIFY(obj != 0);

    QCOMPARE(obj->count(), 3);

    delete obj;
}

void tst_QDeclarativePathView::mouseDrag()
{
    QDeclarativeView *canvas = createView();
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/dragpath.qml"));
    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    QDeclarativePathView *pathview = qobject_cast<QDeclarativePathView*>(canvas->rootObject());
    QVERIFY(pathview != 0);

    int current = pathview->currentIndex();

    QTest::mousePress(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(10,100)));

    {
        QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(QPoint(30,100)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &mv);
    }
    {
        QMouseEvent mv(QEvent::MouseMove, canvas->mapFromScene(QPoint(90,100)), Qt::LeftButton, Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(canvas->viewport(), &mv);
    }

    QVERIFY(pathview->currentIndex() != current);

    QTest::mouseRelease(canvas->viewport(), Qt::LeftButton, 0, canvas->mapFromScene(QPoint(40,100)));

    delete canvas;
}

void tst_QDeclarativePathView::treeModel()
{
    QDeclarativeView *canvas = createView();

    QStandardItemModel model;
    initStandardTreeModel(&model);
    canvas->engine()->rootContext()->setContextProperty("myModel", &model);

    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/treemodel.qml"));

    QDeclarativePathView *pathview = qobject_cast<QDeclarativePathView*>(canvas->rootObject());
    QVERIFY(pathview != 0);
    QCOMPARE(pathview->count(), 3);

    QDeclarativeText *item = findItem<QDeclarativeText>(pathview, "wrapper", 0);
    QVERIFY(item);
    QCOMPARE(item->text(), QLatin1String("Row 1 Item"));

    QVERIFY(QMetaObject::invokeMethod(pathview, "setRoot", Q_ARG(QVariant, 1)));
    QCOMPARE(pathview->count(), 1);

    QTRY_VERIFY(item = findItem<QDeclarativeText>(pathview, "wrapper", 0));
    QTRY_COMPARE(item->text(), QLatin1String("Row 2 Child Item"));

    delete canvas;
}

void tst_QDeclarativePathView::changePreferredHighlight()
{
    QDeclarativeView *canvas = createView();
    canvas->setFixedSize(400,200);
    canvas->setSource(QUrl::fromLocalFile(SRCDIR "/data/dragpath.qml"));
    canvas->show();
    QApplication::setActiveWindow(canvas);
    QVERIFY(QTest::qWaitForWindowActive(canvas));
    QCOMPARE(QApplication::activeWindow(), static_cast<QWidget *>(canvas));

    QDeclarativePathView *pathview = qobject_cast<QDeclarativePathView*>(canvas->rootObject());
    QVERIFY(pathview != 0);

    int current = pathview->currentIndex();
    QCOMPARE(current, 0);

    QDeclarativeRectangle *firstItem = findItem<QDeclarativeRectangle>(pathview, "wrapper", 0);
    QVERIFY(firstItem);
    QDeclarativePath *path = qobject_cast<QDeclarativePath*>(pathview->path());
    QVERIFY(path);
    QPointF start = path->pointAt(0.5);
    start.setX(qRound(start.x()));
    start.setY(qRound(start.y()));
    QPointF offset;//Center of item is at point, but pos is from corner
    offset.setX(firstItem->width()/2);
    offset.setY(firstItem->height()/2);
    QTRY_COMPARE(firstItem->pos() + offset, start);

    pathview->setPreferredHighlightBegin(0.8);
    pathview->setPreferredHighlightEnd(0.8);
    start = path->pointAt(0.8);
    start.setX(qRound(start.x()));
    start.setY(qRound(start.y()));
    QTRY_COMPARE(firstItem->pos() + offset, start);
    QCOMPARE(pathview->currentIndex(), 0);

    delete canvas;
}

QDeclarativeView *tst_QDeclarativePathView::createView()
{
    QDeclarativeView *canvas = new QDeclarativeView(0);
    canvas->setFixedSize(240,320);

    return canvas;
}

/*
   Find an item with the specified objectName.  If index is supplied then the
   item must also evaluate the {index} expression equal to index
 */
template<typename T>
T *tst_QDeclarativePathView::findItem(QGraphicsObject *parent, const QString &objectName, int index)
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

template<typename T>
QList<T*> tst_QDeclarativePathView::findItems(QGraphicsObject *parent, const QString &objectName)
{
    QList<T*> items;
    const QMetaObject &mo = T::staticMetaObject;
    //qDebug() << parent->QGraphicsObject::children().count() << "children";
    for (int i = 0; i < parent->childItems().count(); ++i) {
        QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(parent->childItems().at(i));
        if(!item)
            continue;
        //qDebug() << "try" << item;
        if (mo.cast(item) && (objectName.isEmpty() || item->objectName() == objectName))
            items.append(static_cast<T*>(item));
        items += findItems<T>(item, objectName);
    }

    return items;
}

QTEST_MAIN(tst_QDeclarativePathView)

#include "tst_qdeclarativepathview.moc"
