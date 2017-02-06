/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#include <QDebug>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <qqml.h>
#include <QMetaMethod>

#include "../../shared/util.h"

class ExportedClass : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int qmlObjectProp READ qmlObjectProp NOTIFY qmlObjectPropChanged)
    Q_PROPERTY(int cppObjectProp READ cppObjectProp NOTIFY cppObjectPropChanged)
    Q_PROPERTY(int unboundProp READ unboundProp NOTIFY unboundPropChanged)
    Q_PROPERTY(int v8BindingProp READ v8BindingProp NOTIFY v8BindingPropChanged)
    Q_PROPERTY(int v4BindingProp READ v4BindingProp NOTIFY v4BindingPropChanged)
    Q_PROPERTY(int v4BindingProp2 READ v4BindingProp2 NOTIFY v4BindingProp2Changed)
    Q_PROPERTY(int scriptBindingProp READ scriptBindingProp NOTIFY scriptBindingPropChanged)
public:
    int qmlObjectPropConnections;
    int cppObjectPropConnections;
    int unboundPropConnections;
    int v8BindingPropConnections;
    int v4BindingPropConnections;
    int v4BindingProp2Connections;
    int scriptBindingPropConnections;
    int boundSignalConnections;
    int unusedSignalConnections;

    ExportedClass()
        : qmlObjectPropConnections(0), cppObjectPropConnections(0), unboundPropConnections(0),
          v8BindingPropConnections(0), v4BindingPropConnections(0), v4BindingProp2Connections(0),
          scriptBindingPropConnections(0), boundSignalConnections(0), unusedSignalConnections(0)
    {}

    ~ExportedClass()
    {
        QCOMPARE(qmlObjectPropConnections, 0);
        QCOMPARE(cppObjectPropConnections, 0);
        QCOMPARE(unboundPropConnections, 0);
        QCOMPARE(v8BindingPropConnections, 0);
        QCOMPARE(v4BindingPropConnections, 0);
        QCOMPARE(v4BindingProp2Connections, 0);
        QCOMPARE(scriptBindingPropConnections, 0);
        QCOMPARE(boundSignalConnections, 0);
        QCOMPARE(unusedSignalConnections, 0);
    }

    int qmlObjectProp() const { return 42; }
    int unboundProp() const { return 42; }
    int v8BindingProp() const { return 42; }
    int v4BindingProp() const { return 42; }
    int v4BindingProp2() const { return 42; }
    int cppObjectProp() const { return 42; }
    int scriptBindingProp() const { return 42; }

    void verifyReceiverCount()
    {
        //Note: QTBUG-34829 means we can't call this from within disconnectNotify or it can lock
        QCOMPARE(receivers(SIGNAL(qmlObjectPropChanged())), qmlObjectPropConnections);
        QCOMPARE(receivers(SIGNAL(cppObjectPropChanged())), cppObjectPropConnections);
        QCOMPARE(receivers(SIGNAL(unboundPropChanged())), unboundPropConnections);
        QCOMPARE(receivers(SIGNAL(v8BindingPropChanged())), v8BindingPropConnections);
        QCOMPARE(receivers(SIGNAL(v4BindingPropChanged())), v4BindingPropConnections);
        QCOMPARE(receivers(SIGNAL(v4BindingProp2Changed())), v4BindingProp2Connections);
        QCOMPARE(receivers(SIGNAL(scriptBindingPropChanged())), scriptBindingPropConnections);
        QCOMPARE(receivers(SIGNAL(boundSignal())), boundSignalConnections);
        QCOMPARE(receivers(SIGNAL(unusedSignal())), unusedSignalConnections);
    }

