/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtTest/QtTest>

#include <QtScript/qscriptcontext.h>
#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalueiterator.h>

Q_DECLARE_METATYPE(QScriptValueList)
Q_DECLARE_METATYPE(QScriptContext::Error)

QT_BEGIN_NAMESPACE
extern bool qt_script_isJITEnabled();
QT_END_NAMESPACE

class tst_QScriptContext : public QObject
{
    Q_OBJECT

public:
    tst_QScriptContext();
    virtual ~tst_QScriptContext();

private slots:
    void callee();
    void callee_implicitCall();
    void arguments();
    void argumentsInJS();
    void thisObject();
    void returnValue();
    void throwError_data();
    void throwError_fromEvaluate_data();
    void throwError_fromEvaluate();
    void throwError_fromCpp_data();
    void throwError_fromCpp();
    void throwValue();
    void evaluateInFunction();
    void pushAndPopContext();
    void pushAndPopContext_variablesInActivation();
    void pushAndPopContext_setThisObject();
    void pushAndPopContext_throwException();
    void lineNumber();
    void backtrace_data();
    void backtrace();
    void scopeChain_globalContext();
    void scopeChain_closure();
    void scopeChain_withStatement();
    void pushAndPopScope_globalContext();
    void pushAndPopScope_globalContext2();
    void getSetActivationObject_globalContext();
    void pushScopeEvaluate();
    void pushScopeCall();
    void popScopeSimple();
    void pushAndPopGlobalObjectSimple();
    void pushAndPopIterative();
    void getSetActivationObject_customContext();
    void inheritActivationAndThisObject();
    void toString();
    void calledAsConstructor_fromCpp();
    void calledAsConstructor_fromJS();
    void calledAsConstructor_parentContext();
    void argumentsObjectInNative();
    void jsActivationObject();
    void qobjectAsActivationObject();
    void parentContextCallee_QT2270();
    void popNativeContextScope();
    void throwErrorInGlobalContext();
    void throwErrorWithTypeInGlobalContext_data();
    void throwErrorWithTypeInGlobalContext();
    void throwValueInGlobalContext();
};

tst_QScriptContext::tst_QScriptContext()
{
}

tst_QScriptContext::~tst_QScriptContext()
{
}

static QScriptValue get_callee(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->callee();
}

static QScriptValue store_callee_and_return_primitive(QScriptContext *ctx, QScriptEngine *eng)
{
    ctx->thisObject().setProperty("callee", ctx->callee());
    return QScriptValue(eng, 123);
}

void tst_QScriptContext::callee()
{
    QScriptEngine eng;

    QScriptValue fun = eng.newFunction(get_callee);
    fun.setProperty("foo", QScriptValue(&eng, "bar"));
    eng.globalObject().setProperty("get_callee", fun);

    QScriptValue result = eng.evaluate("get_callee()");
    QCOMPARE(result.isFunction(), true);
    QCOMPARE(result.property("foo").toString(), QString("bar"));
}

void tst_QScriptContext::callee_implicitCall()
{
    QScriptEngine eng;
    // callee when toPrimitive() is called internally
    QScriptValue fun = eng.newFunction(store_callee_and_return_primitive);
    QScriptValue obj = eng.newObject();
    obj.setProperty("toString", fun);
    QVERIFY(!obj.property("callee").isValid());
    (void)obj.toString();
    QVERIFY(obj.property("callee").isFunction());
    QVERIFY(obj.property("callee").strictlyEquals(fun));

    obj.setProperty("callee", QScriptValue());
    QVERIFY(!obj.property("callee").isValid());
    obj.setProperty("valueOf", fun);
    (void)obj.toNumber();
    QVERIFY(obj.property("callee").isFunction());
    QVERIFY(obj.property("callee").strictlyEquals(fun));
}

static QScriptValue get_arguments(QScriptContext *ctx, QScriptEngine *eng)
{
    QScriptValue array = eng->newArray();
    for (int i = 0; i < ctx->argumentCount(); ++i)
        array.setProperty(QString::number(i), ctx->argument(i));
    return array;
}

static QScriptValue get_argumentsObject(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->argumentsObject();
}

void tst_QScriptContext::arguments()
{
    QScriptEngine eng;

    // See section 10.6 ("Arguments Object") of ECMA-262.

    {
        QScriptValue args = eng.currentContext()->argumentsObject();
        QVERIFY(args.isObject());
        QCOMPARE(args.property("length").toInt32(), 0);
    }
    {
        QScriptValue fun = eng.newFunction(get_arguments);
        eng.globalObject().setProperty("get_arguments", fun);
    }

    for (int x = 0; x < 2; ++x) {
        // The expected arguments array should be the same regardless of
        // whether get_arguments() is called as a constructor.
        QString prefix;
        if (x == 0)
            prefix = "";
        else
            prefix = "new ";
        {
            QScriptValue result = eng.evaluate(prefix+"get_arguments()");
            QCOMPARE(result.isArray(), true);
            QCOMPARE(result.property("length").toUInt32(), quint32(0));
        }

        {
            QScriptValue result = eng.evaluate(prefix+"get_arguments(123)");
            QCOMPARE(result.isArray(), true);
            QCOMPARE(result.property("length").toUInt32(), quint32(1));
            QCOMPARE(result.property("0").isNumber(), true);
            QCOMPARE(result.property("0").toNumber(), 123.0);
        }

        {
            QScriptValue result = eng.evaluate(prefix+"get_arguments(\"ciao\", null, true, undefined)");
            QCOMPARE(result.isArray(), true);
            QCOMPARE(result.property("length").toUInt32(), quint32(4));
            QCOMPARE(result.property("0").isString(), true);
            QCOMPARE(result.property("0").toString(), QString("ciao"));
            QCOMPARE(result.property("1").isNull(), true);
            QCOMPARE(result.property("2").isBoolean(), true);
            QCOMPARE(result.property("2").toBoolean(), true);
            QCOMPARE(result.property("3").isUndefined(), true);
        }

        {
            QScriptValue fun = eng.newFunction(get_argumentsObject);
            eng.globalObject().setProperty("get_argumentsObject", fun);
        }

        {
            QScriptValue fun = eng.evaluate("get_argumentsObject");
            QCOMPARE(fun.isFunction(), true);
            QScriptValue result = eng.evaluate(prefix+"get_argumentsObject()");
            QCOMPARE(result.isArray(), false);
            QVERIFY(result.isObject());
            QCOMPARE(result.property("length").toUInt32(), quint32(0));
            QCOMPARE(result.propertyFlags("length"), QScriptValue::SkipInEnumeration);
            QCOMPARE(result.property("callee").strictlyEquals(fun), true);
            QCOMPARE(result.propertyFlags("callee"), QScriptValue::SkipInEnumeration);

            // callee and length properties should be writable.
            QScriptValue replacedCallee(&eng, 123);
            result.setProperty("callee", replacedCallee);
            QVERIFY(result.property("callee").equals(replacedCallee));
            QScriptValue replacedLength(&eng, 456);
            result.setProperty("length", replacedLength);

            // callee and length properties should be deletable.
            QVERIFY(result.property("length").equals(replacedLength));
            result.setProperty("callee", QScriptValue());
            QVERIFY(!result.property("callee").isValid());
            result.setProperty("length", QScriptValue());
            QVERIFY(!result.property("length").isValid());
        }

        {
            QScriptValue result = eng.evaluate(prefix+"get_argumentsObject(123)");
            eng.evaluate("function nestedArg(x,y,z) { var w = get_argumentsObject('ABC' , x+y+z); return w; }");
            QScriptValue result2 = eng.evaluate("nestedArg(1, 'a', 2)");
            QCOMPARE(result.isArray(), false);
            QVERIFY(result.isObject());
            QCOMPARE(result.property("length").toUInt32(), quint32(1));
            QCOMPARE(result.property("0").isNumber(), true);
            QCOMPARE(result.property("0").toNumber(), 123.0);
            QVERIFY(result2.isObject());
            QCOMPARE(result2.property("length").toUInt32(), quint32(2));
            QCOMPARE(result2.property("0").toString(), QString::fromLatin1("ABC"));
            QCOMPARE(result2.property("1").toString(), QString::fromLatin1("1a2"));
        }

        {
            QScriptValue result = eng.evaluate(prefix+"get_argumentsObject(\"ciao\", null, true, undefined)");
            QCOMPARE(result.isArray(), false);
            QCOMPARE(result.property("length").toUInt32(), quint32(4));
            QCOMPARE(result.property("0").isString(), true);
            QCOMPARE(result.property("0").toString(), QString("ciao"));
            QCOMPARE(result.property("1").isNull(), true);
            QCOMPARE(result.property("2").isBoolean(), true);
            QCOMPARE(result.property("2").toBoolean(), true);
            QCOMPARE(result.property("3").isUndefined(), true);
        }
    }
}

