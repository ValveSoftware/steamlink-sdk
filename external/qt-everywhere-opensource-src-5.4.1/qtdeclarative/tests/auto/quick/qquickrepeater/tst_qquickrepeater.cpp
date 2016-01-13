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
#include <QtTest/QSignalSpy>
#include <QtQml/qqmlengine.h>
#include <QtQuick/qquickview.h>
#include <QtQml/qqmlcontext.h>
#include <QtQml/qqmlexpression.h>
#include <QtQml/qqmlincubator.h>
#include <private/qquickrepeater_p.h>
#include <QtQuick/private/qquicktext_p.h>

#include "../../shared/util.h"
#include "../shared/viewtestutil.h"
#include "../shared/visualtestutil.h"

using namespace QQuickViewTestUtil;
using namespace QQuickVisualTestUtil;


class tst_QQuickRepeater : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_QQuickRepeater();

private slots:
    void numberModel();
    void objectList();
    void stringList();
    void dataModel_adding();
    void dataModel_removing();
    void dataModel_changes();
    void itemModel();
    void resetModel();
    void modelChanged();
    void modelReset();
    void properties();
    void asynchronous();
    void initParent();
    void dynamicModelCrash();
    void visualItemModelCrash();
    void invalidContextCrash();
    void jsArrayChange();
};

class TestObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool error READ error WRITE setError)
    Q_PROPERTY(bool useModel READ useModel NOTIFY useModelChanged)

public:
    TestObject() : QObject(), mError(true), mUseModel(false) {}

    bool error() const { return mError; }
    void setError(bool err) { mError = err; }

    bool useModel() const { return mUseModel; }
    void setUseModel(bool use) { mUseModel = use; emit useModelChanged(); }

signals:
    void useModelChanged();

private:
    bool mError;
    bool mUseModel;
};

tst_QQuickRepeater::tst_QQuickRepeater()
{
}

void tst_QQuickRepeater::numberModel()
{
    QQuickView *window = createView();

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testData", 5);
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("intmodel.qml"));
    qApp->processEvents();

    QQuickRepeater *repeater = findItem<QQuickRepeater>(window->rootObject(), "repeater");
    QVERIFY(repeater != 0);
    QCOMPARE(repeater->parentItem()->childItems().count(), 5+1);

    QVERIFY(!repeater->itemAt(-1));
    for (int i=0; i<repeater->count(); i++)
        QCOMPARE(repeater->itemAt(i), repeater->parentItem()->childItems().at(i));
    QVERIFY(!repeater->itemAt(repeater->count()));

    QMetaObject::invokeMethod(window->rootObject(), "checkProperties");
    QVERIFY(testObject->error() == false);

    delete testObject;
    delete window;
}

class MyObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int idx READ idx CONSTANT)
public:
    MyObject(int i) : QObject(), m_idx(i) {}

    int idx() const { return m_idx; }

    int m_idx;
};

void tst_QQuickRepeater::objectList()
{
    QQuickView *window = createView();
    QObjectList data;
    for (int i=0; i<100; i++)
        data << new MyObject(i);

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testData", QVariant::fromValue(data));

    window->setSource(testFileUrl("objlist.qml"));
    qApp->processEvents();

    QQuickRepeater *repeater = findItem<QQuickRepeater>(window->rootObject(), "repeater");
    QVERIFY(repeater != 0);
    QCOMPARE(repeater->property("errors").toInt(), 0);//If this fails either they are out of order or can't find the object's data
    QCOMPARE(repeater->property("instantiated").toInt(), 100);

    QVERIFY(!repeater->itemAt(-1));
    for (int i=0; i<data.count(); i++)
        QCOMPARE(repeater->itemAt(i), repeater->parentItem()->childItems().at(i));
    QVERIFY(!repeater->itemAt(data.count()));

    QSignalSpy addedSpy(repeater, SIGNAL(itemAdded(int,QQuickItem*)));
    QSignalSpy removedSpy(repeater, SIGNAL(itemRemoved(int,QQuickItem*)));
    ctxt->setContextProperty("testData", QVariant::fromValue(data));
    QCOMPARE(addedSpy.count(), data.count());
    QCOMPARE(removedSpy.count(), data.count());

    qDeleteAll(data);
    delete window;
}

