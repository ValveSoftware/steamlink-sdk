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
#include <private/qqmlmetatype_p.h>
#include <QDebug>
#include <QQuickItem>
#include <QQmlContext>
#include <private/qobject_p.h>

class tst_creation : public QObject
{
    Q_OBJECT
public:
    tst_creation();

private slots:
    void qobject_cpp();
    void qobject_qml();
    void qobject_qmltype();

    void qobject_10flat_qml();
    void qobject_10flat_cpp();

    void qobject_10tree_qml();
    void qobject_10tree_cpp();

    void itemtree_notree_cpp();
    void itemtree_objtree_cpp();
    void itemtree_cpp();
    void itemtree_data_cpp();
    void itemtree_qml();
    void itemtree_scene_cpp();

    void itemtests_qml_data();
    void itemtests_qml();

    void bindings_cpp();
    void bindings_cpp2();
    void bindings_qml();

    void bindings_parent_qml();

    void anchors_creation();
    void anchors_heightChange();

private:
    QQmlEngine engine;
};

class TestType : public QObject
{
Q_OBJECT
Q_PROPERTY(QQmlListProperty<QObject> resources READ resources)
Q_CLASSINFO("DefaultProperty", "resources")
public:
    TestType(QObject *parent = 0)
    : QObject(parent) {}

    QQmlListProperty<QObject> resources() {
        return QQmlListProperty<QObject>(this, 0, resources_append, 0, 0, 0);
    }

    static void resources_append(QQmlListProperty<QObject> *p, QObject *o) {
        o->setParent(p->object);
    }
};

tst_creation::tst_creation()
{
    qmlRegisterType<TestType>("Qt.test", 1, 0, "TestType");

    // Ensure QtQuick is loaded and imported. Some benchmark like elements() rely on QQmlMetaType::qmlTypeNames() to
    // be populated.
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nItem{}", QUrl());
    QScopedPointer<QObject> obj(component.create());
}

inline QUrl TEST_FILE(const QString &filename)
{
    return QUrl::fromLocalFile(QLatin1String(SRCDIR) + QLatin1String("/data/") + filename);
}

void tst_creation::qobject_cpp()
{
    QBENCHMARK {
        QObject *obj = new QObject;
        delete obj;
    }
}

void tst_creation::qobject_qml()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nQtObject {}", QUrl());
    QObject *obj = component.create();
    delete obj;

    QBENCHMARK {
        QObject *obj = component.create();
        delete obj;
    }
}

void tst_creation::qobject_10flat_qml()
{
    QQmlComponent component(&engine);
    component.setData("import Qt.test 1.0\nTestType { resources: [ TestType{},TestType{},TestType{},TestType{},TestType{},TestType{},TestType{},TestType{},TestType{},TestType{} ] }", QUrl());
    QObject *obj = component.create();
    delete obj;

    QBENCHMARK {
        QObject *obj = component.create();
        delete obj;
    }
}

void tst_creation::qobject_10flat_cpp()
{
    QBENCHMARK {
        QObject *item = new TestType;
        new TestType(item);
        new TestType(item);
        new TestType(item);
        new TestType(item);
        new TestType(item);
        new TestType(item);
        new TestType(item);
        new TestType(item);
        new TestType(item);
        new TestType(item);
        delete item;
    }
}

void tst_creation::qobject_10tree_qml()
{
    QQmlComponent component(&engine);
    component.setData("import Qt.test 1.0\nTestType { TestType{ TestType { TestType{ TestType{ TestType{ TestType{ TestType{ TestType{ TestType{ TestType{ } } } } } } } } } } }", QUrl());

    QObject *obj = component.create();
    delete obj;

    QBENCHMARK {
        QObject *obj = component.create();
        delete obj;
    }
}

