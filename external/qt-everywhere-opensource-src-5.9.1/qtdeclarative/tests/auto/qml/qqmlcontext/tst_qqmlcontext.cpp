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
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlExpression>
#include <private/qqmlcontext_p.h>
#include "../../shared/util.h"

class tst_qqmlcontext : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlcontext() {}

private slots:
    void baseUrl();
    void resolvedUrl();
    void engineMethod();
    void parentContext();
    void setContextProperty();
    void setContextObject();
    void destruction();
    void idAsContextProperty();
    void readOnlyContexts();
    void nameForObject();

    void refreshExpressions();
    void refreshExpressionsCrash();
    void refreshExpressionsRootContext();
    void skipExpressionRefresh_qtbug_53431();

    void qtbug_22535();
    void evalAfterInvalidate();
    void qobjectDerived();
    void qtbug_49232();

private:
    QQmlEngine engine;
};

void tst_qqmlcontext::baseUrl()
{
    QQmlContext ctxt(&engine);

    QCOMPARE(ctxt.baseUrl(), QUrl());

    ctxt.setBaseUrl(QUrl("http://www.qt-project.org/"));

    QCOMPARE(ctxt.baseUrl(), QUrl("http://www.qt-project.org/"));
}

void tst_qqmlcontext::resolvedUrl()
{
    // Relative to the component
    {
        QQmlContext ctxt(&engine);
        ctxt.setBaseUrl(QUrl("http://www.qt-project.org/"));

        QCOMPARE(ctxt.resolvedUrl(QUrl("main.qml")), QUrl("http://www.qt-project.org/main.qml"));
    }

    // Relative to a parent
    {
        QQmlContext ctxt(&engine);
        ctxt.setBaseUrl(QUrl("http://www.qt-project.org/"));

        QQmlContext ctxt2(&ctxt);
        QCOMPARE(ctxt2.resolvedUrl(QUrl("main2.qml")), QUrl("http://www.qt-project.org/main2.qml"));
    }

    // Relative to the engine
    {
        QQmlContext ctxt(&engine);
        QCOMPARE(ctxt.resolvedUrl(QUrl("main.qml")), engine.baseUrl().resolved(QUrl("main.qml")));
    }

    // Relative to a deleted parent
    {
        QQmlContext *ctxt = new QQmlContext(&engine);
        ctxt->setBaseUrl(QUrl("http://www.qt-project.org/"));

        QQmlContext ctxt2(ctxt);
        QCOMPARE(ctxt2.resolvedUrl(QUrl("main2.qml")), QUrl("http://www.qt-project.org/main2.qml"));

        delete ctxt; ctxt = 0;

        QCOMPARE(ctxt2.resolvedUrl(QUrl("main2.qml")), QUrl());
    }

    // Absolute
    {
        QQmlContext ctxt(&engine);

        QCOMPARE(ctxt.resolvedUrl(QUrl("http://www.qt-project.org/main2.qml")), QUrl("http://www.qt-project.org/main2.qml"));
        QCOMPARE(ctxt.resolvedUrl(QUrl("file:///main2.qml")), QUrl("file:///main2.qml"));
    }
}

void tst_qqmlcontext::engineMethod()
{
    QQmlEngine *engine = new QQmlEngine;

    QQmlContext ctxt(engine);
    QQmlContext ctxt2(&ctxt);
    QQmlContext ctxt3(&ctxt2);
    QQmlContext ctxt4(&ctxt2);

    QCOMPARE(ctxt.engine(), engine);
    QCOMPARE(ctxt2.engine(), engine);
    QCOMPARE(ctxt3.engine(), engine);
    QCOMPARE(ctxt4.engine(), engine);

    delete engine; engine = 0;

    QCOMPARE(ctxt.engine(), engine);
    QCOMPARE(ctxt2.engine(), engine);
    QCOMPARE(ctxt3.engine(), engine);
    QCOMPARE(ctxt4.engine(), engine);
}