/*
The Repeater element creates children at its own position in its parent's
stacking order.  In this test we insert a repeater between two other Text
elements to test this.
*/
void tst_QQuickRepeater::stringList()
{
    QQuickView *window = createView();

    QStringList data;
    data << "One";
    data << "Two";
    data << "Three";
    data << "Four";

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testData", data);

    window->setSource(testFileUrl("repeater1.qml"));
    qApp->processEvents();

    QQuickRepeater *repeater = findItem<QQuickRepeater>(window->rootObject(), "repeater");
    QVERIFY(repeater != 0);

    QQuickItem *container = findItem<QQuickItem>(window->rootObject(), "container");
    QVERIFY(container != 0);

    QCOMPARE(container->childItems().count(), data.count() + 3);

    bool saw_repeater = false;
    for (int i = 0; i < container->childItems().count(); ++i) {

        if (i == 0) {
            QQuickText *name = qobject_cast<QQuickText*>(container->childItems().at(i));
            QVERIFY(name != 0);
            QCOMPARE(name->text(), QLatin1String("Zero"));
        } else if (i == container->childItems().count() - 2) {
            // The repeater itself
            QQuickRepeater *rep = qobject_cast<QQuickRepeater*>(container->childItems().at(i));
            QCOMPARE(rep, repeater);
            saw_repeater = true;
            continue;
        } else if (i == container->childItems().count() - 1) {
            QQuickText *name = qobject_cast<QQuickText*>(container->childItems().at(i));
            QVERIFY(name != 0);
            QCOMPARE(name->text(), QLatin1String("Last"));
        } else {
            QQuickText *name = qobject_cast<QQuickText*>(container->childItems().at(i));
            QVERIFY(name != 0);
            QCOMPARE(name->text(), data.at(i-1));
        }
    }
    QVERIFY(saw_repeater);

    delete window;
}

void tst_QQuickRepeater::dataModel_adding()
{
    QQuickView *window = createView();
    QQmlContext *ctxt = window->rootContext();
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    QaimModel testModel;
    ctxt->setContextProperty("testData", &testModel);
    window->setSource(testFileUrl("repeater2.qml"));
    qApp->processEvents();

    QQuickRepeater *repeater = findItem<QQuickRepeater>(window->rootObject(), "repeater");
    QVERIFY(repeater != 0);
    QQuickItem *container = findItem<QQuickItem>(window->rootObject(), "container");
    QVERIFY(container != 0);

    QVERIFY(!repeater->itemAt(0));

    QSignalSpy countSpy(repeater, SIGNAL(countChanged()));
    QSignalSpy addedSpy(repeater, SIGNAL(itemAdded(int,QQuickItem*)));

    // add to empty model
    testModel.addItem("two", "2");
    QCOMPARE(repeater->itemAt(0), container->childItems().at(0));
    QCOMPARE(countSpy.count(), 1); countSpy.clear();
    QCOMPARE(addedSpy.count(), 1);
    QCOMPARE(addedSpy.at(0).at(0).toInt(), 0);
    QCOMPARE(addedSpy.at(0).at(1).value<QQuickItem*>(), container->childItems().at(0));
    addedSpy.clear();

    // insert at start
    testModel.insertItem(0, "one", "1");
    QCOMPARE(repeater->itemAt(0), container->childItems().at(0));
    QCOMPARE(countSpy.count(), 1); countSpy.clear();
    QCOMPARE(addedSpy.count(), 1);
    QCOMPARE(addedSpy.at(0).at(0).toInt(), 0);
    QCOMPARE(addedSpy.at(0).at(1).value<QQuickItem*>(), container->childItems().at(0));
    addedSpy.clear();

    // insert at end
    testModel.insertItem(2, "four", "4");
    QCOMPARE(repeater->itemAt(2), container->childItems().at(2));
    QCOMPARE(countSpy.count(), 1); countSpy.clear();
    QCOMPARE(addedSpy.count(), 1);
    QCOMPARE(addedSpy.at(0).at(0).toInt(), 2);
    QCOMPARE(addedSpy.at(0).at(1).value<QQuickItem*>(), container->childItems().at(2));
    addedSpy.clear();

    // insert in middle
    testModel.insertItem(2, "three", "3");
    QCOMPARE(repeater->itemAt(2), container->childItems().at(2));
    QCOMPARE(countSpy.count(), 1); countSpy.clear();
    QCOMPARE(addedSpy.count(), 1);
    QCOMPARE(addedSpy.at(0).at(0).toInt(), 2);
    QCOMPARE(addedSpy.at(0).at(1).value<QQuickItem*>(), container->childItems().at(2));
    addedSpy.clear();

    delete testObject;
    addedSpy.clear();
    countSpy.clear();
    delete window;
}

