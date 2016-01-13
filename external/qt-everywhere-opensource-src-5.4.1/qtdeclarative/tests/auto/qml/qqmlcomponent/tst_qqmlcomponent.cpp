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
#include <QDebug>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlproperty.h>
#include <QtQml/qqmlincubator.h>
#include <QtQuick>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQuick/private/qquickmousearea_p.h>
#include <qcolor.h>
#include "../../shared/util.h"
#include "testhttpserver.h"

#define SERVER_PORT 14450

class MyIC : public QObject, public QQmlIncubationController
{
    Q_OBJECT
public:
    MyIC() { startTimer(5); }
protected:
    virtual void timerEvent(QTimerEvent*) {
        incubateFor(5);
    }
};

class ComponentWatcher : public QObject
{
    Q_OBJECT
public:
    ComponentWatcher(QQmlComponent *comp) : loading(0), error(0), ready(0) {
        connect(comp, SIGNAL(statusChanged(QQmlComponent::Status)),
                this, SLOT(statusChanged(QQmlComponent::Status)));
    }

    int loading;
    int error;
    int ready;

public slots:
    void statusChanged(QQmlComponent::Status status) {
        switch (status) {
        case QQmlComponent::Loading:
            ++loading;
            break;
        case QQmlComponent::Error:
            ++error;
            break;
        case QQmlComponent::Ready:
            ++ready;
            break;
        default:
            break;
        }
    }
};

static void gc(QQmlEngine &engine)
{
    engine.collectGarbage();
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

class tst_qqmlcomponent : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlcomponent() { engine.setIncubationController(&ic); }

private slots:
    void null();
    void loadEmptyUrl();
    void qmlCreateWindow();
    void qmlCreateObject();
    void qmlCreateObjectWithProperties();
    void qmlIncubateObject();
    void qmlCreateParentReference();
    void async();
    void asyncHierarchy();
    void componentUrlCanonicalization();
    void onDestructionLookup();
    void onDestructionCount();
    void recursion();
    void recursionContinuation();

private:
    QQmlEngine engine;
    MyIC ic;
};

void tst_qqmlcomponent::null()
{
    {
        QQmlComponent c;
        QVERIFY(c.isNull());
    }

    {
        QQmlComponent c(&engine);
        QVERIFY(c.isNull());
    }
}


void tst_qqmlcomponent::loadEmptyUrl()
{
    QQmlComponent c(&engine);
    c.loadUrl(QUrl());

    QVERIFY(c.isError());
    QCOMPARE(c.errors().count(), 1);
    QQmlError error = c.errors().first();
    QCOMPARE(error.url(), QUrl());
    QCOMPARE(error.line(), -1);
    QCOMPARE(error.column(), -1);
    QCOMPARE(error.description(), QLatin1String("Invalid empty URL"));
}

void tst_qqmlcomponent::qmlIncubateObject()
{
    QQmlComponent component(&engine, testFileUrl("incubateObject.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("test1").toBool(), true);
    QCOMPARE(object->property("test2").toBool(), false);

    QTRY_VERIFY(object->property("test2").toBool() == true);

    delete object;
}

void tst_qqmlcomponent::qmlCreateWindow()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("createWindow.qml"));
    QQuickWindow* window = qobject_cast<QQuickWindow *>(component.create());
    QVERIFY(window);
}

void tst_qqmlcomponent::qmlCreateObject()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("createObject.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QObject *testObject1 = object->property("qobject").value<QObject*>();
    QVERIFY(testObject1);
    QVERIFY(testObject1->parent() == object);

    QObject *testObject2 = object->property("declarativeitem").value<QObject*>();
    QVERIFY(testObject2);
    QVERIFY(testObject2->parent() == object);
    QCOMPARE(testObject2->metaObject()->className(), "QQuickItem");
}

