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
#include <private/qqmlengine_p.h>
#include <private/qquickrectangle_p.h>
#include <QJSEngine>
#include <QJSValue>

class tst_script : public QObject
{
    Q_OBJECT
public:
    tst_script() {}

private slots:
    void initTestCase();

    void property_js();
    void property_getter_js();
#if 0
   //no native functions for now
   void property_getter();
   void property_getter_qobject();
   void property_getter_qmetaproperty();
#endif
    void property_qobject();
    void property_qmlobject();

    void setproperty_js();
    void setproperty_qmlobject();

    void function_js();
#if 0
    //no native functions for now
    void function_cpp();
#endif
    void function_qobject();
    void function_qmlobject();

    void function_args_js();
#if 0
    //no native functions for now
    void function_args_cpp();
#endif
    void function_args_qobject();
    void function_args_qmlobject();

    void signal_unconnected();
    void signal_qml();
    void signal_args();
    void signal_unusedArgs();
    void signal_heavyArgsAccess();
    void signal_heavyIdAccess();

    void slot_simple();
    void slot_simple_js();
    void slot_complex();
    void slot_complex_js();

    void block_data();
    void block();

    void global_property_js();
    void global_property_qml();
    void global_property_qml_js();

    void scriptfile_property();

    void enums();
    void namespacedEnums();
    void scriptCall();
};

inline QUrl TEST_FILE(const QString &filename)
{
    return QUrl::fromLocalFile(QLatin1String(SRCDIR) + QLatin1String("/data/") + filename);
}

class TestObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int x READ x WRITE setX)

public:
    TestObject(QObject *parent = 0);

    int x();
    void setX(int x) { m_x = x; }

    void emitMySignal() { emit mySignal(); }
    void emitMySignalWithArgs(int n) { emit mySignalWithArgs(n); }

signals:
    void mySignal();
    void mySignalWithArgs(int n);

public slots:
    int method() {
        return x();
    }

    int methodArgs(int val) {
        return val + x();
    }

private:
    int m_x;
};
QML_DECLARE_TYPE(TestObject);

TestObject::TestObject(QObject *parent)
: QObject(parent), m_x(0)
{
}

int TestObject::x()
{
    return m_x++;
}

void tst_script::initTestCase()
{
    qmlRegisterType<TestObject>("Qt.test", 1, 0, "TestObject");
}


#define PROPERTY_PROGRAM \
    "(function(testObject) { return (function() { " \
    "    var test = 0; " \
    "    for (var ii = 0; ii < 10000; ++ii) { " \
    "        test += testObject.x; " \
    "    } " \
    "    return test; " \
    "}); })"

void tst_script::property_js()
{
    QJSEngine engine;

    QJSValue v = engine.newObject();
    v.setProperty(QLatin1String("x"), 10);

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(PROPERTY_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call().toNumber();
    }
}

#if 0
static QJSValue property_getter_method(QScriptContext *, QJSEngine *engine)
{
    static int x = 0;
    return QJSValue(engine,x++);
}

