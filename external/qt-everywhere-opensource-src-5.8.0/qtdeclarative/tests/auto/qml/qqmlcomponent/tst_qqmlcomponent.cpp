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
#include <QDebug>

#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlproperty.h>
#include <QtQml/qqmlincubator.h>
#include <QtQuick>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQuick/private/qquickmousearea_p.h>
#include <private/qv8engine_p.h>
#include <private/qqmlcontext_p.h>
#include <private/qv4scopedvalue_p.h>
#include <qcolor.h>
#include "../../shared/util.h"
#include "testhttpserver.h"

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
    void qmlCreateObjectAutoParent_data();
    void qmlCreateObjectAutoParent();
    void qmlCreateObjectWithProperties();
    void qmlIncubateObject();
    void qmlCreateParentReference();
    void async();
    void asyncHierarchy();
    void asyncForceSync();
    void componentUrlCanonicalization();
    void onDestructionLookup();
    void onDestructionCount();
    void recursion();
    void recursionContinuation();
    void callingContextForInitialProperties();

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

    QTRY_VERIFY(object->property("test2").toBool());

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

void tst_qqmlcomponent::qmlCreateObjectAutoParent_data()
{
    QTest::addColumn<QString>("testFile");

    QTest::newRow("createObject") << QStringLiteral("createObject.qml");
    QTest::newRow("createQmlObject") <<  QStringLiteral("createQmlObject.qml");
}


void tst_qqmlcomponent::qmlCreateObjectAutoParent()
{
    QFETCH(QString, testFile);

    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl(testFile));
    QQuickItem *root = qobject_cast<QQuickItem *>(component.create());
    QVERIFY(root);
    QObject *qtobjectParent = root->property("qtobjectParent").value<QObject*>();
    QQuickItem *itemParent = qobject_cast<QQuickItem *>(root->property("itemParent").value<QObject*>());
    QQuickWindow *windowParent = qobject_cast<QQuickWindow *>(root->property("windowParent").value<QObject*>());
    QVERIFY(qtobjectParent);
    QVERIFY(itemParent);
    QVERIFY(windowParent);

    QObject *qtobject_qtobject = root->property("qtobject_qtobject").value<QObject*>();
    QObject *qtobject_item = root->property("qtobject_item").value<QObject*>();
    QObject *qtobject_window = root->property("qtobject_window").value<QObject*>();
    QObject *item_qtobject = root->property("item_qtobject").value<QObject*>();
    QObject *item_item = root->property("item_item").value<QObject*>();
    QObject *item_window = root->property("item_window").value<QObject*>();
    QObject *window_qtobject = root->property("window_qtobject").value<QObject*>();
    QObject *window_item = root->property("window_item").value<QObject*>();
    QObject *window_window = root->property("window_window").value<QObject*>();

    QVERIFY(qtobject_qtobject);
    QVERIFY(qtobject_item);
    QVERIFY(qtobject_window);
    QVERIFY(item_qtobject);
    QVERIFY(item_item);
    QVERIFY(item_window);
    QVERIFY(window_qtobject);
    QVERIFY(window_item);
    QVERIFY(window_window);

    QCOMPARE(qtobject_item->metaObject()->className(), "QQuickItem");
    QCOMPARE(qtobject_window->metaObject()->className(), "QQuickWindow");
    QCOMPARE(item_item->metaObject()->className(), "QQuickItem");
    QCOMPARE(item_window->metaObject()->className(), "QQuickWindow");
    QCOMPARE(window_item->metaObject()->className(), "QQuickItem");
    QCOMPARE(window_window->metaObject()->className(), "QQuickWindow");

    QCOMPARE(qtobject_qtobject->parent(), qtobjectParent);
    QCOMPARE(qtobject_item->parent(), qtobjectParent);
    QCOMPARE(qtobject_window->parent(), qtobjectParent);
    QCOMPARE(item_qtobject->parent(), itemParent);
    QCOMPARE(item_item->parent(), itemParent);
    QCOMPARE(item_window->parent(), itemParent);
    QCOMPARE(window_qtobject->parent(), windowParent);
    QCOMPARE(window_item->parent(), windowParent);
    QCOMPARE(window_window->parent(), windowParent);

    QCOMPARE(qobject_cast<QQuickItem *>(qtobject_item)->parentItem(), (QQuickItem *)0);
    QCOMPARE(qobject_cast<QQuickWindow *>(qtobject_window)->transientParent(), (QQuickWindow *)0);
    QCOMPARE(qobject_cast<QQuickItem *>(item_item)->parentItem(), itemParent);
    QCOMPARE(qobject_cast<QQuickWindow *>(item_window)->transientParent(), itemParent->window());
    QCOMPARE(qobject_cast<QQuickItem *>(window_item)->parentItem(), windowParent->contentItem());
    QCOMPARE(qobject_cast<QQuickWindow *>(window_window)->transientParent(), windowParent);
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
    QCOMPARE(testObject1->parent(), object);
    QCOMPARE(testObject1->property("x").value<int>(), 17);
    QCOMPARE(testObject1->property("y").value<int>(), 17);
    QCOMPARE(testObject1->property("color").value<QColor>(), QColor(255,255,255));
    QCOMPARE(QQmlProperty::read(testObject1,"border.width").toInt(), 3);
    QCOMPARE(QQmlProperty::read(testObject1,"innerRect.border.width").toInt(), 20);
    delete testObject1;

    QObject *testObject2 = object->property("declarativeitem").value<QObject*>();
    QVERIFY(testObject2);
    QCOMPARE(testObject2->parent(), object);
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
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    QQmlComponent component(&engine);
    ComponentWatcher watcher(&component);
    component.loadUrl(server.url("/TestComponent.qml"), QQmlComponent::Asynchronous);
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
    QVERIFY2(server.listen(), qPrintable(server.errorString()));
    server.serveDirectory(dataDirectory());

    // ensure that the item hierarchy is compiled correctly.
    QQmlComponent component(&engine);
    ComponentWatcher watcher(&component);
    component.loadUrl(server.url("/TestComponent.2.qml"), QQmlComponent::Asynchronous);
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