void tst_qqmlcontext::parentContext()
{
    QQmlEngine *engine = new QQmlEngine;

    QCOMPARE(engine->rootContext()->parentContext(), (QQmlContext *)0);

    QQmlContext *ctxt = new QQmlContext(engine);
    QQmlContext *ctxt2 = new QQmlContext(ctxt);
    QQmlContext *ctxt3 = new QQmlContext(ctxt2);
    QQmlContext *ctxt4 = new QQmlContext(ctxt2);
    QQmlContext *ctxt5 = new QQmlContext(ctxt);
    QQmlContext *ctxt6 = new QQmlContext(engine);
    QQmlContext *ctxt7 = new QQmlContext(engine->rootContext());

    QCOMPARE(ctxt->parentContext(), engine->rootContext());
    QCOMPARE(ctxt2->parentContext(), ctxt);
    QCOMPARE(ctxt3->parentContext(), ctxt2);
    QCOMPARE(ctxt4->parentContext(), ctxt2);
    QCOMPARE(ctxt5->parentContext(), ctxt);
    QCOMPARE(ctxt6->parentContext(), engine->rootContext());
    QCOMPARE(ctxt7->parentContext(), engine->rootContext());

    delete ctxt2; ctxt2 = 0;

    QCOMPARE(ctxt->parentContext(), engine->rootContext());
    QCOMPARE(ctxt3->parentContext(), (QQmlContext *)0);
    QCOMPARE(ctxt4->parentContext(), (QQmlContext *)0);
    QCOMPARE(ctxt5->parentContext(), ctxt);
    QCOMPARE(ctxt6->parentContext(), engine->rootContext());
    QCOMPARE(ctxt7->parentContext(), engine->rootContext());

    delete engine; engine = 0;

    QCOMPARE(ctxt->parentContext(), (QQmlContext *)0);
    QCOMPARE(ctxt3->parentContext(), (QQmlContext *)0);
    QCOMPARE(ctxt4->parentContext(), (QQmlContext *)0);
    QCOMPARE(ctxt5->parentContext(), (QQmlContext *)0);
    QCOMPARE(ctxt6->parentContext(), (QQmlContext *)0);
    QCOMPARE(ctxt7->parentContext(), (QQmlContext *)0);

    delete ctxt7;
    delete ctxt6;
    delete ctxt5;
    delete ctxt4;
    delete ctxt3;
    delete ctxt;
}

class TestObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int a READ a NOTIFY aChanged)
    Q_PROPERTY(int b READ b NOTIFY bChanged)
    Q_PROPERTY(int c READ c NOTIFY cChanged)
    Q_PROPERTY(char d READ d NOTIFY dChanged)
    Q_PROPERTY(uchar e READ e NOTIFY eChanged)

public:
    TestObject() : _a(10), _b(10), _c(10) {}

    int a() const { return _a; }
    void setA(int a) { _a = a; emit aChanged(); }

    int b() const { return _b; }
    void setB(int b) { _b = b; emit bChanged(); }

    int c() const { return _c; }
    void setC(int c) { _c = c; emit cChanged(); }

    char d() const { return _d; }
    void setD(char d) { _d = d; emit dChanged(); }

    uchar e() const { return _e; }
    void setE(uchar e) { _e = e; emit eChanged(); }

signals:
    void aChanged();
    void bChanged();
    void cChanged();
    void dChanged();
    void eChanged();

private:
    int _a;
    int _b;
    int _c;
    char _d;
    uchar _e;
};