void tst_QQuickRepeater::dataModel_removing()
{
    QQuickView *window = createView();
    QQmlContext *ctxt = window->rootContext();
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    QaimModel testModel;
    testModel.addItem("one", "1");
    testModel.addItem("two", "2");
    testModel.addItem("three", "3");
    testModel.addItem("four", "4");
    testModel.addItem("five", "5");

    ctxt->setContextProperty("testData", &testModel);
    window->setSource(testFileUrl("repeater2.qml"));
    qApp->processEvents();

    QQuickRepeater *repeater = findItem<QQuickRepeater>(window->rootObject(), "repeater");
    QVERIFY(repeater != 0);
    QQuickItem *container = findItem<QQuickItem>(window->rootObject(), "container");
    QVERIFY(container != 0);
    QCOMPARE(container->childItems().count(), repeater->count()+1);

    QSignalSpy countSpy(repeater, SIGNAL(countChanged()));
    QSignalSpy removedSpy(repeater, SIGNAL(itemRemoved(int,QQuickItem*)));

    // remove at start
    QQuickItem *item = repeater->itemAt(0);
    QCOMPARE(item, container->childItems().at(0));

    testModel.removeItem(0);
    QVERIFY(repeater->itemAt(0) != item);
    QCOMPARE(countSpy.count(), 1); countSpy.clear();
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.at(0).at(0).toInt(), 0);
    QCOMPARE(removedSpy.at(0).at(1).value<QQuickItem*>(), item);
    removedSpy.clear();

    // remove at end
    int lastIndex = testModel.count()-1;
    item = repeater->itemAt(lastIndex);
    QCOMPARE(item, container->childItems().at(lastIndex));

    testModel.removeItem(lastIndex);
    QVERIFY(repeater->itemAt(lastIndex) != item);
    QCOMPARE(countSpy.count(), 1); countSpy.clear();
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.at(0).at(0).toInt(), lastIndex);
    QCOMPARE(removedSpy.at(0).at(1).value<QQuickItem*>(), item);
    removedSpy.clear();

    // remove from middle
    item = repeater->itemAt(1);
    QCOMPARE(item, container->childItems().at(1));

    testModel.removeItem(1);
    QVERIFY(repeater->itemAt(lastIndex) != item);
    QCOMPARE(countSpy.count(), 1); countSpy.clear();
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.at(0).at(0).toInt(), 1);
    QCOMPARE(removedSpy.at(0).at(1).value<QQuickItem*>(), item);
    removedSpy.clear();

    delete testObject;
    delete window;
}

void tst_QQuickRepeater::dataModel_changes()
{
    QQuickView *window = createView();
    QQmlContext *ctxt = window->rootContext();
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    QaimModel testModel;
    testModel.addItem("one", "1");
    testModel.addItem("two", "2");
    testModel.addItem("three", "3");

    ctxt->setContextProperty("testData", &testModel);
    window->setSource(testFileUrl("repeater2.qml"));
    qApp->processEvents();

    QQuickRepeater *repeater = findItem<QQuickRepeater>(window->rootObject(), "repeater");
    QVERIFY(repeater != 0);
    QQuickItem *container = findItem<QQuickItem>(window->rootObject(), "container");
    QVERIFY(container != 0);
    QCOMPARE(container->childItems().count(), repeater->count()+1);

    // Check that model changes are propagated
    QQuickText *text = findItem<QQuickText>(window->rootObject(), "myName", 1);
    QVERIFY(text);
    QCOMPARE(text->text(), QString("two"));

    testModel.modifyItem(1, "Item two", "_2");
    text = findItem<QQuickText>(window->rootObject(), "myName", 1);
    QVERIFY(text);
    QCOMPARE(text->text(), QString("Item two"));

    text = findItem<QQuickText>(window->rootObject(), "myNumber", 1);
    QVERIFY(text);
    QCOMPARE(text->text(), QString("_2"));

    delete testObject;
    delete window;
}