void tst_script::property_getter()
{
    QJSEngine engine;

    QJSValue v = engine.newObject();
    v.setProperty(QLatin1String("x"), engine.newFunction(property_getter_method),
                  QJSValue::PropertyGetter);

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(PROPERTY_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

static TestObject *property_getter_qobject_object = 0;
static QJSValue property_getter_qobject_method(QScriptContext *, QJSEngine *)
{
    static int idx = -1;
    if (idx == -1)
        idx = TestObject::staticMetaObject.indexOfProperty("x");

    int value = 0;
    void *args[] = { &value, 0 };
    QMetaObject::metacall(property_getter_qobject_object, QMetaObject::ReadProperty, idx, args);

    return QJSValue(value);
}

static QJSValue property_getter_qmetaproperty_method(QScriptContext *, QJSEngine *)
{
    static int idx = -1;
    if (idx == -1)
        idx = TestObject::staticMetaObject.indexOfProperty("x");

    int value = 0;
    value = property_getter_qobject_object->metaObject()->property(idx).read(property_getter_qobject_object).toInt();

    return QJSValue(value);
}

void tst_script::property_getter_qobject()
{
    QJSEngine engine;

    TestObject to;
    property_getter_qobject_object = &to;
    QJSValue v = engine.newObject();
    v.setProperty(QLatin1String("x"), engine.newFunction(property_getter_qobject_method),
                  QJSValue::PropertyGetter);

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(PROPERTY_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
    property_getter_qobject_object = 0;
}

void tst_script::property_getter_qmetaproperty()
{
    QJSEngine engine;

    TestObject to;
    property_getter_qobject_object = &to;
    QJSValue v = engine.newObject();
    v.setProperty(QLatin1String("x"), engine.newFunction(property_getter_qmetaproperty_method),
                  QJSValue::PropertyGetter);

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(PROPERTY_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
    property_getter_qobject_object = 0;
}
#endif

void tst_script::property_getter_js()
{
    QJSEngine engine;

    QJSValue v = engine.evaluate("(function() { var o = new Object; o._x = 0; o.__defineGetter__(\"x\", function() { return this._x++; }); return o; })").call();

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(PROPERTY_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

void tst_script::property_qobject()
{
    QJSEngine engine;

    TestObject to;
    QJSValue v = engine.newQObject(&to);

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(PROPERTY_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

void tst_script::property_qmlobject()
{
    QQmlEngine qmlengine;

    TestObject to;
    QV8Engine *engine = QQmlEnginePrivate::getV8Engine(&qmlengine);
    v8::HandleScope handle_scope;
    v8::Context::Scope scope(engine->context());
    QJSValue v = engine->scriptValueFromInternal(engine->qobjectWrapper()->newQObject(&to));

    QJSValueList args;
    args << v;
    QJSValue prog = qmlengine.evaluate(PROPERTY_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

#define SETPROPERTY_PROGRAM \
    "(function(testObject) { return (function() { " \
    "    for (var ii = 0; ii < 10000; ++ii) { " \
    "        testObject.x = ii; " \
    "    } " \
    "}); })"

void tst_script::setproperty_js()
{
    QJSEngine engine;

    QJSValue v = engine.newObject();
    v.setProperty(QLatin1String("x"), 0);

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(SETPROPERTY_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

void tst_script::setproperty_qmlobject()
{
    QQmlEngine qmlengine;

    TestObject to;

    QV8Engine *engine = QQmlEnginePrivate::getV8Engine(&qmlengine);
    v8::HandleScope handle_scope;
    v8::Context::Scope scope(engine->context());
    QJSValue v = engine->scriptValueFromInternal(engine->qobjectWrapper()->newQObject(&to));

    QJSValueList args;
    args << v;
    QJSValue prog = qmlengine.evaluate(SETPROPERTY_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

#define FUNCTION_PROGRAM \
    "(function(testObject) { return (function() { " \
    "    var test = 0; " \
    "    for (var ii = 0; ii < 10000; ++ii) { " \
    "        test += testObject.method(); " \
    "    } " \
    "    return test; " \
    "}); })"

void tst_script::function_js()
{
    QJSEngine engine;

    QJSValue v = engine.evaluate("(function() { var o = new Object; o._x = 0; o.method = (function() { return this._x++; }); return o; })").call();

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(FUNCTION_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

#if 0
static QJSValue function_method(QScriptContext *, QJSEngine *)
{
    static int x = 0;
    return QJSValue(x++);
}

void tst_script::function_cpp()
{
    QJSEngine engine;

    QJSValue v = engine.newObject();
    v.setProperty(QLatin1String("method"), engine.newFunction(function_method));

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(FUNCTION_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}
#endif

void tst_script::function_qobject()
{
    QJSEngine engine;

    TestObject to;
    QJSValue v = engine.newQObject(&to);

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(FUNCTION_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

void tst_script::function_qmlobject()
{
    QQmlEngine qmlengine;

    TestObject to;

    QV8Engine *engine = QQmlEnginePrivate::getV8Engine(&qmlengine);
    v8::HandleScope handle_scope;
    v8::Context::Scope scope(engine->context());
    QJSValue v = engine->scriptValueFromInternal(engine->qobjectWrapper()->newQObject(&to));

    QJSValueList args;
    args << v;
    QJSValue prog = qmlengine.evaluate(FUNCTION_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

#define FUNCTION_ARGS_PROGRAM \
    "(function(testObject) { return (function() { " \
    "    var test = 0; " \
    "    for (var ii = 0; ii < 10000; ++ii) { " \
    "        test += testObject.methodArgs(ii); " \
    "    } " \
    "    return test; " \
    "}); })"

void tst_script::function_args_js()
{
    QJSEngine engine;

    QJSValue v = engine.evaluate("(function() { var o = new Object; o._x = 0; o.methodArgs = (function(a) { return a + this._x++; }); return o; })").call();

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(FUNCTION_ARGS_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

#if 0
static QJSValue function_args_method(QScriptContext *ctxt, QJSEngine *)
{
    static int x = 0;
    return QJSValue(ctxt->argument(0).toNumber() + x++);
}

void tst_script::function_args_cpp()
{
    QJSEngine engine;

    QJSValue v = engine.newObject();
    v.setProperty(QLatin1String("methodArgs"), engine.newFunction(function_args_method));

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(FUNCTION_ARGS_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}
#endif

void tst_script::function_args_qobject()
{
    QJSEngine engine;

    TestObject to;
    QJSValue v = engine.newQObject(&to);

    QJSValueList args;
    args << v;
    QJSValue prog = engine.evaluate(FUNCTION_ARGS_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

void tst_script::function_args_qmlobject()
{
    QQmlEngine qmlengine;

    TestObject to;

    QV8Engine *engine = QQmlEnginePrivate::getV8Engine(&qmlengine);
    v8::HandleScope handle_scope;
    v8::Context::Scope scope(engine->context());
    QJSValue v = engine->scriptValueFromInternal(engine->qobjectWrapper()->newQObject(&to));

    QJSValueList args;
    args << v;
    QJSValue prog = qmlengine.evaluate(FUNCTION_ARGS_PROGRAM).call(args);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

void tst_script::signal_unconnected()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("signal_unconnected.qml"));
    TestObject *object = qobject_cast<TestObject *>(component.create());
    QVERIFY(object != 0);

    QBENCHMARK {
        object->emitMySignal();
    }

    delete object;
}

void tst_script::signal_qml()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("signal_qml.qml"));
    TestObject *object = qobject_cast<TestObject *>(component.create());
    QVERIFY(object != 0);

    QBENCHMARK {
        object->emitMySignal();
    }

    delete object;
}

void tst_script::signal_args()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("signal_args.qml"));
    TestObject *object = qobject_cast<TestObject *>(component.create());
    QVERIFY(object != 0);

    QBENCHMARK {
        object->emitMySignalWithArgs(11);
    }

    delete object;
}

void tst_script::signal_unusedArgs()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("signal_unusedArgs.qml"));
    TestObject *object = qobject_cast<TestObject *>(component.create());
    QVERIFY(object != 0);

    QBENCHMARK {
        object->emitMySignalWithArgs(11);
    }

    delete object;
}

void tst_script::signal_heavyArgsAccess()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("signal_heavyArgsAccess.qml"));
    TestObject *object = qobject_cast<TestObject *>(component.create());
    QVERIFY(object != 0);

    QBENCHMARK {
        object->emitMySignalWithArgs(11);
    }

    delete object;
}

void tst_script::signal_heavyIdAccess()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("signal_heavyIdAccess.qml"));
    TestObject *object = qobject_cast<TestObject *>(component.create());
    QVERIFY(object != 0);

    QBENCHMARK {
        object->emitMySignalWithArgs(11);
    }

    delete object;
}

void tst_script::slot_simple()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("slot_simple.qml"));
    TestObject *object = qobject_cast<TestObject *>(component.create());
    QVERIFY(object != 0);

    QBENCHMARK {
        object->emitMySignal();
    }

    delete object;
}

void tst_script::slot_simple_js()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("slot_simple_js.qml"));
    TestObject *object = qobject_cast<TestObject *>(component.create());
    QVERIFY(object != 0);

    QBENCHMARK {
        object->emitMySignal();
    }

    delete object;
}

void tst_script::slot_complex()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("slot_complex.qml"));
    TestObject *object = qobject_cast<TestObject *>(component.create());
    QVERIFY(object != 0);

    QBENCHMARK {
        object->emitMySignal();
    }

    delete object;
}

void tst_script::slot_complex_js()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("slot_complex_js.qml"));
    TestObject *object = qobject_cast<TestObject *>(component.create());
    QVERIFY(object != 0);

    QBENCHMARK {
        object->emitMySignal();
    }

    delete object;
}

void tst_script::block_data()
{
    QTest::addColumn<QString>("methodName");
    QTest::newRow("direct") << "doSomethingDirect()";
    QTest::newRow("localObj") << "doSomethingLocalObj()";
    QTest::newRow("local") << "doSomethingLocal()";
}

void tst_script::block()
{
    QFETCH(QString, methodName);
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("block.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle *>(component.create());
    QVERIFY(rect != 0);

    int index = rect->metaObject()->indexOfMethod(methodName.toUtf8());
    QVERIFY(index != -1);
    QMetaMethod method = rect->metaObject()->method(index);

    QBENCHMARK {
        method.invoke(rect, Qt::DirectConnection);
    }

    delete rect;
}

#define GLOBALPROPERTY_PROGRAM \
    "(function() { " \
    "    for (var ii = 0; ii < 10000; ++ii) { " \
    "        Math.sin(90); " \
    "    } " \
    "})"

void tst_script::global_property_js()
{
    QJSEngine engine;

    QJSValue prog = engine.evaluate(GLOBALPROPERTY_PROGRAM);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

void tst_script::global_property_qml()
{
    QQmlEngine qmlengine;

    QJSValue prog = qmlengine.evaluate(GLOBALPROPERTY_PROGRAM);
    prog.call();

    QBENCHMARK {
        prog.call();
    }
}

void tst_script::global_property_qml_js()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("global_prop.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle *>(component.create());
    QVERIFY(rect != 0);

    int index = rect->metaObject()->indexOfMethod("triggered()");
    QVERIFY(index != -1);
    QMetaMethod method = rect->metaObject()->method(index);

    QBENCHMARK {
        method.invoke(rect, Qt::DirectConnection);
    }

    delete rect;
}

void tst_script::scriptfile_property()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("global_prop.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle *>(component.create());
    QVERIFY(rect != 0);

    int index = rect->metaObject()->indexOfMethod("incrementTriggered()");
    QVERIFY(index != -1);
    QMetaMethod method = rect->metaObject()->method(index);

    QBENCHMARK {
        method.invoke(rect, Qt::DirectConnection);
    }

    delete rect;
}

void tst_script::enums()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("enums.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    int index = o->metaObject()->indexOfMethod("runtest()");
    QVERIFY(index != -1);
    QMetaMethod method = o->metaObject()->method(index);

    QBENCHMARK {
        method.invoke(o, Qt::DirectConnection);
    }

    delete o;
}

void tst_script::namespacedEnums()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("namespacedEnums.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    int index = o->metaObject()->indexOfMethod("runtest()");
    QVERIFY(index != -1);
    QMetaMethod method = o->metaObject()->method(index);

    QBENCHMARK {
        method.invoke(o, Qt::DirectConnection);
    }

    delete o;
}

void tst_script::scriptCall()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, TEST_FILE("scriptCall.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    int index = o->metaObject()->indexOfMethod("runtest()");
    QVERIFY(index != -1);
    QMetaMethod method = o->metaObject()->method(index);

    QBENCHMARK {
        method.invoke(o, Qt::DirectConnection);
    }

    delete o;
}

QTEST_MAIN(tst_script)

#include "tst_script.moc"