void tst_qqmlcomponent::asyncForceSync()
{
    {
        // 1) make sure that HTTP URLs cannot be completed synchronously
        TestHTTPServer server;
        QVERIFY2(server.listen(), qPrintable(server.errorString()));
        server.serveDirectory(dataDirectory());

        // ensure that the item hierarchy is compiled correctly.
        QQmlComponent component(&engine);
        component.loadUrl(server.url("/TestComponent.2.qml"), QQmlComponent::Asynchronous);
        QCOMPARE(component.status(), QQmlComponent::Loading);
        QQmlComponent component2(&engine, server.url("/TestComponent.2.qml"), QQmlComponent::PreferSynchronous);
        QCOMPARE(component2.status(), QQmlComponent::Loading);
    }
    {
        // 2) make sure that file:// URL can be completed synchronously

        // ensure that the item hierarchy is compiled correctly.
        QQmlComponent component(&engine);
        component.loadUrl(testFileUrl("/TestComponent.2.qml"), QQmlComponent::Asynchronous);
        QCOMPARE(component.status(), QQmlComponent::Loading);
        QQmlComponent component2(&engine, testFileUrl("/TestComponent.2.qml"), QQmlComponent::PreferSynchronous);
        QCOMPARE(component2.status(), QQmlComponent::Ready);
        QCOMPARE(component.status(), QQmlComponent::Loading);
        QTRY_COMPARE_WITH_TIMEOUT(component.status(), QQmlComponent::Ready, 0);
    }
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
        QVERIFY(object.isNull());
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

class CallingContextCheckingClass : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue)
public:
    CallingContextCheckingClass()
        : m_value(0)
    {}

    int value() const { return m_value; }
    void setValue(int v) {
        scopeObject.clear();
        callingContextData.setContextData(0);

        m_value = v;
        QJSEngine *jsEngine = qjsEngine(this);
        if (!jsEngine)
            return;
        QV4::ExecutionEngine *v4 = QV8Engine::getV4(jsEngine);
        if (!v4)
            return;
        QV4::Scope scope(v4);
        QV4::Scoped<QV4::QmlContext> qmlContext(scope, v4->qmlContext());
        if (!qmlContext)
            return;
        callingContextData = qmlContext->qmlContext();
        scopeObject = qmlContext->qmlScope();
    }

    int m_value;
    QQmlGuardedContextData callingContextData;
    QPointer<QObject> scopeObject;
};

void tst_qqmlcomponent::callingContextForInitialProperties()
{
    qmlRegisterType<CallingContextCheckingClass>("qqmlcomponenttest", 1, 0, "CallingContextCheckingClass");

    QQmlComponent testFactory(&engine, testFileUrl("callingQmlContextComponent.qml"));

    QQmlComponent component(&engine, testFileUrl("callingQmlContext.qml"));
    QScopedPointer<QObject> root(component.beginCreate(engine.rootContext()));
    QVERIFY(!root.isNull());
    root->setProperty("factory", QVariant::fromValue(&testFactory));
    component.completeCreate();
    QTRY_VERIFY(qvariant_cast<QObject *>(root->property("incubatedObject")));
    QObject *o = qvariant_cast<QObject *>(root->property("incubatedObject"));
    CallingContextCheckingClass *checker = qobject_cast<CallingContextCheckingClass*>(o);
    QVERIFY(checker);

    QVERIFY(!checker->callingContextData.isNull());
    QVERIFY(checker->callingContextData->urlString().endsWith(QStringLiteral("callingQmlContext.qml")));

    QVERIFY(!checker->scopeObject.isNull());
    QVERIFY(checker->scopeObject->metaObject()->indexOfProperty("incubatedObject") != -1);
}

QTEST_MAIN(tst_qqmlcomponent)

#include "tst_qqmlcomponent.moc"