void tst_creation::qobject_10tree_cpp()
{
    QBENCHMARK {
        QObject *item = new TestType;
        QObject *root = item;
        item = new TestType(item);
        item = new TestType(item);
        item = new TestType(item);
        item = new TestType(item);
        item = new TestType(item);
        item = new TestType(item);
        item = new TestType(item);
        item = new TestType(item);
        item = new TestType(item);
        item = new TestType(item);
        delete root;
    }
}

void tst_creation::qobject_qmltype()
{
    QQmlType *t = QQmlMetaType::qmlType("QtQuick/QtObject", 2, 0);

    QBENCHMARK {
        QObject *obj = t->create();
        delete obj;
    }
}

struct QQmlGraphics_Derived : public QObject
{
    void setParent_noEvent(QObject *parent) {
        bool sce = d_ptr->sendChildEvents;
        d_ptr->sendChildEvents = false;
        setParent(parent);
        d_ptr->sendChildEvents = sce;
    }
};

inline void QQmlGraphics_setParent_noEvent(QObject *object, QObject *parent)
{
    static_cast<QQmlGraphics_Derived *>(object)->setParent_noEvent(parent);
}

void tst_creation::itemtree_notree_cpp()
{
    std::vector<QQuickItem *> kids;
    kids.resize(30);
    QBENCHMARK {
        QQuickItem *item = new QQuickItem;
        for (int i = 0; i < 30; ++i) {
            QQuickItem *child = new QQuickItem;
            kids[i] = child;
        }
        qDeleteAll(kids);
        delete item;
    }
}

void tst_creation::itemtree_objtree_cpp()
{
    QBENCHMARK {
        QQuickItem *item = new QQuickItem;
        for (int i = 0; i < 30; ++i) {
            QQuickItem *child = new QQuickItem;
            QQmlGraphics_setParent_noEvent(child,item);
        }
        delete item;
    }
}

void tst_creation::itemtree_cpp()
{
    QBENCHMARK {
        QQuickItem *item = new QQuickItem;
        for (int i = 0; i < 30; ++i) {
            QQuickItem *child = new QQuickItem;
            QQmlGraphics_setParent_noEvent(child,item);
            child->setParentItem(item);
        }
        delete item;
    }
}

void tst_creation::itemtree_data_cpp()
{
    QQmlEngine engine;
    QBENCHMARK {
        QQuickItem *item = new QQuickItem;
        for (int i = 0; i < 30; ++i) {
            QQuickItem *child = new QQuickItem;
            QQmlGraphics_setParent_noEvent(child,item);
            QQmlListReference ref(item, "data", &engine);
            ref.append(child);
        }
        delete item;
    }
}

void tst_creation::itemtree_qml()
{
    QQmlComponent component(&engine, TEST_FILE("item.qml"));
    QObject *obj = component.create();
    delete obj;

    QBENCHMARK {
        QObject *obj = component.create();
        delete obj;
    }
}

void tst_creation::itemtree_scene_cpp()
{
    QQuickItem *root = new QQuickItem;
    QBENCHMARK {
        QQuickItem *item = new QQuickItem;
        for (int i = 0; i < 30; ++i) {
            QQuickItem *child = new QQuickItem;
            QQmlGraphics_setParent_noEvent(child,item);
            child->setParentItem(item);
        }
        item->setParentItem(root);
        delete item;
    }
    delete root;
}

void tst_creation::itemtests_qml_data()
{
    QTest::addColumn<QString>("filepath");

    QTest::newRow("emptyItem") << "emptyItem.qml";
    QTest::newRow("emptyCustomItem") << "emptyCustomItem.qml";
    QTest::newRow("itemWithProperties") << "itemWithProperties.qml";
    QTest::newRow("itemUsingOnComponentCompleted") << "itemUsingOnComponentCompleted.qml";
    QTest::newRow("itemWithAnchoredChild") << "itemWithAnchoredChild.qml";
    QTest::newRow("itemWithChildBindedToSize") << "itemWithChildBindedToSize.qml";
    QTest::newRow("itemWithPropertyBindingsTest1") << "itemWithPropertyBindingsTest1.qml";
    QTest::newRow("itemWithPropertyBindingsTest2") << "itemWithPropertyBindingsTest2.qml";
    QTest::newRow("itemWithPropertyBindingsTest3") << "itemWithPropertyBindingsTest3.qml";
    QTest::newRow("itemWithPropertyBindingsTest4") << "itemWithPropertyBindingsTest4.qml";
    QTest::newRow("itemWithPropertyBindingsTest5") << "itemWithPropertyBindingsTest5.qml";
}