protected:
    void connectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE {
        if (signal.name() == "qmlObjectPropChanged") qmlObjectPropConnections++;
        if (signal.name() == "cppObjectPropChanged") cppObjectPropConnections++;
        if (signal.name() == "unboundPropChanged") unboundPropConnections++;
        if (signal.name() == "v8BindingPropChanged") v8BindingPropConnections++;
        if (signal.name() == "v4BindingPropChanged") v4BindingPropConnections++;
        if (signal.name() == "v4BindingProp2Changed") v4BindingProp2Connections++;
        if (signal.name() == "scriptBindingPropChanged") scriptBindingPropConnections++;
        if (signal.name() == "boundSignal")   boundSignalConnections++;
        if (signal.name() == "unusedSignal") unusedSignalConnections++;
        verifyReceiverCount();
        //qDebug() << Q_FUNC_INFO << this << signal.name();
    }

    void disconnectNotify(const QMetaMethod &signal) Q_DECL_OVERRIDE {
        if (signal.name() == "qmlObjectPropChanged") qmlObjectPropConnections--;
        if (signal.name() == "cppObjectPropChanged") cppObjectPropConnections--;
        if (signal.name() == "unboundPropChanged") unboundPropConnections--;
        if (signal.name() == "v8BindingPropChanged") v8BindingPropConnections--;
        if (signal.name() == "v4BindingPropChanged") v4BindingPropConnections--;
        if (signal.name() == "v4BindingProp2Changed") v4BindingProp2Connections--;
        if (signal.name() == "scriptBindingPropChanged") scriptBindingPropConnections--;
        if (signal.name() == "boundSignal")   boundSignalConnections--;
        if (signal.name() == "unusedSignal") unusedSignalConnections--;
        //qDebug() << Q_FUNC_INFO << this << signal.methodSignature();
    }

signals:
    void qmlObjectPropChanged();
    void cppObjectPropChanged();
    void unboundPropChanged();
    void v8BindingPropChanged();
    void v4BindingPropChanged();
    void v4BindingProp2Changed();
    void scriptBindingPropChanged();
    void boundSignal();
    void unusedSignal();
};

class tst_qqmlnotifier : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlnotifier()
        : root(0), exportedClass(0), exportedObject(0)
    {}

private slots:
    void initTestCase() Q_DECL_OVERRIDE;
    void cleanupTestCase();
    void testConnectNotify();

    void removeV4Binding();
    void removeV4Binding2();
    void removeV8Binding();
    void removeScriptBinding();
    // No need to test value type proxy bindings - the user can't override disconnectNotify() anyway,
    // as the classes are private to the QML engine

    void readProperty();
    void propertyChange();
    void disconnectOnDestroy();
    void lotsOfBindings();

private:
    void createObjects();

    QQmlEngine engine;
    QObject *root;
    ExportedClass *exportedClass;
    ExportedClass *exportedObject;
};

void tst_qqmlnotifier::initTestCase()
{
    QQmlDataTest::initTestCase();
    qmlRegisterType<ExportedClass>("Test", 1, 0, "ExportedClass");
}

void tst_qqmlnotifier::createObjects()
{
    delete root;
    root = 0;
    exportedClass = exportedObject = 0;

    QQmlComponent component(&engine, testFileUrl("connectnotify.qml"));
    exportedObject = new ExportedClass();
    exportedObject->setObjectName("exportedObject");
    engine.rootContext()->setContextProperty("_exportedObject", exportedObject);
    root = component.create();
    QVERIFY(root != 0);

    exportedClass = qobject_cast<ExportedClass *>(
                root->findChild<ExportedClass*>("exportedClass"));
    QVERIFY(exportedClass != 0);
    exportedClass->verifyReceiverCount();
}

void tst_qqmlnotifier::cleanupTestCase()
{
    delete root;
    root = 0;
    delete exportedObject;
    exportedObject = 0;
}

void tst_qqmlnotifier::testConnectNotify()
{
    createObjects();

    QCOMPARE(exportedClass->qmlObjectPropConnections, 1);
    QCOMPARE(exportedClass->cppObjectPropConnections, 0);
    QCOMPARE(exportedClass->unboundPropConnections, 0);
    QCOMPARE(exportedClass->v8BindingPropConnections, 1);
    QCOMPARE(exportedClass->v4BindingPropConnections, 1);
    QCOMPARE(exportedClass->v4BindingProp2Connections, 2);
    QCOMPARE(exportedClass->scriptBindingPropConnections, 1);
    QCOMPARE(exportedClass->boundSignalConnections, 1);
    QCOMPARE(exportedClass->unusedSignalConnections, 0);
    exportedClass->verifyReceiverCount();

    QCOMPARE(exportedObject->qmlObjectPropConnections, 0);
    QCOMPARE(exportedObject->cppObjectPropConnections, 1);
    QCOMPARE(exportedObject->unboundPropConnections, 0);
    QCOMPARE(exportedObject->v8BindingPropConnections, 0);
    QCOMPARE(exportedObject->v4BindingPropConnections, 0);
    QCOMPARE(exportedObject->v4BindingProp2Connections, 0);
    QCOMPARE(exportedObject->scriptBindingPropConnections, 0);
    QCOMPARE(exportedObject->boundSignalConnections, 0);
    QCOMPARE(exportedObject->unusedSignalConnections, 0);
    exportedObject->verifyReceiverCount();
}