void tst_QQuickRepeater::itemModel()
{
    QQuickView *window = createView();
    QQmlContext *ctxt = window->rootContext();
    TestObject *testObject = new TestObject;
    ctxt->setContextProperty("testObject", testObject);

    window->setSource(testFileUrl("itemlist.qml"));
    qApp->processEvents();

    QQuickRepeater *repeater = findItem<QQuickRepeater>(window->rootObject(), "repeater");
    QVERIFY(repeater != 0);

    QQuickItem *container = findItem<QQuickItem>(window->rootObject(), "container");
    QVERIFY(container != 0);

    QCOMPARE(container->childItems().count(), 1);

    testObject->setUseModel(true);
    QMetaObject::invokeMethod(window->rootObject(), "checkProperties");
    QVERIFY(testObject->error() == false);

    QCOMPARE(container->childItems().count(), 4);
    QVERIFY(qobject_cast<QObject*>(container->childItems().at(0))->objectName() == "item1");
    QVERIFY(qobject_cast<QObject*>(container->childItems().at(1))->objectName() == "item2");
    QVERIFY(qobject_cast<QObject*>(container->childItems().at(2))->objectName() == "item3");
    QVERIFY(container->childItems().at(3) == repeater);

    QMetaObject::invokeMethod(window->rootObject(), "switchModel");
    QCOMPARE(container->childItems().count(), 3);
    QVERIFY(qobject_cast<QObject*>(container->childItems().at(0))->objectName() == "item4");
    QVERIFY(qobject_cast<QObject*>(container->childItems().at(1))->objectName() == "item5");
    QVERIFY(container->childItems().at(2) == repeater);

    testObject->setUseModel(false);
    QCOMPARE(container->childItems().count(), 1);

    delete testObject;
    delete window;
}

void tst_QQuickRepeater::resetModel()
{
    QQuickView *window = createView();

    QStringList dataA;
    for (int i=0; i<10; i++)
        dataA << QString::number(i);

    QQmlContext *ctxt = window->rootContext();
    ctxt->setContextProperty("testData", dataA);
    window->setSource(testFileUrl("repeater1.qml"));
    qApp->processEvents();
    QQuickRepeater *repeater = findItem<QQuickRepeater>(window->rootObject(), "repeater");
    QVERIFY(repeater != 0);
    QQuickItem *container = findItem<QQuickItem>(window->rootObject(), "container");
    QVERIFY(container != 0);

    QCOMPARE(repeater->count(), dataA.count());
    for (int i=0; i<repeater->count(); i++)
        QCOMPARE(repeater->itemAt(i), container->childItems().at(i+1)); // +1 to skip first Text object

    QSignalSpy modelChangedSpy(repeater, SIGNAL(modelChanged()));
    QSignalSpy countSpy(repeater, SIGNAL(countChanged()));
    QSignalSpy addedSpy(repeater, SIGNAL(itemAdded(int,QQuickItem*)));
    QSignalSpy removedSpy(repeater, SIGNAL(itemRemoved(int,QQuickItem*)));

    QStringList dataB;
    for (int i=0; i<20; i++)
        dataB << QString::number(i);

    // reset context property
    ctxt->setContextProperty("testData", dataB);
    QCOMPARE(repeater->count(), dataB.count());

    QCOMPARE(modelChangedSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(removedSpy.count(), dataA.count());
    QCOMPARE(addedSpy.count(), dataB.count());
    for (int i=0; i<dataB.count(); i++) {
        QCOMPARE(addedSpy.at(i).at(0).toInt(), i);
        QCOMPARE(addedSpy.at(i).at(1).value<QQuickItem*>(), repeater->itemAt(i));
    }
    modelChangedSpy.clear();
    countSpy.clear();
    removedSpy.clear();
    addedSpy.clear();

    // reset via setModel()
    repeater->setModel(dataA);
    QCOMPARE(repeater->count(), dataA.count());

    QCOMPARE(modelChangedSpy.count(), 1);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(removedSpy.count(), dataB.count());
    QCOMPARE(addedSpy.count(), dataA.count());
    for (int i=0; i<dataA.count(); i++) {
        QCOMPARE(addedSpy.at(i).at(0).toInt(), i);
        QCOMPARE(addedSpy.at(i).at(1).value<QQuickItem*>(), repeater->itemAt(i));
    }

    modelChangedSpy.clear();
    countSpy.clear();
    removedSpy.clear();
    addedSpy.clear();

    delete window;
}

// QTBUG-17156
void tst_QQuickRepeater::modelChanged()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("modelChanged.qml"));

    QQuickItem *rootObject = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(rootObject);
    QQuickRepeater *repeater = findItem<QQuickRepeater>(rootObject, "repeater");
    QVERIFY(repeater);

    repeater->setModel(4);
    QCOMPARE(repeater->count(), 4);
    QCOMPARE(repeater->property("itemsCount").toInt(), 4);
    QCOMPARE(repeater->property("itemsFound").toList().count(), 4);

    repeater->setModel(10);
    QCOMPARE(repeater->count(), 10);
    QCOMPARE(repeater->property("itemsCount").toInt(), 10);
    QCOMPARE(repeater->property("itemsFound").toList().count(), 10);

    delete rootObject;
}