#define TEST_CONTEXT_PROPERTY(ctxt, name, value) \
{ \
    QQmlComponent component(&engine); \
    component.setData("import QtQuick 2.0; QtObject { property variant test: " #name " }", QUrl()); \
\
    QObject *obj = component.create(ctxt); \
\
    QCOMPARE(obj->property("test"), value); \
\
    delete obj; \
}

void tst_qqmlcontext::setContextProperty()
{
    QQmlContext ctxt(&engine);
    QQmlContext ctxt2(&ctxt);

    TestObject obj1;
    obj1.setA(3345);
    TestObject obj2;
    obj2.setA(-19);

    // Static context properties
    ctxt.setContextProperty("a", QVariant(10));
    ctxt.setContextProperty("b", QVariant(9));
    ctxt2.setContextProperty("d", &obj2);
    ctxt2.setContextProperty("b", QVariant(19));
    ctxt2.setContextProperty("c", QVariant(QString("Hello World!")));
    ctxt.setContextProperty("d", &obj1);
    ctxt.setContextProperty("e", &obj1);

    TEST_CONTEXT_PROPERTY(&ctxt2, a, QVariant(10));
    TEST_CONTEXT_PROPERTY(&ctxt2, b, QVariant(19));
    TEST_CONTEXT_PROPERTY(&ctxt2, c, QVariant(QString("Hello World!")));
    TEST_CONTEXT_PROPERTY(&ctxt2, d.a, QVariant(-19));
    TEST_CONTEXT_PROPERTY(&ctxt2, e.a, QVariant(3345));

    ctxt.setContextProperty("a", QVariant(13));
    ctxt.setContextProperty("b", QVariant(4));
    ctxt2.setContextProperty("b", QVariant(8));
    ctxt2.setContextProperty("c", QVariant(QString("Hi World!")));
    ctxt2.setContextProperty("d", &obj1);
    obj1.setA(12);

    TEST_CONTEXT_PROPERTY(&ctxt2, a, QVariant(13));
    TEST_CONTEXT_PROPERTY(&ctxt2, b, QVariant(8));
    TEST_CONTEXT_PROPERTY(&ctxt2, c, QVariant(QString("Hi World!")));
    TEST_CONTEXT_PROPERTY(&ctxt2, d.a, QVariant(12));
    TEST_CONTEXT_PROPERTY(&ctxt2, e.a, QVariant(12));

    // Changes in context properties
    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0; QtObject { property variant test: a }", QUrl());

        QObject *obj = component.create(&ctxt2);

        QCOMPARE(obj->property("test"), QVariant(13));
        ctxt.setContextProperty("a", QVariant(19));
        QCOMPARE(obj->property("test"), QVariant(19));

        delete obj;
    }
    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0; QtObject { property variant test: b }", QUrl());

        QObject *obj = component.create(&ctxt2);

        QCOMPARE(obj->property("test"), QVariant(8));
        ctxt.setContextProperty("b", QVariant(5));
        QCOMPARE(obj->property("test"), QVariant(8));
        ctxt2.setContextProperty("b", QVariant(1912));
        QCOMPARE(obj->property("test"), QVariant(1912));

        delete obj;
    }
    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0; QtObject { property variant test: e.a }", QUrl());

        QObject *obj = component.create(&ctxt2);

        QCOMPARE(obj->property("test"), QVariant(12));
        obj1.setA(13);
        QCOMPARE(obj->property("test"), QVariant(13));

        delete obj;
    }

    // New context properties
    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0; QtObject { property variant test: a }", QUrl());

        QObject *obj = component.create(&ctxt2);

        QCOMPARE(obj->property("test"), QVariant(19));
        ctxt2.setContextProperty("a", QVariant(1945));
        QCOMPARE(obj->property("test"), QVariant(1945));

        delete obj;
    }

    // Setting an object-variant context property
    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0; QtObject { id: root; property int a: 10; property int test: ctxtProp.a; property variant obj: root; }", QUrl());

        QQmlContext ctxt(engine.rootContext());
        ctxt.setContextProperty("ctxtProp", QVariant());

        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>:1: TypeError: Cannot read property 'a' of undefined");
        QObject *obj = component.create(&ctxt);

        QVariant v = obj->property("obj");

        ctxt.setContextProperty("ctxtProp", v);

        QCOMPARE(obj->property("test"), QVariant(10));

        delete obj;
    }
}

void tst_qqmlcontext::setContextObject()
{
    QQmlContext ctxt(&engine);

    TestObject to;

    to.setA(2);
    to.setB(192);
    to.setC(18);

    ctxt.setContextObject(&to);
    ctxt.setContextProperty("c", QVariant(9));

    // Static context properties
    TEST_CONTEXT_PROPERTY(&ctxt, a, QVariant(2));
    TEST_CONTEXT_PROPERTY(&ctxt, b, QVariant(192));
    TEST_CONTEXT_PROPERTY(&ctxt, c, QVariant(9));

    to.setA(12);
    to.setB(100);
    to.setC(7);
    ctxt.setContextProperty("c", QVariant(3));

    TEST_CONTEXT_PROPERTY(&ctxt, a, QVariant(12));
    TEST_CONTEXT_PROPERTY(&ctxt, b, QVariant(100));
    TEST_CONTEXT_PROPERTY(&ctxt, c, QVariant(3));

    // Changes in context properties
    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick 2.0; QtObject { property variant test: a }", QUrl());

        QObject *obj = component.create(&ctxt);

        QCOMPARE(obj->property("test"), QVariant(12));
        to.setA(14);
        QCOMPARE(obj->property("test"), QVariant(14));

        delete obj;
    }

    // Change of context object
    ctxt.setContextProperty("c", QVariant(30));
    TestObject to2;
    to2.setA(10);
    to2.setB(20);
    to2.setC(40);
    ctxt.setContextObject(&to2);

    TEST_CONTEXT_PROPERTY(&ctxt, a, QVariant(10));
    TEST_CONTEXT_PROPERTY(&ctxt, b, QVariant(20));
    TEST_CONTEXT_PROPERTY(&ctxt, c, QVariant(30));
}