void tst_creation::itemtests_qml()
{
    QFETCH(QString, filepath);

    QUrl url = TEST_FILE(filepath);
    QQmlComponent component(&engine, url);

    if (!component.isReady()) {
        qWarning() << "Unable to create component: " << url;
        return;
    }

    delete component.create();
    QBENCHMARK { delete component.create(); }
}

void tst_creation::bindings_cpp()
{
    QQuickItem item;
    QMetaProperty widthProp = item.metaObject()->property(item.metaObject()->indexOfProperty("width"));
    QMetaProperty heightProp = item.metaObject()->property(item.metaObject()->indexOfProperty("height"));
    connect(&item, &QQuickItem::heightChanged, [&item, &widthProp, &heightProp](){
        QVariant height = heightProp.read(&item);
        widthProp.write(&item, height);
    });

    int height = 0;
    QBENCHMARK {
        item.setHeight(++height);
    }
}

void tst_creation::bindings_cpp2()
{
    QQuickItem item;
    int widthProp = item.metaObject()->indexOfProperty("width");
    int heightProp = item.metaObject()->indexOfProperty("height");
    connect(&item, &QQuickItem::heightChanged, [&item, widthProp, heightProp](){

        qreal height = -1;
        void *args[] = { &height, 0 };
        QMetaObject::metacall(&item, QMetaObject::ReadProperty, heightProp, args);

        int flags = 0;
        int status = -1;
        void *argv[] = { &height, 0, &status, &flags };
        QMetaObject::metacall(&item, QMetaObject::WriteProperty, widthProp, argv);
    });

    int height = 0;
    QBENCHMARK {
        item.setHeight(++height);
    }
}

void tst_creation::bindings_qml()
{
    QByteArray data = "import QtQuick 2.0\nItem { width: height }";

    QQmlComponent component(&engine);
    component.setData(data, QUrl());
    if (!component.isReady()) {
        qWarning() << "Unable to create component: " << component.errorString();
        return;
    }

    QQuickItem *obj = dynamic_cast<QQuickItem *>(component.create());
    QVERIFY(obj != nullptr);

    int height = 0;
    QBENCHMARK {
        obj->setHeight(++height);
    }

    delete obj;
}

void tst_creation::bindings_parent_qml()
{
    QByteArray data = "import QtQuick 2.0\nItem { Item { width: parent.height }}";

    QQmlComponent component(&engine);
    component.setData(data, QUrl());
    if (!component.isReady()) {
        qWarning() << "Unable to create component: " << component.errorString();
        return;
    }

    QQuickItem *obj = dynamic_cast<QQuickItem *>(component.create());
    QVERIFY(obj != nullptr);

    int height = 0;
    QBENCHMARK {
        obj->setHeight(++height);
    }

    delete obj;
}

void tst_creation::anchors_creation()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nItem { Item { anchors.bottom: parent.bottom } }", QUrl());

    QObject *obj = component.create();
    delete obj;

    QBENCHMARK {
        QObject *obj = component.create();
        delete obj;
    }
}

void tst_creation::anchors_heightChange()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nItem { Item { anchors.bottom: parent.bottom } }", QUrl());

    QObject *obj = component.create();
    auto item = qobject_cast<QQuickItem *>(obj);
    Q_ASSERT(item);
    int height = 1;

    QBENCHMARK {
        item->setHeight(height);
        height += 1;
    }

    delete obj;
}

QTEST_MAIN(tst_creation)

#include "tst_creation.moc"