void tst_QScriptContext::argumentsInJS()
{
    QScriptEngine eng;
    {
        QScriptValue result = eng.evaluate("(function() { return arguments; })(123)");
        QCOMPARE(result.isArray(), false);
        QVERIFY(result.isObject());
        QCOMPARE(result.property("length").toUInt32(), quint32(1));
        QCOMPARE(result.property("0").isNumber(), true);
        QCOMPARE(result.property("0").toNumber(), 123.0);
    }

    {
        QScriptValue result = eng.evaluate("(function() { return arguments; })('ciao', null, true, undefined)");
        QCOMPARE(result.isArray(), false);
        QCOMPARE(result.property("length").toUInt32(), quint32(4));
        QCOMPARE(result.property("0").isString(), true);
        QCOMPARE(result.property("0").toString(), QString("ciao"));
        QCOMPARE(result.property("1").isNull(), true);
        QCOMPARE(result.property("2").isBoolean(), true);
        QCOMPARE(result.property("2").toBoolean(), true);
        QCOMPARE(result.property("3").isUndefined(), true);
    }
}

static QScriptValue get_thisObject(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->thisObject();
}

void tst_QScriptContext::thisObject()
{
    QScriptEngine eng;

    QScriptValue fun = eng.newFunction(get_thisObject);
    eng.globalObject().setProperty("get_thisObject", fun);

    {
        QScriptValue result = eng.evaluate("get_thisObject()");
        QCOMPARE(result.isObject(), true);
        QCOMPARE(result.toString(), QString("[object global]"));
    }

    {
        QScriptValue result = eng.evaluate("get_thisObject.apply(new Number(123))");
        QCOMPARE(result.isObject(), true);
        QCOMPARE(result.toNumber(), 123.0);
    }

    {
        QScriptValue obj = eng.newObject();
        eng.currentContext()->setThisObject(obj);
        QVERIFY(eng.currentContext()->thisObject().equals(obj));
        eng.currentContext()->setThisObject(QScriptValue());
        QVERIFY(eng.currentContext()->thisObject().equals(obj));

        QScriptEngine eng2;
        QScriptValue obj2 = eng2.newObject();
        QTest::ignoreMessage(QtWarningMsg, "QScriptContext::setThisObject() failed: cannot set an object created in a different engine");
        eng.currentContext()->setThisObject(obj2);
    }
}

void tst_QScriptContext::returnValue()
{
    QSKIP("Internal function not implemented in JSC-based back-end");
    QScriptEngine eng;
    eng.evaluate("123");
    QCOMPARE(eng.currentContext()->returnValue().toNumber(), 123.0);
    eng.evaluate("\"ciao\"");
    QCOMPARE(eng.currentContext()->returnValue().toString(), QString("ciao"));
}

static QScriptValue throw_Error(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->throwError(QScriptContext::UnknownError, "foo");
}

static QScriptValue throw_TypeError(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->throwError(QScriptContext::TypeError, "foo");
}

static QScriptValue throw_ReferenceError(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->throwError(QScriptContext::ReferenceError, "foo");
}

static QScriptValue throw_SyntaxError(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->throwError(QScriptContext::SyntaxError, "foo");
}

static QScriptValue throw_RangeError(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->throwError(QScriptContext::RangeError, "foo");
}

static QScriptValue throw_URIError(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->throwError(QScriptContext::URIError, "foo");
}

static QScriptValue throw_ErrorAndReturnUndefined(QScriptContext *ctx, QScriptEngine *eng)
{
    ctx->throwError(QScriptContext::UnknownError, "foo");
    return eng->undefinedValue();
}

static QScriptValue throw_ErrorAndReturnString(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->throwError(QScriptContext::UnknownError, "foo").toString();
}

static QScriptValue throw_ErrorAndReturnObject(QScriptContext *ctx, QScriptEngine *eng)
{
    ctx->throwError(QScriptContext::UnknownError, "foo");
    return eng->newObject();
}

void tst_QScriptContext::throwError_data()
{
    QTest::addColumn<void*>("nativeFunctionPtr");
    QTest::addColumn<QString>("stringRepresentation");

    QTest::newRow("Error") << reinterpret_cast<void*>(throw_Error) << QString("Error: foo");
    QTest::newRow("TypeError") << reinterpret_cast<void*>(throw_TypeError) << QString("TypeError: foo");
    QTest::newRow("ReferenceError") << reinterpret_cast<void*>(throw_ReferenceError) << QString("ReferenceError: foo");
    QTest::newRow("SyntaxError") << reinterpret_cast<void*>(throw_SyntaxError) << QString("SyntaxError: foo");
    QTest::newRow("RangeError") << reinterpret_cast<void*>(throw_RangeError) << QString("RangeError: foo");
    QTest::newRow("URIError") << reinterpret_cast<void*>(throw_URIError) << QString("URIError: foo");
    QTest::newRow("ErrorAndReturnUndefined") << reinterpret_cast<void*>(throw_ErrorAndReturnUndefined) << QString("Error: foo");
    QTest::newRow("ErrorAndReturnString") << reinterpret_cast<void*>(throw_ErrorAndReturnString) << QString("Error: foo");
    QTest::newRow("ErrorAndReturnObject") << reinterpret_cast<void*>(throw_ErrorAndReturnObject) << QString("Error: foo");
}

void tst_QScriptContext::throwError_fromEvaluate_data()
{
    throwError_data();
}