void tst_qqmlcomponent::qmlCreateObjectWithProperties()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("createObjectWithScript.qml"));
    QVERIFY2(component.errorString().isEmpty(), component.errorString().toUtf8());
    QObject *object = component.create();
    QVERIFY(object != 0);

    QObject *testObject1 = object->property("declarativerectangle").value<QObject*>();
    QVERIFY(testObject1);
    QVERIFY(testObject1->parent() == object);
    QCOMPARE(testObject1->property("x").value<int>(), 17);
    QCOMPARE(testObject1->property("y").value<int>(), 17);
    QCOMPARE(testObject1->property("color").value<QColor>(), QColor(255,255,255));
    QCOMPARE(QQmlProperty::read(testObject1,"border.width").toInt(), 3);
    QCOMPARE(QQmlProperty::read(testObject1,"innerRect.border.width").toInt(), 20);
    delete testObject1;

    QObject *testObject2 = object->property("declarativeitem").value<QObject*>();
    QVERIFY(testObject2);
    QVERIFY(testObject2->parent() == object);
    //QCOMPARE(testObject2->metaObject()->className(), "QDeclarativeItem_QML_2");
    QCOMPARE(testObject2->property("x").value<int>(), 17);
    QCOMPARE(testObject2->property("y").value<int>(), 17);
    QCOMPARE(testObject2->property("testBool").value<bool>(), true);
    QCOMPARE(testObject2->property("testInt").value<int>(), 17);
    QCOMPARE(testObject2->property("testObject").value<QObject*>(), object);
    delete testObject2;

    QObject *testBindingObj = object->property("bindingTestObject").value<QObject*>();
    QVERIFY(testBindingObj);
    QCOMPARE(testBindingObj->parent(), object);
    QCOMPARE(testBindingObj->property("testValue").value<int>(), 300);
    object->setProperty("width", 150);
    QCOMPARE(testBindingObj->property("testValue").value<int>(), 150 * 3);
    delete testBindingObj;

    QObject *testBindingThisObj = object->property("bindingThisTestObject").value<QObject*>();
    QVERIFY(testBindingThisObj);
    QCOMPARE(testBindingThisObj->parent(), object);
    QCOMPARE(testBindingThisObj->property("testValue").value<int>(), 900);
    testBindingThisObj->setProperty("width", 200);
    QCOMPARE(testBindingThisObj->property("testValue").value<int>(), 200 * 3);
    delete testBindingThisObj;
}

void tst_qqmlcomponent::qmlCreateParentReference()
{
    QQmlEngine engine;

    QCOMPARE(engine.outputWarningsToStandardError(), true);

    QQmlTestMessageHandler messageHandler;

    QQmlComponent component(&engine, testFileUrl("createParentReference.qml"));
    QVERIFY2(component.errorString().isEmpty(), component.errorString().toUtf8());
    QObject *object = component.create();
    QVERIFY(object != 0);

    QVERIFY(QMetaObject::invokeMethod(object, "createChild"));
    delete object;

    engine.setOutputWarningsToStandardError(false);
    QCOMPARE(engine.outputWarningsToStandardError(), false);

    QVERIFY2(messageHandler.messages().isEmpty(), qPrintable(messageHandler.messageString()));
}

void tst_qqmlcomponent::async()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(SERVER_PORT), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlComponent component(&engine);
    ComponentWatcher watcher(&component);
    component.loadUrl(QUrl("http://127.0.0.1:14450/TestComponent.qml"), QQmlComponent::Asynchronous);
    QCOMPARE(watcher.loading, 1);
    QTRY_VERIFY(component.isReady());
    QCOMPARE(watcher.ready, 1);
    QCOMPARE(watcher.error, 0);

    QObject *object = component.create();
    QVERIFY(object != 0);

    delete object;
}