void tst_qqmlnotifier::removeV4Binding()
{
    createObjects();

    // Removing a binding should disconnect all of its guarded properties
    QVERIFY(QMetaObject::invokeMethod(root, "removeV4Binding"));
    QCOMPARE(exportedClass->v4BindingPropConnections, 0);
    exportedClass->verifyReceiverCount();
}

void tst_qqmlnotifier::removeV4Binding2()
{
    createObjects();

    // In this case, the v4BindingProp2 property is used by two v4 bindings.
    // Make sure that removing one binding doesn't by accident disconnect all.
    QVERIFY(QMetaObject::invokeMethod(root, "removeV4Binding2"));
    QCOMPARE(exportedClass->v4BindingProp2Connections, 1);
    exportedClass->verifyReceiverCount();
}

void tst_qqmlnotifier::removeV8Binding()
{
    createObjects();

    // Removing a binding should disconnect all of its guarded properties
    QVERIFY(QMetaObject::invokeMethod(root, "removeV8Binding"));
    QCOMPARE(exportedClass->v8BindingPropConnections, 0);
    exportedClass->verifyReceiverCount();
}

void tst_qqmlnotifier::removeScriptBinding()
{
    createObjects();

    // Removing a binding should disconnect all of its guarded properties
    QVERIFY(QMetaObject::invokeMethod(root, "removeScriptBinding"));
    QCOMPARE(exportedClass->scriptBindingPropConnections, 0);
    exportedClass->verifyReceiverCount();
}

void tst_qqmlnotifier::readProperty()
{
    createObjects();

    // Reading a property should not connect to it
    QVERIFY(QMetaObject::invokeMethod(root, "readProperty"));
    QCOMPARE(exportedClass->unboundPropConnections, 0);
    exportedClass->verifyReceiverCount();
}

void tst_qqmlnotifier::propertyChange()
{
    createObjects();

    // Changing the state will trigger the PropertyChange to overwrite a value with a binding.
    // For this, the new binding needs to be connected, and afterwards disconnected.
    QVERIFY(QMetaObject::invokeMethod(root, "changeState"));
    QCOMPARE(exportedClass->unboundPropConnections, 1);
    exportedClass->verifyReceiverCount();
    QVERIFY(QMetaObject::invokeMethod(root, "changeState"));
    QCOMPARE(exportedClass->unboundPropConnections, 0);
    exportedClass->verifyReceiverCount();
}

void tst_qqmlnotifier::disconnectOnDestroy()
{
    createObjects();

    // Deleting a QML object should remove all connections. For exportedClass, this is tested in
    // the destructor, and for exportedObject, it is tested below.
    delete root;
    root = 0;
    QCOMPARE(exportedObject->cppObjectPropConnections, 0);
    exportedObject->verifyReceiverCount();
}

class TestObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int a READ a NOTIFY aChanged)

public:
    int a() const { return 0; }

signals:
    void aChanged();
};

void tst_qqmlnotifier::lotsOfBindings()
{
    TestObject o;
    QQmlEngine *e = new QQmlEngine;

    e->rootContext()->setContextProperty(QStringLiteral("test"), &o);

    QList<QQmlComponent *> components;
    for (int i = 0; i < 20000; ++i) {
        QQmlComponent *component = new QQmlComponent(e);
        component->setData("import QtQuick 2.0; Item { width: test.a; }", QUrl());
        component->create(e->rootContext());
        components.append(component);
    }

    o.aChanged();

    qDeleteAll(components);
    delete e;
}

QTEST_MAIN(tst_qqmlnotifier)

#include "tst_qqmlnotifier.moc"