void tst_QScriptContext::throwError_fromEvaluate()
{
    QFETCH(void*, nativeFunctionPtr);
    QScriptEngine::FunctionSignature nativeFunction = reinterpret_cast<QScriptEngine::FunctionSignature>(nativeFunctionPtr);
    QFETCH(QString, stringRepresentation);
    QScriptEngine engine;

    QScriptValue fun = engine.newFunction(nativeFunction);
    engine.globalObject().setProperty("throw_Error", fun);
    QScriptValue result = engine.evaluate("throw_Error()");
    QCOMPARE(engine.hasUncaughtException(), true);
    QCOMPARE(result.isError(), true);
    QCOMPARE(result.toString(), stringRepresentation);
}

void tst_QScriptContext::throwError_fromCpp_data()
{
    throwError_data();
}

void tst_QScriptContext::throwError_fromCpp()
{
    QFETCH(void*, nativeFunctionPtr);
    QScriptEngine::FunctionSignature nativeFunction = reinterpret_cast<QScriptEngine::FunctionSignature>(nativeFunctionPtr);
    QFETCH(QString, stringRepresentation);
    QScriptEngine engine;

    QScriptValue fun = engine.newFunction(nativeFunction);
    engine.globalObject().setProperty("throw_Error", fun);
    QScriptValue result = fun.call();
    QCOMPARE(engine.hasUncaughtException(), true);
    QCOMPARE(result.isError(), true);
    QCOMPARE(result.toString(), stringRepresentation);
}

static QScriptValue throw_value(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->throwValue(ctx->argument(0));
}

void tst_QScriptContext::throwValue()
{
    QScriptEngine eng;

    QScriptValue fun = eng.newFunction(throw_value);
    eng.globalObject().setProperty("throw_value", fun);

    {
        QScriptValue result = eng.evaluate("throw_value(123)");
        QCOMPARE(result.isError(), false);
        QCOMPARE(result.toNumber(), 123.0);
        QCOMPARE(eng.hasUncaughtException(), true);
    }
}

static QScriptValue evaluate(QScriptContext *, QScriptEngine *eng)
{
    return eng->evaluate("a = 123; a");
//    return eng->evaluate("a");
}

void tst_QScriptContext::evaluateInFunction()
{
    QScriptEngine eng;

    QScriptValue fun = eng.newFunction(evaluate);
    eng.globalObject().setProperty("evaluate", fun);

    QScriptValue result = eng.evaluate("evaluate()");
    QCOMPARE(result.isError(), false);
    QCOMPARE(result.isNumber(), true);
    QCOMPARE(result.toNumber(), 123.0);
    QCOMPARE(eng.hasUncaughtException(), false);

    QCOMPARE(eng.evaluate("a").toNumber(), 123.0);
}

void tst_QScriptContext::pushAndPopContext()
{
    QScriptEngine eng;
    QScriptContext *topLevel = eng.currentContext();
    QCOMPARE(topLevel->engine(), &eng);

    QScriptContext *ctx = eng.pushContext();
    QVERIFY(ctx != 0);
    QCOMPARE(ctx->parentContext(), topLevel);
    QCOMPARE(eng.currentContext(), ctx);
    QCOMPARE(ctx->engine(), &eng);
    QCOMPARE(ctx->state(), QScriptContext::NormalState);
    QCOMPARE(ctx->isCalledAsConstructor(), false);
    QCOMPARE(ctx->argumentCount(), 0);
    QCOMPARE(ctx->argument(0).isUndefined(), true);
    QVERIFY(!ctx->argument(-1).isValid());
    QCOMPARE(ctx->argumentsObject().isObject(), true);
    QCOMPARE(ctx->activationObject().isObject(), true);
    QCOMPARE(ctx->callee().isValid(), false);
    QCOMPARE(ctx->thisObject().strictlyEquals(eng.globalObject()), true);
    QCOMPARE(ctx->scopeChain().size(), 2);
    QVERIFY(ctx->scopeChain().at(0).equals(ctx->activationObject()));
    QVERIFY(ctx->scopeChain().at(1).equals(eng.globalObject()));

    QScriptContext *ctx2 = eng.pushContext();
    QCOMPARE(ctx2->parentContext(), ctx);
    QCOMPARE(eng.currentContext(), ctx2);

    eng.popContext();
    QCOMPARE(eng.currentContext(), ctx);
    eng.popContext();
    QCOMPARE(eng.currentContext(), topLevel);

    // popping the top-level context is not allowed
    QTest::ignoreMessage(QtWarningMsg, "QScriptEngine::popContext() doesn't match with pushContext()");
    eng.popContext();
    QCOMPARE(eng.currentContext(), topLevel);
}

void tst_QScriptContext::pushAndPopContext_variablesInActivation()
{
    QScriptEngine eng;
    QScriptContext *ctx = eng.pushContext();
    ctx->activationObject().setProperty("foo", QScriptValue(&eng, 123));
    // evaluate() should use the current context.
    QVERIFY(eng.evaluate("foo").strictlyEquals(QScriptValue(&eng, 123)));
    QCOMPARE(ctx->activationObject().propertyFlags("foo"), QScriptValue::PropertyFlags(0));

    ctx->activationObject().setProperty(4, 456);
    QVERIFY(ctx->activationObject().property(4, QScriptValue::ResolveLocal).equals(456));

    // New JS variables should become properties of the current context's activation.
    eng.evaluate("var bar = 'ciao'");
    QVERIFY(ctx->activationObject().property("bar", QScriptValue::ResolveLocal).strictlyEquals(QScriptValue(&eng, "ciao")));

    ctx->activationObject().setProperty("baz", 789, QScriptValue::ReadOnly);
    QVERIFY(eng.evaluate("baz").equals(789));
    QCOMPARE(ctx->activationObject().propertyFlags("baz"), QScriptValue::ReadOnly);

    QSet<QString> activationPropertyNames;
    QScriptValueIterator it(ctx->activationObject());
    while (it.hasNext()) {
        it.next();
        activationPropertyNames.insert(it.name());
    }
    QCOMPARE(activationPropertyNames.size(), 4);
    QVERIFY(activationPropertyNames.contains("foo"));
    QVERIFY(activationPropertyNames.contains("4"));
    QVERIFY(activationPropertyNames.contains("bar"));
    QVERIFY(activationPropertyNames.contains("baz"));

    eng.popContext();
}

void tst_QScriptContext::pushAndPopContext_setThisObject()
{
    QScriptEngine eng;
    QScriptContext *ctx = eng.pushContext();
    QScriptValue obj = eng.newObject();
    obj.setProperty("prop", QScriptValue(&eng, 456));
    ctx->setThisObject(obj);
    QScriptValue ret = eng.evaluate("var tmp = this.prop; tmp + 1");
    QCOMPARE(eng.currentContext(), ctx);
    QVERIFY(ret.strictlyEquals(QScriptValue(&eng, 457)));
    eng.popContext();
}

void tst_QScriptContext::pushAndPopContext_throwException()
{
    QScriptEngine eng;
    QScriptContext *ctx = eng.pushContext();
    QScriptValue ret = eng.evaluate("throw new Error('oops')");
    QVERIFY(ret.isError());
    QVERIFY(eng.hasUncaughtException());
    QCOMPARE(eng.currentContext(), ctx);
    eng.popContext();
}