void tst_qqmlcontext::destruction()
{
    QQmlContext *ctxt = new QQmlContext(&engine);

    QObject obj;
    QQmlEngine::setContextForObject(&obj, ctxt);
    QQmlExpression expr(ctxt, 0, "a");

    QCOMPARE(ctxt, QQmlEngine::contextForObject(&obj));
    QCOMPARE(ctxt, expr.context());

    delete ctxt; ctxt = 0;

    QCOMPARE(ctxt, QQmlEngine::contextForObject(&obj));
    QCOMPARE(ctxt, expr.context());
}

void tst_qqmlcontext::idAsContextProperty()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; QtObject { property variant a; a: QtObject { id: myObject } }", QUrl());

    QObject *obj = component.create();
    QVERIFY(obj);

    QVariant a = obj->property("a");
    QCOMPARE(a.userType(), int(QMetaType::QObjectStar));

    QVariant ctxt = qmlContext(obj)->contextProperty("myObject");
    QCOMPARE(ctxt.userType(), int(QMetaType::QObjectStar));

    QCOMPARE(a, ctxt);

    delete obj;
}

// Internal contexts should be read-only
void tst_qqmlcontext::readOnlyContexts()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; QtObject { id: me }", QUrl());

    QObject *obj = component.create();
    QVERIFY(obj);

    QQmlContext *context = qmlContext(obj);
    QVERIFY(context);

    QCOMPARE(qvariant_cast<QObject*>(context->contextProperty("me")), obj);
    QCOMPARE(context->contextObject(), obj);

    QTest::ignoreMessage(QtWarningMsg, "QQmlContext: Cannot set property on internal context.");
    context->setContextProperty("hello", 12);
    QCOMPARE(context->contextProperty("hello"), QVariant());

    QTest::ignoreMessage(QtWarningMsg, "QQmlContext: Cannot set property on internal context.");
    context->setContextProperty("hello", obj);
    QCOMPARE(context->contextProperty("hello"), QVariant());

    QTest::ignoreMessage(QtWarningMsg, "QQmlContext: Cannot set context object for internal context.");
    context->setContextObject(0);
    QCOMPARE(context->contextObject(), obj);

    delete obj;
}

void tst_qqmlcontext::nameForObject()
{
    QObject o1;
    QObject o2;
    QObject o3;

    QQmlEngine engine;

    // As a context property
    engine.rootContext()->setContextProperty("o1", &o1);
    engine.rootContext()->setContextProperty("o2", &o2);
    engine.rootContext()->setContextProperty("o1_2", &o1);

    QCOMPARE(engine.rootContext()->nameForObject(&o1), QString("o1"));
    QCOMPARE(engine.rootContext()->nameForObject(&o2), QString("o2"));
    QCOMPARE(engine.rootContext()->nameForObject(&o3), QString());

    // As an id
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; QtObject { id: root; property QtObject o: QtObject { id: nested } }", QUrl());

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(qmlContext(o)->nameForObject(o), QString("root"));
    QCOMPARE(qmlContext(o)->nameForObject(qvariant_cast<QObject*>(o->property("o"))), QString("nested"));
    QCOMPARE(qmlContext(o)->nameForObject(&o1), QString());

    delete o;
}

class DeleteCommand : public QObject
{
Q_OBJECT
public:
    DeleteCommand() : object(0) {}

    QObject *object;

public slots:
    void doCommand() { if (object) delete object; object = 0; }
};

// Calling refresh expressions would crash if an expression or context was deleted during
// the refreshing
void tst_qqmlcontext::refreshExpressionsCrash()
{
    {
    QQmlEngine engine;

    DeleteCommand command;
    engine.rootContext()->setContextProperty("deleteCommand", &command);
    // We use a fresh context here to bypass any root-context optimizations in
    // the engine
    QQmlContext ctxt(engine.rootContext());

    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; QtObject { property var binding: deleteCommand.doCommand() }", QUrl());
    QVERIFY(component.isReady());

    QObject *o1 = component.create(&ctxt);
    QObject *o2 = component.create(&ctxt);

    command.object = o2;

    QQmlContextData::get(&ctxt)->refreshExpressions();

    delete o1;
    }
    {
    QQmlEngine engine;

    DeleteCommand command;
    engine.rootContext()->setContextProperty("deleteCommand", &command);
    // We use a fresh context here to bypass any root-context optimizations in
    // the engine
    QQmlContext ctxt(engine.rootContext());

    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; QtObject { property var binding: deleteCommand.doCommand() }", QUrl());
    QVERIFY(component.isReady());

    QObject *o1 = component.create(&ctxt);
    QObject *o2 = component.create(&ctxt);

    command.object = o1;

    QQmlContextData::get(&ctxt)->refreshExpressions();

    delete o2;
    }
}

