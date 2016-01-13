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
#include <QQmlEngine>
#include <QQmlComponent>
#include <private/qqmlmetatype_p.h>
#include <QDebug>
#include <QGraphicsScene>
#include <QGraphicsItem>
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
    void qobject_alloc();

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

    void elements_data();
    void elements();

    void itemtests_qml_data();
    void itemtests_qml();

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

struct QObjectFakeData {
    char data[sizeof(QObjectPrivate)];
};

struct QObjectFake {
    QObjectFake();
    virtual ~QObjectFake();
private:
    QObjectFakeData *d;
};

QObjectFake::QObjectFake()
{
    d = new QObjectFakeData;
}

QObjectFake::~QObjectFake()
{
    delete d;
}

void tst_creation::qobject_alloc()
{
    QBENCHMARK {
        QObjectFake *obj = new QObjectFake;
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
    QBENCHMARK {
        QQuickItem *item = new QQuickItem;
        for (int i = 0; i < 30; ++i) {
            QQuickItem *child = new QQuickItem;
            Q_UNUSED(child);
        }
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
    QBENCHMARK {
        QQuickItem *item = new QQuickItem;
        for (int i = 0; i < 30; ++i) {
            QQuickItem *child = new QQuickItem;
            QQmlGraphics_setParent_noEvent(child,item);
            QQmlListReference ref(item, "data");
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

void tst_creation::elements_data()
{
    QTest::addColumn<QString>("type");

    QList<QString> types = QQmlMetaType::qmlTypeNames();
    foreach (QString type, types)
        QTest::newRow(type.toLatin1()) << type;
}

void tst_creation::elements()
{
    QFETCH(QString, type);
    QQmlType *t = QQmlMetaType::qmlType(type, 2, 0);
    if (!t || !t->isCreatable())
        QSKIP("Non-creatable type");

    QBENCHMARK {
        QObject *obj = t->create();
        delete obj;
    }
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

QTEST_MAIN(tst_creation)

#include "tst_creation.moc"