void tst_qqmlcomponent::asyncHierarchy()
{
    TestHTTPServer server;
    QVERIFY2(server.listen(SERVER_PORT), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    // ensure that the item hierarchy is compiled correctly.
    QQmlComponent component(&engine);
    ComponentWatcher watcher(&component);
    component.loadUrl(QUrl("http://127.0.0.1:14450/TestComponent.2.qml"), QQmlComponent::Asynchronous);
    QCOMPARE(watcher.loading, 1);
    QTRY_VERIFY(component.isReady());
    QCOMPARE(watcher.ready, 1);
    QCOMPARE(watcher.error, 0);

    QObject *root = component.create();
    QVERIFY(root != 0);

    // ensure that the parent-child relationship hierarchy is correct
    // (use QQuickItem* for all children rather than types which are not publicly exported)
    QQuickItem *c1 = root->findChild<QQuickItem*>("c1", Qt::FindDirectChildrenOnly);
    QVERIFY(c1);
    QQuickItem *c1c1 = c1->findChild<QQuickItem*>("c1c1", Qt::FindDirectChildrenOnly);
    QVERIFY(c1c1);
    QQuickItem *c1c2 = c1->findChild<QQuickItem*>("c1c2", Qt::FindDirectChildrenOnly);
    QVERIFY(c1c2);
    QQuickItem *c1c2c3 = c1c2->findChild<QQuickItem*>("c1c2c3", Qt::FindDirectChildrenOnly);
    QVERIFY(c1c2c3);
    QQuickItem *c2 = root->findChild<QQuickItem*>("c2", Qt::FindDirectChildrenOnly);
    QVERIFY(c2);
    QQuickItem *c2c1 = c2->findChild<QQuickItem*>("c2c1", Qt::FindDirectChildrenOnly);
    QVERIFY(c2c1);
    QQuickItem *c2c1c1 = c2c1->findChild<QQuickItem*>("c2c1c1", Qt::FindDirectChildrenOnly);
    QVERIFY(c2c1c1);
    QQuickItem *c2c1c2 = c2c1->findChild<QQuickItem*>("c2c1c2", Qt::FindDirectChildrenOnly);
    QVERIFY(c2c1c2);

    // ensure that values and bindings are assigned correctly
    QVERIFY(root->property("success").toBool());

    delete root;
}

void tst_qqmlcomponent::componentUrlCanonicalization()
{
    // ensure that url canonicalization succeeds so that type information
    // is not generated multiple times for the same component.
    {
        // load components via import
        QQmlEngine engine;
        QQmlComponent component(&engine, testFileUrl("componentUrlCanonicalization.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY(object != 0);
        QVERIFY(object->property("success").toBool());
    }

    {
        // load one of the components dynamically, which would trigger
        // import of the other if it were not already loaded.
        QQmlEngine engine;
        QQmlComponent component(&engine, testFileUrl("componentUrlCanonicalization.2.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY(object != 0);
        QVERIFY(object->property("success").toBool());
    }

    {
        // load components with more deeply nested imports
        QQmlEngine engine;
        QQmlComponent component(&engine, testFileUrl("componentUrlCanonicalization.3.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY(object != 0);
        QVERIFY(object->property("success").toBool());
    }

    {
        // load components with unusually specified import paths
        QQmlEngine engine;
        QQmlComponent component(&engine, testFileUrl("componentUrlCanonicalization.4.qml"));
        QScopedPointer<QObject> object(component.create());
        QVERIFY(object != 0);
        QVERIFY(object->property("success").toBool());
    }

    {
        // Do not crash with various nonsense import paths
        QQmlEngine engine;
        QQmlComponent component(&engine, testFileUrl("componentUrlCanonicalization.5.qml"));
        QTest::ignoreMessage(QtWarningMsg, QLatin1String("QQmlComponent: Component is not ready").data());
        QScopedPointer<QObject> object(component.create());
        QVERIFY(object == 0);
    }
}

void tst_qqmlcomponent::onDestructionLookup()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("onDestructionLookup.qml"));
    QScopedPointer<QObject> object(component.create());
    gc(engine);
    QVERIFY(object != 0);
    QVERIFY(object->property("success").toBool());
}

void tst_qqmlcomponent::onDestructionCount()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("onDestructionCount.qml"));

    QLatin1String warning("Component.onDestruction");

    {
        // Warning should be emitted during create()
        QTest::ignoreMessage(QtWarningMsg, warning.data());

        QScopedPointer<QObject> object(component.create());
        QVERIFY(object != 0);
    }

    // Warning should not be emitted any further
    QCOMPARE(engine.outputWarningsToStandardError(), true);

    QStringList warnings;
    {
        QQmlTestMessageHandler messageHandler;

        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
        warnings = messageHandler.messages();
    }

    engine.setOutputWarningsToStandardError(false);
    QCOMPARE(engine.outputWarningsToStandardError(), false);

    QCOMPARE(warnings.count(), 0);
}

void tst_qqmlcomponent::recursion()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("recursion.qml"));

    QTest::ignoreMessage(QtWarningMsg, QLatin1String("QQmlComponent: Component creation is recursing - aborting").data());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(object != 0);

    // Sub-object creation does not succeed
    QCOMPARE(object->property("success").toBool(), false);
}

void tst_qqmlcomponent::recursionContinuation()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("recursionContinuation.qml"));

    for (int i = 0; i < 10; ++i)
        QTest::ignoreMessage(QtWarningMsg, QLatin1String("QQmlComponent: Component creation is recursing - aborting").data());

    QScopedPointer<QObject> object(component.create());
    QVERIFY(object != 0);

    // Eventual sub-object creation succeeds
    QVERIFY(object->property("success").toBool());
}

QTEST_MAIN(tst_qqmlcomponent)

#include "tst_qqmlcomponent.moc"