class CountCommand : public QObject
{
Q_OBJECT
public:
    CountCommand() : count(0) {}

    int count;

public slots:
    void doCommand() { ++count; }
};


// Test that calling refresh expressions causes all the expressions to refresh
void tst_qqmlcontext::refreshExpressions()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("refreshExpressions.qml"));
    QQmlComponent component2(&engine, testFileUrl("RefreshExpressionsType.qml"));

    CountCommand command;
    engine.rootContext()->setContextProperty("countCommand", &command);

    // We use a fresh context here to bypass any root-context optimizations in
    // the engine
    QQmlContext context(engine.rootContext());
    QQmlContext context2(&context);

    QObject *o1 = component.create(&context);
    QObject *o2 = component.create(&context2);
    QObject *o3 = component2.create(&context);

    QCOMPARE(command.count, 5);

    QQmlContextData::get(&context)->refreshExpressions();

    QCOMPARE(command.count, 10);

    delete o3;
    delete o2;
    delete o1;
}

// Test that updating the root context, only causes expressions in contexts with an
// unresolved name to reevaluate
void tst_qqmlcontext::refreshExpressionsRootContext()
{
    QQmlEngine engine;

    CountCommand command;
    engine.rootContext()->setContextProperty("countCommand", &command);

    QQmlComponent component(&engine, testFileUrl("refreshExpressions.qml"));
    QQmlComponent component2(&engine, testFileUrl("refreshExpressionsRootContext.qml"));

    QQmlContext context(engine.rootContext());
    QQmlContext context2(engine.rootContext());

    QString warning = component2.url().toString() + QLatin1String(":4: ReferenceError: unresolvedName is not defined");

    QObject *o1 = component.create(&context);

    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));
    QObject *o2 = component2.create(&context2);

    QCOMPARE(command.count, 3);

    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));
    QQmlContextData::get(engine.rootContext())->refreshExpressions();

    QCOMPARE(command.count, 4);

    delete o2;
    delete o1;
}

void tst_qqmlcontext::skipExpressionRefresh_qtbug_53431()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("qtbug_53431.qml"));
    QScopedPointer<QObject> object(component.create(0));
    QVERIFY(!object.isNull());
    QCOMPARE(object->property("value").toInt(), 1);
    object->setProperty("value", 10);
    QCOMPARE(object->property("value").toInt(), 10);
    engine.rootContext()->setContextProperty("randomContextProperty", 42);
    QCOMPARE(object->property("value").toInt(), 10);
}

void tst_qqmlcontext::qtbug_22535()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("qtbug_22535.qml"));
    QQmlContext context(engine.rootContext());

    QObject *o = component.create(&context);

    // Don't crash!
    delete o;
}

void tst_qqmlcontext::evalAfterInvalidate()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("evalAfterInvalidate.qml"));
    QScopedPointer<QObject> o(component.create());

    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

void tst_qqmlcontext::qobjectDerived()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("refreshExpressions.qml"));

    CountCommand command;
    // This test is similar to refreshExpressions, but with the key difference that
    // we use the QVariant overload of setContextProperty. That way, we test that
    // QVariant knowledge that it contains a QObject derived pointer is used.
    engine.rootContext()->setContextProperty("countCommand", QVariant::fromValue(&command));

    // We use a fresh context here to bypass any root-context optimizations in
    // the engine
    QQmlContext context(engine.rootContext());

    QObject *o1 = component.create(&context);
    Q_UNUSED(o1);

    QCOMPARE(command.count, 2);
}

void tst_qqmlcontext::qtbug_49232()
{
    TestObject testObject;
    testObject.setD('a');
    testObject.setE(97);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("TestObject", &testObject);
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; QtObject { property int valueOne: TestObject.d; property int valueTwo: TestObject.e }", QUrl());
    QScopedPointer<QObject> obj(component.create());

    QCOMPARE(obj->property("valueOne"), QVariant('a'));
    QCOMPARE(obj->property("valueTwo"), QVariant(97));
}

QTEST_MAIN(tst_qqmlcontext)

#include "tst_qqmlcontext.moc"