void tst_QQuickRepeater::modelReset()
{
    QaimModel model;

    QQmlEngine engine;
    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("testData", &model);

    QQmlComponent component(&engine, testFileUrl("repeater2.qml"));
    QScopedPointer<QObject> object(component.create());
    QQuickItem *rootItem = qobject_cast<QQuickItem *>(object.data());
    QVERIFY(rootItem);

    QQuickRepeater *repeater = findItem<QQuickRepeater>(rootItem, "repeater");
    QVERIFY(repeater != 0);
    QQuickItem *container = findItem<QQuickItem>(rootItem, "container");
    QVERIFY(container != 0);

    QCOMPARE(repeater->count(), 0);

    QSignalSpy countSpy(repeater, SIGNAL(countChanged()));
    QSignalSpy addedSpy(repeater, SIGNAL(itemAdded(int,QQuickItem*)));
    QSignalSpy removedSpy(repeater, SIGNAL(itemRemoved(int,QQuickItem*)));


    QList<QPair<QString, QString> > items = QList<QPair<QString, QString> >()
            << qMakePair(QString::fromLatin1("one"), QString::fromLatin1("1"))
            << qMakePair(QString::fromLatin1("two"), QString::fromLatin1("2"))
            << qMakePair(QString::fromLatin1("three"), QString::fromLatin1("3"));

    model.resetItems(items);

    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(removedSpy.count(), 0);
    QCOMPARE(addedSpy.count(), items.count());
    for (int i = 0; i< items.count(); i++) {
        QCOMPARE(addedSpy.at(i).at(0).toInt(), i);
        QCOMPARE(addedSpy.at(i).at(1).value<QQuickItem*>(), repeater->itemAt(i));
    }

    countSpy.clear();
    addedSpy.clear();

    model.reset();
    QCOMPARE(countSpy.count(), 0);
    QCOMPARE(removedSpy.count(), 3);
    QCOMPARE(addedSpy.count(), 3);
    for (int i = 0; i< items.count(); i++) {
        QCOMPARE(addedSpy.at(i).at(0).toInt(), i);
        QCOMPARE(addedSpy.at(i).at(1).value<QQuickItem*>(), repeater->itemAt(i));
    }

    addedSpy.clear();
    removedSpy.clear();

    items.append(qMakePair(QString::fromLatin1("four"), QString::fromLatin1("4")));
    items.append(qMakePair(QString::fromLatin1("five"), QString::fromLatin1("5")));

    model.resetItems(items);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(removedSpy.count(), 3);
    QCOMPARE(addedSpy.count(), 5);
    for (int i = 0; i< items.count(); i++) {
        QCOMPARE(addedSpy.at(i).at(0).toInt(), i);
        QCOMPARE(addedSpy.at(i).at(1).value<QQuickItem*>(), repeater->itemAt(i));
    }

    countSpy.clear();
    addedSpy.clear();
    removedSpy.clear();

    items.clear();
    model.resetItems(items);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(removedSpy.count(), 5);
    QCOMPARE(addedSpy.count(), 0);
}

void tst_QQuickRepeater::properties()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("properties.qml"));

    QQuickItem *rootObject = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(rootObject);

    QQuickRepeater *repeater = findItem<QQuickRepeater>(rootObject, "repeater");
    QVERIFY(repeater);

    QSignalSpy modelSpy(repeater, SIGNAL(modelChanged()));
    repeater->setModel(3);
    QCOMPARE(modelSpy.count(),1);
    repeater->setModel(3);
    QCOMPARE(modelSpy.count(),1);

    QSignalSpy delegateSpy(repeater, SIGNAL(delegateChanged()));

    QQmlComponent rectComponent(&engine);
    rectComponent.setData("import QtQuick 2.0; Rectangle {}", QUrl::fromLocalFile(""));

    repeater->setDelegate(&rectComponent);
    QCOMPARE(delegateSpy.count(),1);
    repeater->setDelegate(&rectComponent);
    QCOMPARE(delegateSpy.count(),1);

    delete rootObject;
}