void tst_QScriptContext::popNativeContextScope()
{
    QScriptEngine eng;
    QScriptContext *ctx = eng.pushContext();
    QVERIFY(ctx->popScope().isObject()); // the activation object

    QCOMPARE(ctx->scopeChain().size(), 1);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(eng.globalObject()));
    // This was different in 4.5: scope and activation were decoupled
    QVERIFY(ctx->activationObject().strictlyEquals(eng.globalObject()));

    QVERIFY(!eng.evaluate("var foo = 123; function bar() {}").isError());
    QVERIFY(eng.globalObject().property("foo").isNumber());
    QVERIFY(eng.globalObject().property("bar").isFunction());

    QScriptValue customScope = eng.newObject();
    ctx->pushScope(customScope);
    QCOMPARE(ctx->scopeChain().size(), 2);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(customScope));
    QVERIFY(ctx->scopeChain().at(1).strictlyEquals(eng.globalObject()));
    QVERIFY(ctx->activationObject().strictlyEquals(eng.globalObject()));
    ctx->setActivationObject(customScope);
    QVERIFY(ctx->activationObject().strictlyEquals(customScope));
    QCOMPARE(ctx->scopeChain().size(), 2);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(customScope));
    QEXPECT_FAIL("", "QTBUG-11012: Global object is replaced in scope chain", Continue);
    QVERIFY(ctx->scopeChain().at(1).strictlyEquals(eng.globalObject()));

    QVERIFY(!eng.evaluate("baz = 456; var foo = 789; function barbar() {}").isError());
    QEXPECT_FAIL("", "QTBUG-11012", Continue);
    QVERIFY(eng.globalObject().property("baz").isNumber());
    QVERIFY(customScope.property("foo").isNumber());
    QVERIFY(customScope.property("barbar").isFunction());

    QVERIFY(ctx->popScope().strictlyEquals(customScope));
    QCOMPARE(ctx->scopeChain().size(), 1);
    QEXPECT_FAIL("", "QTBUG-11012", Continue);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(eng.globalObject()));

    // Need to push another object, otherwise we crash in popContext() (QTBUG-11012)
    ctx->pushScope(customScope);
    eng.popContext();
}

void tst_QScriptContext::lineNumber()
{
    QScriptEngine eng;

    QScriptValue result = eng.evaluate("try { eval(\"foo = 123;\\n this[is{a{syntax|error@#$%@#% \"); } catch (e) { e.lineNumber; }", "foo.qs", 123);
    QVERIFY(!eng.hasUncaughtException());
    QVERIFY(result.isNumber());
    QCOMPARE(result.toInt32(), 2);

    result = eng.evaluate("foo = 123;\n bar = 42\n0 = 0");
    QVERIFY(eng.hasUncaughtException());
    QCOMPARE(eng.uncaughtExceptionLineNumber(), 3);
    QCOMPARE(result.property("lineNumber").toInt32(), 3);
}

static QScriptValue getBacktrace(QScriptContext *ctx, QScriptEngine *eng)
{
    return qScriptValueFromValue(eng, ctx->backtrace());
}

static QScriptValue custom_eval(QScriptContext *ctx, QScriptEngine *eng)
{
    return eng->evaluate(ctx->argumentsObject().property(0).toString(), ctx->argumentsObject().property(1).toString());
}

static QScriptValue custom_call(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->argumentsObject().property(0).call(QScriptValue(), QScriptValueList() << ctx->argumentsObject().property(1));
}

static QScriptValue native_recurse(QScriptContext *ctx, QScriptEngine *eng)
{
    QScriptValue func = ctx->argumentsObject().property(0);
    QScriptValue n = ctx->argumentsObject().property(1);

    if(n.toUInt32() <= 1) {
        return func.call(QScriptValue(), QScriptValueList());
    } else {
        return eng->evaluate("native_recurse").call(QScriptValue(),
                                                    QScriptValueList() << func << QScriptValue(n.toUInt32() - 1));
    }
}