void tst_QQuickRepeater::asynchronous()
{
    QQuickView *window = createView();
    window->show();
    QQmlIncubationController controller;
    window->engine()->setIncubationController(&controller);

    window->setSource(testFileUrl("asyncloader.qml"));

    QQuickItem *rootObject = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(rootObject);

    QQuickItem *container = findItem<QQuickItem>(rootObject, "container");
    QVERIFY(!container);
    while (!container) {
        bool b = false;
        controller.incubateWhile(&b);
        container = findItem<QQuickItem>(rootObject, "container");
    }

    QQuickRepeater *repeater = 0;
    while (!repeater) {
        bool b = false;
        controller.incubateWhile(&b);
        repeater = findItem<QQuickRepeater>(rootObject, "repeater");
    }

    // items will be created one at a time
    for (int i = 0; i < 10; ++i) {
        QString name("delegate");
        name += QString::number(i);
        QVERIFY(findItem<QQuickItem>(container, name) == 0);
        QQuickItem *item = 0;
        while (!item) {
            bool b = false;
            controller.incubateWhile(&b);
            item = findItem<QQuickItem>(container, name);
        }
    }

    {
        bool b = true;
        controller.incubateWhile(&b);
    }

    // verify positioning
    for (int i = 0; i < 10; ++i) {
        QString name("delegate");
        name += QString::number(i);
        QQuickItem *item = findItem<QQuickItem>(container, name);
        QTRY_COMPARE(item->y(), i * 50.0);
    }

    delete window;
}

void tst_QQuickRepeater::initParent()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("initparent.qml"));

    QQuickItem *rootObject = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(rootObject);

    QCOMPARE(qvariant_cast<QQuickItem*>(rootObject->property("parentItem")), rootObject);
}

void tst_QQuickRepeater::dynamicModelCrash()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("dynamicmodelcrash.qml"));

    // Don't crash
    QQuickItem *rootObject = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(rootObject);

    QQuickRepeater *repeater = findItem<QQuickRepeater>(rootObject, "rep");
    QVERIFY(repeater);
    QVERIFY(qvariant_cast<QObject *>(repeater->model()) == 0);
}

void tst_QQuickRepeater::visualItemModelCrash()
{
    // This used to crash because the model would get
    // deleted before the repeater, leading to double-deletion
    // of the items.
    QQuickView *window = createView();
    window->setSource(testFileUrl("visualitemmodel.qml"));
    qApp->processEvents();
    delete window;
}

class BadModel : public QAbstractListModel
{
public:
    ~BadModel()
    {
        beginResetModel();
        endResetModel();
    }

    QVariant data(const QModelIndex &, int) const { return QVariant(); }
    int rowCount(const QModelIndex &) const { return 0; }
};


void tst_QQuickRepeater::invalidContextCrash()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("invalidContextCrash.qml"));

    BadModel* model = new BadModel;
    engine.rootContext()->setContextProperty("badModel", model);

    QScopedPointer<QObject> root(component.create());
    QCOMPARE(root->children().count(), 1);
    QObject *repeater = root->children().first();

    // Make sure the model comes first in the child list, so it will be
    // deleted first and then the repeater. During deletion the QML context
    // has been deleted already and is invalid.
    model->setParent(root.data());
    repeater->setParent(0);
    repeater->setParent(root.data());

    QCOMPARE(root->children().count(), 2);
    QVERIFY(root->children().at(0) == model);
    QVERIFY(root->children().at(1) == repeater);

    // Delete the root object, which will invalidate/delete the QML context
    // and then delete the child QObjects, which may try to access the context.
    root.reset(0);
}

void tst_QQuickRepeater::jsArrayChange()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.4; Repeater {}", QUrl());

    QScopedPointer<QQuickRepeater> repeater(qobject_cast<QQuickRepeater *>(component.create()));
    QVERIFY(!repeater.isNull());

    QSignalSpy spy(repeater.data(), SIGNAL(modelChanged()));
    QVERIFY(spy.isValid());

    QJSValue array1 = engine.newArray(3);
    QJSValue array2 = engine.newArray(3);
    for (int i = 0; i < 3; ++i) {
        array1.setProperty(i, i);
        array2.setProperty(i, i);
    }

    repeater->setModel(QVariant::fromValue(array1));
    QCOMPARE(spy.count(), 1);

    // no change
    repeater->setModel(QVariant::fromValue(array2));
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(tst_QQuickRepeater)

#include "tst_qquickrepeater.moc"