void tst_QScriptContext::backtrace_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<QStringList>("expectedbacktrace");

    {
        QString source(
                 "function foo() {\n"
                 "  return bt(123);\n"
                 "}\n"
                 "foo('hello', { })\n"
                 "var r = 0;");

        QStringList expected;
        expected << "<native>(123) at -1"
                 << "foo('hello', [object Object]) at testfile:2"
                 << "<global>() at testfile:4";


        QTest::newRow("simple") << source << expected;
    }

    {
        QStringList expected;
        QString source = QString(
            "function foo(arg1 , arg2) {\n"
            "  return eval(\"%1\");\n"
            "}\n"
            "foo('hello', 456)\n"
            "var a = 0;"
           ).arg("\\n \\n bt('hey'); \\n");

           expected << "<native>('hey') at -1"
                    << "<eval>() at 3"
                    << "foo(arg1 = 'hello', arg2 = 456) at testfile:2"
                    << "<global>() at testfile:4";

            QTest::newRow("eval") << source << expected;
    }

    {
        QString eval_code(
                "function bar(a) {\\n"
                "  return bt('m');\\n"
                "}\\n"
                "bar('b'); \\n");
        QString source = QString(
                "function foo() {\n"
                "  return custom_eval(\"%1\", 'eval.js');\n"
                "}\n"
                "foo()"
            ).arg(eval_code);

        QStringList expected;
        expected << "<native>('m') at -1"
                 << "bar(a = 'b') at eval.js:2"
                 << "<eval>() at eval.js:4"
                 << QString("<native>('%1', 'eval.js') at -1").arg(eval_code.replace("\\n", "\n"))
                 << "foo() at testfile:2"
                 << "<global>() at testfile:4";

        QTest::newRow("custom_eval") << source << expected;
    }
    {
        QString f("function (a) {\n return bt(a); \n  }");
        QString source = QString(
                "function foo(f) {\n"
                "  return f('b');\n"
                "}\n"
                "foo(%1)"
            ).arg(f);

        QStringList expected;
        expected << "<native>('b') at -1"
                 << "<anonymous>(a = 'b') at testfile:5"
                 << QString("foo(f = %1) at testfile:2").arg(f)
                 << "<global>() at testfile:6";

        QTest::newRow("closure") << source << expected;
    }

    {
        QStringList expected;
        QString source = QString(
            "var o = new Object;\n"
            "o.foo = function plop() {\n"
            "  return eval(\"%1\");\n"
            "}\n"
            "o.foo('hello', 456)\n"
           ).arg("\\n \\n bt('hey'); \\n");

           expected << "<native>('hey') at -1"
                    << "<eval>() at 3"
                    << "plop('hello', 456) at testfile:3"
                    << "<global>() at testfile:5";

            QTest::newRow("eval in member") << source << expected;
    }

    {
        QString source(
                 "function foo(a) {\n"
                 "  return bt(123);\n"
                 "}\n"
                 "function bar() {\n"
                 "  var v = foo('arg', 4);\n"
                 "  return v;\n"
                 "}\n"
                 "bar('hello', { });\n");

        QStringList expected;
        expected << "<native>(123) at -1"
                 << "foo(a = 'arg', 4) at testfile:2"
                 << "bar('hello', [object Object]) at testfile:5"
                 << "<global>() at testfile:8";


        QTest::newRow("two function") << source << expected;
    }

    {
        QString func("function foo(a, b) {\n"
                     "  return bt(a);\n"
                     "}");

        QString source = func + QString::fromLatin1("\n"
                 "custom_call(foo, 'hello');\n"
                 "var a = 1\n");

        QStringList expected;
        expected << "<native>('hello') at -1"
                 << "foo(a = 'hello') at testfile:2"
                 << QString("<native>(%1, 'hello') at -1").arg(func)
                 << "<global>() at testfile:4";

        QTest::newRow("call") << source << expected;
    }

    {
        QString source = QString::fromLatin1("\n"
        "custom_call(bt, 'hello_world');\n"
        "var a = 1\n");

        QStringList expected;
        expected << "<native>('hello_world') at -1"
        << "<native>(function () {\n    [native code]\n}, 'hello_world') at -1"
        << "<global>() at testfile:2";

        QTest::newRow("call native") << source << expected;
    }

    {
        QLatin1String func( "function f1() {\n"
            "    eval('var q = 4');\n"
            "    return custom_call(bt, 22);\n"
            "}");

        QString source = QString::fromLatin1("\n"
            "function f2() {\n"
            "    func = %1\n"
            "    return custom_call(func, 12);\n"
            "}\n"
            "f2();\n").arg(func);

        QStringList expected;
        expected << "<native>(22) at -1"
            << "<native>(function () {\n    [native code]\n}, 22) at -1"
            << "f1(12) at testfile:5"
            << QString::fromLatin1("<native>(%1, 12) at -1").arg(func)
            << "f2() at testfile:7"
            << "<global>() at testfile:9";


        QTest::newRow("calls with closures") << source << expected;
    }

    {
        QLatin1String func( "function js_bt() {\n"
            "    return bt();\n"
            "}");

        QString source = QString::fromLatin1("\n"
            "%1\n"
            "function f() {\n"
            "    return native_recurse(js_bt, 12);\n"
            "}\n"
            "f();\n").arg(func);

        QStringList expected;
        expected << "<native>() at -1" << "js_bt() at testfile:3";
        for(int n = 1; n <= 12; n++) {
            expected << QString::fromLatin1("<native>(%1, %2) at -1")
                .arg(func).arg(n);
        }
        expected << "f() at testfile:6";
        expected << "<global>() at testfile:8";

        QTest::newRow("native recursive") << source << expected;
    }

    {
        QString source = QString::fromLatin1("\n"
            "function finish() {\n"
            "    return bt();\n"
            "}\n"
            "function rec(n) {\n"
            "    if(n <= 1)\n"
            "        return finish();\n"
            "    else\n"
            "        return rec (n - 1);\n"
            "}\n"
            "function f() {\n"
            "    return rec(12);\n"
            "}\n"
            "f();\n");

        QStringList expected;
        expected << "<native>() at -1" << "finish() at testfile:3";
        for(int n = 1; n <= 12; n++) {
            expected << QString::fromLatin1("rec(n = %1) at testfile:%2")
                .arg(n).arg((n==1) ? 7 : 9);
        }
        expected << "f() at testfile:12";
        expected << "<global>() at testfile:14";

        QTest::newRow("js recursive") << source << expected;
    }

    {
        QString source = QString::fromLatin1(
            "[0].forEach(\n"
            "    function() {\n"
            "        result = bt();\n"
            "}); result");

        QStringList expected;
        expected << "<native>() at -1"
                 << "<anonymous>(0, 0, 0) at testfile:3"
                 << QString::fromLatin1("forEach(%0) at -1")
                    // Because the JIT doesn't store the arguments in the frame
                    // for built-in functions, arguments are not available.
                    // Will work when the copy of JavaScriptCore is updated
                    // (QTBUG-16568).
                    .arg(qt_script_isJITEnabled()
                         ? ""
                         : "function () {\n        result = bt();\n}")
                 << "<global>() at testfile:4";
        QTest::newRow("js callback from built-in") << source << expected;
    }

    {
        QString source = QString::fromLatin1(
            "[10,20].forEach(\n"
            "    function() {\n"
            "        result = bt();\n"
            "}); result");

        QStringList expected;
        expected << "<native>() at -1"
                 << "<anonymous>(20, 1, 10,20) at testfile:3"
                 << QString::fromLatin1("forEach(%0) at -1")
                    // Because the JIT doesn't store the arguments in the frame
                    // for built-in functions, arguments are not available.
                    // Will work when the copy of JavaScriptCore is updated
                    // (QTBUG-16568).
                    .arg(qt_script_isJITEnabled()
                         ? ""
                         : "function () {\n        result = bt();\n}")
                 << "<global>() at testfile:4";
        QTest::newRow("js callback from built-in") << source << expected;
    }
}


void tst_QScriptContext::backtrace()
{
    QFETCH(QString, code);
    QFETCH(QStringList, expectedbacktrace);

    QScriptEngine eng;
    eng.globalObject().setProperty("bt", eng.newFunction(getBacktrace));
    eng.globalObject().setProperty("custom_eval", eng.newFunction(custom_eval));
    eng.globalObject().setProperty("custom_call", eng.newFunction(custom_call));
    eng.globalObject().setProperty("native_recurse", eng.newFunction(native_recurse));

    QString fileName = "testfile";
    QScriptValue ret = eng.evaluate(code, fileName);
    QVERIFY(!eng.hasUncaughtException());
    QVERIFY(ret.isArray());
    QStringList slist = qscriptvalue_cast<QStringList>(ret);
    QEXPECT_FAIL("eval", "QTBUG-17842: Missing line number in backtrace when function calls eval()", Continue);
    QEXPECT_FAIL("eval in member", "QTBUG-17842: Missing line number in backtrace when function calls eval()", Continue);
    QCOMPARE(slist, expectedbacktrace);
}

static QScriptValue getScopeChain(QScriptContext *ctx, QScriptEngine *eng)
{
    return qScriptValueFromValue(eng, ctx->parentContext()->scopeChain());
}

void tst_QScriptContext::scopeChain_globalContext()
{
    QScriptEngine eng;
    {
        QScriptValueList ret = eng.currentContext()->scopeChain();
        QCOMPARE(ret.size(), 1);
        QVERIFY(ret.at(0).strictlyEquals(eng.globalObject()));
    }
    {
        eng.globalObject().setProperty("getScopeChain", eng.newFunction(getScopeChain));
        QScriptValueList ret = qscriptvalue_cast<QScriptValueList>(eng.evaluate("getScopeChain()"));
        QCOMPARE(ret.size(), 1);
        QVERIFY(ret.at(0).strictlyEquals(eng.globalObject()));
    }
}

void tst_QScriptContext::scopeChain_closure()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("getScopeChain", eng.newFunction(getScopeChain));

    eng.evaluate("function foo() { function bar() { return getScopeChain(); } return bar() }");
    QScriptValueList ret = qscriptvalue_cast<QScriptValueList>(eng.evaluate("foo()"));
    // JSC will not create an activation for bar() unless we insert a call
    // to eval() in the function body; JSC has no way of knowing that the
    // native function will be asking for the activation, and we don't want
    // to needlessly create it.
    QEXPECT_FAIL("", "QTBUG-10313: JSC optimizes away the activation object", Abort);
    QCOMPARE(ret.size(), 3);
    QVERIFY(ret.at(2).strictlyEquals(eng.globalObject()));
    QCOMPARE(ret.at(1).toString(), QString::fromLatin1("activation"));
    QVERIFY(ret.at(1).property("arguments").isObject());
    QCOMPARE(ret.at(0).toString(), QString::fromLatin1("activation"));
    QVERIFY(ret.at(0).property("arguments").isObject());
}

void tst_QScriptContext::scopeChain_withStatement()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("getScopeChain", eng.newFunction(getScopeChain));
    {
        QScriptValueList ret = qscriptvalue_cast<QScriptValueList>(eng.evaluate("o = { x: 123 }; with(o) getScopeChain();"));
        QEXPECT_FAIL("", "QTBUG-17131: with-scope isn't reflected by QScriptContext", Abort);
        QCOMPARE(ret.size(), 2);
        QVERIFY(ret.at(1).strictlyEquals(eng.globalObject()));
        QVERIFY(ret.at(0).isObject());
        QCOMPARE(ret.at(0).property("x").toInt32(), 123);
    }
    {
        QScriptValueList ret = qscriptvalue_cast<QScriptValueList>(
            eng.evaluate("o1 = { x: 123}; o2 = { y: 456 }; with(o1) { with(o2) { getScopeChain(); } }"));
        QCOMPARE(ret.size(), 3);
        QVERIFY(ret.at(2).strictlyEquals(eng.globalObject()));
        QVERIFY(ret.at(1).isObject());
        QCOMPARE(ret.at(1).property("x").toInt32(), 123);
        QVERIFY(ret.at(0).isObject());
        QCOMPARE(ret.at(0).property("y").toInt32(), 456);
    }
}

void tst_QScriptContext::pushScopeEvaluate()
{
    QScriptEngine engine;
    QScriptValue object = engine.newObject();
    object.setProperty("foo", 1234);
    object.setProperty(1, 1234);
    engine.currentContext()->pushScope(object);
    object.setProperty("bar", 4321);
    object.setProperty(2, 4321);
    QVERIFY(engine.evaluate("foo").equals(1234));
    QVERIFY(engine.evaluate("bar").equals(4321));
}

void tst_QScriptContext::pushScopeCall()
{
    QScriptEngine engine;
    QScriptValue object = engine.globalObject();
    QScriptValue thisObject = engine.newObject();
    QScriptValue function = engine.evaluate("(function(property){return this[property]; })");
    QVERIFY(function.isFunction());
    object.setProperty("foo", 1234);
    thisObject.setProperty("foo", "foo");
    engine.currentContext()->pushScope(object);
    object.setProperty("bar", 4321);
    thisObject.setProperty("bar", "bar");
    QVERIFY(function.call(QScriptValue(), QScriptValueList() << "foo").equals(1234));
    QVERIFY(function.call(QScriptValue(), QScriptValueList() << "bar").equals(4321));
    QVERIFY(function.call(thisObject, QScriptValueList() << "foo").equals("foo"));
    QVERIFY(function.call(thisObject, QScriptValueList() << "bar").equals("bar"));
}

void tst_QScriptContext::popScopeSimple()
{
    QScriptEngine engine;
    QScriptValue object = engine.newObject();
    QScriptValue globalObject = engine.globalObject();
    engine.currentContext()->pushScope(object);
    QVERIFY(engine.currentContext()->popScope().strictlyEquals(object));
    QVERIFY(engine.globalObject().strictlyEquals(globalObject));
}

void tst_QScriptContext::pushAndPopGlobalObjectSimple()
{
    QScriptEngine engine;
    QScriptValue globalObject = engine.globalObject();
    engine.currentContext()->pushScope(globalObject);
    QVERIFY(engine.currentContext()->popScope().strictlyEquals(globalObject));
    QVERIFY(engine.globalObject().strictlyEquals(globalObject));
}

void tst_QScriptContext::pushAndPopIterative()
{
    QScriptEngine engine;
    for (uint repeat = 0; repeat < 2; ++repeat) {
        for (uint i = 1; i < 11; ++i) {
            QScriptValue object = engine.newObject();
            object.setProperty("x", i + 10 * repeat);
            engine.currentContext()->pushScope(object);
        }
        for (uint i = 10; i > 0; --i) {
            QScriptValue object = engine.currentContext()->popScope();
            QVERIFY(object.property("x").equals(i + 10 * repeat));
        }
    }
}

void tst_QScriptContext::pushAndPopScope_globalContext()
{
    QScriptEngine eng;
    QScriptContext *ctx = eng.currentContext();
    QCOMPARE(ctx->scopeChain().size(), 1);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(eng.globalObject()));

    QVERIFY(ctx->popScope().strictlyEquals(eng.globalObject()));
    ctx->pushScope(eng.globalObject());
    QCOMPARE(ctx->scopeChain().size(), 1);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(eng.globalObject()));

    QScriptValue obj = eng.newObject();
    ctx->pushScope(obj);
    QCOMPARE(ctx->scopeChain().size(), 2);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(obj));
    QVERIFY(ctx->scopeChain().at(1).strictlyEquals(eng.globalObject()));

    QVERIFY(ctx->popScope().strictlyEquals(obj));
    QCOMPARE(ctx->scopeChain().size(), 1);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(eng.globalObject()));

    {
        QScriptValue ret = eng.evaluate("x");
        QVERIFY(ret.isError());
        eng.clearExceptions();
    }
    QCOMPARE(ctx->scopeChain().size(), 1);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(eng.globalObject()));

    // task 236685
    QScriptValue qobj = eng.newQObject(this, QScriptEngine::QtOwnership, QScriptEngine::AutoCreateDynamicProperties);
    ctx->pushScope(qobj);
    QCOMPARE(ctx->scopeChain().size(), 2);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(qobj));
    QVERIFY(ctx->scopeChain().at(1).strictlyEquals(eng.globalObject()));
    {
        QScriptValue ret = eng.evaluate("print");
        QVERIFY(ret.isFunction());
    }
    ctx->popScope();

    ctx->pushScope(obj);
    QCOMPARE(ctx->scopeChain().size(), 2);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(obj));
    obj.setProperty("x", 123);
    {
        QScriptValue ret = eng.evaluate("x");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }
    QVERIFY(ctx->popScope().strictlyEquals(obj));
    QCOMPARE(ctx->scopeChain().size(), 1);
    QVERIFY(ctx->scopeChain().at(0).strictlyEquals(eng.globalObject()));

    ctx->pushScope(QScriptValue());
    QCOMPARE(ctx->scopeChain().size(), 1);
}

void tst_QScriptContext::pushAndPopScope_globalContext2()
{
    QScriptEngine eng;
    QScriptContext *ctx = eng.currentContext();
    QVERIFY(ctx->popScope().strictlyEquals(eng.globalObject()));
    QVERIFY(ctx->scopeChain().isEmpty());

    // Used to work with old back-end, doesn't with new one because JSC requires that the last object in
    // a scope chain is the Global Object.
    QTest::ignoreMessage(QtWarningMsg, "QScriptContext::pushScope() failed: initial object in scope chain has to be the Global Object");
    ctx->pushScope(eng.newObject());
    QCOMPARE(ctx->scopeChain().size(), 0);

    QScriptEngine eng2;
    QScriptValue obj2 = eng2.newObject();
    QTest::ignoreMessage(QtWarningMsg, "QScriptContext::pushScope() failed: cannot push an object created in a different engine");
    ctx->pushScope(obj2);
    QVERIFY(ctx->scopeChain().isEmpty());

    QVERIFY(!ctx->popScope().isValid());
}

static QScriptValue get_activationObject(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->activationObject();
}

void tst_QScriptContext::getSetActivationObject_globalContext()
{
    QScriptEngine eng;
    QScriptContext *ctx = eng.currentContext();
    QVERIFY(ctx->activationObject().equals(eng.globalObject()));

    ctx->setActivationObject(QScriptValue());
    QVERIFY(ctx->activationObject().equals(eng.globalObject()));
    QCOMPARE(ctx->engine(), &eng);

    QScriptValue obj = eng.newObject();
    ctx->setActivationObject(obj);
    QVERIFY(ctx->activationObject().equals(obj));
    QCOMPARE(ctx->scopeChain().size(), 1);
    QVERIFY(ctx->scopeChain().at(0).equals(obj));

    {
        QScriptEngine eng2;
        QScriptValue obj2 = eng2.newObject();
        QTest::ignoreMessage(QtWarningMsg, "QScriptContext::setActivationObject() failed: cannot set an object created in a different engine");
        QScriptValue was = ctx->activationObject();
        ctx->setActivationObject(obj2);
        QVERIFY(ctx->activationObject().equals(was));
    }

    ctx->setActivationObject(eng.globalObject());
    QVERIFY(ctx->activationObject().equals(eng.globalObject()));
    QScriptValue fun = eng.newFunction(get_activationObject);
    eng.globalObject().setProperty("get_activationObject", fun);
    {
        QScriptValue ret = eng.evaluate("get_activationObject(1, 2, 3)");
        QVERIFY(ret.isObject());
        QScriptValue arguments = ret.property("arguments");
        QEXPECT_FAIL("", "QTBUG-17136: arguments property of activation object is missing", Abort);
        QVERIFY(arguments.isObject());
        QCOMPARE(arguments.property("length").toInt32(), 3);
        QCOMPARE(arguments.property("0").toInt32(), 1);
        QCOMPARE(arguments.property("1").toInt32(), 1);
        QCOMPARE(arguments.property("2").toInt32(), 1);
    }
}

void tst_QScriptContext::getSetActivationObject_customContext()
{
    QScriptEngine eng;
    QScriptContext *ctx = eng.pushContext();
    QVERIFY(ctx->activationObject().isObject());
    QScriptValue act = eng.newObject();
    ctx->setActivationObject(act);
    QVERIFY(ctx->activationObject().equals(act));
    eng.evaluate("var foo = 123");
    QCOMPARE(act.property("foo").toInt32(), 123);
    eng.popContext();
    QCOMPARE(act.property("foo").toInt32(), 123);
}

// Helper function that's intended to have the same behavior
// as the built-in eval() function.
static QScriptValue myEval(QScriptContext *ctx, QScriptEngine *eng)
{
     QString code = ctx->argument(0).toString();
     ctx->setActivationObject(ctx->parentContext()->activationObject());
     ctx->setThisObject(ctx->parentContext()->thisObject());
     return eng->evaluate(code);
}

void tst_QScriptContext::inheritActivationAndThisObject()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("myEval", eng.newFunction(myEval));
    {
        QScriptValue ret = eng.evaluate("var a = 123; myEval('a')");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }
    {
        QScriptValue ret = eng.evaluate("(function() { return myEval('this'); }).call(Number)");
        QVERIFY(ret.isFunction());
        QVERIFY(ret.equals(eng.globalObject().property("Number")));
    }
    {
        QScriptValue ret = eng.evaluate("(function(a) { return myEval('a'); })(123)");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }

    {
        eng.globalObject().setProperty("a", 123);
        QScriptValue ret = eng.evaluate("(function() { myEval('var a = 456'); return a; })()");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 456);
        // Since JSC doesn't create an activation object for the anonymous function call,
        // myEval() will use the global object as the activation, which is wrong.
        QEXPECT_FAIL("", "QTBUG-10313: Wrong activation object is returned from native function's parent context", Continue);
        QVERIFY(eng.globalObject().property("a").strictlyEquals(123));
    }
}

static QScriptValue parentContextToString(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->parentContext()->toString();
}

void tst_QScriptContext::toString()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("parentContextToString", eng.newFunction(parentContextToString));
    QScriptValue ret = eng.evaluate("function foo(first, second, third) {\n"
                                    "    return parentContextToString();\n"
                                    "}; foo(1, 2, 3)", "script.qs");
    QVERIFY(ret.isString());
    QCOMPARE(ret.toString(), QString::fromLatin1("foo(first = 1, second = 2, third = 3) at script.qs:2"));
}

static QScriptValue storeCalledAsConstructor(QScriptContext *ctx, QScriptEngine *eng)
{
    ctx->callee().setProperty("calledAsConstructor", ctx->isCalledAsConstructor());
    return eng->undefinedValue();
}

static QScriptValue storeCalledAsConstructorV2(QScriptContext *ctx, QScriptEngine *eng, void *)
{
    ctx->callee().setProperty("calledAsConstructor", ctx->isCalledAsConstructor());
    return eng->undefinedValue();
}

static QScriptValue storeCalledAsConstructorV3(QScriptContext *ctx, QScriptEngine *eng)
{
    ctx->callee().setProperty("calledAsConstructor", ctx->parentContext()->isCalledAsConstructor());
    return eng->undefinedValue();
}

void tst_QScriptContext::calledAsConstructor_fromCpp()
{
    QScriptEngine eng;
    {
        QScriptValue fun1 = eng.newFunction(storeCalledAsConstructor);
        fun1.call();
        QVERIFY(!fun1.property("calledAsConstructor").toBool());
        fun1.construct();
        QVERIFY(fun1.property("calledAsConstructor").toBool());
    }
    {
        QScriptValue fun = eng.newFunction(storeCalledAsConstructorV2, (void*)0);
        fun.call();
        QVERIFY(!fun.property("calledAsConstructor").toBool());
        fun.construct();
        QVERIFY(fun.property("calledAsConstructor").toBool());
    }
}

void tst_QScriptContext::calledAsConstructor_fromJS()
{
    QScriptEngine eng;
    QScriptValue fun1 = eng.newFunction(storeCalledAsConstructor);
    eng.globalObject().setProperty("fun1", fun1);
    eng.evaluate("fun1();");
    QVERIFY(!fun1.property("calledAsConstructor").toBool());
    eng.evaluate("new fun1();");
    QVERIFY(fun1.property("calledAsConstructor").toBool());
}

void tst_QScriptContext::calledAsConstructor_parentContext()
{
    QScriptEngine eng;
    QScriptValue fun3 = eng.newFunction(storeCalledAsConstructorV3);
    eng.globalObject().setProperty("fun3", fun3);
    eng.evaluate("function test() { fun3() }");
    eng.evaluate("test();");
    QVERIFY(!fun3.property("calledAsConstructor").toBool());
    eng.evaluate("new test();");
    if (qt_script_isJITEnabled())
        QEXPECT_FAIL("", "QTBUG-6132: calledAsConstructor is not correctly set for JS functions when JIT is enabled", Continue);
    QVERIFY(fun3.property("calledAsConstructor").toBool());
}

static QScriptValue argumentsObjectInNative_test1(QScriptContext *ctx, QScriptEngine *eng)
{
#define VERIFY(statement) \
    do {\
        if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__))\
            return QString::fromLatin1("Failed "  #statement);\
    } while (0)

    QScriptValue obj = ctx->argumentsObject();
    VERIFY(obj.isObject());
    VERIFY(obj.property(0).toUInt32() == 123);
    VERIFY(obj.property(1).toString() == QString::fromLatin1("456"));

    obj.setProperty(0, "abc");
    VERIFY(eng->evaluate("arguments[0]").toString() == QString::fromLatin1("abc") );

    return QString::fromLatin1("success");
#undef VERIFY
}

void tst_QScriptContext::argumentsObjectInNative()
{
    {
        QScriptEngine eng;
        QScriptValue fun = eng.newFunction(argumentsObjectInNative_test1);
        QScriptValueList args;
        args << QScriptValue(&eng, 123.0);
        args << QScriptValue(&eng, QString::fromLatin1("456"));
        QScriptValue result = fun.call(eng.undefinedValue(), args);
        QVERIFY(!eng.hasUncaughtException());
        QCOMPARE(result.toString(), QString::fromLatin1("success"));
    }
    {
        QScriptEngine eng;
        QScriptValue fun = eng.newFunction(argumentsObjectInNative_test1);
        eng.globalObject().setProperty("func", fun);
        QScriptValue result = eng.evaluate("func(123.0 , 456);");
        QVERIFY(!eng.hasUncaughtException());
        QCOMPARE(result.toString(), QString::fromLatin1("success"));
    }
}

static QScriptValue get_jsActivationObject(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->parentContext()->parentContext()->activationObject();
}

void tst_QScriptContext::jsActivationObject()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("get_jsActivationObject", eng.newFunction(get_jsActivationObject));
    eng.evaluate("function f1() { var w = get_jsActivationObject('arg1');  return w; }");
    eng.evaluate("function f2(x,y,z) { var v1 = 42;\n"
                        //   "function foo() {};\n" //this would avoid JSC to optimize
                        "var v2 = f1(); return v2; }");
    eng.evaluate("function f3() { var v1 = 'nothing'; return f2(1,2,3); }");
    QScriptValue result1 = eng.evaluate("f2('hello', 'useless', 'world')");
    QScriptValue result2 = eng.evaluate("f3()");
    QVERIFY(result1.isObject());
    QEXPECT_FAIL("", "QTBUG-10313: JSC optimizes away the activation object", Abort);
    QCOMPARE(result1.property("v1").toInt32() , 42);
    QCOMPARE(result1.property("arguments").property(1).toString() , QString::fromLatin1("useless"));
    QVERIFY(result2.isObject());
    QCOMPARE(result2.property("v1").toInt32() , 42);
    QCOMPARE(result2.property("arguments").property(1).toString() , QString::fromLatin1("2"));
}

void tst_QScriptContext::qobjectAsActivationObject()
{
    QScriptEngine eng;
    QObject object;
    QScriptValue scriptObject = eng.newQObject(&object);
    QScriptContext *ctx = eng.pushContext();
    ctx->setActivationObject(scriptObject);
    QVERIFY(ctx->activationObject().equals(scriptObject));

    QVERIFY(!scriptObject.property("foo").isValid());
    eng.evaluate("function foo() { return 123; }");
    {
        QScriptValue val = scriptObject.property("foo");
        QVERIFY(val.isValid());
        QVERIFY(val.isFunction());
    }
    QVERIFY(!eng.globalObject().property("foo").isValid());

    QVERIFY(!scriptObject.property("bar").isValid());
    eng.evaluate("var bar = 123");
    {
        QScriptValue val = scriptObject.property("bar");
        QVERIFY(val.isValid());
        QVERIFY(val.isNumber());
        QCOMPARE(val.toInt32(), 123);
    }
    QVERIFY(!eng.globalObject().property("bar").isValid());

    {
        QScriptValue val = eng.evaluate("delete foo");
        QVERIFY(val.isBool());
        QVERIFY(val.toBool());
        QVERIFY(!scriptObject.property("foo").isValid());
    }
}

static QScriptValue getParentContextCallee(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->parentContext()->callee();
}

void tst_QScriptContext::parentContextCallee_QT2270()
{
    QScriptEngine engine;
    engine.globalObject().setProperty("getParentContextCallee", engine.newFunction(getParentContextCallee));
    QScriptValue fun = engine.evaluate("(function() { return getParentContextCallee(); })");
    QVERIFY(fun.isFunction());
    QScriptValue callee = fun.call();
    QVERIFY(callee.equals(fun));
}

void tst_QScriptContext::throwErrorInGlobalContext()
{
    QScriptEngine eng;
    QScriptValue ret = eng.currentContext()->throwError("foo");
    QVERIFY(ret.isError());
    QVERIFY(eng.hasUncaughtException());
    QVERIFY(eng.uncaughtException().strictlyEquals(ret));
    QCOMPARE(ret.toString(), QString::fromLatin1("Error: foo"));
}

void tst_QScriptContext::throwErrorWithTypeInGlobalContext_data()
{
    QTest::addColumn<QScriptContext::Error>("error");
    QTest::addColumn<QString>("stringRepresentation");
    QTest::newRow("ReferenceError") << QScriptContext::ReferenceError << QString::fromLatin1("ReferenceError: foo");
    QTest::newRow("SyntaxError") << QScriptContext::SyntaxError << QString::fromLatin1("SyntaxError: foo");
    QTest::newRow("TypeError") << QScriptContext::TypeError << QString::fromLatin1("TypeError: foo");
    QTest::newRow("RangeError") << QScriptContext::RangeError << QString::fromLatin1("RangeError: foo");
    QTest::newRow("URIError") << QScriptContext::URIError << QString::fromLatin1("URIError: foo");
    QTest::newRow("UnknownError") << QScriptContext::UnknownError << QString::fromLatin1("Error: foo");
}

void tst_QScriptContext::throwErrorWithTypeInGlobalContext()
{
    QFETCH(QScriptContext::Error, error);
    QFETCH(QString, stringRepresentation);
    QScriptEngine eng;
    QScriptValue ret = eng.currentContext()->throwError(error, "foo");
    QVERIFY(ret.isError());
    QVERIFY(eng.hasUncaughtException());
    QVERIFY(eng.uncaughtException().strictlyEquals(ret));
    QCOMPARE(ret.toString(), stringRepresentation);
}

void tst_QScriptContext::throwValueInGlobalContext()
{
    QScriptEngine eng;
    QScriptValue val(&eng, 123);
    QScriptValue ret = eng.currentContext()->throwValue(val);
    QVERIFY(ret.strictlyEquals(val));
    QVERIFY(eng.hasUncaughtException());
    QVERIFY(eng.uncaughtException().strictlyEquals(val));
}

QTEST_MAIN(tst_QScriptContext)
#include "tst_qscriptcontext.moc"
