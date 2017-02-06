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

#include <qscriptengine.h>
#include <qscriptengineagent.h>
#include <qscriptprogram.h>
#include <qscriptvalueiterator.h>
#include <qgraphicsitem.h>
#include <qstandarditemmodel.h>
#include <QtCore/qnumeric.h>
#include <stdlib.h>

#include <QtScript/private/qscriptdeclarativeclass_p.h>

#include "../shared/util.h"

Q_DECLARE_METATYPE(QList<int>)
Q_DECLARE_METATYPE(QObjectList)
Q_DECLARE_METATYPE(QScriptProgram)

class tst_QScriptEngine : public QObject
{
    Q_OBJECT

public:
    tst_QScriptEngine();
    virtual ~tst_QScriptEngine();

private slots:
    void constructWithParent();
    void currentContext();
    void pushPopContext();
    void getSetDefaultPrototype_int();
    void getSetDefaultPrototype_customType();
    void newFunction();
    void newFunctionWithArg();
    void newFunctionWithProto();
    void newObject();
    void newArray();
    void newArray_HooliganTask218092();
    void newArray_HooliganTask233836();
    void newVariant();
    void newVariant_defaultPrototype();
    void newVariant_promoteObject();
    void newVariant_replaceValue();
    void newVariant_valueOfToString();
    void newVariant_promoteNonObject();
    void newVariant_promoteNonQScriptObject();
    void newRegExp();
    void jsRegExp();
    void newDate();
    void jsParseDate();
    void newQObject();
    void newQObject_ownership();
    void newQObject_promoteObject();
    void newQObject_sameQObject();
    void newQObject_defaultPrototype();
    void newQObject_promoteNonObject();
    void newQObject_promoteNonQScriptObject();
    void newQMetaObject();
    void newActivationObject();
    void getSetGlobalObject();
    void globalObjectProperties();
    void globalObjectProperties_enumerate();
    void createGlobalObjectProperty();
    void globalObjectGetterSetterProperty();
    void customGlobalObjectWithPrototype();
    void globalObjectWithCustomPrototype();
    void builtinFunctionNames_data();
    void builtinFunctionNames();
    void checkSyntax_data();
    void checkSyntax();
    void canEvaluate_data();
    void canEvaluate();
    void evaluate_data();
    void evaluate();
    void nestedEvaluate();
    void uncaughtException();
    void errorMessage_QT679();
    void valueConversion_basic();
    void valueConversion_customType();
    void valueConversion_sequence();
    void valueConversion_QVariant();
    void valueConversion_hooliganTask248802();
    void valueConversion_basic2();
    void valueConversion_dateTime();
    void valueConversion_regExp();
    void valueConversion_long();
    void qScriptValueFromValue_noEngine();
    void importExtension();
    void infiniteRecursion();
    void castWithPrototypeChain();
    void castWithMultipleInheritance();
    void collectGarbage();
    void reportAdditionalMemoryCost();
    void gcWithNestedDataStructure();
    void processEventsWhileRunning();
    void throwErrorFromProcessEvents_data();
    void throwErrorFromProcessEvents();
    void disableProcessEventsInterval();
    void stacktrace();
    void stacktrace_callJSFromCpp_data();
    void stacktrace_callJSFromCpp();
    void numberParsing_data();
    void numberParsing();
    void automaticSemicolonInsertion();
    void abortEvaluation_notEvaluating();
    void abortEvaluation_data();
    void abortEvaluation();
    void abortEvaluation_tryCatch();
    void abortEvaluation_fromNative();
    void abortEvaluation_QTBUG9433();
    void isEvaluating_notEvaluating();
    void isEvaluating_fromNative();
    void isEvaluating_fromEvent();
    void printFunctionWithCustomHandler();
    void printThrowsException();
    void errorConstructors();
    void argumentsProperty_globalContext();
    void argumentsProperty_JS();
    void argumentsProperty_evaluateInNativeFunction();
    void jsNumberClass();
    void jsForInStatement_simple();
    void jsForInStatement_prototypeProperties();
    void jsForInStatement_mutateWhileIterating();
    void jsForInStatement_arrays();
    void jsForInStatement_nullAndUndefined();
    void jsFunctionDeclarationAsStatement();
    void stringObjects();
    void jsStringPrototypeReplaceBugs();
    void getterSetterThisObject_global();
    void getterSetterThisObject_plain();
    void getterSetterThisObject_prototypeChain();
    void getterSetterThisObject_activation();
    void jsContinueInSwitch();
    void jsShadowReadOnlyPrototypeProperty();
    void toObject();
    void jsReservedWords_data();
    void jsReservedWords();
    void jsFutureReservedWords_data();
    void jsFutureReservedWords();
    void jsThrowInsideWithStatement();
    void getSetAgent_ownership();
    void getSetAgent_deleteAgent();
    void getSetAgent_differentEngine();
    void reentrancy_stringHandles();
    void reentrancy_processEventsInterval();
    void reentrancy_typeConversion();
    void reentrancy_globalObjectProperties();
    void reentrancy_Array();
    void reentrancy_objectCreation();
    void jsIncDecNonObjectProperty();
    void installTranslatorFunctions_data();
    void installTranslatorFunctions();
    void translateScript_data();
    void translateScript();
    void translateScript_crossScript();
    void translateScript_callQsTrFromNative();
    void translateScript_trNoOp();
    void translateScript_callQsTrFromCpp();
    void translateWithInvalidArgs_data();
    void translateWithInvalidArgs();
    void translationContext_data();
    void translationContext();
    void translateScriptIdBased();
    void translateScriptUnicode_data();
    void translateScriptUnicode();
    void translateScriptUnicodeIdBased_data();
    void translateScriptUnicodeIdBased();
    void translateFromBuiltinCallback();
    void functionScopes();
    void nativeFunctionScopes();
    void evaluateProgram();
    void evaluateProgram_customScope();
    void evaluateProgram_closure();
    void evaluateProgram_executeLater();
    void evaluateProgram_multipleEngines();
    void evaluateProgram_empty();
    void collectGarbageAfterConnect();
    void collectGarbageAfterNativeArguments();
    void promoteThisObjectToQObjectInConstructor();
    void scriptValueFromQMetaObject();

    void qRegExpInport_data();
    void qRegExpInport();
    void reentrency();
    void newFixedStaticScopeObject();
    void newGrowingStaticScopeObject();
    void dateRoundtripJSQtJS();
    void dateRoundtripQtJSQt();
    void dateConversionJSQt();
    void dateConversionQtJS();
    void stringListFromArrayWithEmptyElement();
    void collectQObjectWithCachedWrapper_data();
    void collectQObjectWithCachedWrapper();
    void pushContext_noInheritedScope();
};

tst_QScriptEngine::tst_QScriptEngine()
{
}

tst_QScriptEngine::~tst_QScriptEngine()
{
}

void tst_QScriptEngine::constructWithParent()
{
    QPointer<QScriptEngine> ptr;
    {
        QObject obj;
        QScriptEngine *engine = new QScriptEngine(&obj);
        ptr = engine;
    }
    QVERIFY(ptr == 0);
}

void tst_QScriptEngine::currentContext()
{
    QScriptEngine eng;
    QScriptContext *globalCtx = eng.currentContext();
    QVERIFY(globalCtx != 0);
    QVERIFY(globalCtx->parentContext() == 0);
    QCOMPARE(globalCtx->engine(), &eng);
    QCOMPARE(globalCtx->argumentCount(), 0);
    QCOMPARE(globalCtx->backtrace().size(), 1);
    QVERIFY(!globalCtx->isCalledAsConstructor());
    QVERIFY(!globalCtx->callee().isValid());
    QCOMPARE(globalCtx->state(), QScriptContext::NormalState);
    QVERIFY(globalCtx->thisObject().strictlyEquals(eng.globalObject()));
    QVERIFY(globalCtx->activationObject().strictlyEquals(eng.globalObject()));
    QVERIFY(globalCtx->argumentsObject().isObject());
}

void tst_QScriptEngine::pushPopContext()
{
    QScriptEngine eng;
    QScriptContext *globalCtx = eng.currentContext();
    QScriptContext *ctx = eng.pushContext();
    QVERIFY(ctx != 0);
    QCOMPARE(ctx->parentContext(), globalCtx);
    QVERIFY(!ctx->isCalledAsConstructor());
    QVERIFY(!ctx->callee().isValid());
    QVERIFY(ctx->thisObject().strictlyEquals(eng.globalObject()));
    QCOMPARE(ctx->argumentCount(), 0);
    QCOMPARE(ctx->backtrace().size(), 2);
    QCOMPARE(ctx->engine(), &eng);
    QCOMPARE(ctx->state(), QScriptContext::NormalState);
    QVERIFY(ctx->activationObject().isObject());
    QVERIFY(ctx->argumentsObject().isObject());

    QScriptContext *ctx2 = eng.pushContext();
    QVERIFY(ctx2 != 0);
    QCOMPARE(ctx2->parentContext(), ctx);
    QVERIFY(!ctx2->activationObject().strictlyEquals(ctx->activationObject()));
    QVERIFY(!ctx2->argumentsObject().strictlyEquals(ctx->argumentsObject()));

    eng.popContext();
    eng.popContext();
    QTest::ignoreMessage(QtWarningMsg, "QScriptEngine::popContext() doesn't match with pushContext()");
    eng.popContext(); // ignored
    QTest::ignoreMessage(QtWarningMsg, "QScriptEngine::popContext() doesn't match with pushContext()");
    eng.popContext(); // ignored
}

static QScriptValue myFunction(QScriptContext *, QScriptEngine *eng)
{
    return eng->nullValue();
}

static QScriptValue myFunctionWithVoidArg(QScriptContext *, QScriptEngine *eng, void *)
{
    return eng->nullValue();
}

static QScriptValue myThrowingFunction(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->throwError("foo");
}

void tst_QScriptEngine::newFunction()
{
    QScriptEngine eng;
    {
        QScriptValue fun = eng.newFunction(myFunction);
        QCOMPARE(fun.isValid(), true);
        QCOMPARE(fun.isFunction(), true);
        QCOMPARE(fun.isObject(), true);
        QCOMPARE(fun.scriptClass(), (QScriptClass*)0);
        // a prototype property is automatically constructed
        {
            QScriptValue prot = fun.property("prototype", QScriptValue::ResolveLocal);
            QVERIFY(prot.isObject());
            QVERIFY(prot.property("constructor").strictlyEquals(fun));
            QCOMPARE(fun.propertyFlags("prototype"), QScriptValue::Undeletable | QScriptValue::SkipInEnumeration);
            QCOMPARE(prot.propertyFlags("constructor"), QScriptValue::SkipInEnumeration);
        }
        // prototype should be Function.prototype
        QCOMPARE(fun.prototype().isValid(), true);
        QCOMPARE(fun.prototype().isFunction(), true);
        QCOMPARE(fun.prototype().strictlyEquals(eng.evaluate("Function.prototype")), true);

        QCOMPARE(fun.call().isNull(), true);
        QCOMPARE(fun.construct().isObject(), true);
    }
}

void tst_QScriptEngine::newFunctionWithArg()
{
    QScriptEngine eng;
    {
        QScriptValue fun = eng.newFunction(myFunctionWithVoidArg, (void*)this);
        QVERIFY(fun.isFunction());
        QCOMPARE(fun.scriptClass(), (QScriptClass*)0);
        // a prototype property is automatically constructed
        {
            QScriptValue prot = fun.property("prototype", QScriptValue::ResolveLocal);
            QVERIFY(prot.isObject());
            QVERIFY(prot.property("constructor").strictlyEquals(fun));
            QCOMPARE(fun.propertyFlags("prototype"), QScriptValue::Undeletable | QScriptValue::SkipInEnumeration);
            QCOMPARE(prot.propertyFlags("constructor"), QScriptValue::SkipInEnumeration);
        }
        // prototype should be Function.prototype
        QCOMPARE(fun.prototype().isValid(), true);
        QCOMPARE(fun.prototype().isFunction(), true);
        QCOMPARE(fun.prototype().strictlyEquals(eng.evaluate("Function.prototype")), true);

        QCOMPARE(fun.call().isNull(), true);
        QCOMPARE(fun.construct().isObject(), true);
    }
}

void tst_QScriptEngine::newFunctionWithProto()
{
    QScriptEngine eng;
    {
        QScriptValue proto = eng.newObject();
        QScriptValue fun = eng.newFunction(myFunction, proto);
        QCOMPARE(fun.isValid(), true);
        QCOMPARE(fun.isFunction(), true);
        QCOMPARE(fun.isObject(), true);
        // internal prototype should be Function.prototype
        QCOMPARE(fun.prototype().isValid(), true);
        QCOMPARE(fun.prototype().isFunction(), true);
        QCOMPARE(fun.prototype().strictlyEquals(eng.evaluate("Function.prototype")), true);
        // public prototype should be the one we passed
        QCOMPARE(fun.property("prototype").strictlyEquals(proto), true);
        QCOMPARE(fun.propertyFlags("prototype"), QScriptValue::Undeletable | QScriptValue::SkipInEnumeration);
        QCOMPARE(proto.property("constructor").strictlyEquals(fun), true);
        QCOMPARE(proto.propertyFlags("constructor"), QScriptValue::SkipInEnumeration);

        QCOMPARE(fun.call().isNull(), true);
        QCOMPARE(fun.construct().isObject(), true);
    }
}

void tst_QScriptEngine::newObject()
{
    QScriptEngine eng;
    QScriptValue object = eng.newObject();
    QCOMPARE(object.isValid(), true);
    QCOMPARE(object.isObject(), true);
    QCOMPARE(object.isFunction(), false);
    QCOMPARE(object.scriptClass(), (QScriptClass*)0);
    // prototype should be Object.prototype
    QCOMPARE(object.prototype().isValid(), true);
    QCOMPARE(object.prototype().isObject(), true);
    QCOMPARE(object.prototype().strictlyEquals(eng.evaluate("Object.prototype")), true);
}

void tst_QScriptEngine::newArray()
{
    QScriptEngine eng;
    QScriptValue array = eng.newArray();
    QCOMPARE(array.isValid(), true);
    QCOMPARE(array.isArray(), true);
    QCOMPARE(array.isObject(), true);
    QVERIFY(!array.isFunction());
    QCOMPARE(array.scriptClass(), (QScriptClass*)0);
    // prototype should be Array.prototype
    QCOMPARE(array.prototype().isValid(), true);
    QCOMPARE(array.prototype().isArray(), true);
    QCOMPARE(array.prototype().strictlyEquals(eng.evaluate("Array.prototype")), true);
}

void tst_QScriptEngine::newArray_HooliganTask218092()
{
    QScriptEngine eng;
    {
        QScriptValue ret = eng.evaluate("[].splice(0, 0, 'a')");
        QVERIFY(ret.isArray());
        QCOMPARE(ret.property("length").toInt32(), 0);
    }
    {
        QScriptValue ret = eng.evaluate("['a'].splice(0, 1, 'b')");
        QVERIFY(ret.isArray());
        QCOMPARE(ret.property("length").toInt32(), 1);
    }
    {
        QScriptValue ret = eng.evaluate("['a', 'b'].splice(0, 1, 'c')");
        QVERIFY(ret.isArray());
        QCOMPARE(ret.property("length").toInt32(), 1);
    }
    {
        QScriptValue ret = eng.evaluate("['a', 'b', 'c'].splice(0, 2, 'd')");
        QVERIFY(ret.isArray());
        QCOMPARE(ret.property("length").toInt32(), 2);
    }
    {
        QScriptValue ret = eng.evaluate("['a', 'b', 'c'].splice(1, 2, 'd', 'e', 'f')");
        QVERIFY(ret.isArray());
        QCOMPARE(ret.property("length").toInt32(), 2);
    }
}

void tst_QScriptEngine::newArray_HooliganTask233836()
{
    QScriptEngine eng;
    {
        // According to ECMA-262, this should cause a RangeError.
        QScriptValue ret = eng.evaluate("a = new Array(4294967295); a.push('foo')");
        QEXPECT_FAIL("", "ECMA compliance bug in Array.prototype.push: https://bugs.webkit.org/show_bug.cgi?id=55033", Continue);
        QVERIFY(ret.isError() && ret.toString().contains(QLatin1String("RangeError")));
    }
    {
        QScriptValue ret = eng.newArray(0xFFFFFFFF);
        QCOMPARE(ret.property("length").toUInt32(), uint(0xFFFFFFFF));
        ret.setProperty(0xFFFFFFFF, 123);
        QCOMPARE(ret.property("length").toUInt32(), uint(0xFFFFFFFF));
        QVERIFY(ret.property(0xFFFFFFFF).isNumber());
        QCOMPARE(ret.property(0xFFFFFFFF).toInt32(), 123);
        ret.setProperty(123, 456);
        QCOMPARE(ret.property("length").toUInt32(), uint(0xFFFFFFFF));
        QVERIFY(ret.property(123).isNumber());
        QCOMPARE(ret.property(123).toInt32(), 456);
    }
}

void tst_QScriptEngine::newVariant()
{
    QScriptEngine eng;
    {
        QScriptValue opaque = eng.newVariant(QVariant());
        QCOMPARE(opaque.isValid(), true);
        QCOMPARE(opaque.isVariant(), true);
        QVERIFY(!opaque.isFunction());
        QCOMPARE(opaque.isObject(), true);
        QCOMPARE(opaque.prototype().isValid(), true);
        QCOMPARE(opaque.prototype().isVariant(), true);
        QVERIFY(opaque.property("valueOf").call(opaque).isUndefined());
    }
}

void tst_QScriptEngine::newVariant_defaultPrototype()
{
    // default prototype should be set automatically
    QScriptEngine eng;
    {
        QScriptValue proto = eng.newObject();
        eng.setDefaultPrototype(qMetaTypeId<QString>(), proto);
        QScriptValue ret = eng.newVariant(QVariant(QString::fromLatin1("hello")));
        QVERIFY(ret.isVariant());
        QCOMPARE(ret.scriptClass(), (QScriptClass*)0);
        QVERIFY(ret.prototype().strictlyEquals(proto));
        eng.setDefaultPrototype(qMetaTypeId<QString>(), QScriptValue());
        QScriptValue ret2 = eng.newVariant(QVariant(QString::fromLatin1("hello")));
        QVERIFY(ret2.isVariant());
        QVERIFY(!ret2.prototype().strictlyEquals(proto));
    }
}

void tst_QScriptEngine::newVariant_promoteObject()
{
    // "promote" plain object to variant
    QScriptEngine eng;
    {
        QScriptValue object = eng.newObject();
        object.setProperty("foo", eng.newObject());
        object.setProperty("bar", object.property("foo"));
        QVERIFY(object.property("foo").isObject());
        QVERIFY(!object.property("foo").isVariant());
        QScriptValue originalProto = object.property("foo").prototype();
        QScriptValue ret = eng.newVariant(object.property("foo"), QVariant(123));
        QVERIFY(ret.isValid());
        QVERIFY(ret.strictlyEquals(object.property("foo")));
        QVERIFY(ret.isVariant());
        QVERIFY(object.property("foo").isVariant());
        QVERIFY(object.property("bar").isVariant());
        QCOMPARE(ret.toVariant(), QVariant(123));
        QVERIFY(ret.prototype().strictlyEquals(originalProto));
    }
}

void tst_QScriptEngine::newVariant_replaceValue()
{
    // replace value of existing object
    QScriptEngine eng;
    {
        QScriptValue object = eng.newVariant(QVariant(123));
        for (int x = 0; x < 2; ++x) {
            QScriptValue ret = eng.newVariant(object, QVariant(456));
            QVERIFY(ret.isValid());
            QVERIFY(ret.strictlyEquals(object));
            QVERIFY(ret.isVariant());
            QCOMPARE(ret.toVariant(), QVariant(456));
        }
    }
}

void tst_QScriptEngine::newVariant_valueOfToString()
{
    // valueOf() and toString()
    QScriptEngine eng;
    {
        QScriptValue object = eng.newVariant(QVariant(123));
        QScriptValue value = object.property("valueOf").call(object);
        QVERIFY(value.isNumber());
        QCOMPARE(value.toInt32(), 123);
        QCOMPARE(object.toString(), QString::fromLatin1("123"));
        QCOMPARE(object.toVariant().toString(), object.toString());
    }
    {
        QScriptValue object = eng.newVariant(QVariant(QString::fromLatin1("hello")));
        QScriptValue value = object.property("valueOf").call(object);
        QVERIFY(value.isString());
        QCOMPARE(value.toString(), QString::fromLatin1("hello"));
        QCOMPARE(object.toString(), QString::fromLatin1("hello"));
        QCOMPARE(object.toVariant().toString(), object.toString());
    }
    {
        QScriptValue object = eng.newVariant(QVariant(false));
        QScriptValue value = object.property("valueOf").call(object);
        QVERIFY(value.isBoolean());
        QCOMPARE(value.toBoolean(), false);
        QCOMPARE(object.toString(), QString::fromLatin1("false"));
        QCOMPARE(object.toVariant().toString(), object.toString());
    }
    {
        QScriptValue object = eng.newVariant(QVariant(QPoint(10, 20)));
        QScriptValue value = object.property("valueOf").call(object);
        QVERIFY(value.isObject());
        QVERIFY(value.strictlyEquals(object));
        QCOMPARE(object.toString(), QString::fromLatin1("QVariant(QPoint)"));
    }
}

void tst_QScriptEngine::newVariant_promoteNonObject()
{
    QScriptEngine eng;
    {
        QVariant var(456);
        QScriptValue ret = eng.newVariant(123, var);
        QVERIFY(ret.isVariant());
        QCOMPARE(ret.toVariant(), var);
    }
}

void tst_QScriptEngine::newVariant_promoteNonQScriptObject()
{
    QScriptEngine eng;
    {
        QTest::ignoreMessage(QtWarningMsg, "QScriptEngine::newVariant(): changing class of non-QScriptObject not supported");
        QScriptValue ret = eng.newVariant(eng.newArray(), 123);
        QVERIFY(!ret.isValid());
    }
}

void tst_QScriptEngine::newRegExp()
{
    QScriptEngine eng;
    for (int x = 0; x < 2; ++x) {
        QScriptValue rexp;
        if (x == 0)
            rexp = eng.newRegExp("foo", "bar");
        else
            rexp = eng.newRegExp(QRegExp("foo"));
        QCOMPARE(rexp.isValid(), true);
        QCOMPARE(rexp.isRegExp(), true);
        QCOMPARE(rexp.isObject(), true);
        QVERIFY(rexp.isFunction()); // in JSC, RegExp objects are callable
        // prototype should be RegExp.prototype
        QCOMPARE(rexp.prototype().isValid(), true);
        QCOMPARE(rexp.prototype().isObject(), true);
        QCOMPARE(rexp.prototype().isRegExp(), false);
        QCOMPARE(rexp.prototype().strictlyEquals(eng.evaluate("RegExp.prototype")), true);

        QCOMPARE(rexp.toRegExp().pattern(), QRegExp("foo").pattern());
    }
}

void tst_QScriptEngine::jsRegExp()
{
    // See ECMA-262 Section 15.10, "RegExp Objects".
    // These should really be JS-only tests, as they test the implementation's
    // ECMA-compliance, not the C++ API. Compliance should already be covered
    // by the Mozilla tests (qscriptjstestsuite).
    // We can consider updating the expected results of this test if the
    // RegExp implementation changes.

    QScriptEngine eng;
    QScriptValue r = eng.evaluate("/foo/gim");
    QVERIFY(r.isRegExp());
    QCOMPARE(r.toString(), QString::fromLatin1("/foo/gim"));

    QScriptValue rxCtor = eng.globalObject().property("RegExp");
    QScriptValue r2 = rxCtor.call(QScriptValue(), QScriptValueList() << r);
    QVERIFY(r2.isRegExp());
    QVERIFY(r2.strictlyEquals(r));

    QScriptValue r3 = rxCtor.call(QScriptValue(), QScriptValueList() << r << "gim");
    QVERIFY(r3.isError());
    QVERIFY(r3.toString().contains(QString::fromLatin1("TypeError"))); // Cannot supply flags when constructing one RegExp from another

    QScriptValue r4 = rxCtor.call(QScriptValue(), QScriptValueList() << "foo" << "gim");
    QVERIFY(r4.isRegExp());

    QScriptValue r5 = rxCtor.construct(QScriptValueList() << r);
    QVERIFY(r5.isRegExp());
    QCOMPARE(r5.toString(), QString::fromLatin1("/foo/gim"));
    // In JSC, constructing a RegExp from another produces the same identical object.
    // This is different from SpiderMonkey and old back-end.
    QEXPECT_FAIL("", "RegExp copy-constructor should return a new object: https://bugs.webkit.org/show_bug.cgi?id=55040", Continue);
    QVERIFY(!r5.strictlyEquals(r));

    QScriptValue r6 = rxCtor.construct(QScriptValueList() << "foo" << "bar");
    QVERIFY(r6.isError());
    QVERIFY(r6.toString().contains(QString::fromLatin1("SyntaxError"))); // Invalid regular expression flag

    QScriptValue r7 = eng.evaluate("/foo/gimp");
    QVERIFY(r7.isError());
    QVERIFY(r7.toString().contains(QString::fromLatin1("SyntaxError"))); // Invalid regular expression flag

    // JSC doesn't complain about duplicate flags.
    QScriptValue r8 = eng.evaluate("/foo/migmigmig");
    QVERIFY(r8.isRegExp());
    QCOMPARE(r8.toString(), QString::fromLatin1("/foo/gim"));

    QScriptValue r9 = rxCtor.construct();
    QVERIFY(r9.isRegExp());
    QCOMPARE(r9.toString(), QString::fromLatin1("/(?:)/"));

    QScriptValue r10 = rxCtor.construct(QScriptValueList() << "" << "gim");
    QVERIFY(r10.isRegExp());
    QCOMPARE(r10.toString(), QString::fromLatin1("/(?:)/gim"));

    QScriptValue r11 = rxCtor.construct(QScriptValueList() << "{1.*}" << "g");
    QVERIFY(r11.isRegExp());
    QCOMPARE(r11.toString(), QString::fromLatin1("/{1.*}/g"));
}

void tst_QScriptEngine::newDate()
{
    QScriptEngine eng;

    {
        QScriptValue date = eng.newDate(0);
        QCOMPARE(date.isValid(), true);
        QCOMPARE(date.isDate(), true);
        QCOMPARE(date.isObject(), true);
        QVERIFY(!date.isFunction());
        // prototype should be Date.prototype
        QCOMPARE(date.prototype().isValid(), true);
        QCOMPARE(date.prototype().isDate(), true);
        QCOMPARE(date.prototype().strictlyEquals(eng.evaluate("Date.prototype")), true);
    }

    {
        QDateTime dt = QDateTime(QDate(1, 2, 3), QTime(4, 5, 6, 7), Qt::LocalTime);
        QScriptValue date = eng.newDate(dt);
        QCOMPARE(date.isValid(), true);
        QCOMPARE(date.isDate(), true);
        QCOMPARE(date.isObject(), true);
        // prototype should be Date.prototype
        QCOMPARE(date.prototype().isValid(), true);
        QCOMPARE(date.prototype().isDate(), true);
        QCOMPARE(date.prototype().strictlyEquals(eng.evaluate("Date.prototype")), true);

        QCOMPARE(date.toDateTime(), dt);
    }

    {
        QDateTime dt = QDateTime(QDate(1, 2, 3), QTime(4, 5, 6, 7), Qt::UTC);
        QScriptValue date = eng.newDate(dt);
        // toDateTime() result should be in local time
        QCOMPARE(date.toDateTime(), dt.toLocalTime());
    }
}

void tst_QScriptEngine::jsParseDate()
{
    QScriptEngine eng;
    // Date.parse() should return NaN when it fails
    {
        QScriptValue ret = eng.evaluate("Date.parse()");
        QVERIFY(ret.isNumber());
        QVERIFY(qIsNaN(ret.toNumber()));
    }

    // Date.parse() should be able to parse the output of Date().toString()
#ifndef Q_WS_WIN // TODO: Test and remove this since 169701 has been fixed
    {
        QScriptValue ret = eng.evaluate("var x = new Date(); var s = x.toString(); s == new Date(Date.parse(s)).toString()");
        QVERIFY(ret.isBoolean());
        QCOMPARE(ret.toBoolean(), true);
    }
#endif
}

void tst_QScriptEngine::newQObject()
{
    QScriptEngine eng;

    {
        QScriptValue qobject = eng.newQObject(0);
        QCOMPARE(qobject.isValid(), true);
        QCOMPARE(qobject.isNull(), true);
        QCOMPARE(qobject.isObject(), false);
        QCOMPARE(qobject.toQObject(), (QObject *)0);
    }
    {
        QScriptValue qobject = eng.newQObject(this);
        QCOMPARE(qobject.isValid(), true);
        QCOMPARE(qobject.isQObject(), true);
        QCOMPARE(qobject.isObject(), true);
        QCOMPARE(qobject.toQObject(), (QObject *)this);
        QVERIFY(!qobject.isFunction());
        // prototype should be QObject.prototype
        QCOMPARE(qobject.prototype().isValid(), true);
        QCOMPARE(qobject.prototype().isQObject(), true);
        QCOMPARE(qobject.scriptClass(), (QScriptClass*)0);
    }
}

void tst_QScriptEngine::newQObject_ownership()
{
    QScriptEngine eng;
    {
        QPointer<QObject> ptr = new QObject();
        QVERIFY(ptr != 0);
        {
            QScriptValue v = eng.newQObject(ptr, QScriptEngine::ScriptOwnership);
        }
        eng.evaluate("gc()");
        if (ptr)
            QEXPECT_FAIL("", "In the JSC-based back-end, script-owned QObjects are not always deleted immediately during GC", Continue);
        QVERIFY(ptr == 0);
    }
    {
        QPointer<QObject> ptr = new QObject();
        QVERIFY(ptr != 0);
        {
            QScriptValue v = eng.newQObject(ptr, QScriptEngine::QtOwnership);
        }
        QObject *before = ptr;
        eng.evaluate("gc()");
        QVERIFY(ptr == before);
        delete ptr;
    }
    {
        QObject *parent = new QObject();
        QObject *child = new QObject(parent);
        QScriptValue v = eng.newQObject(child, QScriptEngine::QtOwnership);
        QCOMPARE(v.toQObject(), child);
        delete parent;
        QCOMPARE(v.toQObject(), (QObject *)0);
    }
    {
        QPointer<QObject> ptr = new QObject();
        QVERIFY(ptr != 0);
        {
            QScriptValue v = eng.newQObject(ptr, QScriptEngine::AutoOwnership);
        }
        eng.evaluate("gc()");
        // no parent, so it should be like ScriptOwnership
        if (ptr)
            QEXPECT_FAIL("", "In the JSC-based back-end, script-owned QObjects are not always deleted immediately during GC", Continue);
        QVERIFY(ptr == 0);
    }
    {
        QObject *parent = new QObject();
        QPointer<QObject> child = new QObject(parent);
        QVERIFY(child != 0);
        {
            QScriptValue v = eng.newQObject(child, QScriptEngine::AutoOwnership);
        }
        eng.evaluate("gc()");
        // has parent, so it should be like QtOwnership
        QVERIFY(child != 0);
        delete parent;
    }
}

void tst_QScriptEngine::newQObject_promoteObject()
{
    QScriptEngine eng;
    // "promote" plain object to QObject
    {
        QScriptValue obj = eng.newObject();
        QScriptValue originalProto = obj.prototype();
        QScriptValue ret = eng.newQObject(obj, this);
        QVERIFY(ret.isValid());
        QVERIFY(ret.isQObject());
        QVERIFY(ret.strictlyEquals(obj));
        QVERIFY(obj.isQObject());
        QCOMPARE(ret.toQObject(), (QObject *)this);
        QVERIFY(ret.prototype().strictlyEquals(originalProto));
        QScriptValue val = ret.property("objectName");
        QVERIFY(val.isString());
    }
    // "promote" variant object to QObject
    {
        QScriptValue obj = eng.newVariant(123);
        QVERIFY(obj.isVariant());
        QScriptValue originalProto = obj.prototype();
        QScriptValue ret = eng.newQObject(obj, this);
        QVERIFY(ret.isQObject());
        QVERIFY(ret.strictlyEquals(obj));
        QVERIFY(obj.isQObject());
        QCOMPARE(ret.toQObject(), (QObject *)this);
        QVERIFY(ret.prototype().strictlyEquals(originalProto));
    }
    // replace QObject* of existing object
    {
        QScriptValue object = eng.newVariant(123);
        QScriptValue originalProto = object.prototype();
        QObject otherQObject;
        for (int x = 0; x < 2; ++x) {
            QScriptValue ret = eng.newQObject(object, &otherQObject);
            QVERIFY(ret.isValid());
            QVERIFY(ret.isQObject());
            QVERIFY(ret.strictlyEquals(object));
            QCOMPARE(ret.toQObject(), (QObject *)&otherQObject);
            QVERIFY(ret.prototype().strictlyEquals(originalProto));
        }
    }
}

void tst_QScriptEngine::newQObject_sameQObject()
{
    QScriptEngine eng;
    // calling newQObject() several times with same object
    for (int x = 0; x < 2; ++x) {
        QObject qobj;
        // the default is to create a new wrapper object
        QScriptValue obj1 = eng.newQObject(&qobj);
        QScriptValue obj2 = eng.newQObject(&qobj);
        QVERIFY(!obj2.strictlyEquals(obj1));

        QScriptEngine::QObjectWrapOptions opt = 0;
        bool preferExisting = (x != 0);
        if (preferExisting)
            opt |= QScriptEngine::PreferExistingWrapperObject;

        QScriptValue obj3 = eng.newQObject(&qobj, QScriptEngine::AutoOwnership, opt);
        QVERIFY(!obj3.strictlyEquals(obj2));
        QScriptValue obj4 = eng.newQObject(&qobj, QScriptEngine::AutoOwnership, opt);
        QCOMPARE(obj4.strictlyEquals(obj3), preferExisting);

        QScriptValue obj5 = eng.newQObject(&qobj, QScriptEngine::ScriptOwnership, opt);
        QVERIFY(!obj5.strictlyEquals(obj4));
        QScriptValue obj6 = eng.newQObject(&qobj, QScriptEngine::ScriptOwnership, opt);
        QCOMPARE(obj6.strictlyEquals(obj5), preferExisting);

        QScriptValue obj7 = eng.newQObject(&qobj, QScriptEngine::ScriptOwnership,
                                           QScriptEngine::ExcludeSuperClassMethods | opt);
        QVERIFY(!obj7.strictlyEquals(obj6));
        QScriptValue obj8 = eng.newQObject(&qobj, QScriptEngine::ScriptOwnership,
                                           QScriptEngine::ExcludeSuperClassMethods | opt);
        QCOMPARE(obj8.strictlyEquals(obj7), preferExisting);
    }
}

void tst_QScriptEngine::newQObject_defaultPrototype()
{
    QScriptEngine eng;
    // newQObject() should set the default prototype, if one has been registered
    {
        QScriptValue oldQObjectProto = eng.defaultPrototype(qMetaTypeId<QObject*>());

        QScriptValue qobjectProto = eng.newObject();
        eng.setDefaultPrototype(qMetaTypeId<QObject*>(), qobjectProto);
        {
            QScriptValue ret = eng.newQObject(this);
            QVERIFY(ret.prototype().equals(qobjectProto));
        }
        QScriptValue tstProto = eng.newObject();
        int typeId = qRegisterMetaType<tst_QScriptEngine*>("tst_QScriptEngine*");
        eng.setDefaultPrototype(typeId, tstProto);
        {
            QScriptValue ret = eng.newQObject(this);
            QVERIFY(ret.prototype().equals(tstProto));
        }

        eng.setDefaultPrototype(qMetaTypeId<QObject*>(), oldQObjectProto);
        eng.setDefaultPrototype(typeId, QScriptValue());
    }
}

void tst_QScriptEngine::newQObject_promoteNonObject()
{
    QScriptEngine eng;
    {
        QScriptValue ret = eng.newQObject(123, this);
        QVERIFY(ret.isQObject());
        QCOMPARE(ret.toQObject(), this);
    }
}

void tst_QScriptEngine::newQObject_promoteNonQScriptObject()
{
    QScriptEngine eng;
    {
        QTest::ignoreMessage(QtWarningMsg, "QScriptEngine::newQObject(): changing class of non-QScriptObject not supported");
        QScriptValue ret = eng.newQObject(eng.newArray(), this);
        QVERIFY(!ret.isValid());
    }
}

QT_BEGIN_NAMESPACE
Q_SCRIPT_DECLARE_QMETAOBJECT(QObject, QObject*)
Q_SCRIPT_DECLARE_QMETAOBJECT(QWidget, QWidget*)
QT_END_NAMESPACE

static QScriptValue myConstructor(QScriptContext *ctx, QScriptEngine *eng)
{
    QScriptValue obj;
    if (ctx->isCalledAsConstructor()) {
        obj = ctx->thisObject();
    } else {
        obj = eng->newObject();
        obj.setPrototype(ctx->callee().property("prototype"));
    }
    obj.setProperty("isCalledAsConstructor", QScriptValue(eng, ctx->isCalledAsConstructor()));
    return obj;
}

static QScriptValue instanceofJS(const QScriptValue &inst, const QScriptValue &ctor)
{
    return inst.engine()->evaluate("(function(inst, ctor) { return inst instanceof ctor; })")
        .call(QScriptValue(), QScriptValueList() << inst << ctor);
}

void tst_QScriptEngine::newQMetaObject()
{
    QScriptEngine eng;
#if 0
    QScriptValue qclass = eng.newQMetaObject<QObject>();
    QScriptValue qclass2 = eng.newQMetaObject<QWidget>();
#else
    QScriptValue qclass = qScriptValueFromQMetaObject<QObject>(&eng);
    QScriptValue qclass2 = qScriptValueFromQMetaObject<QWidget>(&eng);
#endif
    QCOMPARE(qclass.isValid(), true);
    QCOMPARE(qclass.isQMetaObject(), true);
    QCOMPARE(qclass.toQMetaObject(), &QObject::staticMetaObject);
    QCOMPARE(qclass.isFunction(), true);
    QVERIFY(qclass.property("prototype").isObject());

    QCOMPARE(qclass2.isValid(), true);
    QCOMPARE(qclass2.isQMetaObject(), true);
    QCOMPARE(qclass2.toQMetaObject(), &QWidget::staticMetaObject);
    QCOMPARE(qclass2.isFunction(), true);
    QVERIFY(qclass2.property("prototype").isObject());

    // prototype should be QMetaObject.prototype
    QCOMPARE(qclass.prototype().isObject(), true);
    QCOMPARE(qclass2.prototype().isObject(), true);

    QScriptValue instance = qclass.construct();
    QCOMPARE(instance.isQObject(), true);
    QCOMPARE(instance.toQObject()->metaObject(), qclass.toQMetaObject());
    QVERIFY(instance.instanceOf(qclass));
    QVERIFY(instanceofJS(instance, qclass).strictlyEquals(true));

    QScriptValue instance2 = qclass2.construct();
    QCOMPARE(instance2.isQObject(), true);
    QCOMPARE(instance2.toQObject()->metaObject(), qclass2.toQMetaObject());
    QVERIFY(instance2.instanceOf(qclass2));
    QVERIFY(instanceofJS(instance2, qclass2).strictlyEquals(true));
    QVERIFY(!instance2.instanceOf(qclass));
    QVERIFY(instanceofJS(instance2, qclass).strictlyEquals(false));

    QScriptValueList args;
    args << instance;
    QScriptValue instance3 = qclass.construct(args);
    QCOMPARE(instance3.isQObject(), true);
    QCOMPARE(instance3.toQObject()->parent(), instance.toQObject());
    QVERIFY(instance3.instanceOf(qclass));
    QVERIFY(instanceofJS(instance3, qclass).strictlyEquals(true));
    QVERIFY(!instance3.instanceOf(qclass2));
    QVERIFY(instanceofJS(instance3, qclass2).strictlyEquals(false));
    args.clear();

    QPointer<QObject> qpointer1 = instance.toQObject();
    QPointer<QObject> qpointer2 = instance2.toQObject();
    QPointer<QObject> qpointer3 = instance3.toQObject();

    QVERIFY(qpointer1);
    QVERIFY(qpointer2);
    QVERIFY(qpointer3);

    // verify that AutoOwnership is in effect
    instance = QScriptValue();
    collectGarbage_helper(eng);

    QVERIFY(!qpointer1);
    QVERIFY(qpointer2);
    QVERIFY(!qpointer3); // was child of instance

    QVERIFY(instance.toQObject() == 0);
    QVERIFY(instance3.toQObject() == 0); // was child of instance
    QVERIFY(instance2.toQObject() != 0);
    instance2 = QScriptValue();
    collectGarbage_helper(eng);
    QVERIFY(instance2.toQObject() == 0);

    // with custom constructor
    QScriptValue ctor = eng.newFunction(myConstructor);
    QScriptValue qclass3 = eng.newQMetaObject(&QObject::staticMetaObject, ctor);
    QVERIFY(qclass3.property("prototype").equals(ctor.property("prototype")));
    {
        QScriptValue ret = qclass3.call();
        QVERIFY(ret.isObject());
        QVERIFY(ret.property("isCalledAsConstructor").isBoolean());
        QVERIFY(!ret.property("isCalledAsConstructor").toBoolean());
        QVERIFY(ret.instanceOf(qclass3));
        QVERIFY(instanceofJS(ret, qclass3).strictlyEquals(true));
        QVERIFY(!ret.instanceOf(qclass));
        QVERIFY(instanceofJS(ret, qclass).strictlyEquals(false));
    }
    {
        QScriptValue ret = qclass3.construct();
        QVERIFY(ret.isObject());
        QVERIFY(ret.property("isCalledAsConstructor").isBoolean());
        QVERIFY(ret.property("isCalledAsConstructor").toBoolean());
        QVERIFY(ret.instanceOf(qclass3));
        QVERIFY(instanceofJS(ret, qclass3).strictlyEquals(true));
        QVERIFY(!ret.instanceOf(qclass2));
        QVERIFY(instanceofJS(ret, qclass2).strictlyEquals(false));
    }

    // subclassing
    qclass2.setProperty("prototype", qclass.construct());
    QVERIFY(qclass2.construct().instanceOf(qclass));
    QVERIFY(instanceofJS(qclass2.construct(), qclass).strictlyEquals(true));

    // with meta-constructor
    QScriptValue qclass4 = eng.newQMetaObject(&QObject::staticMetaObject);
    {
        QScriptValue inst = qclass4.construct();
        QVERIFY(inst.isQObject());
        QVERIFY(inst.toQObject() != 0);
        QCOMPARE(inst.toQObject()->parent(), (QObject*)0);
        QVERIFY(inst.instanceOf(qclass4));
        QVERIFY(instanceofJS(inst, qclass4).strictlyEquals(true));
        QVERIFY(!inst.instanceOf(qclass3));
        QVERIFY(instanceofJS(inst, qclass3).strictlyEquals(false));
    }
    {
        QScriptValue inst = qclass4.construct(QScriptValueList() << eng.newQObject(this));
        QVERIFY(inst.isQObject());
        QVERIFY(inst.toQObject() != 0);
        QCOMPARE(inst.toQObject()->parent(), (QObject*)this);
        QVERIFY(inst.instanceOf(qclass4));
        QVERIFY(instanceofJS(inst, qclass4).strictlyEquals(true));
        QVERIFY(!inst.instanceOf(qclass2));
        QVERIFY(instanceofJS(inst, qclass2).strictlyEquals(false));
    }
}

void tst_QScriptEngine::newActivationObject()
{
    QSKIP("internal function not implemented in JSC-based back-end");
    QScriptEngine eng;
    QScriptValue act = eng.newActivationObject();
    QEXPECT_FAIL("", "", Continue);
    QCOMPARE(act.isValid(), true);
    QEXPECT_FAIL("", "", Continue);
    QCOMPARE(act.isObject(), true);
    QVERIFY(!act.isFunction());
    QScriptValue v(&eng, 123);
    act.setProperty("prop", v);
    QEXPECT_FAIL("", "", Continue);
    QCOMPARE(act.property("prop").strictlyEquals(v), true);
    QCOMPARE(act.scope().isValid(), false);
    QEXPECT_FAIL("", "", Continue);
    QVERIFY(act.prototype().isNull());
}

void tst_QScriptEngine::getSetGlobalObject()
{
    QScriptEngine eng;
    QScriptValue glob = eng.globalObject();
    QCOMPARE(glob.isValid(), true);
    QCOMPARE(glob.isObject(), true);
    QVERIFY(!glob.isFunction());
    QVERIFY(eng.currentContext()->thisObject().strictlyEquals(glob));
    QVERIFY(eng.currentContext()->activationObject().strictlyEquals(glob));
    QCOMPARE(glob.toString(), QString::fromLatin1("[object global]"));
    // prototype should be Object.prototype
    QCOMPARE(glob.prototype().isValid(), true);
    QCOMPARE(glob.prototype().isObject(), true);
    QCOMPARE(glob.prototype().strictlyEquals(eng.evaluate("Object.prototype")), true);

    eng.setGlobalObject(glob);
    QVERIFY(eng.globalObject().equals(glob));
    eng.setGlobalObject(123);
    QVERIFY(eng.globalObject().equals(glob));

    QScriptValue obj = eng.newObject();
    eng.setGlobalObject(obj);
    QVERIFY(eng.globalObject().strictlyEquals(obj));
    QVERIFY(eng.currentContext()->thisObject().strictlyEquals(obj));
    QVERIFY(eng.currentContext()->activationObject().strictlyEquals(obj));
    QVERIFY(eng.evaluate("this").strictlyEquals(obj));
    QCOMPARE(eng.globalObject().toString(), QString::fromLatin1("[object Object]"));

    glob = QScriptValue(); // kill reference to old global object
    collectGarbage_helper(eng);
    obj = eng.newObject();
    eng.setGlobalObject(obj);
    QVERIFY(eng.globalObject().strictlyEquals(obj));
    QVERIFY(eng.currentContext()->thisObject().strictlyEquals(obj));
    QVERIFY(eng.currentContext()->activationObject().strictlyEquals(obj));

    collectGarbage_helper(eng);
    QVERIFY(eng.globalObject().strictlyEquals(obj));
    QVERIFY(eng.currentContext()->thisObject().strictlyEquals(obj));
    QVERIFY(eng.currentContext()->activationObject().strictlyEquals(obj));

    QVERIFY(!obj.property("foo").isValid());
    eng.evaluate("var foo = 123");
    {
        QScriptValue ret = obj.property("foo");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }

    QVERIFY(!obj.property("bar").isValid());
    eng.evaluate("bar = 456");
    {
        QScriptValue ret = obj.property("bar");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 456);
    }

    QVERIFY(!obj.property("baz").isValid());
    eng.evaluate("this['baz'] = 789");
    {
        QScriptValue ret = obj.property("baz");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 789);
    }

    {
        QScriptValue ret = eng.evaluate("(function() { return this; })()");
        QVERIFY(ret.strictlyEquals(obj));
    }

    // Delete property.
    {
        QScriptValue ret = eng.evaluate("delete foo");
        QVERIFY(ret.isBool());
        QVERIFY(ret.toBool());
        QVERIFY(!obj.property("foo").isValid());
    }

    // Getter/setter property.
    QVERIFY(eng.evaluate("this.__defineGetter__('oof', function() { return this.bar; })").isUndefined());
    QVERIFY(eng.evaluate("this.__defineSetter__('oof', function(v) { this.bar = v; })").isUndefined());
    QVERIFY(eng.evaluate("this.__lookupGetter__('oof')").isFunction());
    QVERIFY(eng.evaluate("this.__lookupSetter__('oof')").isFunction());
    eng.evaluate("oof = 123");
    QVERIFY(eng.evaluate("oof").equals(obj.property("bar")));

    // Enumeration.
    {
        QScriptValue ret = eng.evaluate("a = []; for (var p in this) a.push(p); a");
        QCOMPARE(ret.toString(), QString::fromLatin1("bar,baz,oof,p,a"));
    }
}

static QScriptValue getSetFoo(QScriptContext *ctx, QScriptEngine *)
{
    if (ctx->argumentCount() > 0)
        ctx->thisObject().setProperty("foo", ctx->argument(0));
    return ctx->thisObject().property("foo");
}

void tst_QScriptEngine::globalObjectProperties()
{
    // See ECMA-262 Section 15.1, "The Global Object".

    QScriptEngine eng;
    QScriptValue global = eng.globalObject();

    QVERIFY(global.property("NaN").isNumber());
    QVERIFY(qIsNaN(global.property("NaN").toNumber()));
    QCOMPARE(global.propertyFlags("NaN"), QScriptValue::SkipInEnumeration | QScriptValue::Undeletable);

    QVERIFY(global.property("Infinity").isNumber());
    QVERIFY(qIsInf(global.property("Infinity").toNumber()));
    QCOMPARE(global.propertyFlags("NaN"), QScriptValue::SkipInEnumeration | QScriptValue::Undeletable);

    QVERIFY(global.property("undefined").isUndefined());
    QCOMPARE(global.propertyFlags("undefined"), QScriptValue::SkipInEnumeration | QScriptValue::Undeletable);

    QVERIFY(global.property("eval").isFunction());
    QCOMPARE(global.propertyFlags("eval"), QScriptValue::SkipInEnumeration);

    QVERIFY(global.property("parseInt").isFunction());
    QCOMPARE(global.propertyFlags("parseInt"), QScriptValue::SkipInEnumeration);

    QVERIFY(global.property("parseFloat").isFunction());
    QCOMPARE(global.propertyFlags("parseFloat"), QScriptValue::SkipInEnumeration);

    QVERIFY(global.property("isNaN").isFunction());
    QCOMPARE(global.propertyFlags("isNaN"), QScriptValue::SkipInEnumeration);

    QVERIFY(global.property("isFinite").isFunction());
    QCOMPARE(global.propertyFlags("isFinite"), QScriptValue::SkipInEnumeration);

    QVERIFY(global.property("decodeURI").isFunction());
    QCOMPARE(global.propertyFlags("decodeURI"), QScriptValue::SkipInEnumeration);

    QVERIFY(global.property("decodeURIComponent").isFunction());
    QCOMPARE(global.propertyFlags("decodeURIComponent"), QScriptValue::SkipInEnumeration);

    QVERIFY(global.property("encodeURI").isFunction());
    QCOMPARE(global.propertyFlags("encodeURI"), QScriptValue::SkipInEnumeration);

    QVERIFY(global.property("encodeURIComponent").isFunction());
    QCOMPARE(global.propertyFlags("encodeURIComponent"), QScriptValue::SkipInEnumeration);

    QVERIFY(global.property("Object").isFunction());
    QCOMPARE(global.propertyFlags("Object"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("Function").isFunction());
    QCOMPARE(global.propertyFlags("Function"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("Array").isFunction());
    QCOMPARE(global.propertyFlags("Array"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("String").isFunction());
    QCOMPARE(global.propertyFlags("String"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("Boolean").isFunction());
    QCOMPARE(global.propertyFlags("Boolean"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("Number").isFunction());
    QCOMPARE(global.propertyFlags("Number"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("Date").isFunction());
    QCOMPARE(global.propertyFlags("Date"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("RegExp").isFunction());
    QCOMPARE(global.propertyFlags("RegExp"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("Error").isFunction());
    QCOMPARE(global.propertyFlags("Error"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("EvalError").isFunction());
    QCOMPARE(global.propertyFlags("EvalError"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("RangeError").isFunction());
    QCOMPARE(global.propertyFlags("RangeError"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("ReferenceError").isFunction());
    QCOMPARE(global.propertyFlags("ReferenceError"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("SyntaxError").isFunction());
    QCOMPARE(global.propertyFlags("SyntaxError"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("TypeError").isFunction());
    QCOMPARE(global.propertyFlags("TypeError"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("URIError").isFunction());
    QCOMPARE(global.propertyFlags("URIError"), QScriptValue::SkipInEnumeration);
    QVERIFY(global.property("Math").isObject());
    QVERIFY(!global.property("Math").isFunction());
    QEXPECT_FAIL("", "[ECMA compliance] JSC sets DontDelete flag for Math object: https://bugs.webkit.org/show_bug.cgi?id=55034", Continue);
    QCOMPARE(global.propertyFlags("Math"), QScriptValue::SkipInEnumeration);
}

void tst_QScriptEngine::globalObjectProperties_enumerate()
{
    QScriptEngine eng;
    QScriptValue global = eng.globalObject();

    QSet<QString> expectedNames;
    expectedNames
        << "isNaN"
        << "parseFloat"
        << "String"
        << "EvalError"
        << "URIError"
        << "Math"
        << "encodeURIComponent"
        << "RangeError"
        << "eval"
        << "isFinite"
        << "ReferenceError"
        << "Infinity"
        << "Function"
        << "RegExp"
        << "Number"
        << "parseInt"
        << "Object"
        << "decodeURI"
        << "TypeError"
        << "Boolean"
        << "encodeURI"
        << "NaN"
        << "Error"
        << "decodeURIComponent"
        << "Date"
        << "Array"
        << "escape"
        << "unescape"
        << "SyntaxError"
        << "undefined"
        // non-standard
        << "gc"
        << "version"
        << "print"
        // JavaScriptCore
        << "JSON"
        ;
    QSet<QString> actualNames;
    {
        QScriptValueIterator it(global);
        while (it.hasNext()) {
            it.next();
            actualNames.insert(it.name());
        }
    }

    QSet<QString> remainingNames = actualNames;
    {
        QSet<QString>::const_iterator it;
        for (it = expectedNames.constBegin(); it != expectedNames.constEnd(); ++it) {
            QString name = *it;
            QVERIFY(actualNames.contains(name));
            remainingNames.remove(name);
        }
    }
    QVERIFY(remainingNames.isEmpty());
}

void tst_QScriptEngine::createGlobalObjectProperty()
{
    QScriptEngine eng;
    QScriptValue global = eng.globalObject();
    // create property with no attributes
    {
        QString name = QString::fromLatin1("foo");
        QVERIFY(!global.property(name).isValid());
        QScriptValue val(123);
        global.setProperty(name, val);
        QVERIFY(global.property(name).equals(val));
        QVERIFY(global.propertyFlags(name) == 0);
        global.setProperty(name, QScriptValue());
        QVERIFY(!global.property(name).isValid());
    }
    // create property with attributes
    {
        QString name = QString::fromLatin1("bar");
        QVERIFY(!global.property(name).isValid());
        QScriptValue val(QString::fromLatin1("ciao"));
        QScriptValue::PropertyFlags flags = QScriptValue::ReadOnly | QScriptValue::SkipInEnumeration;
        global.setProperty(name, val, flags);
        QVERIFY(global.property(name).equals(val));
        QEXPECT_FAIL("", "QTBUG-6134: custom Global Object properties don't retain attributes", Continue);
        QCOMPARE(global.propertyFlags(name), flags);
        global.setProperty(name, QScriptValue());
        QVERIFY(!global.property(name).isValid());
    }
}

void tst_QScriptEngine::globalObjectGetterSetterProperty()
{
    QScriptEngine engine;
    QScriptValue global = engine.globalObject();
    global.setProperty("bar", engine.newFunction(getSetFoo),
                       QScriptValue::PropertySetter | QScriptValue::PropertyGetter);
    global.setProperty("foo", 123);
    QVERIFY(global.property("bar").equals(global.property("foo")));
    QVERIFY(engine.evaluate("bar").equals(global.property("foo")));
    global.setProperty("bar", 456);
    QVERIFY(global.property("bar").equals(global.property("foo")));

    engine.evaluate("__defineGetter__('baz', function() { return 789; })");
    QVERIFY(engine.evaluate("baz").equals(789));
    QVERIFY(global.property("baz").equals(789));
}

void tst_QScriptEngine::customGlobalObjectWithPrototype()
{
    for (int x = 0; x < 2; ++x) {
        QScriptEngine engine;
        QScriptValue wrap = engine.newObject();
        QScriptValue global = engine.globalObject();
        QScriptValue originalGlobalProto = global.prototype();
        if (!x) {
            // Set prototype before setting global object
            wrap.setPrototype(global);
            QVERIFY(wrap.prototype().strictlyEquals(global));
            engine.setGlobalObject(wrap);
        } else {
            // Set prototype after setting global object
            engine.setGlobalObject(wrap);
            wrap.setPrototype(global);
            QVERIFY(wrap.prototype().strictlyEquals(global));
        }
        {
            QScriptValue ret = engine.evaluate("print");
            QVERIFY(ret.isFunction());
            QVERIFY(ret.strictlyEquals(wrap.property("print")));
        }
        {
            QScriptValue ret = engine.evaluate("this.print");
            QVERIFY(ret.isFunction());
            QVERIFY(ret.strictlyEquals(wrap.property("print")));
        }
        {
            QScriptValue ret = engine.evaluate("hasOwnProperty('print')");
            QVERIFY(ret.isBool());
            QVERIFY(!ret.toBool());
        }
        {
            QScriptValue ret = engine.evaluate("this.hasOwnProperty('print')");
            QVERIFY(ret.isBool());
            QVERIFY(!ret.toBool());
        }

        QScriptValue anotherProto = engine.newObject();
        anotherProto.setProperty("anotherProtoProperty", 123);
        global.setPrototype(anotherProto);
        {
            QScriptValue ret = engine.evaluate("print");
            QVERIFY(ret.isFunction());
            QVERIFY(ret.strictlyEquals(wrap.property("print")));
        }
        {
            QScriptValue ret = engine.evaluate("anotherProtoProperty");
            QVERIFY(ret.isNumber());
            QVERIFY(ret.strictlyEquals(wrap.property("anotherProtoProperty")));
        }
        {
            QScriptValue ret = engine.evaluate("this.anotherProtoProperty");
            QVERIFY(ret.isNumber());
            QVERIFY(ret.strictlyEquals(wrap.property("anotherProtoProperty")));
        }

        wrap.setPrototype(anotherProto);
        {
            QScriptValue ret = engine.evaluate("print");
            QVERIFY(ret.isError());
            QCOMPARE(ret.toString(), QString::fromLatin1("ReferenceError: Can't find variable: print"));
        }
        {
            QScriptValue ret = engine.evaluate("anotherProtoProperty");
            QVERIFY(ret.isNumber());
            QVERIFY(ret.strictlyEquals(wrap.property("anotherProtoProperty")));
        }
        QVERIFY(global.prototype().strictlyEquals(anotherProto));

        global.setPrototype(originalGlobalProto);
        engine.setGlobalObject(global);
        {
            QScriptValue ret = engine.evaluate("anotherProtoProperty");
            QVERIFY(ret.isError());
            QCOMPARE(ret.toString(), QString::fromLatin1("ReferenceError: Can't find variable: anotherProtoProperty"));
        }
        {
            QScriptValue ret = engine.evaluate("print");
            QVERIFY(ret.isFunction());
            QVERIFY(ret.strictlyEquals(global.property("print")));
        }
        QVERIFY(!anotherProto.property("print").isValid());
    }
}

void tst_QScriptEngine::globalObjectWithCustomPrototype()
{
    QScriptEngine engine;
    QScriptValue proto = engine.newObject();
    proto.setProperty("protoProperty", 123);
    QScriptValue global = engine.globalObject();
    QScriptValue originalProto = global.prototype();
    global.setPrototype(proto);
    {
        QScriptValue ret = engine.evaluate("protoProperty");
        QVERIFY(ret.isNumber());
        QVERIFY(ret.strictlyEquals(global.property("protoProperty")));
    }
    {
        QScriptValue ret = engine.evaluate("this.protoProperty");
        QVERIFY(ret.isNumber());
        QVERIFY(ret.strictlyEquals(global.property("protoProperty")));
    }
    {
        QScriptValue ret = engine.evaluate("hasOwnProperty('protoProperty')");
        QVERIFY(ret.isBool());
        QVERIFY(!ret.toBool());
    }
    {
        QScriptValue ret = engine.evaluate("this.hasOwnProperty('protoProperty')");
        QVERIFY(ret.isBool());
        QVERIFY(!ret.toBool());
    }

    // Custom prototype set from JS
    {
        QScriptValue ret = engine.evaluate("this.__proto__ = { 'a': 123 }; a");
        QVERIFY(ret.isNumber());
        QEXPECT_FAIL("", "QTBUG-9737: Prototype change in JS not reflected on C++ side", Continue);
        QVERIFY(ret.strictlyEquals(global.property("a")));
    }
}

void tst_QScriptEngine::builtinFunctionNames_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QString>("expectedName");

    // See ECMA-262 Chapter 15, "Standard Built-in ECMAScript Objects".

    QTest::newRow("print") << QString("print") << QString("print"); // Qt Script extension.
    QTest::newRow("parseInt") << QString("parseInt") << QString("parseInt");
    QTest::newRow("parseFloat") << QString("parseFloat") << QString("parseFloat");
    QTest::newRow("isNaN") << QString("isNaN") << QString("isNaN");
    QTest::newRow("isFinite") << QString("isFinite") << QString("isFinite");
    QTest::newRow("decodeURI") << QString("decodeURI") << QString("decodeURI");
    QTest::newRow("decodeURIComponent") << QString("decodeURIComponent") << QString("decodeURIComponent");
    QTest::newRow("encodeURI") << QString("encodeURI") << QString("encodeURI");
    QTest::newRow("encodeURIComponent") << QString("encodeURIComponent") << QString("encodeURIComponent");
    QTest::newRow("escape") << QString("escape") << QString("escape");
    QTest::newRow("unescape") << QString("unescape") << QString("unescape");
    QTest::newRow("version") << QString("version") << QString("version"); // Qt Script extension.
    QTest::newRow("gc") << QString("gc") << QString("gc"); // Qt Script extension.

    QTest::newRow("Array") << QString("Array") << QString("Array");
    QTest::newRow("Array.prototype.toString") << QString("Array.prototype.toString") << QString("toString");
    QTest::newRow("Array.prototype.toLocaleString") << QString("Array.prototype.toLocaleString") << QString("toLocaleString");
    QTest::newRow("Array.prototype.concat") << QString("Array.prototype.concat") << QString("concat");
    QTest::newRow("Array.prototype.join") << QString("Array.prototype.join") << QString("join");
    QTest::newRow("Array.prototype.pop") << QString("Array.prototype.pop") << QString("pop");
    QTest::newRow("Array.prototype.push") << QString("Array.prototype.push") << QString("push");
    QTest::newRow("Array.prototype.reverse") << QString("Array.prototype.reverse") << QString("reverse");
    QTest::newRow("Array.prototype.shift") << QString("Array.prototype.shift") << QString("shift");
    QTest::newRow("Array.prototype.slice") << QString("Array.prototype.slice") << QString("slice");
    QTest::newRow("Array.prototype.sort") << QString("Array.prototype.sort") << QString("sort");
    QTest::newRow("Array.prototype.splice") << QString("Array.prototype.splice") << QString("splice");
    QTest::newRow("Array.prototype.unshift") << QString("Array.prototype.unshift") << QString("unshift");

    QTest::newRow("Boolean") << QString("Boolean") << QString("Boolean");
    QTest::newRow("Boolean.prototype.toString") << QString("Boolean.prototype.toString") << QString("toString");

    QTest::newRow("Date") << QString("Date") << QString("Date");
    QTest::newRow("Date.prototype.toString") << QString("Date.prototype.toString") << QString("toString");
    QTest::newRow("Date.prototype.toDateString") << QString("Date.prototype.toDateString") << QString("toDateString");
    QTest::newRow("Date.prototype.toTimeString") << QString("Date.prototype.toTimeString") << QString("toTimeString");
    QTest::newRow("Date.prototype.toLocaleString") << QString("Date.prototype.toLocaleString") << QString("toLocaleString");
    QTest::newRow("Date.prototype.toLocaleDateString") << QString("Date.prototype.toLocaleDateString") << QString("toLocaleDateString");
    QTest::newRow("Date.prototype.toLocaleTimeString") << QString("Date.prototype.toLocaleTimeString") << QString("toLocaleTimeString");
    QTest::newRow("Date.prototype.valueOf") << QString("Date.prototype.valueOf") << QString("valueOf");
    QTest::newRow("Date.prototype.getTime") << QString("Date.prototype.getTime") << QString("getTime");
    QTest::newRow("Date.prototype.getYear") << QString("Date.prototype.getYear") << QString("getYear");
    QTest::newRow("Date.prototype.getFullYear") << QString("Date.prototype.getFullYear") << QString("getFullYear");
    QTest::newRow("Date.prototype.getUTCFullYear") << QString("Date.prototype.getUTCFullYear") << QString("getUTCFullYear");
    QTest::newRow("Date.prototype.getMonth") << QString("Date.prototype.getMonth") << QString("getMonth");
    QTest::newRow("Date.prototype.getUTCMonth") << QString("Date.prototype.getUTCMonth") << QString("getUTCMonth");
    QTest::newRow("Date.prototype.getDate") << QString("Date.prototype.getDate") << QString("getDate");
    QTest::newRow("Date.prototype.getUTCDate") << QString("Date.prototype.getUTCDate") << QString("getUTCDate");
    QTest::newRow("Date.prototype.getDay") << QString("Date.prototype.getDay") << QString("getDay");
    QTest::newRow("Date.prototype.getUTCDay") << QString("Date.prototype.getUTCDay") << QString("getUTCDay");
    QTest::newRow("Date.prototype.getHours") << QString("Date.prototype.getHours") << QString("getHours");
    QTest::newRow("Date.prototype.getUTCHours") << QString("Date.prototype.getUTCHours") << QString("getUTCHours");
    QTest::newRow("Date.prototype.getMinutes") << QString("Date.prototype.getMinutes") << QString("getMinutes");
    QTest::newRow("Date.prototype.getUTCMinutes") << QString("Date.prototype.getUTCMinutes") << QString("getUTCMinutes");
    QTest::newRow("Date.prototype.getSeconds") << QString("Date.prototype.getSeconds") << QString("getSeconds");
    QTest::newRow("Date.prototype.getUTCSeconds") << QString("Date.prototype.getUTCSeconds") << QString("getUTCSeconds");
    QTest::newRow("Date.prototype.getMilliseconds") << QString("Date.prototype.getMilliseconds") << QString("getMilliseconds");
    QTest::newRow("Date.prototype.getUTCMilliseconds") << QString("Date.prototype.getUTCMilliseconds") << QString("getUTCMilliseconds");
    QTest::newRow("Date.prototype.getTimezoneOffset") << QString("Date.prototype.getTimezoneOffset") << QString("getTimezoneOffset");
    QTest::newRow("Date.prototype.setTime") << QString("Date.prototype.setTime") << QString("setTime");
    QTest::newRow("Date.prototype.setMilliseconds") << QString("Date.prototype.setMilliseconds") << QString("setMilliseconds");
    QTest::newRow("Date.prototype.setUTCMilliseconds") << QString("Date.prototype.setUTCMilliseconds") << QString("setUTCMilliseconds");
    QTest::newRow("Date.prototype.setSeconds") << QString("Date.prototype.setSeconds") << QString("setSeconds");
    QTest::newRow("Date.prototype.setUTCSeconds") << QString("Date.prototype.setUTCSeconds") << QString("setUTCSeconds");
    QTest::newRow("Date.prototype.setMinutes") << QString("Date.prototype.setMinutes") << QString("setMinutes");
    QTest::newRow("Date.prototype.setUTCMinutes") << QString("Date.prototype.setUTCMinutes") << QString("setUTCMinutes");
    QTest::newRow("Date.prototype.setHours") << QString("Date.prototype.setHours") << QString("setHours");
    QTest::newRow("Date.prototype.setUTCHours") << QString("Date.prototype.setUTCHours") << QString("setUTCHours");
    QTest::newRow("Date.prototype.setDate") << QString("Date.prototype.setDate") << QString("setDate");
    QTest::newRow("Date.prototype.setUTCDate") << QString("Date.prototype.setUTCDate") << QString("setUTCDate");
    QTest::newRow("Date.prototype.setMonth") << QString("Date.prototype.setMonth") << QString("setMonth");
    QTest::newRow("Date.prototype.setUTCMonth") << QString("Date.prototype.setUTCMonth") << QString("setUTCMonth");
    QTest::newRow("Date.prototype.setYear") << QString("Date.prototype.setYear") << QString("setYear");
    QTest::newRow("Date.prototype.setFullYear") << QString("Date.prototype.setFullYear") << QString("setFullYear");
    QTest::newRow("Date.prototype.setUTCFullYear") << QString("Date.prototype.setUTCFullYear") << QString("setUTCFullYear");
    QTest::newRow("Date.prototype.toUTCString") << QString("Date.prototype.toUTCString") << QString("toUTCString");
    QTest::newRow("Date.prototype.toGMTString") << QString("Date.prototype.toGMTString") << QString("toGMTString");

    QTest::newRow("Error") << QString("Error") << QString("Error");
//    QTest::newRow("Error.prototype.backtrace") << QString("Error.prototype.backtrace") << QString("backtrace");
    QTest::newRow("Error.prototype.toString") << QString("Error.prototype.toString") << QString("toString");

    QTest::newRow("EvalError") << QString("EvalError") << QString("EvalError");
    QTest::newRow("RangeError") << QString("RangeError") << QString("RangeError");
    QTest::newRow("ReferenceError") << QString("ReferenceError") << QString("ReferenceError");
    QTest::newRow("SyntaxError") << QString("SyntaxError") << QString("SyntaxError");
    QTest::newRow("TypeError") << QString("TypeError") << QString("TypeError");
    QTest::newRow("URIError") << QString("URIError") << QString("URIError");

    QTest::newRow("Function") << QString("Function") << QString("Function");
    QTest::newRow("Function.prototype.toString") << QString("Function.prototype.toString") << QString("toString");
    QTest::newRow("Function.prototype.apply") << QString("Function.prototype.apply") << QString("apply");
    QTest::newRow("Function.prototype.call") << QString("Function.prototype.call") << QString("call");
    QTest::newRow("Function.prototype.connect") << QString("Function.prototype.connect") << QString("connect");
    QTest::newRow("Function.prototype.disconnect") << QString("Function.prototype.disconnect") << QString("disconnect");

    QTest::newRow("Math.abs") << QString("Math.abs") << QString("abs");
    QTest::newRow("Math.acos") << QString("Math.acos") << QString("acos");
    QTest::newRow("Math.asin") << QString("Math.asin") << QString("asin");
    QTest::newRow("Math.atan") << QString("Math.atan") << QString("atan");
    QTest::newRow("Math.atan2") << QString("Math.atan2") << QString("atan2");
    QTest::newRow("Math.ceil") << QString("Math.ceil") << QString("ceil");
    QTest::newRow("Math.cos") << QString("Math.cos") << QString("cos");
    QTest::newRow("Math.exp") << QString("Math.exp") << QString("exp");
    QTest::newRow("Math.floor") << QString("Math.floor") << QString("floor");
    QTest::newRow("Math.log") << QString("Math.log") << QString("log");
    QTest::newRow("Math.max") << QString("Math.max") << QString("max");
    QTest::newRow("Math.min") << QString("Math.min") << QString("min");
    QTest::newRow("Math.pow") << QString("Math.pow") << QString("pow");
    QTest::newRow("Math.random") << QString("Math.random") << QString("random");
    QTest::newRow("Math.round") << QString("Math.round") << QString("round");
    QTest::newRow("Math.sin") << QString("Math.sin") << QString("sin");
    QTest::newRow("Math.sqrt") << QString("Math.sqrt") << QString("sqrt");
    QTest::newRow("Math.tan") << QString("Math.tan") << QString("tan");

    QTest::newRow("Number") << QString("Number") << QString("Number");
    QTest::newRow("Number.prototype.toString") << QString("Number.prototype.toString") << QString("toString");
    QTest::newRow("Number.prototype.toLocaleString") << QString("Number.prototype.toLocaleString") << QString("toLocaleString");
    QTest::newRow("Number.prototype.valueOf") << QString("Number.prototype.valueOf") << QString("valueOf");
    QTest::newRow("Number.prototype.toFixed") << QString("Number.prototype.toFixed") << QString("toFixed");
    QTest::newRow("Number.prototype.toExponential") << QString("Number.prototype.toExponential") << QString("toExponential");
    QTest::newRow("Number.prototype.toPrecision") << QString("Number.prototype.toPrecision") << QString("toPrecision");

    QTest::newRow("Object") << QString("Object") << QString("Object");
    QTest::newRow("Object.prototype.toString") << QString("Object.prototype.toString") << QString("toString");
    QTest::newRow("Object.prototype.toLocaleString") << QString("Object.prototype.toLocaleString") << QString("toLocaleString");
    QTest::newRow("Object.prototype.valueOf") << QString("Object.prototype.valueOf") << QString("valueOf");
    QTest::newRow("Object.prototype.hasOwnProperty") << QString("Object.prototype.hasOwnProperty") << QString("hasOwnProperty");
    QTest::newRow("Object.prototype.isPrototypeOf") << QString("Object.prototype.isPrototypeOf") << QString("isPrototypeOf");
    QTest::newRow("Object.prototype.propertyIsEnumerable") << QString("Object.prototype.propertyIsEnumerable") << QString("propertyIsEnumerable");
    QTest::newRow("Object.prototype.__defineGetter__") << QString("Object.prototype.__defineGetter__") << QString("__defineGetter__");
    QTest::newRow("Object.prototype.__defineSetter__") << QString("Object.prototype.__defineSetter__") << QString("__defineSetter__");

    QTest::newRow("RegExp") << QString("RegExp") << QString("RegExp");
    QTest::newRow("RegExp.prototype.exec") << QString("RegExp.prototype.exec") << QString("exec");
    QTest::newRow("RegExp.prototype.test") << QString("RegExp.prototype.test") << QString("test");
    QTest::newRow("RegExp.prototype.toString") << QString("RegExp.prototype.toString") << QString("toString");

    QTest::newRow("String") << QString("String") << QString("String");
    QTest::newRow("String.prototype.toString") << QString("String.prototype.toString") << QString("toString");
    QTest::newRow("String.prototype.valueOf") << QString("String.prototype.valueOf") << QString("valueOf");
    QTest::newRow("String.prototype.charAt") << QString("String.prototype.charAt") << QString("charAt");
    QTest::newRow("String.prototype.charCodeAt") << QString("String.prototype.charCodeAt") << QString("charCodeAt");
    QTest::newRow("String.prototype.concat") << QString("String.prototype.concat") << QString("concat");
    QTest::newRow("String.prototype.indexOf") << QString("String.prototype.indexOf") << QString("indexOf");
    QTest::newRow("String.prototype.lastIndexOf") << QString("String.prototype.lastIndexOf") << QString("lastIndexOf");
    QTest::newRow("String.prototype.localeCompare") << QString("String.prototype.localeCompare") << QString("localeCompare");
    QTest::newRow("String.prototype.match") << QString("String.prototype.match") << QString("match");
    QTest::newRow("String.prototype.replace") << QString("String.prototype.replace") << QString("replace");
    QTest::newRow("String.prototype.search") << QString("String.prototype.search") << QString("search");
    QTest::newRow("String.prototype.slice") << QString("String.prototype.slice") << QString("slice");
    QTest::newRow("String.prototype.split") << QString("String.prototype.split") << QString("split");
    QTest::newRow("String.prototype.substring") << QString("String.prototype.substring") << QString("substring");
    QTest::newRow("String.prototype.toLowerCase") << QString("String.prototype.toLowerCase") << QString("toLowerCase");
    QTest::newRow("String.prototype.toLocaleLowerCase") << QString("String.prototype.toLocaleLowerCase") << QString("toLocaleLowerCase");
    QTest::newRow("String.prototype.toUpperCase") << QString("String.prototype.toUpperCase") << QString("toUpperCase");
    QTest::newRow("String.prototype.toLocaleUpperCase") << QString("String.prototype.toLocaleUpperCase") << QString("toLocaleUpperCase");
}

void tst_QScriptEngine::builtinFunctionNames()
{
    QFETCH(QString, expression);
    QFETCH(QString, expectedName);
    QScriptEngine eng;
    // The "name" property is actually non-standard, but JSC supports it.
    QScriptValue ret = eng.evaluate(QString::fromLatin1("%0.name").arg(expression));
    QVERIFY(ret.isString());
    QCOMPARE(ret.toString(), expectedName);
}

void tst_QScriptEngine::checkSyntax_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<int>("expectedState");
    QTest::addColumn<int>("errorLineNumber");
    QTest::addColumn<int>("errorColumnNumber");
    QTest::addColumn<QString>("errorMessage");

    QTest::newRow("0")
        << QString("0") << int(QScriptSyntaxCheckResult::Valid)
        << -1 << -1 << "";
    QTest::newRow("if (")
        << QString("if (\n") << int(QScriptSyntaxCheckResult::Intermediate)
        << 1 << 4 << "";
    QTest::newRow("if else")
        << QString("\nif else") << int(QScriptSyntaxCheckResult::Error)
        << 2 << 4 << "Expected `('";
    QTest::newRow("foo[")
        << QString("foo[") << int(QScriptSyntaxCheckResult::Error)
        << 1 << 4 << "";
    QTest::newRow("foo['bar']")
        << QString("foo['bar']") << int(QScriptSyntaxCheckResult::Valid)
        << -1 << -1 << "";

    QTest::newRow("/*")
        << QString("/*") << int(QScriptSyntaxCheckResult::Intermediate)
        << 1 << 1 << "Unclosed comment at end of file";
    QTest::newRow("/*\nMy comment")
        << QString("/*\nMy comment") << int(QScriptSyntaxCheckResult::Intermediate)
        << 1 << 1 << "Unclosed comment at end of file";
    QTest::newRow("/*\nMy comment */\nfoo = 10")
        << QString("/*\nMy comment */\nfoo = 10") << int(QScriptSyntaxCheckResult::Valid)
        << -1 << -1 << "";
    QTest::newRow("foo = 10 /*")
        << QString("foo = 10 /*") << int(QScriptSyntaxCheckResult::Intermediate)
        << -1 << -1 << "";
    QTest::newRow("foo = 10; /*")
        << QString("foo = 10; /*") << int(QScriptSyntaxCheckResult::Intermediate)
        << 1 << 11 << "Expected `end of file'";
    QTest::newRow("foo = 10 /* My comment */")
        << QString("foo = 10 /* My comment */") << int(QScriptSyntaxCheckResult::Valid)
        << -1 << -1 << "";

    QTest::newRow("/=/")
        << QString("/=/") << int(QScriptSyntaxCheckResult::Valid) << -1 << -1 << "";
    QTest::newRow("/=/g")
        << QString("/=/g") << int(QScriptSyntaxCheckResult::Valid) << -1 << -1 << "";
    QTest::newRow("/a/")
        << QString("/a/") << int(QScriptSyntaxCheckResult::Valid) << -1 << -1 << "";
    QTest::newRow("/a/g")
        << QString("/a/g") << int(QScriptSyntaxCheckResult::Valid) << -1 << -1 << "";
}

void tst_QScriptEngine::checkSyntax()
{
    QFETCH(QString, code);
    QFETCH(int, expectedState);
    QFETCH(int, errorLineNumber);
    QFETCH(int, errorColumnNumber);
    QFETCH(QString, errorMessage);

    QScriptSyntaxCheckResult result = QScriptEngine::checkSyntax(code);
    QCOMPARE(result.state(), QScriptSyntaxCheckResult::State(expectedState));
    QCOMPARE(result.errorLineNumber(), errorLineNumber);
    QCOMPARE(result.errorColumnNumber(), errorColumnNumber);
    QCOMPARE(result.errorMessage(), errorMessage);

    // assignment
    {
        QScriptSyntaxCheckResult copy = result;
        QCOMPARE(copy.state(), result.state());
        QCOMPARE(copy.errorLineNumber(), result.errorLineNumber());
        QCOMPARE(copy.errorColumnNumber(), result.errorColumnNumber());
        QCOMPARE(copy.errorMessage(), result.errorMessage());
    }
    {
        QScriptSyntaxCheckResult copy(result);
        QCOMPARE(copy.state(), result.state());
        QCOMPARE(copy.errorLineNumber(), result.errorLineNumber());
        QCOMPARE(copy.errorColumnNumber(), result.errorColumnNumber());
        QCOMPARE(copy.errorMessage(), result.errorMessage());
    }
}

void tst_QScriptEngine::canEvaluate_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<bool>("expectSuccess");

    QTest::newRow("") << QString("") << true;
    QTest::newRow("0") << QString("0") << true;
    QTest::newRow("!") << QString("!\n") << false;
    QTest::newRow("if (") << QString("if (\n") << false;
    QTest::newRow("if (10) //") << QString("if (10) //\n") << false;
    QTest::newRow("a = 1; if (") << QString("a = 1;\nif (\n") << false;
    QTest::newRow("./test.js") << QString("./test.js\n") << true;
    QTest::newRow("if (0) print(1)") << QString("if (0)\nprint(1)\n") << true;
    QTest::newRow("0 = ") << QString("0 = \n") << false;
    QTest::newRow("0 = 0") << QString("0 = 0\n") << true;
    QTest::newRow("foo[") << QString("foo[") << true; // automatic semicolon will be inserted
    QTest::newRow("foo[") << QString("foo[\n") << false;
    QTest::newRow("foo['bar']") << QString("foo['bar']") << true;

    QTest::newRow("/*") << QString("/*") << false;
    QTest::newRow("/*\nMy comment") << QString("/*\nMy comment") << false;
    QTest::newRow("/*\nMy comment */\nfoo = 10") << QString("/*\nMy comment */\nfoo = 10") << true;
    QTest::newRow("foo = 10 /*") << QString("foo = 10 /*") << false;
    QTest::newRow("foo = 10; /*") << QString("foo = 10; /*") << false;
    QTest::newRow("foo = 10 /* My comment */") << QString("foo = 10 /* My comment */") << true;

    QTest::newRow("/=/") << QString("/=/") << true;
    QTest::newRow("/=/g") << QString("/=/g") << true;
    QTest::newRow("/a/") << QString("/a/") << true;
    QTest::newRow("/a/g") << QString("/a/g") << true;
}

void tst_QScriptEngine::canEvaluate()
{
    QFETCH(QString, code);
    QFETCH(bool, expectSuccess);

    QScriptEngine eng;
    QCOMPARE(eng.canEvaluate(code), expectSuccess);
}

void tst_QScriptEngine::evaluate_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<int>("lineNumber");
    QTest::addColumn<bool>("expectHadError");
    QTest::addColumn<int>("expectErrorLineNumber");

    QTest::newRow("(newline)") << QString("\n") << -1 << false << -1;
    QTest::newRow("0 //")   << QString("0 //") << -1 << false << -1;
    QTest::newRow("/* */")   << QString("/* */") << -1 << false << -1;
    QTest::newRow("//") << QString("//") << -1 << false << -1;
    QTest::newRow("(spaces)")  << QString("  ") << -1 << false << -1;
    QTest::newRow("(empty)")   << QString("") << -1 << false << -1;
    QTest::newRow("0")     << QString("0")       << -1 << false << -1;
    QTest::newRow("0=1")   << QString("\n0=1;\n") << -1 << true  << 2;
    QTest::newRow("a=1")   << QString("a=1\n")   << -1 << false << -1;
    QTest::newRow("a=1;K") << QString("a=1;\nK") << -1 << true  << 2;

    QTest::newRow("f()") << QString("function f()\n"
                                    "{\n"
                                    "  var a;\n"
                                    "  var b=\";\n" // here's the error
                                    "}\n"
                                    "f();\n")
                         << -1 << true << 4;

    QTest::newRow("0")     << QString("0")       << 10 << false << -1;
    QTest::newRow("0=1")   << QString("\n\n0=1\n") << 10 << true  << 13;
    QTest::newRow("a=1")   << QString("a=1\n")   << 10 << false << -1;
    QTest::newRow("a=1;K") << QString("a=1;\n\nK") << 10 << true  << 12;

    QTest::newRow("f()") << QString("function f()\n"
                                    "{\n"
                                    "  var a;\n"
                                    "\n\n"
                                    "  var b=\";\n" // here's the error
                                    "}\n"
                                    "f();\n")
                         << 10 << true << 15;
    QTest::newRow("functionThatDoesntExist()")
        << QString(";\n;\n;\nfunctionThatDoesntExist()")
        << -1 << true << 4;
    QTest::newRow("for (var p in this) { continue labelThatDoesntExist; }")
        << QString("for (var p in this) {\ncontinue labelThatDoesntExist; }")
        << 4 << true << 5;
    QTest::newRow("duplicateLabel: { duplicateLabel: ; }")
        << QString("duplicateLabel: { duplicateLabel: ; }")
        << 12 << true << 12;

    QTest::newRow("/=/") << QString("/=/") << -1 << false << -1;
    QTest::newRow("/=/g") << QString("/=/g") << -1 << false << -1;
    QTest::newRow("/a/") << QString("/a/") << -1 << false << -1;
    QTest::newRow("/a/g") << QString("/a/g") << -1 << false << -1;
    QTest::newRow("/a/gim") << QString("/a/gim") << -1 << false << -1;
    QTest::newRow("/a/gimp") << QString("/a/gimp") << 1 << true << 1;
}

void tst_QScriptEngine::evaluate()
{
    QFETCH(QString, code);
    QFETCH(int, lineNumber);
    QFETCH(bool, expectHadError);
    QFETCH(int, expectErrorLineNumber);

    QScriptEngine eng;
    QScriptValue ret;
    if (lineNumber != -1)
        ret = eng.evaluate(code, /*fileName =*/QString(), lineNumber);
    else
        ret = eng.evaluate(code);
    QCOMPARE(eng.hasUncaughtException(), expectHadError);
    QCOMPARE(eng.uncaughtExceptionLineNumber(), expectErrorLineNumber);
    if (eng.hasUncaughtException() && ret.isError())
        QVERIFY(ret.property("lineNumber").strictlyEquals(QScriptValue(&eng, expectErrorLineNumber)));
    else
        QVERIFY(eng.uncaughtExceptionBacktrace().isEmpty());
}

static QScriptValue eval_nested(QScriptContext *ctx, QScriptEngine *eng)
{
    QScriptValue result = eng->newObject();
    eng->evaluate("var bar = 'local';");
    result.setProperty("thisObjectIdBefore", ctx->thisObject().property("id"));
    QScriptValue evaluatedThisObject = eng->evaluate("this");
    result.setProperty("thisObjectIdAfter", ctx->thisObject().property("id"));
    result.setProperty("evaluatedThisObjectId", evaluatedThisObject.property("id"));
    result.setProperty("local_bar", eng->evaluate("bar"));

    return result;
}

// Tests that nested evaluate uses the "this" that was passed.
void tst_QScriptEngine::nestedEvaluate()
{
    QScriptEngine eng;
    QScriptValue fun = eng.newFunction(eval_nested);
    eng.globalObject().setProperty("fun", fun);
    // From JS function call
    {
        QScriptValue result = eng.evaluate("o = { id:'foo'}; o.fun = fun; o.fun()");
        QCOMPARE(result.property("local_bar").toString(), QString("local"));
        QCOMPARE(result.property("thisObjectIdBefore").toString(), QString("foo"));
        QCOMPARE(result.property("thisObjectIdAfter").toString(), QString("foo"));
        QCOMPARE(result.property("evaluatedThisObjectId").toString(), QString("foo"));
        QScriptValue bar = eng.evaluate("bar"); // Was introduced in local scope.
        QVERIFY(bar.isError());
        QVERIFY(bar.toString().contains(QString::fromLatin1("ReferenceError")));
    }
    // From QScriptValue::call()
    {
        QScriptValue result = fun.call(eng.evaluate("p = { id:'foo' }") , QScriptValueList() );
        QCOMPARE(result.property("local_bar").toString(), QString("local"));
        QCOMPARE(result.property("thisObjectIdBefore").toString(), QString("foo"));
        QCOMPARE(result.property("thisObjectIdAfter").toString(), QString("foo"));
        QCOMPARE(result.property("evaluatedThisObjectId").toString(), QString("foo"));
        QScriptValue bar = eng.evaluate("bar");
        QVERIFY(bar.isError());
        QVERIFY(bar.toString().contains(QString::fromLatin1("ReferenceError")));
    }
}

void tst_QScriptEngine::uncaughtException()
{
    QScriptEngine eng;
    QScriptValue fun = eng.newFunction(myFunction);
    QScriptValue throwFun = eng.newFunction(myThrowingFunction);
    for (int x = -1; x < 2; ++x) {
        {
            QScriptValue ret = eng.evaluate("a = 10;\nb = 20;\n0 = 0;\n", /*fileName=*/QString(), /*lineNumber=*/x);
            QVERIFY(eng.hasUncaughtException());
            QCOMPARE(eng.uncaughtExceptionLineNumber(), x+2);
            QVERIFY(eng.uncaughtException().strictlyEquals(ret));
            (void)ret.toString();
            QVERIFY(eng.hasUncaughtException());
            QVERIFY(eng.uncaughtException().strictlyEquals(ret));
            QVERIFY(fun.call().isNull());
            QVERIFY(eng.hasUncaughtException());
            QCOMPARE(eng.uncaughtExceptionLineNumber(), x+2);
            QVERIFY(eng.uncaughtException().strictlyEquals(ret));
            eng.clearExceptions();
            QVERIFY(!eng.hasUncaughtException());
            QCOMPARE(eng.uncaughtExceptionLineNumber(), -1);
            QVERIFY(!eng.uncaughtException().isValid());

            eng.evaluate("2 = 3");
            QVERIFY(eng.hasUncaughtException());
            QScriptValue ret2 = throwFun.call();
            QVERIFY(ret2.isError());
            QVERIFY(eng.hasUncaughtException());
            QVERIFY(eng.uncaughtException().strictlyEquals(ret2));
            QCOMPARE(eng.uncaughtExceptionLineNumber(), 0);
            eng.clearExceptions();
            QVERIFY(!eng.hasUncaughtException());
            eng.evaluate("1 + 2");
            QVERIFY(!eng.hasUncaughtException());
        }
        {
            QScriptValue ret = eng.evaluate("a = 10");
            QVERIFY(!eng.hasUncaughtException());
            QVERIFY(!eng.uncaughtException().isValid());
        }
        {
            QScriptValue ret = eng.evaluate("1 = 2");
            QVERIFY(eng.hasUncaughtException());
            eng.clearExceptions();
            QVERIFY(!eng.hasUncaughtException());
        }
        {
            eng.globalObject().setProperty("throwFun", throwFun);
            eng.evaluate("1;\nthrowFun();");
            QVERIFY(eng.hasUncaughtException());
            QCOMPARE(eng.uncaughtExceptionLineNumber(), 2);
            eng.clearExceptions();
            QVERIFY(!eng.hasUncaughtException());
        }
    }
}

void tst_QScriptEngine::errorMessage_QT679()
{
    QScriptEngine engine;
    engine.globalObject().setProperty("foo", 15);
    QScriptValue error = engine.evaluate("'hello world';\nfoo.bar.blah");
    QVERIFY(error.isError());
    // The exact message is back-end specific and subject to change.
    QCOMPARE(error.toString(), QString::fromLatin1("TypeError: Result of expression 'foo.bar' [undefined] is not an object."));
}

struct Foo {
public:
    int x, y;
    Foo() : x(-1), y(-1) { }
};

Q_DECLARE_METATYPE(Foo)
Q_DECLARE_METATYPE(Foo*)

void tst_QScriptEngine::getSetDefaultPrototype_int()
{
    QScriptEngine eng;

    QScriptValue object = eng.newObject();
    QCOMPARE(eng.defaultPrototype(qMetaTypeId<int>()).isValid(), false);
    eng.setDefaultPrototype(qMetaTypeId<int>(), object);
    QCOMPARE(eng.defaultPrototype(qMetaTypeId<int>()).strictlyEquals(object), true);
    QScriptValue value = eng.newVariant(int(123));
    QCOMPARE(value.prototype().isObject(), true);
    QCOMPARE(value.prototype().strictlyEquals(object), true);

    eng.setDefaultPrototype(qMetaTypeId<int>(), QScriptValue());
    QCOMPARE(eng.defaultPrototype(qMetaTypeId<int>()).isValid(), false);
    QScriptValue value2 = eng.newVariant(int(123));
    QCOMPARE(value2.prototype().strictlyEquals(object), false);
}

void tst_QScriptEngine::getSetDefaultPrototype_customType()
{
    QScriptEngine eng;

    QScriptValue object = eng.newObject();
    QCOMPARE(eng.defaultPrototype(qMetaTypeId<Foo>()).isValid(), false);
    eng.setDefaultPrototype(qMetaTypeId<Foo>(), object);
    QCOMPARE(eng.defaultPrototype(qMetaTypeId<Foo>()).strictlyEquals(object), true);
    QScriptValue value = eng.newVariant(qVariantFromValue(Foo()));
    QCOMPARE(value.prototype().isObject(), true);
    QCOMPARE(value.prototype().strictlyEquals(object), true);

    eng.setDefaultPrototype(qMetaTypeId<Foo>(), QScriptValue());
    QCOMPARE(eng.defaultPrototype(qMetaTypeId<Foo>()).isValid(), false);
    QScriptValue value2 = eng.newVariant(qVariantFromValue(Foo()));
    QCOMPARE(value2.prototype().strictlyEquals(object), false);
}

static QScriptValue fooToScriptValue(QScriptEngine *eng, const Foo &foo)
{
    QScriptValue obj = eng->newObject();
    obj.setProperty("x", QScriptValue(eng, foo.x));
    obj.setProperty("y", QScriptValue(eng, foo.y));
    return obj;
}

static void fooFromScriptValue(const QScriptValue &value, Foo &foo)
{
    foo.x = value.property("x").toInt32();
    foo.y = value.property("y").toInt32();
}

static QScriptValue fooToScriptValueV2(QScriptEngine *eng, const Foo &foo)
{
    return QScriptValue(eng, foo.x);
}

static void fooFromScriptValueV2(const QScriptValue &value, Foo &foo)
{
    foo.x = value.toInt32();
}

Q_DECLARE_METATYPE(QLinkedList<QString>)
Q_DECLARE_METATYPE(QList<Foo>)
Q_DECLARE_METATYPE(QVector<QChar>)
Q_DECLARE_METATYPE(QStack<int>)
Q_DECLARE_METATYPE(QQueue<char>)
Q_DECLARE_METATYPE(QLinkedList<QStack<int> >)

void tst_QScriptEngine::valueConversion_basic()
{
    QScriptEngine eng;
    {
        QScriptValue num = qScriptValueFromValue(&eng, 123);
        QCOMPARE(num.isNumber(), true);
        QCOMPARE(num.strictlyEquals(QScriptValue(&eng, 123)), true);

        int inum = qScriptValueToValue<int>(num);
        QCOMPARE(inum, 123);

        QString snum = qScriptValueToValue<QString>(num);
        QCOMPARE(snum, QLatin1String("123"));
    }
    {
        QScriptValue num = eng.toScriptValue(123);
        QCOMPARE(num.isNumber(), true);
        QCOMPARE(num.strictlyEquals(QScriptValue(&eng, 123)), true);

        int inum = eng.fromScriptValue<int>(num);
        QCOMPARE(inum, 123);

        QString snum = eng.fromScriptValue<QString>(num);
        QCOMPARE(snum, QLatin1String("123"));
    }
    {
        QScriptValue num(&eng, 123);
        QCOMPARE(qScriptValueToValue<char>(num), char(123));
        QCOMPARE(qScriptValueToValue<unsigned char>(num), (unsigned char)(123));
        QCOMPARE(qScriptValueToValue<short>(num), short(123));
        QCOMPARE(qScriptValueToValue<unsigned short>(num), (unsigned short)(123));
        QCOMPARE(qScriptValueToValue<float>(num), float(123));
        QCOMPARE(qScriptValueToValue<double>(num), double(123));
        QCOMPARE(qScriptValueToValue<qlonglong>(num), qlonglong(123));
        QCOMPARE(qScriptValueToValue<qulonglong>(num), qulonglong(123));
    }
    {
        QScriptValue num(123);
        QCOMPARE(qScriptValueToValue<char>(num), char(123));
        QCOMPARE(qScriptValueToValue<unsigned char>(num), (unsigned char)(123));
        QCOMPARE(qScriptValueToValue<short>(num), short(123));
        QCOMPARE(qScriptValueToValue<unsigned short>(num), (unsigned short)(123));
        QCOMPARE(qScriptValueToValue<float>(num), float(123));
        QCOMPARE(qScriptValueToValue<double>(num), double(123));
        QCOMPARE(qScriptValueToValue<qlonglong>(num), qlonglong(123));
        QCOMPARE(qScriptValueToValue<qulonglong>(num), qulonglong(123));
    }

    {
        QScriptValue num = qScriptValueFromValue(&eng, Q_INT64_C(0x100000000));
        QCOMPARE(qScriptValueToValue<qlonglong>(num), Q_INT64_C(0x100000000));
        QCOMPARE(qScriptValueToValue<qulonglong>(num), Q_UINT64_C(0x100000000));
    }

    {
        QChar c = QLatin1Char('c');
        QScriptValue str = QScriptValue(&eng, "ciao");
        QCOMPARE(qScriptValueToValue<QChar>(str), c);
        QScriptValue code = QScriptValue(&eng, c.unicode());
        QCOMPARE(qScriptValueToValue<QChar>(code), c);
        QCOMPARE(qScriptValueToValue<QChar>(qScriptValueFromValue(&eng, c)), c);
    }
}

void tst_QScriptEngine::valueConversion_customType()
{
    QScriptEngine eng;
    {
        // a type that we don't have built-in conversion of
        // (it's stored as a variant)
        QTime tm(1, 2, 3, 4);
        QScriptValue val = qScriptValueFromValue(&eng, tm);
        QCOMPARE(qScriptValueToValue<QTime>(val), tm);
    }

    {
        Foo foo;
        foo.x = 12;
        foo.y = 34;
        QScriptValue fooVal = qScriptValueFromValue(&eng, foo);
        QCOMPARE(fooVal.isVariant(), true);

        Foo foo2 = qScriptValueToValue<Foo>(fooVal);
        QCOMPARE(foo2.x, foo.x);
        QCOMPARE(foo2.y, foo.y);
    }

    qScriptRegisterMetaType<Foo>(&eng, fooToScriptValue, fooFromScriptValue);

    {
        Foo foo;
        foo.x = 12;
        foo.y = 34;
        QScriptValue fooVal = qScriptValueFromValue(&eng, foo);
        QCOMPARE(fooVal.isObject(), true);
        QVERIFY(fooVal.prototype().strictlyEquals(eng.evaluate("Object.prototype")));
        QCOMPARE(fooVal.property("x").strictlyEquals(QScriptValue(&eng, 12)), true);
        QCOMPARE(fooVal.property("y").strictlyEquals(QScriptValue(&eng, 34)), true);
        fooVal.setProperty("x", QScriptValue(&eng, 56));
        fooVal.setProperty("y", QScriptValue(&eng, 78));

        Foo foo2 = qScriptValueToValue<Foo>(fooVal);
        QCOMPARE(foo2.x, 56);
        QCOMPARE(foo2.y, 78);

        QScriptValue fooProto = eng.newObject();
        eng.setDefaultPrototype(qMetaTypeId<Foo>(), fooProto);
        QScriptValue fooVal2 = qScriptValueFromValue(&eng, foo2);
        QVERIFY(fooVal2.prototype().strictlyEquals(fooProto));
        QVERIFY(fooVal2.property("x").strictlyEquals(QScriptValue(&eng, 56)));
        QVERIFY(fooVal2.property("y").strictlyEquals(QScriptValue(&eng, 78)));
    }
}

void tst_QScriptEngine::valueConversion_sequence()
{
    QScriptEngine eng;
    qScriptRegisterSequenceMetaType<QLinkedList<QString> >(&eng);

    {
        QLinkedList<QString> lst;
        lst << QLatin1String("foo") << QLatin1String("bar");
        QScriptValue lstVal = qScriptValueFromValue(&eng, lst);
        QCOMPARE(lstVal.isArray(), true);
        QCOMPARE(lstVal.property("length").toInt32(), 2);
        QCOMPARE(lstVal.property("0").isString(), true);
        QCOMPARE(lstVal.property("0").toString(), QLatin1String("foo"));
        QCOMPARE(lstVal.property("1").isString(), true);
        QCOMPARE(lstVal.property("1").toString(), QLatin1String("bar"));
    }

    qScriptRegisterSequenceMetaType<QList<Foo> >(&eng);
    qScriptRegisterSequenceMetaType<QStack<int> >(&eng);
    qScriptRegisterSequenceMetaType<QVector<QChar> >(&eng);
    qScriptRegisterSequenceMetaType<QQueue<char> >(&eng);
    qScriptRegisterSequenceMetaType<QLinkedList<QStack<int> > >(&eng);

    {
        QLinkedList<QStack<int> > lst;
        QStack<int> first; first << 13 << 49; lst << first;
        QStack<int> second; second << 99999;lst << second;
        QScriptValue lstVal = qScriptValueFromValue(&eng, lst);
        QCOMPARE(lstVal.isArray(), true);
        QCOMPARE(lstVal.property("length").toInt32(), 2);
        QCOMPARE(lstVal.property("0").isArray(), true);
        QCOMPARE(lstVal.property("0").property("length").toInt32(), 2);
        QCOMPARE(lstVal.property("0").property("0").toInt32(), first.at(0));
        QCOMPARE(lstVal.property("0").property("1").toInt32(), first.at(1));
        QCOMPARE(lstVal.property("1").isArray(), true);
        QCOMPARE(lstVal.property("1").property("length").toInt32(), 1);
        QCOMPARE(lstVal.property("1").property("0").toInt32(), second.at(0));
        QCOMPARE(qscriptvalue_cast<QStack<int> >(lstVal.property("0")), first);
        QCOMPARE(qscriptvalue_cast<QStack<int> >(lstVal.property("1")), second);
        QCOMPARE(qscriptvalue_cast<QLinkedList<QStack<int> > >(lstVal), lst);
    }

    // pointers
    {
        Foo foo;
        {
            QScriptValue v = qScriptValueFromValue(&eng, &foo);
            Foo *pfoo = qscriptvalue_cast<Foo*>(v);
            QCOMPARE(pfoo, &foo);
        }
        {
            Foo *pfoo = 0;
            QScriptValue v = qScriptValueFromValue(&eng, pfoo);
            QCOMPARE(v.isNull(), true);
            QVERIFY(qscriptvalue_cast<Foo*>(v) == 0);
        }
    }

    // QList<int> and QObjectList should be converted from/to arrays by default
    {
        QList<int> lst;
        lst << 1 << 2 << 3;
        QScriptValue val = qScriptValueFromValue(&eng, lst);
        QVERIFY(val.isArray());
        QCOMPARE(val.property("length").toInt32(), lst.size());
        QCOMPARE(val.property(0).toInt32(), lst.at(0));
        QCOMPARE(val.property(1).toInt32(), lst.at(1));
        QCOMPARE(val.property(2).toInt32(), lst.at(2));

        QCOMPARE(qscriptvalue_cast<QList<int> >(val), lst);
    }
    {
        QObjectList lst;
        lst << this;
        QScriptValue val = qScriptValueFromValue(&eng, lst);
        QVERIFY(val.isArray());
        QCOMPARE(val.property("length").toInt32(), lst.size());
        QCOMPARE(val.property(0).toQObject(), (QObject *)this);

        QCOMPARE(qscriptvalue_cast<QObjectList>(val), lst);
    }
}

void tst_QScriptEngine::valueConversion_QVariant()
{
    QScriptEngine eng;
    // qScriptValueFromValue() should be "smart" when the argument is a QVariant
    {
        QScriptValue val = qScriptValueFromValue(&eng, QVariant());
        QVERIFY(!val.isVariant());
        QVERIFY(val.isUndefined());
    }
    // Checking nested QVariants
    {
        QVariant tmp1;
        QVariant tmp2(QMetaType::QVariant, &tmp1);
        QVERIFY(QMetaType::Type(tmp2.type()) == QMetaType::QVariant);

        QScriptValue val1 = qScriptValueFromValue(&eng, tmp1);
        QScriptValue val2 = qScriptValueFromValue(&eng, tmp2);
        QVERIFY(val1.isValid());
        QVERIFY(val2.isValid());
        QVERIFY(val1.isUndefined());
        QVERIFY(!val2.isUndefined());
        QVERIFY(!val1.isVariant());
        QVERIFY(val2.isVariant());
    }
    {
        QVariant tmp1(123);
        QVariant tmp2(QMetaType::QVariant, &tmp1);
        QVariant tmp3(QMetaType::QVariant, &tmp2);
        QVERIFY(QMetaType::Type(tmp1.type()) == QMetaType::Int);
        QVERIFY(QMetaType::Type(tmp2.type()) == QMetaType::QVariant);
        QVERIFY(QMetaType::Type(tmp3.type()) == QMetaType::QVariant);

        QScriptValue val1 = qScriptValueFromValue(&eng, tmp2);
        QScriptValue val2 = qScriptValueFromValue(&eng, tmp3);
        QVERIFY(val1.isValid());
        QVERIFY(val2.isValid());
        QVERIFY(val1.isVariant());
        QVERIFY(val2.isVariant());
        QVERIFY(val1.toVariant().toInt() == 123);
        QVERIFY(qScriptValueFromValue(&eng, val2.toVariant()).toVariant().toInt() == 123);
    }
    {
        QScriptValue val = qScriptValueFromValue(&eng, QVariant(true));
        QVERIFY(!val.isVariant());
        QVERIFY(val.isBoolean());
        QCOMPARE(val.toBoolean(), true);
    }
    {
        QScriptValue val = qScriptValueFromValue(&eng, QVariant(int(123)));
        QVERIFY(!val.isVariant());
        QVERIFY(val.isNumber());
        QCOMPARE(val.toNumber(), qsreal(123));
    }
    {
        QScriptValue val = qScriptValueFromValue(&eng, QVariant(qsreal(1.25)));
        QVERIFY(!val.isVariant());
        QVERIFY(val.isNumber());
        QCOMPARE(val.toNumber(), qsreal(1.25));
    }
    {
        QString str = QString::fromLatin1("ciao");
        QScriptValue val = qScriptValueFromValue(&eng, QVariant(str));
        QVERIFY(!val.isVariant());
        QVERIFY(val.isString());
        QCOMPARE(val.toString(), str);
    }
    {
        QScriptValue val = qScriptValueFromValue(&eng, qVariantFromValue((QObject*)this));
        QVERIFY(!val.isVariant());
        QVERIFY(val.isQObject());
        QCOMPARE(val.toQObject(), (QObject*)this);
    }
    {
        QVariant var = qVariantFromValue(QPoint(123, 456));
        QScriptValue val = qScriptValueFromValue(&eng, var);
        QVERIFY(val.isVariant());
        QCOMPARE(val.toVariant(), var);
    }

    QCOMPARE(qscriptvalue_cast<QVariant>(QScriptValue(123)), QVariant(123));
}

void tst_QScriptEngine::valueConversion_hooliganTask248802()
{
    QScriptEngine eng;
    qScriptRegisterMetaType<Foo>(&eng, fooToScriptValueV2, fooFromScriptValueV2);
    {
        QScriptValue num(&eng, 123);
        Foo foo = qScriptValueToValue<Foo>(num);
        QCOMPARE(foo.x, 123);
    }
    {
        QScriptValue num(123);
        Foo foo = qScriptValueToValue<Foo>(num);
        QCOMPARE(foo.x, -1);
    }
    {
        QScriptValue str(&eng, "123");
        Foo foo = qScriptValueToValue<Foo>(str);
        QCOMPARE(foo.x, 123);
    }

}

void tst_QScriptEngine::valueConversion_basic2()
{
    QScriptEngine eng;
    // more built-in types
    {
        QScriptValue val = qScriptValueFromValue(&eng, uint(123));
        QVERIFY(val.isNumber());
        QCOMPARE(val.toInt32(), 123);
    }
    {
        QScriptValue val = qScriptValueFromValue(&eng, qulonglong(123));
        QVERIFY(val.isNumber());
        QCOMPARE(val.toInt32(), 123);
    }
    {
        QScriptValue val = qScriptValueFromValue(&eng, float(123));
        QVERIFY(val.isNumber());
        QCOMPARE(val.toInt32(), 123);
    }
    {
        QScriptValue val = qScriptValueFromValue(&eng, short(123));
        QVERIFY(val.isNumber());
        QCOMPARE(val.toInt32(), 123);
    }
    {
        QScriptValue val = qScriptValueFromValue(&eng, ushort(123));
        QVERIFY(val.isNumber());
        QCOMPARE(val.toInt32(), 123);
    }
    {
        QScriptValue val = qScriptValueFromValue(&eng, char(123));
        QVERIFY(val.isNumber());
        QCOMPARE(val.toInt32(), 123);
    }
    {
        QScriptValue val = qScriptValueFromValue(&eng, uchar(123));
        QVERIFY(val.isNumber());
        QCOMPARE(val.toInt32(), 123);
    }
}

void tst_QScriptEngine::valueConversion_dateTime()
{
    QScriptEngine eng;
    {
        QDateTime in = QDateTime::currentDateTime();
        QScriptValue val = qScriptValueFromValue(&eng, in);
        QVERIFY(val.isDate());
        QCOMPARE(val.toDateTime(), in);
    }
    {
        QDate in = QDate::currentDate();
        QScriptValue val = qScriptValueFromValue(&eng, in);
        QVERIFY(val.isDate());
        QCOMPARE(val.toDateTime().date(), in);
    }
}

void tst_QScriptEngine::valueConversion_regExp()
{
    QScriptEngine eng;
    {
        QRegExp in = QRegExp("foo");
        QScriptValue val = qScriptValueFromValue(&eng, in);
        QVERIFY(val.isRegExp());
        QRegExp out = val.toRegExp();
        QEXPECT_FAIL("", "QTBUG-6136: JSC-based back-end doesn't preserve QRegExp::patternSyntax (always uses RegExp2)", Continue);
        QCOMPARE(out.patternSyntax(), in.patternSyntax());
        QCOMPARE(out.pattern(), in.pattern());
        QCOMPARE(out.caseSensitivity(), in.caseSensitivity());
        QCOMPARE(out.isMinimal(), in.isMinimal());
    }
    {
        QRegExp in = QRegExp("foo", Qt::CaseSensitive, QRegExp::RegExp2);
        QScriptValue val = qScriptValueFromValue(&eng, in);
        QVERIFY(val.isRegExp());
        QCOMPARE(val.toRegExp(), in);
    }
    {
        QRegExp in = QRegExp("foo");
        in.setMinimal(true);
        QScriptValue val = qScriptValueFromValue(&eng, in);
        QVERIFY(val.isRegExp());
        QEXPECT_FAIL("", "QTBUG-6136: JSC-based back-end doesn't preserve QRegExp::minimal (always false)", Continue);
        QCOMPARE(val.toRegExp().isMinimal(), in.isMinimal());
    }
}

void tst_QScriptEngine::valueConversion_long()
{
    QScriptEngine eng;
    {
        QScriptValue num(&eng, 123);
        QCOMPARE(qscriptvalue_cast<long>(num), long(123));
        QCOMPARE(qscriptvalue_cast<ulong>(num), ulong(123));
    }
    {
        QScriptValue num(456);
        QCOMPARE(qscriptvalue_cast<long>(num), long(456));
        QCOMPARE(qscriptvalue_cast<ulong>(num), ulong(456));
    }
    {
        QScriptValue str(&eng, "123");
        QCOMPARE(qscriptvalue_cast<long>(str), long(123));
        QCOMPARE(qscriptvalue_cast<ulong>(str), ulong(123));
    }
    {
        QScriptValue str("456");
        QCOMPARE(qscriptvalue_cast<long>(str), long(456));
        QCOMPARE(qscriptvalue_cast<ulong>(str), ulong(456));
    }
    {
        QScriptValue num = qScriptValueFromValue<long>(&eng, long(123));
        QCOMPARE(num.toInt32(), 123);
    }
    {
        QScriptValue num = qScriptValueFromValue<ulong>(&eng, ulong(456));
        QCOMPARE(num.toInt32(), 456);
    }
}

void tst_QScriptEngine::qScriptValueFromValue_noEngine()
{
    QVERIFY(!qScriptValueFromValue(0, 123).isValid());
    QVERIFY(!qScriptValueFromValue(0, QVariant(123)).isValid());
}

static QScriptValue __import__(QScriptContext *ctx, QScriptEngine *eng)
{
    return eng->importExtension(ctx->argument(0).toString());
}

void tst_QScriptEngine::importExtension()
{
    QStringList libPaths = QCoreApplication::instance()->libraryPaths();
    QCoreApplication::instance()->setLibraryPaths(QStringList() << SRCDIR);

    QStringList availableExtensions;
    {
        QScriptEngine eng;
        QVERIFY(eng.importedExtensions().isEmpty());
        QStringList ret = eng.availableExtensions();
        QCOMPARE(ret.size(), 4);
        QCOMPARE(ret.at(0), QString::fromLatin1("com"));
        QCOMPARE(ret.at(1), QString::fromLatin1("com.trolltech"));
        QCOMPARE(ret.at(2), QString::fromLatin1("com.trolltech.recursive"));
        QCOMPARE(ret.at(3), QString::fromLatin1("com.trolltech.syntaxerror"));
        availableExtensions = ret;
    }

    // try to import something that doesn't exist
    {
        QScriptEngine eng;
        QScriptValue ret = eng.importExtension("this.extension.does.not.exist");
        QCOMPARE(eng.hasUncaughtException(), true);
        QCOMPARE(ret.isError(), true);
        QCOMPARE(ret.toString(), QString::fromLatin1("Error: Unable to import this.extension.does.not.exist: no such extension"));
    }

    {
        QScriptEngine eng;
        for (int x = 0; x < 2; ++x) {
            QCOMPARE(eng.globalObject().property("com").isValid(), x == 1);
            QScriptValue ret = eng.importExtension("com.trolltech");
            QCOMPARE(eng.hasUncaughtException(), false);
            QCOMPARE(ret.isUndefined(), true);

            QScriptValue com = eng.globalObject().property("com");
            QCOMPARE(com.isObject(), true);
            QCOMPARE(com.property("wasDefinedAlready")
                     .strictlyEquals(QScriptValue(&eng, false)), true);
            QCOMPARE(com.property("name")
                     .strictlyEquals(QScriptValue(&eng, "com")), true);
            QCOMPARE(com.property("level")
                     .strictlyEquals(QScriptValue(&eng, 1)), true);
            QVERIFY(com.property("originalPostInit").isUndefined());
            QVERIFY(com.property("postInitCallCount").strictlyEquals(1));

            QScriptValue trolltech = com.property("trolltech");
            QCOMPARE(trolltech.isObject(), true);
            QCOMPARE(trolltech.property("wasDefinedAlready")
                     .strictlyEquals(QScriptValue(&eng, false)), true);
            QCOMPARE(trolltech.property("name")
                     .strictlyEquals(QScriptValue(&eng, "com.trolltech")), true);
            QCOMPARE(trolltech.property("level")
                     .strictlyEquals(QScriptValue(&eng, 2)), true);
            QVERIFY(trolltech.property("originalPostInit").isUndefined());
            QVERIFY(trolltech.property("postInitCallCount").strictlyEquals(1));
        }
        QStringList imp = eng.importedExtensions();
        QCOMPARE(imp.size(), 2);
        QCOMPARE(imp.at(0), QString::fromLatin1("com"));
        QCOMPARE(imp.at(1), QString::fromLatin1("com.trolltech"));
        QCOMPARE(eng.availableExtensions(), availableExtensions);
    }

    // recursive import should throw an error
    {
        QScriptEngine eng;
        QVERIFY(eng.importedExtensions().isEmpty());
        eng.globalObject().setProperty("__import__", eng.newFunction(__import__));
        QScriptValue ret = eng.importExtension("com.trolltech.recursive");
        QCOMPARE(eng.hasUncaughtException(), true);
        QVERIFY(ret.isError());
        QCOMPARE(ret.toString(), QString::fromLatin1("Error: recursive import of com.trolltech.recursive"));
        QStringList imp = eng.importedExtensions();
        QCOMPARE(imp.size(), 2);
        QCOMPARE(imp.at(0), QString::fromLatin1("com"));
        QCOMPARE(imp.at(1), QString::fromLatin1("com.trolltech"));
        QCOMPARE(eng.availableExtensions(), availableExtensions);
    }

    {
        QScriptEngine eng;
        eng.globalObject().setProperty("__import__", eng.newFunction(__import__));
        for (int x = 0; x < 2; ++x) {
            if (x == 0)
                QVERIFY(eng.importedExtensions().isEmpty());
            QScriptValue ret = eng.importExtension("com.trolltech.syntaxerror");
            QVERIFY(eng.hasUncaughtException());
            QEXPECT_FAIL("", "JSC throws syntax error eagerly", Continue);
            QCOMPARE(eng.uncaughtExceptionLineNumber(), 4);
            QVERIFY(ret.isError());
            QVERIFY(ret.toString().contains(QLatin1String("SyntaxError")));
        }
        QStringList imp = eng.importedExtensions();
        QCOMPARE(imp.size(), 2);
        QCOMPARE(imp.at(0), QString::fromLatin1("com"));
        QCOMPARE(imp.at(1), QString::fromLatin1("com.trolltech"));
        QCOMPARE(eng.availableExtensions(), availableExtensions);
    }

    QCoreApplication::instance()->setLibraryPaths(libPaths);
}

#if 0 //The native C++ stack overflow before the JS stack
static QScriptValue recurse(QScriptContext *ctx, QScriptEngine *eng)
{
    Q_UNUSED(eng);
    return ctx->callee().call();
}

static QScriptValue recurse2(QScriptContext *ctx, QScriptEngine *eng)
{
    Q_UNUSED(eng);
    return ctx->callee().construct();
}
#endif

void tst_QScriptEngine::infiniteRecursion()
{
    // Infinite recursion in JS should cause the VM to throw an error
    // when the JS stack is exhausted.
    // The exact error is back-end specific and subject to change.
    const QString stackOverflowError = QString::fromLatin1("RangeError: Maximum call stack size exceeded.");
    QScriptEngine eng;
    {
        QScriptValue ret = eng.evaluate("function foo() { foo(); }; foo();");
        QCOMPARE(ret.isError(), true);
        QCOMPARE(ret.toString(), stackOverflowError);
    }
#if 0 //The native C++ stack overflow before the JS stack
    {
        QScriptValue fun = eng.newFunction(recurse);
        QScriptValue ret = fun.call();
        QCOMPARE(ret.isError(), true);
        QCOMPARE(ret.toString(), stackOverflowError);
    }
    {
        QScriptValue fun = eng.newFunction(recurse2);
        QScriptValue ret = fun.construct();
        QCOMPARE(ret.isError(), true);
        QCOMPARE(ret.toString(), stackOverflowError);
    }
#endif
}

struct Bar {
    int a;
};

struct Baz : public Bar {
    int b;
};

Q_DECLARE_METATYPE(Bar*)
Q_DECLARE_METATYPE(Baz*)

Q_DECLARE_METATYPE(QGradient)
Q_DECLARE_METATYPE(QGradient*)
Q_DECLARE_METATYPE(QLinearGradient)

class Zoo : public QObject
{
    Q_OBJECT
public:
    Zoo() { }
public slots:
    Baz *toBaz(Bar *b) { return reinterpret_cast<Baz*>(b); }
};

void tst_QScriptEngine::castWithPrototypeChain()
{
    QScriptEngine eng;
    Bar bar;
    Baz baz;
    QScriptValue barProto = qScriptValueFromValue(&eng, &bar);
    QScriptValue bazProto = qScriptValueFromValue(&eng, &baz);
    eng.setDefaultPrototype(qMetaTypeId<Bar*>(), barProto);
    eng.setDefaultPrototype(qMetaTypeId<Baz*>(), bazProto);

    Baz baz2;
    baz2.a = 123;
    baz2.b = 456;
    QScriptValue baz2Value = qScriptValueFromValue(&eng, &baz2);
    {
        // qscriptvalue_cast() does magic; if the QScriptValue contains
        // t of type T, and the target type is T*, &t is returned.
        Baz *pbaz = qscriptvalue_cast<Baz*>(baz2Value);
        QVERIFY(pbaz != 0);
        QCOMPARE(pbaz->b, baz2.b);

        Zoo zoo;
        QScriptValue scriptZoo = eng.newQObject(&zoo);
        QScriptValue toBaz = scriptZoo.property("toBaz");
        QVERIFY(toBaz.isFunction());

        // no relation between Bar and Baz's proto --> casting fails
        {
            Bar *pbar = qscriptvalue_cast<Bar*>(baz2Value);
            QVERIFY(pbar == 0);
        }

        {
            QScriptValue ret = toBaz.call(scriptZoo, QScriptValueList() << baz2Value);
            QVERIFY(ret.isError());
            QCOMPARE(ret.toString(), QLatin1String("TypeError: incompatible type of argument(s) in call to toBaz(); candidates were\n    toBaz(Bar*)"));
        }

        // establish chain -- now casting should work
        // Why? because qscriptvalue_cast() does magic again.
        // It the instance itself is not of type T, qscriptvalue_cast()
        // searches the prototype chain for T, and if it finds one, it infers
        // that the instance can also be casted to that type. This cast is
        // _not_ safe and thus relies on the developer doing the right thing.
        // This is an undocumented feature to enable qscriptvalue_cast() to
        // be used by prototype functions to cast the JS this-object to C++.
        bazProto.setPrototype(barProto);

        {
            Bar *pbar = qscriptvalue_cast<Bar*>(baz2Value);
            QVERIFY(pbar != 0);
            QCOMPARE(pbar->a, baz2.a);
        }

        {
            QScriptValue ret = toBaz.call(scriptZoo, QScriptValueList() << baz2Value);
            QVERIFY(!ret.isError());
            QCOMPARE(qscriptvalue_cast<Baz*>(ret), pbaz);
        }
    }

    bazProto.setPrototype(barProto.prototype()); // kill chain
    {
        Baz *pbaz = qscriptvalue_cast<Baz*>(baz2Value);
        QVERIFY(pbaz != 0);
        // should not work anymore
        Bar *pbar = qscriptvalue_cast<Bar*>(baz2Value);
        QVERIFY(pbar == 0);
    }

    bazProto.setPrototype(eng.newQObject(this));
    {
        Baz *pbaz = qscriptvalue_cast<Baz*>(baz2Value);
        QVERIFY(pbaz != 0);
        // should not work now either
        Bar *pbar = qscriptvalue_cast<Bar*>(baz2Value);
        QVERIFY(pbar == 0);
    }

    {
        QScriptValue b = qScriptValueFromValue(&eng, QBrush());
        b.setPrototype(barProto);
        // this shows that a "wrong" cast is possible, if you
        // don't play by the rules (the pointer is actually a QBrush*)...
        Bar *pbar = qscriptvalue_cast<Bar*>(b);
        QVERIFY(pbar != 0);
    }

    {
        QScriptValue gradientProto = qScriptValueFromValue(&eng, QGradient());
        QScriptValue linearGradientProto = qScriptValueFromValue(&eng, QLinearGradient());
        linearGradientProto.setPrototype(gradientProto);
        QLinearGradient lg(10, 20, 30, 40);
        QScriptValue linearGradient = qScriptValueFromValue(&eng, lg);
        {
            QGradient *pgrad = qscriptvalue_cast<QGradient*>(linearGradient);
            QVERIFY(pgrad == 0);
        }
        linearGradient.setPrototype(linearGradientProto);
        {
            QGradient *pgrad = qscriptvalue_cast<QGradient*>(linearGradient);
            QVERIFY(pgrad != 0);
            QCOMPARE(pgrad->type(), QGradient::LinearGradient);
            QLinearGradient *plingrad = static_cast<QLinearGradient*>(pgrad);
            QCOMPARE(plingrad->start(), lg.start());
            QCOMPARE(plingrad->finalStop(), lg.finalStop());
        }
    }
}

class Klazz : public QWidget,
              public QStandardItem,
              public QGraphicsItem
{
    Q_OBJECT
public:
    Klazz(QWidget *parent = 0) : QWidget(parent) { }
    virtual QRectF boundingRect() const { return QRectF(); }
    virtual void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) { }
};

Q_DECLARE_METATYPE(Klazz*)
Q_DECLARE_METATYPE(QStandardItem*)

void tst_QScriptEngine::castWithMultipleInheritance()
{
    QScriptEngine eng;
    Klazz klz;
    QScriptValue v = eng.newQObject(&klz);

    QCOMPARE(qscriptvalue_cast<Klazz*>(v), &klz);
    QCOMPARE(qscriptvalue_cast<QWidget*>(v), (QWidget *)&klz);
    QCOMPARE(qscriptvalue_cast<QObject*>(v), (QObject *)&klz);
    QCOMPARE(qscriptvalue_cast<QStandardItem*>(v), (QStandardItem *)&klz);
    QCOMPARE(qscriptvalue_cast<QGraphicsItem*>(v), (QGraphicsItem *)&klz);
}

void tst_QScriptEngine::collectGarbage()
{
    QScriptEngine eng;
    eng.evaluate("a = new Object(); a = new Object(); a = new Object()");
    QScriptValue a = eng.newObject();
    a = eng.newObject();
    a = eng.newObject();
    QPointer<QObject> ptr = new QObject();
    QVERIFY(ptr != 0);
    (void)eng.newQObject(ptr, QScriptEngine::ScriptOwnership);
    collectGarbage_helper(eng);
    QSKIP("This test does not work reliably"); // (maybe some pointers are still on the stack somewhere)
    QVERIFY(ptr == 0);
}

void tst_QScriptEngine::reportAdditionalMemoryCost()
{
    QScriptEngine eng;
    // There isn't any reliable way to test whether calling
    // this function affects garbage collection responsiveness;
    // the best we can do is call it with a few different values.
    for (int x = 0; x < 1000; ++x) {
        eng.reportAdditionalMemoryCost(0);
        eng.reportAdditionalMemoryCost(10);
        eng.reportAdditionalMemoryCost(1000);
        eng.reportAdditionalMemoryCost(10000);
        eng.reportAdditionalMemoryCost(100000);
        eng.reportAdditionalMemoryCost(1000000);
        eng.reportAdditionalMemoryCost(10000000);
        eng.reportAdditionalMemoryCost(-1);
        eng.reportAdditionalMemoryCost(-1000);
        QScriptValue obj = eng.newObject();
        eng.collectGarbage();
    }
}

void tst_QScriptEngine::gcWithNestedDataStructure()
{
    // The GC must be able to traverse deeply nested objects, otherwise this
    // test would crash.
    QScriptEngine eng;
    eng.evaluate(
        "function makeList(size)"
        "{"
        "  var head = { };"
        "  var l = head;"
        "  for (var i = 0; i < size; ++i) {"
        "    l.data = i + \"\";"
        "    l.next = { }; l = l.next;"
        "  }"
        "  l.next = null;"
        "  return head;"
        "}");
    QCOMPARE(eng.hasUncaughtException(), false);
    const int size = 200;
    QScriptValue head = eng.evaluate(QString::fromLatin1("makeList(%0)").arg(size));
    QCOMPARE(eng.hasUncaughtException(), false);
    for (int x = 0; x < 2; ++x) {
        if (x == 1)
            eng.evaluate("gc()");
        QScriptValue l = head;
        // Make sure all the nodes are still alive.
        for (int i = 0; i < 200; ++i) {
            QCOMPARE(l.property("data").toString(), QString::number(i));
            l = l.property("next");
        }
    }
}

class EventReceiver : public QObject
{
public:
    EventReceiver() {
        received = false;
    }

    bool event(QEvent *e) {
        received |= (e->type() == QEvent::User + 1);
        return QObject::event(e);
    }

    bool received;
};

void tst_QScriptEngine::processEventsWhileRunning()
{
    for (int x = 0; x < 2; ++x) {
        QScriptEngine eng;
        if (x == 0)
            eng.pushContext();

        // This is running for a silly amount of time just to make sure
        // the script doesn't finish before event processing is triggered.
        QString script = QString::fromLatin1(
            "var end = Number(new Date()) + 2000;"
            "var x = 0;"
            "while (Number(new Date()) < end) {"
            "    ++x;"
            "}");

        EventReceiver receiver;
        QCoreApplication::postEvent(&receiver, new QEvent(QEvent::Type(QEvent::User+1)));

        eng.evaluate(script);
        QVERIFY(!eng.hasUncaughtException());
        QVERIFY(!receiver.received);

        QCOMPARE(eng.processEventsInterval(), -1);
        eng.setProcessEventsInterval(100);
        eng.evaluate(script);
        QVERIFY(!eng.hasUncaughtException());
        QVERIFY(receiver.received);

        if (x == 0)
            eng.popContext();
    }
}

class EventReceiver2 : public QObject
{
public:
    EventReceiver2(QScriptEngine *eng) {
        engine = eng;
    }

    bool event(QEvent *e) {
        if (e->type() == QEvent::User + 1) {
            engine->currentContext()->throwError("Killed");
        }
        return QObject::event(e);
    }

    QScriptEngine *engine;
};

void tst_QScriptEngine::throwErrorFromProcessEvents_data()
{
    QTest::addColumn<QString>("script");
    QTest::addColumn<QString>("error");

    QTest::newRow("while (1)")
        << QString::fromLatin1("while (1) { }")
        << QString::fromLatin1("Error: Killed");
    QTest::newRow("while (1) i++")
        << QString::fromLatin1("i = 0; while (1) { i++; }")
        << QString::fromLatin1("Error: Killed");
    // Unlike abortEvaluation(), scripts should be able to catch the
    // exception.
    QTest::newRow("try catch")
        << QString::fromLatin1("try {"
                               "    while (1) { }"
                               "} catch(e) {"
                               "    throw new Error('Caught');"
                               "}")
        << QString::fromLatin1("Error: Caught");
}

void tst_QScriptEngine::throwErrorFromProcessEvents()
{
    QFETCH(QString, script);
    QFETCH(QString, error);

    QScriptEngine eng;

    EventReceiver2 receiver(&eng);
    QCoreApplication::postEvent(&receiver, new QEvent(QEvent::Type(QEvent::User+1)));

    eng.setProcessEventsInterval(100);
    QScriptValue ret = eng.evaluate(script);
    QVERIFY(ret.isError());
    QCOMPARE(ret.toString(), error);
}

void tst_QScriptEngine::disableProcessEventsInterval()
{
    QScriptEngine eng;
    eng.setProcessEventsInterval(100);
    QCOMPARE(eng.processEventsInterval(), 100);
    eng.setProcessEventsInterval(0);
    QCOMPARE(eng.processEventsInterval(), 0);
    eng.setProcessEventsInterval(-1);
    QCOMPARE(eng.processEventsInterval(), -1);
    eng.setProcessEventsInterval(-100);
    QCOMPARE(eng.processEventsInterval(), -100);
}

void tst_QScriptEngine::stacktrace()
{
    QString script = QString::fromLatin1(
        "function foo(counter) {\n"
        "    switch (counter) {\n"
        "        case 0: foo(counter+1); break;\n"
        "        case 1: foo(counter+1); break;\n"
        "        case 2: foo(counter+1); break;\n"
        "        case 3: foo(counter+1); break;\n"
        "        case 4: foo(counter+1); break;\n"
        "        default:\n"
        "        throw new Error('blah');\n"
        "    }\n"
        "}\n"
        "foo(0);");

    const QString fileName("testfile");

    QStringList backtrace;
    backtrace << "foo(counter = 5) at testfile:9"
              << "foo(counter = 4) at testfile:7"
              << "foo(counter = 3) at testfile:6"
              << "foo(counter = 2) at testfile:5"
              << "foo(counter = 1) at testfile:4"
              << "foo(counter = 0) at testfile:3"
              << "<global>() at testfile:12";

    QScriptEngine eng;
    QScriptValue result = eng.evaluate(script, fileName);
    QVERIFY(eng.hasUncaughtException());
    QVERIFY(result.isError());

    QCOMPARE(eng.uncaughtExceptionBacktrace(), backtrace);
    QVERIFY(eng.hasUncaughtException());
    QVERIFY(result.strictlyEquals(eng.uncaughtException()));

    QCOMPARE(result.property("fileName").toString(), fileName);
    QCOMPARE(result.property("lineNumber").toInt32(), 9);

    // throw something that isn't an Error object
    eng.clearExceptions();
    QVERIFY(eng.uncaughtExceptionBacktrace().isEmpty());
    QString script2 = QString::fromLatin1(
        "function foo(counter) {\n"
        "    switch (counter) {\n"
        "        case 0: foo(counter+1); break;\n"
        "        case 1: foo(counter+1); break;\n"
        "        case 2: foo(counter+1); break;\n"
        "        case 3: foo(counter+1); break;\n"
        "        case 4: foo(counter+1); break;\n"
        "        default:\n"
        "        throw 'just a string';\n"
        "    }\n"
        "}\n"
        "foo(0);");

    QScriptValue result2 = eng.evaluate(script2, fileName);
    QVERIFY(eng.hasUncaughtException());
    QVERIFY(!result2.isError());
    QVERIFY(result2.isString());

    QCOMPARE(eng.uncaughtExceptionBacktrace(), backtrace);
    QVERIFY(eng.hasUncaughtException());

    eng.clearExceptions();
    QVERIFY(!eng.hasUncaughtException());
    QVERIFY(eng.uncaughtExceptionBacktrace().isEmpty());
}

void tst_QScriptEngine::stacktrace_callJSFromCpp_data()
{
    QTest::addColumn<QString>("callbackExpression");

    QTest::newRow("explicit throw") << QString::fromLatin1("throw new Error('callback threw')");
    QTest::newRow("reference error") << QString::fromLatin1("noSuchFunction()");
}

// QTBUG-26889
void tst_QScriptEngine::stacktrace_callJSFromCpp()
{
    struct CallbackCaller {
        static QScriptValue call(QScriptContext *, QScriptEngine *eng)
        { return eng->globalObject().property(QStringLiteral("callback")).call(); }

    };

    QFETCH(QString, callbackExpression);
    QString script = QString::fromLatin1(
                "function callback() {\n"
                "    %0\n"
                "}\n"
                "callCallbackFromCpp()").arg(callbackExpression);

    QScriptEngine eng;
    eng.globalObject().setProperty(QStringLiteral("callCallbackFromCpp"),
                                   eng.newFunction(&CallbackCaller::call));
    eng.evaluate(script, QStringLiteral("test.js"));

    QVERIFY(eng.hasUncaughtException());
    QCOMPARE(eng.uncaughtExceptionLineNumber(), 2);

    QStringList expectedBacktrace;
    expectedBacktrace << QStringLiteral("callback() at test.js:2")
                      << QStringLiteral("<native>() at -1")
                      << QStringLiteral("<global>() at test.js:4");
    QCOMPARE(eng.uncaughtExceptionBacktrace(), expectedBacktrace);
}

void tst_QScriptEngine::numberParsing_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<qsreal>("expect");

    QTest::newRow("decimal 0") << QString("0") << qsreal(0);
    QTest::newRow("octal 0") << QString("00") << qsreal(00);
    QTest::newRow("hex 0") << QString("0x0") << qsreal(0x0);
    QTest::newRow("decimal 100") << QString("100") << qsreal(100);
    QTest::newRow("hex 100") << QString("0x100") << qsreal(0x100);
    QTest::newRow("octal 100") << QString("0100") << qsreal(0100);
    QTest::newRow("decimal 4G") << QString("4294967296") << qsreal(Q_UINT64_C(4294967296));
    QTest::newRow("hex 4G") << QString("0x100000000") << qsreal(Q_UINT64_C(0x100000000));
    QTest::newRow("octal 4G") << QString("040000000000") << qsreal(Q_UINT64_C(040000000000));
    QTest::newRow("0.5") << QString("0.5") << qsreal(0.5);
    QTest::newRow("1.5") << QString("1.5") << qsreal(1.5);
    QTest::newRow("1e2") << QString("1e2") << qsreal(100);
}

void tst_QScriptEngine::numberParsing()
{
    QFETCH(QString, string);
    QFETCH(qsreal, expect);

    QScriptEngine eng;
    QScriptValue ret = eng.evaluate(string);
    QVERIFY(ret.isNumber());
    qsreal actual = ret.toNumber();
    QCOMPARE(actual, expect);
}

// see ECMA-262, section 7.9
// This is testing ECMA compliance, not our C++ API, but it's important that
// the back-end is conformant in this regard.
void tst_QScriptEngine::automaticSemicolonInsertion()
{
    QScriptEngine eng;
    {
        QScriptValue ret = eng.evaluate("{ 1 2 } 3");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains("SyntaxError"));
    }
    {
        QScriptValue ret = eng.evaluate("{ 1\n2 } 3");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 3);
    }
    {
        QScriptValue ret = eng.evaluate("for (a; b\n)");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains("SyntaxError"));
    }
    {
        QScriptValue ret = eng.evaluate("(function() { return\n1 + 2 })()");
        QVERIFY(ret.isUndefined());
    }
    {
        eng.evaluate("c = 2; b = 1");
        QScriptValue ret = eng.evaluate("a = b\n++c");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 3);
    }
    {
        QScriptValue ret = eng.evaluate("if (a > b)\nelse c = d");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains("SyntaxError"));
    }
    {
        eng.evaluate("function c() { return { foo: function() { return 5; } } }");
        eng.evaluate("b = 1; d = 2; e = 3");
        QScriptValue ret = eng.evaluate("a = b + c\n(d + e).foo()");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 6);
    }
    {
        QScriptValue ret = eng.evaluate("throw\n1");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains("SyntaxError"));
    }
    {
        QScriptValue ret = eng.evaluate("a = Number(1)\n++a");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 2);
    }

    // "a semicolon is never inserted automatically if the semicolon
    // would then be parsed as an empty statement"
    {
        eng.evaluate("a = 123");
        QScriptValue ret = eng.evaluate("if (0)\n ++a; a");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }
    {
        eng.evaluate("a = 123");
        QScriptValue ret = eng.evaluate("if (0)\n --a; a");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }
    {
        eng.evaluate("a = 123");
        QScriptValue ret = eng.evaluate("if ((0))\n ++a; a");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }
    {
        eng.evaluate("a = 123");
        QScriptValue ret = eng.evaluate("if ((0))\n --a; a");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }
    {
        eng.evaluate("a = 123");
        QScriptValue ret = eng.evaluate("if (0\n)\n ++a; a");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }
    {
        eng.evaluate("a = 123");
        QScriptValue ret = eng.evaluate("if (0\n ++a; a");
        QVERIFY(ret.isError());
    }
    {
        eng.evaluate("a = 123");
        QScriptValue ret = eng.evaluate("if (0))\n ++a; a");
        QVERIFY(ret.isError());
    }
    {
        QScriptValue ret = eng.evaluate("n = 0; for (i = 0; i < 10; ++i)\n ++n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 10);
    }
    {
        QScriptValue ret = eng.evaluate("n = 30; for (i = 0; i < 10; ++i)\n --n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 20);
    }
    {
        QScriptValue ret = eng.evaluate("n = 0; for (var i = 0; i < 10; ++i)\n ++n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 10);
    }
    {
        QScriptValue ret = eng.evaluate("n = 30; for (var i = 0; i < 10; ++i)\n --n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 20);
    }
    {
        QScriptValue ret = eng.evaluate("n = 0; i = 0; while (i++ < 10)\n ++n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 10);
    }
    {
        QScriptValue ret = eng.evaluate("n = 30; i = 0; while (i++ < 10)\n --n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 20);
    }
    {
        QScriptValue ret = eng.evaluate("o = { a: 0, b: 1, c: 2 }; n = 0; for (i in o)\n ++n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 3);
    }
    {
        QScriptValue ret = eng.evaluate("o = { a: 0, b: 1, c: 2 }; n = 9; for (i in o)\n --n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 6);
    }
    {
        QScriptValue ret = eng.evaluate("o = { a: 0, b: 1, c: 2 }; n = 0; for (var i in o)\n ++n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 3);
    }
    {
        QScriptValue ret = eng.evaluate("o = { a: 0, b: 1, c: 2 }; n = 9; for (var i in o)\n --n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 6);
    }
    {
        QScriptValue ret = eng.evaluate("o = { n: 3 }; n = 5; with (o)\n ++n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 5);
    }
    {
        QScriptValue ret = eng.evaluate("o = { n: 3 }; n = 10; with (o)\n --n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 10);
    }
    {
        QScriptValue ret = eng.evaluate("n = 5; i = 0; do\n ++n; while (++i < 10); n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 15);
    }
    {
        QScriptValue ret = eng.evaluate("n = 20; i = 0; do\n --n; while (++i < 10); n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 10);
    }

    {
        QScriptValue ret = eng.evaluate("n = 1; i = 0; if (n) i\n++n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 2);
    }
    {
        QScriptValue ret = eng.evaluate("n = 1; i = 0; if (n) i\n--n; n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 0);
    }

    {
        QScriptValue ret = eng.evaluate("if (0)");
        QVERIFY(ret.isError());
    }
    {
        QScriptValue ret = eng.evaluate("while (0)");
        QVERIFY(ret.isError());
    }
    {
        QScriptValue ret = eng.evaluate("for (;;)");
        QVERIFY(ret.isError());
    }
    {
        QScriptValue ret = eng.evaluate("for (p in this)");
        QVERIFY(ret.isError());
    }
    {
        QScriptValue ret = eng.evaluate("with (this)");
        QVERIFY(ret.isError());
    }
    {
        QScriptValue ret = eng.evaluate("do");
        QVERIFY(ret.isError());
    }
}

class EventReceiver3 : public QObject
{
public:
    enum AbortionResult {
        None = 0,
        String = 1,
        Error = 2,
        Number = 3
    };

    EventReceiver3(QScriptEngine *eng) {
        engine = eng;
        resultType = None;
    }

    bool event(QEvent *e) {
        if (e->type() == QEvent::User + 1) {
            switch (resultType) {
            case None:
                engine->abortEvaluation();
                break;
            case String:
                engine->abortEvaluation(QScriptValue(engine, QString::fromLatin1("Aborted")));
                break;
            case Error:
                engine->abortEvaluation(engine->currentContext()->throwError("AbortedWithError"));
                break;
            case Number:
                engine->abortEvaluation(QScriptValue(1234));
            }
        }
        return QObject::event(e);
    }

    AbortionResult resultType;
    QScriptEngine *engine;
};

static QScriptValue myFunctionAbortingEvaluation(QScriptContext *, QScriptEngine *eng)
{
    eng->abortEvaluation();
    return eng->nullValue(); // should be ignored
}

void tst_QScriptEngine::abortEvaluation_notEvaluating()
{
    QScriptEngine eng;

    eng.abortEvaluation();
    QVERIFY(!eng.hasUncaughtException());

    eng.abortEvaluation(123);
    {
        QScriptValue ret = eng.evaluate("'ciao'");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("ciao"));
    }
}

void tst_QScriptEngine::abortEvaluation_data()
{
    QTest::addColumn<QString>("script");

    QTest::newRow("while (1)")
        << QString::fromLatin1("while (1) { }");
    QTest::newRow("while (1) i++")
        << QString::fromLatin1("i = 0; while (1) { i++; }");
    QTest::newRow("try catch")
        << QString::fromLatin1("try {"
                               "    while (1) { }"
                               "} catch(e) {"
                               "    throw new Error('Caught');"
                               "}");
}

void tst_QScriptEngine::abortEvaluation()
{
    QFETCH(QString, script);

    QScriptEngine eng;
    EventReceiver3 receiver(&eng);

    eng.setProcessEventsInterval(100);
    for (int x = 0; x < 4; ++x) {
        QCoreApplication::postEvent(&receiver, new QEvent(QEvent::Type(QEvent::User+1)));
        receiver.resultType = EventReceiver3::AbortionResult(x);
        QScriptValue ret = eng.evaluate(script);
        switch (receiver.resultType) {
        case EventReceiver3::None:
            QVERIFY(!eng.hasUncaughtException());
            QVERIFY(!ret.isValid());
            break;
        case EventReceiver3::Number:
            QVERIFY(!eng.hasUncaughtException());
            QVERIFY(ret.isNumber());
            QCOMPARE(ret.toInt32(), 1234);
            break;
        case EventReceiver3::String:
            QVERIFY(!eng.hasUncaughtException());
            QVERIFY(ret.isString());
            QCOMPARE(ret.toString(), QString::fromLatin1("Aborted"));
            break;
        case EventReceiver3::Error:
            QVERIFY(eng.hasUncaughtException());
            QVERIFY(ret.isError());
            QCOMPARE(ret.toString(), QString::fromLatin1("Error: AbortedWithError"));
            break;
        }
    }

}

void tst_QScriptEngine::abortEvaluation_tryCatch()
{
    QScriptEngine eng;
    EventReceiver3 receiver(&eng);
    eng.setProcessEventsInterval(100);
    // scripts cannot intercept the abortion with try/catch
    for (int y = 0; y < 4; ++y) {
        QCoreApplication::postEvent(&receiver, new QEvent(QEvent::Type(QEvent::User+1)));
        receiver.resultType = EventReceiver3::AbortionResult(y);
        QScriptValue ret = eng.evaluate(QString::fromLatin1(
                                            "while (1) {\n"
                                            "  try {\n"
                                            "    (function() { while (1) { } })();\n"
                                            "  } catch (e) {\n"
                                            "    ;\n"
                                            "  }\n"
                                            "}"));
        switch (receiver.resultType) {
        case EventReceiver3::None:
            QVERIFY(!eng.hasUncaughtException());
            QVERIFY(!ret.isValid());
            break;
        case EventReceiver3::Number:
            QVERIFY(!eng.hasUncaughtException());
            QVERIFY(ret.isNumber());
            QCOMPARE(ret.toInt32(), 1234);
            break;
        case EventReceiver3::String:
            QVERIFY(!eng.hasUncaughtException());
            QVERIFY(ret.isString());
            QCOMPARE(ret.toString(), QString::fromLatin1("Aborted"));
            break;
        case EventReceiver3::Error:
            QVERIFY(eng.hasUncaughtException());
            QVERIFY(ret.isError());
            break;
        }
    }
}

void tst_QScriptEngine::abortEvaluation_fromNative()
{
    QScriptEngine eng;
    QScriptValue fun = eng.newFunction(myFunctionAbortingEvaluation);
    eng.globalObject().setProperty("myFunctionAbortingEvaluation", fun);
    QScriptValue ret = eng.evaluate("myFunctionAbortingEvaluation()");
    QVERIFY(!ret.isValid());
}

class ThreadedEngine : public QThread {
    Q_OBJECT

private:
    QScriptEngine* m_engine;
protected:
    void run() {
        m_engine = new QScriptEngine();
        m_engine->setGlobalObject(m_engine->newQObject(this));
        m_engine->evaluate("while(1) { sleep(); }");
        delete m_engine;
    }

public slots:
    void sleep()
    {
        QTest::qSleep(25);
        m_engine->abortEvaluation();
    }
};

void tst_QScriptEngine::abortEvaluation_QTBUG9433()
{
    ThreadedEngine engine;
    engine.start();
    QVERIFY(engine.isRunning());
    QTest::qSleep(50);
    for (uint i = 0; i < 50; ++i) { // up to ~2500 ms
        if (engine.isFinished())
            return;
        QTest::qSleep(50);
    }
    if (!engine.isFinished()) {
        engine.terminate();
        engine.wait(7000);
        QFAIL("abortEvaluation doesn't work");
    }

}

static QScriptValue myFunctionReturningIsEvaluating(QScriptContext *, QScriptEngine *eng)
{
    return QScriptValue(eng, eng->isEvaluating());
}

class EventReceiver4 : public QObject
{
public:
    EventReceiver4(QScriptEngine *eng) {
        engine = eng;
        wasEvaluating = false;
    }

    bool event(QEvent *e) {
        if (e->type() == QEvent::User + 1) {
            wasEvaluating = engine->isEvaluating();
        }
        return QObject::event(e);
    }

    QScriptEngine *engine;
    bool wasEvaluating;
};

void tst_QScriptEngine::isEvaluating_notEvaluating()
{
    QScriptEngine eng;

    QVERIFY(!eng.isEvaluating());

    eng.evaluate("");
    QVERIFY(!eng.isEvaluating());
    eng.evaluate("123");
    QVERIFY(!eng.isEvaluating());
    eng.evaluate("0 = 0");
    QVERIFY(!eng.isEvaluating());
}

void tst_QScriptEngine::isEvaluating_fromNative()
{
    QScriptEngine eng;
    QScriptValue fun = eng.newFunction(myFunctionReturningIsEvaluating);
    eng.globalObject().setProperty("myFunctionReturningIsEvaluating", fun);
    QScriptValue ret = eng.evaluate("myFunctionReturningIsEvaluating()");
    QVERIFY(ret.isBoolean());
    QVERIFY(ret.toBoolean());
}

void tst_QScriptEngine::isEvaluating_fromEvent()
{
    QScriptEngine eng;
    EventReceiver4 receiver(&eng);
    QCoreApplication::postEvent(&receiver, new QEvent(QEvent::Type(QEvent::User+1)));

    QString script = QString::fromLatin1(
        "var end = Number(new Date()) + 1000;"
        "var x = 0;"
        "while (Number(new Date()) < end) {"
        "    ++x;"
        "}");

    eng.setProcessEventsInterval(100);
    eng.evaluate(script);
    QVERIFY(receiver.wasEvaluating);
}

static QtMsgType theMessageType;
static QString theMessage;

static void myMsgHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    theMessageType = type;
    theMessage = msg;
}

void tst_QScriptEngine::printFunctionWithCustomHandler()
{
    // The built-in print() function passes the output to Qt's message
    // handler. By installing a custom message handler, the output can be
    // redirected without changing the print() function itself.
    // This behavior is not documented.
    QScriptEngine eng;
    QtMessageHandler oldHandler = qInstallMessageHandler(myMsgHandler);
    QVERIFY(eng.globalObject().property("print").isFunction());

    theMessageType = QtSystemMsg;
    QVERIFY(theMessage.isEmpty());
    QVERIFY(eng.evaluate("print('test')").isUndefined());
    QCOMPARE(theMessageType, QtDebugMsg);
    QCOMPARE(theMessage, QString::fromLatin1("test"));

    theMessageType = QtSystemMsg;
    theMessage.clear();
    QVERIFY(eng.evaluate("print(3, true, 'little pigs')").isUndefined());
    QCOMPARE(theMessageType, QtDebugMsg);
    QCOMPARE(theMessage, QString::fromLatin1("3 true little pigs"));

    qInstallMessageHandler(oldHandler);
}

void tst_QScriptEngine::printThrowsException()
{
    // If an argument to print() causes an exception to be thrown when
    // it's converted to a string, print() should propagate the exception.
    QScriptEngine eng;
    QScriptValue ret = eng.evaluate("print({ toString: function() { throw 'foo'; } });");
    QVERIFY(eng.hasUncaughtException());
    QVERIFY(ret.strictlyEquals(QScriptValue(&eng, QLatin1String("foo"))));
}

void tst_QScriptEngine::errorConstructors()
{
    QScriptEngine eng;
    QStringList prefixes;
    prefixes << "" << "Eval" << "Range" << "Reference" << "Syntax" << "Type" << "URI";
    for (int x = 0; x < 3; ++x) {
        for (int i = 0; i < prefixes.size(); ++i) {
            QString name = prefixes.at(i) + QLatin1String("Error");
            QString code = QString(i+1, QLatin1Char('\n'));
            if (x == 0)
                code += QLatin1String("throw ");
            else if (x == 1)
                code += QLatin1String("new ");
            code += name + QLatin1String("()");
            QScriptValue ret = eng.evaluate(code);
            QVERIFY(ret.isError());
            QCOMPARE(eng.hasUncaughtException(), x == 0);
            eng.clearExceptions();
            QVERIFY(ret.toString().startsWith(name));
            if (x != 0)
                QEXPECT_FAIL("", "QTBUG-6138: JSC doesn't assign lineNumber when errors are not thrown", Continue);
            QCOMPARE(ret.property("lineNumber").toInt32(), i+2);
        }
    }
}

void tst_QScriptEngine::argumentsProperty_globalContext()
{
    QScriptEngine eng;
    {
        // Unlike function calls, the global context doesn't have an
        // arguments property.
        QScriptValue ret = eng.evaluate("arguments");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("ReferenceError")));
    }
    eng.evaluate("arguments = 10");
    {
        QScriptValue ret = eng.evaluate("arguments");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 10);
    }
    QVERIFY(eng.evaluate("delete arguments").toBoolean());
    QVERIFY(!eng.globalObject().property("arguments").isValid());
}

void tst_QScriptEngine::argumentsProperty_JS()
{
    {
        QScriptEngine eng;
        eng.evaluate("o = { arguments: 123 }");
        QScriptValue ret = eng.evaluate("with (o) { arguments; }");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
    }
    {
        QScriptEngine eng;
        QVERIFY(!eng.globalObject().property("arguments").isValid());
        // This is testing ECMA-262 compliance. In function calls, "arguments"
        // appears like a local variable, and it can be replaced.
        QScriptValue ret = eng.evaluate("(function() { arguments = 456; return arguments; })()");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 456);
        QVERIFY(!eng.globalObject().property("arguments").isValid());
    }
}

static QScriptValue argumentsProperty_fun(QScriptContext *, QScriptEngine *eng)
{
    // Since evaluate() is done in the current context, "arguments" should
    // refer to currentContext()->argumentsObject().
    // This is for consistency with the built-in JS eval() function.
    eng->evaluate("var a = arguments[0];");
    eng->evaluate("arguments[0] = 200;");
    return eng->evaluate("a + arguments[0]");
}

void tst_QScriptEngine::argumentsProperty_evaluateInNativeFunction()
{
    QScriptEngine eng;
    QScriptValue fun = eng.newFunction(argumentsProperty_fun);
    eng.globalObject().setProperty("fun", eng.newFunction(argumentsProperty_fun));
    QScriptValue result = eng.evaluate("fun(18)");
    QVERIFY(result.isNumber());
    QCOMPARE(result.toInt32(), 200+18);
}

void tst_QScriptEngine::jsNumberClass()
{
    // See ECMA-262 Section 15.7, "Number Objects".

    QScriptEngine eng;

    QScriptValue ctor = eng.globalObject().property("Number");
    QVERIFY(ctor.property("length").isNumber());
    QCOMPARE(ctor.property("length").toNumber(), qsreal(1));
    QScriptValue proto = ctor.property("prototype");
    QVERIFY(proto.isObject());
    {
        QScriptValue::PropertyFlags flags = QScriptValue::SkipInEnumeration
                                            | QScriptValue::Undeletable
                                            | QScriptValue::ReadOnly;
        QCOMPARE(ctor.propertyFlags("prototype"), flags);
        QVERIFY(ctor.property("MAX_VALUE").isNumber());
        QCOMPARE(ctor.propertyFlags("MAX_VALUE"), flags);
        QVERIFY(ctor.property("MIN_VALUE").isNumber());
        QCOMPARE(ctor.propertyFlags("MIN_VALUE"), flags);
        QVERIFY(ctor.property("NaN").isNumber());
        QCOMPARE(ctor.propertyFlags("NaN"), flags);
        QVERIFY(ctor.property("NEGATIVE_INFINITY").isNumber());
        QCOMPARE(ctor.propertyFlags("NEGATIVE_INFINITY"), flags);
        QVERIFY(ctor.property("POSITIVE_INFINITY").isNumber());
        QCOMPARE(ctor.propertyFlags("POSITIVE_INFINITY"), flags);
    }
    QVERIFY(proto.instanceOf(eng.globalObject().property("Object")));
    QCOMPARE(proto.toNumber(), qsreal(0));
    QVERIFY(proto.property("constructor").strictlyEquals(ctor));

    {
        QScriptValue ret = eng.evaluate("Number()");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toNumber(), qsreal(0));
    }
    {
        QScriptValue ret = eng.evaluate("Number(123)");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toNumber(), qsreal(123));
    }
    {
        QScriptValue ret = eng.evaluate("Number('456')");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toNumber(), qsreal(456));
    }
    {
        QScriptValue ret = eng.evaluate("new Number()");
        QVERIFY(!ret.isNumber());
        QVERIFY(ret.isObject());
        QCOMPARE(ret.toNumber(), qsreal(0));
    }
    {
        QScriptValue ret = eng.evaluate("new Number(123)");
        QVERIFY(!ret.isNumber());
        QVERIFY(ret.isObject());
        QCOMPARE(ret.toNumber(), qsreal(123));
    }
    {
        QScriptValue ret = eng.evaluate("new Number('456')");
        QVERIFY(!ret.isNumber());
        QVERIFY(ret.isObject());
        QCOMPARE(ret.toNumber(), qsreal(456));
    }

    QVERIFY(proto.property("toString").isFunction());
    {
        QScriptValue ret = eng.evaluate("new Number(123).toString()");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("123"));
    }
    {
        QScriptValue ret = eng.evaluate("new Number(123).toString(8)");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("173"));
    }
    {
        QScriptValue ret = eng.evaluate("new Number(123).toString(16)");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("7b"));
    }
    QVERIFY(proto.property("toLocaleString").isFunction());
    {
        QScriptValue ret = eng.evaluate("new Number(123).toLocaleString()");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("123"));
    }
    QVERIFY(proto.property("valueOf").isFunction());
    {
        QScriptValue ret = eng.evaluate("new Number(123).valueOf()");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toNumber(), qsreal(123));
    }
    QVERIFY(proto.property("toExponential").isFunction());
    {
        QScriptValue ret = eng.evaluate("new Number(123).toExponential()");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("1.23e+2"));
    }
    QVERIFY(proto.property("toFixed").isFunction());
    {
        QScriptValue ret = eng.evaluate("new Number(123).toFixed()");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("123"));
    }
    QVERIFY(proto.property("toPrecision").isFunction());
    {
        QScriptValue ret = eng.evaluate("new Number(123).toPrecision()");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("123"));
    }
}

void tst_QScriptEngine::jsForInStatement_simple()
{
    QScriptEngine eng;
    {
        QScriptValue ret = eng.evaluate("o = { }; r = []; for (var p in o) r[r.length] = p; r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QVERIFY(lst.isEmpty());
    }
    {
        QScriptValue ret = eng.evaluate("o = { p: 123 }; r = [];"
                                        "for (var p in o) r[r.length] = p; r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), 1);
        QCOMPARE(lst.at(0), QString::fromLatin1("p"));
    }
    {
        QScriptValue ret = eng.evaluate("o = { p: 123, q: 456 }; r = [];"
                                        "for (var p in o) r[r.length] = p; r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), 2);
        QCOMPARE(lst.at(0), QString::fromLatin1("p"));
        QCOMPARE(lst.at(1), QString::fromLatin1("q"));
    }
}

void tst_QScriptEngine::jsForInStatement_prototypeProperties()
{
    QScriptEngine eng;
    {
        QScriptValue ret = eng.evaluate("o = { }; o.__proto__ = { p: 123 }; r = [];"
                                        "for (var p in o) r[r.length] = p; r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), 1);
        QCOMPARE(lst.at(0), QString::fromLatin1("p"));
    }
    {
        QScriptValue ret = eng.evaluate("o = { p: 123 }; o.__proto__ = { q: 456 }; r = [];"
                                        "for (var p in o) r[r.length] = p; r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), 2);
        QCOMPARE(lst.at(0), QString::fromLatin1("p"));
        QCOMPARE(lst.at(1), QString::fromLatin1("q"));
    }
    {
        // shadowed property
        QScriptValue ret = eng.evaluate("o = { p: 123 }; o.__proto__ = { p: 456 }; r = [];"
                                        "for (var p in o) r[r.length] = p; r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), 1);
        QCOMPARE(lst.at(0), QString::fromLatin1("p"));
    }

}

void tst_QScriptEngine::jsForInStatement_mutateWhileIterating()
{
    QScriptEngine eng;
    // deleting property during enumeration
    {
        QScriptValue ret = eng.evaluate("o = { p: 123 }; r = [];"
                                        "for (var p in o) { r[r.length] = p; delete r[p]; } r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), 1);
        QCOMPARE(lst.at(0), QString::fromLatin1("p"));
    }
    {
        QScriptValue ret = eng.evaluate("o = { p: 123, q: 456 }; r = [];"
                                        "for (var p in o) { r[r.length] = p; delete o.q; } r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), 1);
        QCOMPARE(lst.at(0), QString::fromLatin1("p"));
    }

    // adding property during enumeration
    {
        QScriptValue ret = eng.evaluate("o = { p: 123 }; r = [];"
                                        "for (var p in o) { r[r.length] = p; o.q = 456; } r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), 1);
        QCOMPARE(lst.at(0), QString::fromLatin1("p"));
    }

}

void tst_QScriptEngine::jsForInStatement_arrays()
{
    QScriptEngine eng;
    {
        QScriptValue ret = eng.evaluate("a = [123, 456]; r = [];"
                                        "for (var p in a) r[r.length] = p; r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), 2);
        QCOMPARE(lst.at(0), QString::fromLatin1("0"));
        QCOMPARE(lst.at(1), QString::fromLatin1("1"));
    }
    {
        QScriptValue ret = eng.evaluate("a = [123, 456]; a.foo = 'bar'; r = [];"
                                        "for (var p in a) r[r.length] = p; r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), 3);
        QCOMPARE(lst.at(0), QString::fromLatin1("0"));
        QCOMPARE(lst.at(1), QString::fromLatin1("1"));
        QCOMPARE(lst.at(2), QString::fromLatin1("foo"));
    }
    {
        QScriptValue ret = eng.evaluate("a = [123, 456]; a.foo = 'bar';"
                                        "b = [111, 222, 333]; b.bar = 'baz';"
                                        "a.__proto__ = b; r = [];"
                                        "for (var p in a) r[r.length] = p; r");
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), 5);
        QCOMPARE(lst.at(0), QString::fromLatin1("0"));
        QCOMPARE(lst.at(1), QString::fromLatin1("1"));
        QCOMPARE(lst.at(2), QString::fromLatin1("foo"));
        QCOMPARE(lst.at(3), QString::fromLatin1("2"));
        QCOMPARE(lst.at(4), QString::fromLatin1("bar"));
    }
}

void tst_QScriptEngine::jsForInStatement_nullAndUndefined()
{
    QScriptEngine eng;
    {
        QScriptValue ret = eng.evaluate("r = true; for (var p in undefined) r = false; r");
        QVERIFY(ret.isBool());
        QVERIFY(ret.toBool());
    }
    {
        QScriptValue ret = eng.evaluate("r = true; for (var p in null) r = false; r");
        QVERIFY(ret.isBool());
        QVERIFY(ret.toBool());
    }
}

void tst_QScriptEngine::jsFunctionDeclarationAsStatement()
{
    // ECMA-262 does not allow function declarations to be used as statements,
    // but several popular implementations (including JSC) do. See the NOTE
    // at the beginning of chapter 12 in ECMA-262 5th edition, where it's
    // recommended that implementations either disallow this usage or issue
    // a warning.
    // Since we had a bug report long ago about Qt Script not supporting this
    // "feature" (and thus deviating from other implementations), we still
    // check this behavior.

    QScriptEngine eng;
    QVERIFY(!eng.globalObject().property("bar").isValid());
    eng.evaluate("function foo(arg) {\n"
                 "  if (arg == 'bar')\n"
                 "    function bar() { return 'bar'; }\n"
                 "  else\n"
                 "    function baz() { return 'baz'; }\n"
                 "  return (arg == 'bar') ? bar : baz;\n"
                 "}");
    QVERIFY(!eng.globalObject().property("bar").isValid());
    QVERIFY(!eng.globalObject().property("baz").isValid());
    QVERIFY(eng.evaluate("foo").isFunction());
    {
        QScriptValue ret = eng.evaluate("foo('bar')");
        QVERIFY(ret.isFunction());
        QScriptValue ret2 = ret.call(QScriptValue());
        QCOMPARE(ret2.toString(), QString::fromLatin1("bar"));
        QVERIFY(!eng.globalObject().property("bar").isValid());
        QVERIFY(!eng.globalObject().property("baz").isValid());
    }
    {
        QScriptValue ret = eng.evaluate("foo('baz')");
        QVERIFY(ret.isFunction());
        QScriptValue ret2 = ret.call(QScriptValue());
        QCOMPARE(ret2.toString(), QString::fromLatin1("baz"));
        QVERIFY(!eng.globalObject().property("bar").isValid());
        QVERIFY(!eng.globalObject().property("baz").isValid());
    }
}

void tst_QScriptEngine::stringObjects()
{
    // See ECMA-262 Section 15.5, "String Objects".

    QScriptEngine eng;
    QString str("ciao");
    // in C++
    {
        QScriptValue obj = QScriptValue(&eng, str).toObject();
        QCOMPARE(obj.property("length").toInt32(), str.length());
        QCOMPARE(obj.propertyFlags("length"), QScriptValue::PropertyFlags(QScriptValue::Undeletable | QScriptValue::SkipInEnumeration | QScriptValue::ReadOnly));
        for (int i = 0; i < str.length(); ++i) {
            QString pname = QString::number(i);
            QVERIFY(obj.property(pname).isString());
            QCOMPARE(obj.property(pname).toString(), QString(str.at(i)));
            QCOMPARE(obj.propertyFlags(pname), QScriptValue::PropertyFlags(QScriptValue::Undeletable | QScriptValue::ReadOnly));
            obj.setProperty(pname, QScriptValue());
            obj.setProperty(pname, QScriptValue(&eng, 123));
            QVERIFY(obj.property(pname).isString());
            QCOMPARE(obj.property(pname).toString(), QString(str.at(i)));
        }
        QVERIFY(!obj.property("-1").isValid());
        QVERIFY(!obj.property(QString::number(str.length())).isValid());

        QScriptValue val(&eng, 123);
        obj.setProperty("-1", val);
        QVERIFY(obj.property("-1").strictlyEquals(val));
        obj.setProperty("100", val);
        QVERIFY(obj.property("100").strictlyEquals(val));
    }

    // in script
    {
        QScriptValue ret = eng.evaluate("s = new String('ciao'); r = []; for (var p in s) r.push(p); r");
        QVERIFY(ret.isArray());
        QStringList lst = qscriptvalue_cast<QStringList>(ret);
        QCOMPARE(lst.size(), str.length());
        for (int i = 0; i < str.length(); ++i)
            QCOMPARE(lst.at(i), QString::number(i));

        QScriptValue ret2 = eng.evaluate("s[0] = 123; s[0]");
        QVERIFY(ret2.isString());
        QCOMPARE(ret2.toString().length(), 1);
        QCOMPARE(ret2.toString().at(0), str.at(0));

        QScriptValue ret3 = eng.evaluate("s[-1] = 123; s[-1]");
        QVERIFY(ret3.isNumber());
        QCOMPARE(ret3.toInt32(), 123);

        QScriptValue ret4 = eng.evaluate("s[s.length] = 456; s[s.length]");
        QVERIFY(ret4.isNumber());
        QCOMPARE(ret4.toInt32(), 456);

        QScriptValue ret5 = eng.evaluate("delete s[0]");
        QVERIFY(ret5.isBoolean());
        QVERIFY(!ret5.toBoolean());

        QScriptValue ret6 = eng.evaluate("delete s[-1]");
        QVERIFY(ret6.isBoolean());
        QVERIFY(ret6.toBoolean());

        QScriptValue ret7 = eng.evaluate("delete s[s.length]");
        QVERIFY(ret7.isBoolean());
        QVERIFY(ret7.toBoolean());
    }
}

void tst_QScriptEngine::jsStringPrototypeReplaceBugs()
{
    QScriptEngine eng;
    // task 212440
    {
        QScriptValue ret = eng.evaluate("replace_args = []; \"a a a\".replace(/(a)/g, function() { replace_args.push(arguments); }); replace_args");
        QVERIFY(ret.isArray());
        int len = ret.property("length").toInt32();
        QCOMPARE(len, 3);
        for (int i = 0; i < len; ++i) {
            QScriptValue args = ret.property(i);
            QCOMPARE(args.property("length").toInt32(), 4);
            QCOMPARE(args.property(0).toString(), QString::fromLatin1("a")); // matched string
            QCOMPARE(args.property(1).toString(), QString::fromLatin1("a")); // capture
            QVERIFY(args.property(2).isNumber());
            QCOMPARE(args.property(2).toInt32(), i*2); // index of match
            QCOMPARE(args.property(3).toString(), QString::fromLatin1("a a a"));
        }
    }
    // task 212501
    {
        QScriptValue ret = eng.evaluate("\"foo\".replace(/a/g, function() {})");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("foo"));
    }
}

void tst_QScriptEngine::getterSetterThisObject_global()
{
    {
        QScriptEngine eng;
        // read
        eng.evaluate("__defineGetter__('x', function() { return this; });");
        {
            QScriptValue ret = eng.evaluate("x");
            QVERIFY(ret.equals(eng.globalObject()));
        }
        {
            QScriptValue ret = eng.evaluate("(function() { return x; })()");
            QVERIFY(ret.equals(eng.globalObject()));
        }
        {
            QScriptValue ret = eng.evaluate("with (this) x");
            QVERIFY(ret.equals(eng.globalObject()));
        }
        {
            QScriptValue ret = eng.evaluate("with ({}) x");
            QVERIFY(ret.equals(eng.globalObject()));
        }
        {
            QScriptValue ret = eng.evaluate("(function() { with ({}) return x; })()");
            QVERIFY(ret.equals(eng.globalObject()));
        }
        // write
        eng.evaluate("__defineSetter__('x', function() { return this; });");
        {
            QScriptValue ret = eng.evaluate("x = 'foo'");
            // SpiderMonkey says setter return value, JSC says RHS.
            QVERIFY(ret.isString());
            QCOMPARE(ret.toString(), QString::fromLatin1("foo"));
        }
        {
            QScriptValue ret = eng.evaluate("(function() { return x = 'foo'; })()");
            // SpiderMonkey says setter return value, JSC says RHS.
            QVERIFY(ret.isString());
            QCOMPARE(ret.toString(), QString::fromLatin1("foo"));
        }
        {
            QScriptValue ret = eng.evaluate("with (this) x = 'foo'");
            // SpiderMonkey says setter return value, JSC says RHS.
            QVERIFY(ret.isString());
            QCOMPARE(ret.toString(), QString::fromLatin1("foo"));
        }
        {
            QScriptValue ret = eng.evaluate("with ({}) x = 'foo'");
            // SpiderMonkey says setter return value, JSC says RHS.
            QVERIFY(ret.isString());
            QCOMPARE(ret.toString(), QString::fromLatin1("foo"));
        }
        {
            QScriptValue ret = eng.evaluate("(function() { with ({}) return x = 'foo'; })()");
            // SpiderMonkey says setter return value, JSC says RHS.
            QVERIFY(ret.isString());
            QCOMPARE(ret.toString(), QString::fromLatin1("foo"));
        }
    }
}

void tst_QScriptEngine::getterSetterThisObject_plain()
{
    {
        QScriptEngine eng;
        eng.evaluate("o = {}");
        // read
        eng.evaluate("o.__defineGetter__('x', function() { return this; })");
        QVERIFY(eng.evaluate("o.x === o").toBoolean());
        QVERIFY(eng.evaluate("with (o) x").equals(eng.evaluate("o")));
        QVERIFY(eng.evaluate("(function() { with (o) return x; })() === o").toBoolean());
        eng.evaluate("q = {}; with (o) with (q) x").equals(eng.evaluate("o"));
        // write
        eng.evaluate("o.__defineSetter__('x', function() { return this; });");
        // SpiderMonkey says setter return value, JSC says RHS.
        QVERIFY(eng.evaluate("(o.x = 'foo') === 'foo'").toBoolean());
        QVERIFY(eng.evaluate("with (o) x = 'foo'").equals("foo"));
        QVERIFY(eng.evaluate("with (o) with (q) x = 'foo'").equals("foo"));
    }
}

void tst_QScriptEngine::getterSetterThisObject_prototypeChain()
{
    {
        QScriptEngine eng;
        eng.evaluate("o = {}; p = {}; o.__proto__ = p");
        // read
        eng.evaluate("p.__defineGetter__('x', function() { return this; })");
        QVERIFY(eng.evaluate("o.x === o").toBoolean());
        QVERIFY(eng.evaluate("with (o) x").equals(eng.evaluate("o")));
        QVERIFY(eng.evaluate("(function() { with (o) return x; })() === o").toBoolean());
        eng.evaluate("q = {}; with (o) with (q) x").equals(eng.evaluate("o"));
        eng.evaluate("with (q) with (o) x").equals(eng.evaluate("o"));
        // write
        eng.evaluate("o.__defineSetter__('x', function() { return this; });");
        // SpiderMonkey says setter return value, JSC says RHS.
        QVERIFY(eng.evaluate("(o.x = 'foo') === 'foo'").toBoolean());
        QVERIFY(eng.evaluate("with (o) x = 'foo'").equals("foo"));
        QVERIFY(eng.evaluate("with (o) with (q) x = 'foo'").equals("foo"));
    }
}

void tst_QScriptEngine::getterSetterThisObject_activation()
{
    {
        QScriptEngine eng;
        QScriptContext *ctx = eng.pushContext();
        QVERIFY(ctx != 0);
        QScriptValue act = ctx->activationObject();
        act.setProperty("act", act);
        // read
        eng.evaluate("act.__defineGetter__('x', function() { return this; })");
        QVERIFY(eng.evaluate("x === act").toBoolean());
        QEXPECT_FAIL("", "QTBUG-17605: Not possible to implement local variables as getter/setter properties", Abort);
        QVERIFY(!eng.hasUncaughtException());
        QVERIFY(eng.evaluate("with (act) x").equals("foo"));
        QVERIFY(eng.evaluate("(function() { with (act) return x; })() === act").toBoolean());
        eng.evaluate("q = {}; with (act) with (q) x").equals(eng.evaluate("act"));
        eng.evaluate("with (q) with (act) x").equals(eng.evaluate("act"));
        // write
        eng.evaluate("act.__defineSetter__('x', function() { return this; });");
        QVERIFY(eng.evaluate("(x = 'foo') === 'foo'").toBoolean());
        QVERIFY(eng.evaluate("with (act) x = 'foo'").equals("foo"));
        QVERIFY(eng.evaluate("with (act) with (q) x = 'foo'").equals("foo"));
        eng.popContext();
    }
}

void tst_QScriptEngine::jsContinueInSwitch()
{
    // This is testing ECMA-262 compliance, not C++ API.

    QScriptEngine eng;
    // switch - continue
    {
        QScriptValue ret = eng.evaluate("switch (1) { default: continue; }");
        QVERIFY(ret.isError());
    }
    // for - switch - case - continue
    {
        QScriptValue ret = eng.evaluate("j = 0; for (i = 0; i < 100000; ++i) {\n"
                                        "  switch (i) {\n"
                                        "    case 1: ++j; continue;\n"
                                        "    case 100: ++j; continue;\n"
                                        "    case 1000: ++j; continue;\n"
                                        "  }\n"
                                        "}; j");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 3);
    }
    // for - switch - case - default - continue
    {
        QScriptValue ret = eng.evaluate("j = 0; for (i = 0; i < 100000; ++i) {\n"
                                        "  switch (i) {\n"
                                        "    case 1: ++j; continue;\n"
                                        "    case 100: ++j; continue;\n"
                                        "    case 1000: ++j; continue;\n"
                                        "    default: if (i < 50000) break; else continue;\n"
                                        "  }\n"
                                        "}; j");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 3);
    }
    // switch - for - continue
    {
        QScriptValue ret = eng.evaluate("j = 123; switch (j) {\n"
                                        "  case 123:\n"
                                        "  for (i = 0; i < 100000; ++i) {\n"
                                        "    continue;\n"
                                        "  }\n"
                                        "}; i\n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 100000);
    }
    // switch - switch - continue
    {
        QScriptValue ret = eng.evaluate("i = 1; switch (i) { default: switch (i) { case 1: continue; } }");
        QVERIFY(ret.isError());
    }
    // for - switch - switch - continue
    {
        QScriptValue ret = eng.evaluate("j = 0; for (i = 0; i < 100000; ++i) {\n"
                                        "  switch (i) {\n"
                                        "    case 1:\n"
                                        "    switch (i) {\n"
                                        "      case 1: ++j; continue;\n"
                                        "    }\n"
                                        "  }\n"
                                        "}; j");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 1);
    }
    // switch - for - switch - continue
    {
        QScriptValue ret = eng.evaluate("j = 123; switch (j) {\n"
                                        "  case 123:\n"
                                        "  for (i = 0; i < 100000; ++i) {\n"
                                        "    switch (i) {\n"
                                        "      case 1:\n"
                                        "      ++j; continue;\n"
                                        "    }\n"
                                        "  }\n"
                                        "}; i\n");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 100000);
    }
}

void tst_QScriptEngine::jsShadowReadOnlyPrototypeProperty()
{
    // SpiderMonkey has different behavior than JSC and V8; it disallows
    // creating a property on the instance if there's a property with the
    // same name in the prototype, and that property is read-only. We
    // adopted that behavior in the old (4.5) Qt Script back-end, but it
    // just seems weird -- and non-compliant. Adopt the JSC behavior instead.
    QScriptEngine eng;
    QVERIFY(eng.evaluate("o = {}; o.__proto__ = parseInt; o.length").isNumber());
    QCOMPARE(eng.evaluate("o.length = 123; o.length").toInt32(), 123);
    QVERIFY(eng.evaluate("o.hasOwnProperty('length')").toBoolean());
}

void tst_QScriptEngine::toObject()
{
    QScriptEngine eng;

    QVERIFY(!eng.toObject(eng.undefinedValue()).isValid());

    QVERIFY(!eng.toObject(eng.nullValue()).isValid());

    QScriptValue falskt(false);
    {
        QScriptValue tmp = eng.toObject(falskt);
        QVERIFY(tmp.isObject());
        QCOMPARE(tmp.toNumber(), falskt.toNumber());
    }
    QVERIFY(falskt.isBool());

    QScriptValue sant(true);
    {
        QScriptValue tmp = eng.toObject(sant);
        QVERIFY(tmp.isObject());
        QCOMPARE(tmp.toNumber(), sant.toNumber());
    }
    QVERIFY(sant.isBool());

    QScriptValue number(123.0);
    {
        QScriptValue tmp = eng.toObject(number);
        QVERIFY(tmp.isObject());
        QCOMPARE(tmp.toNumber(), number.toNumber());
    }
    QVERIFY(number.isNumber());

    QScriptValue str = QScriptValue(&eng, QString("ciao"));
    {
        QScriptValue tmp = eng.toObject(str);
        QVERIFY(tmp.isObject());
        QCOMPARE(tmp.toString(), str.toString());
    }
    QVERIFY(str.isString());

    QScriptValue object = eng.newObject();
    {
        QScriptValue tmp = eng.toObject(object);
        QVERIFY(tmp.isObject());
        QVERIFY(tmp.strictlyEquals(object));
    }

    QScriptValue qobject = eng.newQObject(this);
    QVERIFY(eng.toObject(qobject).strictlyEquals(qobject));

    QVERIFY(!eng.toObject(QScriptValue()).isValid());

    // v1 constructors

    QScriptValue boolValue(&eng, true);
    {
        QScriptValue ret = eng.toObject(boolValue);
        QVERIFY(ret.isObject());
        QCOMPARE(ret.toBool(), boolValue.toBool());
    }
    QVERIFY(boolValue.isBool());

    QScriptValue numberValue(&eng, 123.0);
    {
        QScriptValue ret = eng.toObject(numberValue);
        QVERIFY(ret.isObject());
        QCOMPARE(ret.toNumber(), numberValue.toNumber());
    }
    QVERIFY(numberValue.isNumber());

    QScriptValue stringValue(&eng, QString::fromLatin1("foo"));
    {
        QScriptValue ret = eng.toObject(stringValue);
        QVERIFY(ret.isObject());
        QCOMPARE(ret.toString(), stringValue.toString());
    }
    QVERIFY(stringValue.isString());
}

void tst_QScriptEngine::jsReservedWords_data()
{
    QTest::addColumn<QString>("word");
    QTest::newRow("break") << QString("break");
    QTest::newRow("case") << QString("case");
    QTest::newRow("catch") << QString("catch");
    QTest::newRow("continue") << QString("continue");
    QTest::newRow("default") << QString("default");
    QTest::newRow("delete") << QString("delete");
    QTest::newRow("do") << QString("do");
    QTest::newRow("else") << QString("else");
    QTest::newRow("false") << QString("false");
    QTest::newRow("finally") << QString("finally");
    QTest::newRow("for") << QString("for");
    QTest::newRow("function") << QString("function");
    QTest::newRow("if") << QString("if");
    QTest::newRow("in") << QString("in");
    QTest::newRow("instanceof") << QString("instanceof");
    QTest::newRow("new") << QString("new");
    QTest::newRow("null") << QString("null");
    QTest::newRow("return") << QString("return");
    QTest::newRow("switch") << QString("switch");
    QTest::newRow("this") << QString("this");
    QTest::newRow("throw") << QString("throw");
    QTest::newRow("true") << QString("true");
    QTest::newRow("try") << QString("try");
    QTest::newRow("typeof") << QString("typeof");
    QTest::newRow("var") << QString("var");
    QTest::newRow("void") << QString("void");
    QTest::newRow("while") << QString("while");
    QTest::newRow("with") << QString("with");
}

void tst_QScriptEngine::jsReservedWords()
{
    // See ECMA-262 Section 7.6.1, "Reserved Words".
    // We prefer that the implementation is less strict than the spec; e.g.
    // it's good to allow reserved words as identifiers in object literals,
    // and when accessing properties using dot notation.

    QFETCH(QString, word);
    {
        QScriptEngine eng;
        QScriptValue ret = eng.evaluate(word + " = 123");
        QVERIFY(ret.isError());
        QString str = ret.toString();
        QVERIFY(str.startsWith("SyntaxError") || str.startsWith("ReferenceError"));
    }
    {
        QScriptEngine eng;
        QScriptValue ret = eng.evaluate("var " + word + " = 123");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().startsWith("SyntaxError"));
    }
    {
        QScriptEngine eng;
        QScriptValue ret = eng.evaluate("o = {}; o." + word + " = 123");
        // in the old back-end and in SpiderMonkey this is allowed, but not in JSC
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().startsWith("SyntaxError"));
    }
    {
        QScriptEngine eng;
        QScriptValue ret = eng.evaluate("o = { " + word + ": 123 }");
        // in the old back-end and in SpiderMonkey this is allowed, but not in JSC
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().startsWith("SyntaxError"));
    }
    {
        // SpiderMonkey allows this, but we don't
        QScriptEngine eng;
        QScriptValue ret = eng.evaluate("function " + word + "() {}");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().startsWith("SyntaxError"));
    }
}

void tst_QScriptEngine::jsFutureReservedWords_data()
{
    QTest::addColumn<QString>("word");
    QTest::addColumn<bool>("allowed");
    QTest::newRow("abstract") << QString("abstract") << true;
    QTest::newRow("boolean") << QString("boolean") << true;
    QTest::newRow("byte") << QString("byte") << true;
    QTest::newRow("char") << QString("char") << true;
    QTest::newRow("class") << QString("class") << false;
    QTest::newRow("const") << QString("const") << false;
    QTest::newRow("debugger") << QString("debugger") << false;
    QTest::newRow("double") << QString("double") << true;
    QTest::newRow("enum") << QString("enum") << false;
    QTest::newRow("export") << QString("export") << false;
    QTest::newRow("extends") << QString("extends") << false;
    QTest::newRow("final") << QString("final") << true;
    QTest::newRow("float") << QString("float") << true;
    QTest::newRow("goto") << QString("goto") << true;
    QTest::newRow("implements") << QString("implements") << true;
    QTest::newRow("import") << QString("import") << false;
    QTest::newRow("int") << QString("int") << true;
    QTest::newRow("interface") << QString("interface") << true;
    QTest::newRow("long") << QString("long") << true;
    QTest::newRow("native") << QString("native") << true;
    QTest::newRow("package") << QString("package") << true;
    QTest::newRow("private") << QString("private") << true;
    QTest::newRow("protected") << QString("protected") << true;
    QTest::newRow("public") << QString("public") << true;
    QTest::newRow("short") << QString("short") << true;
    QTest::newRow("static") << QString("static") << true;
    QTest::newRow("super") << QString("super") << false;
    QTest::newRow("synchronized") << QString("synchronized") << true;
    QTest::newRow("throws") << QString("throws") << true;
    QTest::newRow("transient") << QString("transient") << true;
    QTest::newRow("volatile") << QString("volatile") << true;
}

void tst_QScriptEngine::jsFutureReservedWords()
{
    // See ECMA-262 Section 7.6.1.2, "Future Reserved Words".
    // In real-world implementations, most of these words are
    // actually allowed as normal identifiers.

    QFETCH(QString, word);
    QFETCH(bool, allowed);
    {
        QScriptEngine eng;
        QScriptValue ret = eng.evaluate(word + " = 123");
        QCOMPARE(!ret.isError(), allowed);
    }
    {
        QScriptEngine eng;
        QScriptValue ret = eng.evaluate("var " + word + " = 123");
        QCOMPARE(!ret.isError(), allowed);
    }
    {
        // this should probably be allowed (see task 162567)
        QScriptEngine eng;
        QScriptValue ret = eng.evaluate("o = {}; o." + word + " = 123");
        QCOMPARE(ret.isNumber(), allowed);
        QCOMPARE(!ret.isError(), allowed);
    }
    {
        // this should probably be allowed (see task 162567)
        QScriptEngine eng;
        QScriptValue ret = eng.evaluate("o = { " + word + ": 123 }");
        QCOMPARE(!ret.isError(), allowed);
    }
}

void tst_QScriptEngine::jsThrowInsideWithStatement()
{
    // This is testing ECMA-262 compliance, not C++ API.

    // task 209988
    QScriptEngine eng;
    {
        QScriptValue ret = eng.evaluate(
            "try {"
            "  o = { bad : \"bug\" };"
            "  with (o) {"
            "    throw 123;"
            "  }"
            "} catch (e) {"
            "  bad;"
            "}");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("ReferenceError")));
    }
    {
        QScriptValue ret = eng.evaluate(
            "try {"
            "  o = { bad : \"bug\" };"
            "  with (o) {"
            "    throw 123;"
            "  }"
            "} finally {"
            "  bad;"
            "}");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("ReferenceError")));
    }
    {
        eng.clearExceptions();
        QScriptValue ret = eng.evaluate(
            "o = { bug : \"no bug\" };"
            "with (o) {"
            "  try {"
            "    throw 123;"
            "  } finally {"
            "    bug;"
            "  }"
            "}");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 123);
        QVERIFY(eng.hasUncaughtException());
    }
    {
        eng.clearExceptions();
        QScriptValue ret = eng.evaluate(
            "o = { bug : \"no bug\" };"
            "with (o) {"
            "    throw 123;"
            "}");
        QVERIFY(ret.isNumber());
        QScriptValue ret2 = eng.evaluate("bug");
        QVERIFY(ret2.isError());
        QVERIFY(ret2.toString().contains(QString::fromLatin1("ReferenceError")));
    }
}

class TestAgent : public QScriptEngineAgent
{
public:
    TestAgent(QScriptEngine *engine) : QScriptEngineAgent(engine) {}
};

void tst_QScriptEngine::getSetAgent_ownership()
{
    // engine deleted before agent --> agent deleted too
    QScriptEngine *eng = new QScriptEngine;
    QCOMPARE(eng->agent(), (QScriptEngineAgent*)0);
    TestAgent *agent = new TestAgent(eng);
    eng->setAgent(agent);
    QCOMPARE(eng->agent(), (QScriptEngineAgent*)agent);
    eng->setAgent(0); // the engine maintains ownership of the old agent
    QCOMPARE(eng->agent(), (QScriptEngineAgent*)0);
    delete eng;
}

void tst_QScriptEngine::getSetAgent_deleteAgent()
{
    // agent deleted before engine --> engine's agent should become 0
    QScriptEngine *eng = new QScriptEngine;
    TestAgent *agent = new TestAgent(eng);
    eng->setAgent(agent);
    QCOMPARE(eng->agent(), (QScriptEngineAgent*)agent);
    delete agent;
    QCOMPARE(eng->agent(), (QScriptEngineAgent*)0);
    eng->evaluate("(function(){ return 123; })()");
    delete eng;
}

void tst_QScriptEngine::getSetAgent_differentEngine()
{
    QScriptEngine eng;
    QScriptEngine eng2;
    TestAgent *agent = new TestAgent(&eng);
    QTest::ignoreMessage(QtWarningMsg, "QScriptEngine::setAgent(): cannot set agent belonging to different engine");
    eng2.setAgent(agent);
    QCOMPARE(eng2.agent(), (QScriptEngineAgent*)0);
}

void tst_QScriptEngine::reentrancy_stringHandles()
{
    QScriptEngine eng1;
    QScriptEngine eng2;
    QScriptString s1 = eng1.toStringHandle("foo");
    QScriptString s2 = eng2.toStringHandle("foo");
    QVERIFY(s1 != s2);
}

void tst_QScriptEngine::reentrancy_processEventsInterval()
{
    QScriptEngine eng1;
    QScriptEngine eng2;
    eng1.setProcessEventsInterval(123);
    QCOMPARE(eng2.processEventsInterval(), -1);
    eng2.setProcessEventsInterval(456);
    QCOMPARE(eng1.processEventsInterval(), 123);
}

void tst_QScriptEngine::reentrancy_typeConversion()
{
    QScriptEngine eng1;
    QScriptEngine eng2;
    qScriptRegisterMetaType<Foo>(&eng1, fooToScriptValue, fooFromScriptValue);
    Foo foo;
    foo.x = 12;
    foo.y = 34;
    {
        QScriptValue fooVal = qScriptValueFromValue(&eng1, foo);
        QVERIFY(fooVal.isObject());
        QVERIFY(!fooVal.isVariant());
        QCOMPARE(fooVal.property("x").toInt32(), 12);
        QCOMPARE(fooVal.property("y").toInt32(), 34);
        fooVal.setProperty("x", 56);
        fooVal.setProperty("y", 78);

        Foo foo2 = qScriptValueToValue<Foo>(fooVal);
        QCOMPARE(foo2.x, 56);
        QCOMPARE(foo2.y, 78);
    }
    {
        QScriptValue fooVal = qScriptValueFromValue(&eng2, foo);
        QVERIFY(fooVal.isVariant());

        Foo foo2 = qScriptValueToValue<Foo>(fooVal);
        QCOMPARE(foo2.x, 12);
        QCOMPARE(foo2.y, 34);
    }
    QVERIFY(!eng1.defaultPrototype(qMetaTypeId<Foo>()).isValid());
    QVERIFY(!eng2.defaultPrototype(qMetaTypeId<Foo>()).isValid());
    QScriptValue proto1 = eng1.newObject();
    eng1.setDefaultPrototype(qMetaTypeId<Foo>(), proto1);
    QVERIFY(!eng2.defaultPrototype(qMetaTypeId<Foo>()).isValid());
    QScriptValue proto2 = eng2.newObject();
    eng2.setDefaultPrototype(qMetaTypeId<Foo>(), proto2);
    QVERIFY(eng2.defaultPrototype(qMetaTypeId<Foo>()).isValid());
    QVERIFY(eng1.defaultPrototype(qMetaTypeId<Foo>()).strictlyEquals(proto1));
}

void tst_QScriptEngine::reentrancy_globalObjectProperties()
{
    QScriptEngine eng1;
    QScriptEngine eng2;
    QVERIFY(!eng2.globalObject().property("a").isValid());
    eng1.evaluate("a = 10");
    QVERIFY(eng1.globalObject().property("a").isNumber());
    QVERIFY(!eng2.globalObject().property("a").isValid());
    eng2.evaluate("a = 20");
    QVERIFY(eng2.globalObject().property("a").isNumber());
    QCOMPARE(eng1.globalObject().property("a").toInt32(), 10);
}

void tst_QScriptEngine::reentrancy_Array()
{
    // weird bug with JSC backend
    {
        QScriptEngine eng;
        QCOMPARE(eng.evaluate("Array()").toString(), QString());
        eng.evaluate("Array.prototype.toString");
        QCOMPARE(eng.evaluate("Array()").toString(), QString());
    }
    {
        QScriptEngine eng;
        QCOMPARE(eng.evaluate("Array()").toString(), QString());
    }
}

void tst_QScriptEngine::reentrancy_objectCreation()
{
    QScriptEngine eng1;
    QScriptEngine eng2;
    {
        QScriptValue d1 = eng1.newDate(0);
        QScriptValue d2 = eng2.newDate(0);
        QCOMPARE(d1.toDateTime(), d2.toDateTime());
        QCOMPARE(d2.toDateTime(), d1.toDateTime());
    }
    {
        QScriptValue r1 = eng1.newRegExp("foo", "gim");
        QScriptValue r2 = eng2.newRegExp("foo", "gim");
        QCOMPARE(r1.toRegExp(), r2.toRegExp());
        QCOMPARE(r2.toRegExp(), r1.toRegExp());
    }
    {
        QScriptValue o1 = eng1.newQObject(this);
        QScriptValue o2 = eng2.newQObject(this);
        QCOMPARE(o1.toQObject(), o2.toQObject());
        QCOMPARE(o2.toQObject(), o1.toQObject());
    }
    {
        QScriptValue mo1 = eng1.newQMetaObject(&staticMetaObject);
        QScriptValue mo2 = eng2.newQMetaObject(&staticMetaObject);
        QCOMPARE(mo1.toQMetaObject(), mo2.toQMetaObject());
        QCOMPARE(mo2.toQMetaObject(), mo1.toQMetaObject());
    }
}

void tst_QScriptEngine::jsIncDecNonObjectProperty()
{
    // This is testing ECMA-262 compliance, not C++ API.

    QScriptEngine eng;
    {
        QScriptValue ret = eng.evaluate("var a; a.n++");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("TypeError")));
    }
    {
        QScriptValue ret = eng.evaluate("var a; a.n--");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("TypeError")));
    }
    {
        QScriptValue ret = eng.evaluate("var a = null; a.n++");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("TypeError")));
    }
    {
        QScriptValue ret = eng.evaluate("var a = null; a.n--");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("TypeError")));
    }
    {
        QScriptValue ret = eng.evaluate("var a; ++a.n");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("TypeError")));
    }
    {
        QScriptValue ret = eng.evaluate("var a; --a.n");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("TypeError")));
    }
    {
        QScriptValue ret = eng.evaluate("var a; a.n += 1");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("TypeError")));
    }
    {
        QScriptValue ret = eng.evaluate("var a; a.n -= 1");
        QVERIFY(ret.isError());
        QVERIFY(ret.toString().contains(QString::fromLatin1("TypeError")));
    }
    {
        QScriptValue ret = eng.evaluate("var a = 'ciao'; a.length++");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 4);
    }
    {
        QScriptValue ret = eng.evaluate("var a = 'ciao'; a.length--");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 4);
    }
    {
        QScriptValue ret = eng.evaluate("var a = 'ciao'; ++a.length");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 5);
    }
    {
        QScriptValue ret = eng.evaluate("var a = 'ciao'; --a.length");
        QVERIFY(ret.isNumber());
        QCOMPARE(ret.toInt32(), 3);
    }
}

void tst_QScriptEngine::installTranslatorFunctions_data()
{
    QTest::addColumn<bool>("useCustomGlobalObject");

    QTest::newRow("Default global object") << false;
    QTest::newRow("Custom global object") << true;
}

void tst_QScriptEngine::installTranslatorFunctions()
{
    QFETCH(bool, useCustomGlobalObject);

    QScriptEngine eng;
    QScriptValue globalOrig = eng.globalObject();
    QScriptValue global;
    if (useCustomGlobalObject) {
        global = eng.newObject();
        eng.setGlobalObject(global);
    } else {
        global = globalOrig;
    }
    QVERIFY(!global.property("qsTranslate").isValid());
    QVERIFY(!global.property("QT_TRANSLATE_NOOP").isValid());
    QVERIFY(!global.property("qsTr").isValid());
    QVERIFY(!global.property("QT_TR_NOOP").isValid());
    QVERIFY(!global.property("qsTrId").isValid());
    QVERIFY(!global.property("QT_TRID_NOOP").isValid());
    QVERIFY(!globalOrig.property("String").property("prototype").property("arg").isValid());

    eng.installTranslatorFunctions();
    QVERIFY(global.property("qsTranslate").isFunction());
    QVERIFY(global.property("QT_TRANSLATE_NOOP").isFunction());
    QVERIFY(global.property("qsTr").isFunction());
    QVERIFY(global.property("QT_TR_NOOP").isFunction());
    QVERIFY(global.property("qsTrId").isFunction());
    QVERIFY(global.property("QT_TRID_NOOP").isFunction());
    QVERIFY(globalOrig.property("String").property("prototype").property("arg").isFunction());

    if (useCustomGlobalObject) {
        QVERIFY(!globalOrig.property("qsTranslate").isValid());
        QVERIFY(!globalOrig.property("QT_TRANSLATE_NOOP").isValid());
        QVERIFY(!globalOrig.property("qsTr").isValid());
        QVERIFY(!globalOrig.property("QT_TR_NOOP").isValid());
        QVERIFY(!globalOrig.property("qsTrId").isValid());
        QVERIFY(!globalOrig.property("QT_TRID_NOOP").isValid());
    }

    {
        QScriptValue ret = eng.evaluate("qsTr('foo')");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("foo"));
    }
    {
        QScriptValue ret = eng.evaluate("qsTranslate('foo', 'bar')");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("bar"));
    }
    {
        QScriptValue ret = eng.evaluate("QT_TR_NOOP('foo')");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("foo"));
    }
    {
        QScriptValue ret = eng.evaluate("QT_TRANSLATE_NOOP('foo', 'bar')");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("bar"));
    }
    {
        QScriptValue ret = eng.evaluate("'foo%0'.arg('bar')");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("foobar"));
    }
    {
        QScriptValue ret = eng.evaluate("'foo%0'.arg(123)");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("foo123"));
    }
    {
        // Maybe this should throw an error?
        QScriptValue ret = eng.evaluate("'foo%0'.arg()");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString());
    }

    {
        QScriptValue ret = eng.evaluate("qsTrId('foo')");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("foo"));
    }
    {
        QScriptValue ret = eng.evaluate("QT_TRID_NOOP('foo')");
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("foo"));
    }
    QVERIFY(eng.evaluate("QT_TRID_NOOP()").isUndefined());
}

class TranslationScope
{
public:
    TranslationScope(const QString &fileName)
    {
        translator.load(fileName);
        QCoreApplication::instance()->installTranslator(&translator);
    }
    ~TranslationScope()
    {
        QCoreApplication::instance()->removeTranslator(&translator);
    }

private:
    QTranslator translator;
};

void tst_QScriptEngine::translateScript_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("expectedTranslation");

    QString fileName = QString::fromLatin1("translatable.js");
    // Top-level
    QTest::newRow("qsTr('One')@translatable.js")
            << QString::fromLatin1("qsTr('One')") << fileName << QString::fromLatin1("En");
    QTest::newRow("qsTr('Hello')@translatable.js")
            << QString::fromLatin1("qsTr('Hello')") << fileName << QString::fromLatin1("Hallo");
    // From function
    QTest::newRow("(function() { return qsTr('One'); })()@translatable.js")
            << QString::fromLatin1("(function() { return qsTr('One'); })()") << fileName << QString::fromLatin1("En");
    QTest::newRow("(function() { return qsTr('Hello'); })()@translatable.js")
            << QString::fromLatin1("(function() { return qsTr('Hello'); })()") << fileName << QString::fromLatin1("Hallo");
    // From eval
    QTest::newRow("eval('qsTr(\\'One\\')')@translatable.js")
            << QString::fromLatin1("eval('qsTr(\\'One\\')')") << fileName << QString::fromLatin1("En");
    QTest::newRow("eval('qsTr(\\'Hello\\')')@translatable.js")
            << QString::fromLatin1("eval('qsTr(\\'Hello\\')')") << fileName << QString::fromLatin1("Hallo");
    // Plural
    QTest::newRow("qsTr('%n message(s) saved', '', 1)@translatable.js")
            << QString::fromLatin1("qsTr('%n message(s) saved', '', 1)") << fileName << QString::fromLatin1("1 melding lagret");
    QTest::newRow("qsTr('%n message(s) saved', '', 3).arg@translatable.js")
            << QString::fromLatin1("qsTr('%n message(s) saved', '', 3)") << fileName << QString::fromLatin1("3 meldinger lagret");

    // Top-level
    QTest::newRow("qsTranslate('FooContext', 'Two')@translatable.js")
            << QString::fromLatin1("qsTranslate('FooContext', 'Two')") << fileName << QString::fromLatin1("To");
    QTest::newRow("qsTranslate('FooContext', 'Goodbye')@translatable.js")
            << QString::fromLatin1("qsTranslate('FooContext', 'Goodbye')") << fileName << QString::fromLatin1("Farvel");
    // From eval
    QTest::newRow("eval('qsTranslate(\\'FooContext\\', \\'Two\\')')@translatable.js")
            << QString::fromLatin1("eval('qsTranslate(\\'FooContext\\', \\'Two\\')')") << fileName << QString::fromLatin1("To");
    QTest::newRow("eval('qsTranslate(\\'FooContext\\', \\'Goodbye\\')')@translatable.js")
            << QString::fromLatin1("eval('qsTranslate(\\'FooContext\\', \\'Goodbye\\')')") << fileName << QString::fromLatin1("Farvel");

    QTest::newRow("qsTranslate('FooContext', 'Goodbye', '')@translatable.js")
            << QString::fromLatin1("qsTranslate('FooContext', 'Goodbye', '')") << fileName << QString::fromLatin1("Farvel");

    QTest::newRow("qsTranslate('FooContext', 'Goodbye', '', 42)@translatable.js")
            << QString::fromLatin1("qsTranslate('FooContext', 'Goodbye', '', 42)") << fileName << QString::fromLatin1("Goodbye");

    QTest::newRow("qsTr('One', 'not the same one')@translatable.js")
            << QString::fromLatin1("qsTr('One', 'not the same one')") << fileName << QString::fromLatin1("Enda en");

    QTest::newRow("qsTr('One', 'not the same one', 42)@translatable.js")
            << QString::fromLatin1("qsTr('One', 'not the same one', 42)") << fileName << QString::fromLatin1("One");

    // Plural
    QTest::newRow("qsTranslate('FooContext', '%n fooish bar(s) found', '', 1)@translatable.js")
            << QString::fromLatin1("qsTranslate('FooContext', '%n fooish bar(s) found', '', 1)") << fileName << QString::fromLatin1("1 fooaktig bar funnet");
    QTest::newRow("qsTranslate('FooContext', '%n fooish bar(s) found', '', 2)@translatable.js")
            << QString::fromLatin1("qsTranslate('FooContext', '%n fooish bar(s) found', '', 2)") << fileName << QString::fromLatin1("2 fooaktige barer funnet");

    // Don't exist in translation
    QTest::newRow("qsTr('Three')@translatable.js")
            << QString::fromLatin1("qsTr('Three')") << fileName << QString::fromLatin1("Three");
    QTest::newRow("qsTranslate('FooContext', 'So long')@translatable.js")
            << QString::fromLatin1("qsTranslate('FooContext', 'So long')") << fileName << QString::fromLatin1("So long");
    QTest::newRow("qsTranslate('BarContext', 'Goodbye')@translatable.js")
            << QString::fromLatin1("qsTranslate('BarContext', 'Goodbye')") << fileName << QString::fromLatin1("Goodbye");

    // Translate strings from the second script (translatable2.js)

    QString fileName2 = QString::fromLatin1("translatable2.js");
    QTest::newRow("qsTr('Three')@translatable2.js")
            << QString::fromLatin1("qsTr('Three')") << fileName2 << QString::fromLatin1("Tre");
    QTest::newRow("qsTr('Happy birthday!')@translatable2.js")
            << QString::fromLatin1("qsTr('Happy birthday!')") << fileName2 << QString::fromLatin1("Gratulerer med dagen!");

    // Not translated because translation is only in translatable.js
    QTest::newRow("qsTr('One')@translatable2.js")
            << QString::fromLatin1("qsTr('One')") << fileName2 << QString::fromLatin1("One");
    QTest::newRow("(function() { return qsTr('One'); })()@translatable2.js")
            << QString::fromLatin1("(function() { return qsTr('One'); })()") << fileName2 << QString::fromLatin1("One");

    // For qsTranslate() the filename shouldn't matter
    QTest::newRow("qsTranslate('FooContext', 'Two')@translatable2.js")
            << QString::fromLatin1("qsTranslate('FooContext', 'Two')") << fileName2 << QString::fromLatin1("To");
    QTest::newRow("qsTranslate('BarContext', 'Congratulations!')@translatable.js")
            << QString::fromLatin1("qsTranslate('BarContext', 'Congratulations!')") << fileName << QString::fromLatin1("Gratulerer!");
}

void tst_QScriptEngine::translateScript()
{
    QFETCH(QString, expression);
    QFETCH(QString, fileName);
    QFETCH(QString, expectedTranslation);

    QScriptEngine engine;

    TranslationScope tranScope(":/translations/translatable_la");
    engine.installTranslatorFunctions();

    QCOMPARE(engine.evaluate(expression, fileName).toString(), expectedTranslation);
    QVERIFY(!engine.hasUncaughtException());
}

void tst_QScriptEngine::translateScript_crossScript()
{
    QScriptEngine engine;
    TranslationScope tranScope(":/translations/translatable_la");
    engine.installTranslatorFunctions();

    QString fileName = QString::fromLatin1("translatable.js");
    QString fileName2 = QString::fromLatin1("translatable2.js");
    // qsTr() should use the innermost filename as context
    engine.evaluate("function foo(s) { return bar(s); }", fileName);
    engine.evaluate("function bar(s) { return qsTr(s); }", fileName2);
    QCOMPARE(engine.evaluate("bar('Three')", fileName2).toString(), QString::fromLatin1("Tre"));
    QCOMPARE(engine.evaluate("bar('Three')", fileName).toString(), QString::fromLatin1("Tre"));
    QCOMPARE(engine.evaluate("bar('One')", fileName2).toString(), QString::fromLatin1("One"));

    engine.evaluate("function foo(s) { return bar(s); }", fileName2);
    engine.evaluate("function bar(s) { return qsTr(s); }", fileName);
    QCOMPARE(engine.evaluate("bar('Three')", fileName2).toString(), QString::fromLatin1("Three"));
    QCOMPARE(engine.evaluate("bar('One')", fileName).toString(), QString::fromLatin1("En"));
    QCOMPARE(engine.evaluate("bar('One')", fileName2).toString(), QString::fromLatin1("En"));
}

static QScriptValue callQsTr(QScriptContext *ctx, QScriptEngine *eng)
{
    return eng->globalObject().property("qsTr").call(ctx->thisObject(), ctx->argumentsObject());
}

void tst_QScriptEngine::translateScript_callQsTrFromNative()
{
    QScriptEngine engine;
    TranslationScope tranScope(":/translations/translatable_la");
    engine.installTranslatorFunctions();

    QString fileName = QString::fromLatin1("translatable.js");
    QString fileName2 = QString::fromLatin1("translatable2.js");
    // Calling qsTr() from a native function
    engine.globalObject().setProperty("qsTrProxy", engine.newFunction(callQsTr));
    QCOMPARE(engine.evaluate("qsTrProxy('One')", fileName).toString(), QString::fromLatin1("En"));
    QCOMPARE(engine.evaluate("qsTrProxy('One')", fileName2).toString(), QString::fromLatin1("One"));
    QCOMPARE(engine.evaluate("qsTrProxy('Three')", fileName).toString(), QString::fromLatin1("Three"));
    QCOMPARE(engine.evaluate("qsTrProxy('Three')", fileName2).toString(), QString::fromLatin1("Tre"));
}

void tst_QScriptEngine::translateScript_trNoOp()
{
    QScriptEngine engine;
    TranslationScope tranScope(":/translations/translatable_la");
    engine.installTranslatorFunctions();

    QVERIFY(engine.evaluate("QT_TR_NOOP()").isUndefined());
    QCOMPARE(engine.evaluate("QT_TR_NOOP('One')").toString(), QString::fromLatin1("One"));

    QVERIFY(engine.evaluate("QT_TRANSLATE_NOOP()").isUndefined());
    QVERIFY(engine.evaluate("QT_TRANSLATE_NOOP('FooContext')").isUndefined());
    QCOMPARE(engine.evaluate("QT_TRANSLATE_NOOP('FooContext', 'Two')").toString(), QString::fromLatin1("Two"));
}

void tst_QScriptEngine::translateScript_callQsTrFromCpp()
{
    QScriptEngine engine;
    TranslationScope tranScope(":/translations/translatable_la");
    engine.installTranslatorFunctions();

    // There is no context, but it shouldn't crash
    QCOMPARE(engine.globalObject().property("qsTr").call(
             QScriptValue(), QScriptValueList() << "One").toString(), QString::fromLatin1("One"));
}

void tst_QScriptEngine::translateWithInvalidArgs_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QString>("expectedError");

    QTest::newRow("qsTr()")  << "qsTr()" << "Error: qsTr() requires at least one argument";
    QTest::newRow("qsTr(123)")  << "qsTr(123)" << "Error: qsTr(): first argument (text) must be a string";
    QTest::newRow("qsTr('foo', 123)")  << "qsTr('foo', 123)" << "Error: qsTr(): second argument (comment) must be a string";
    QTest::newRow("qsTr('foo', 'bar', 'baz')")  << "qsTr('foo', 'bar', 'baz')" << "Error: qsTr(): third argument (n) must be a number";
    QTest::newRow("qsTr('foo', 'bar', true)")  << "qsTr('foo', 'bar', true)" << "Error: qsTr(): third argument (n) must be a number";

    QTest::newRow("qsTranslate()")  << "qsTranslate()" << "Error: qsTranslate() requires at least two arguments";
    QTest::newRow("qsTranslate('foo')")  << "qsTranslate('foo')" << "Error: qsTranslate() requires at least two arguments";
    QTest::newRow("qsTranslate(123, 'foo')")  << "qsTranslate(123, 'foo')" << "Error: qsTranslate(): first argument (context) must be a string";
    QTest::newRow("qsTranslate('foo', 123)")  << "qsTranslate('foo', 123)" << "Error: qsTranslate(): second argument (text) must be a string";
    QTest::newRow("qsTranslate('foo', 'bar', 123)")  << "qsTranslate('foo', 'bar', 123)" << "Error: qsTranslate(): third argument (comment) must be a string";
    QTest::newRow("qsTranslate('foo', 'bar', 'baz', 'zab', 'rab')")  << "qsTranslate('foo', 'bar', 'baz', 'zab', 'rab')" << "Error: qsTranslate(): fifth argument (n) must be a number";

    QTest::newRow("qsTrId()")  << "qsTrId()" << "Error: qsTrId() requires at least one argument";
    QTest::newRow("qsTrId(123)")  << "qsTrId(123)" << "TypeError: qsTrId(): first argument (id) must be a string";
    QTest::newRow("qsTrId('foo', 'bar')")  << "qsTrId('foo', 'bar')" << "TypeError: qsTrId(): second argument (n) must be a number";
}

void tst_QScriptEngine::translateWithInvalidArgs()
{
    QFETCH(QString, expression);
    QFETCH(QString, expectedError);
    QScriptEngine engine;
    engine.installTranslatorFunctions();
    QScriptValue result = engine.evaluate(expression);
    QVERIFY(result.isError());
    QCOMPARE(result.toString(), expectedError);
}

void tst_QScriptEngine::translationContext_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("expectedTranslation");

    QTest::newRow("translatable.js")  << "translatable.js" << "One" << "En";
    QTest::newRow("/translatable.js")  << "/translatable.js" << "One" << "En";
    QTest::newRow("/foo/translatable.js")  << "/foo/translatable.js" << "One" << "En";
    QTest::newRow("/foo/bar/translatable.js")  << "/foo/bar/translatable.js" << "One" << "En";
    QTest::newRow("./translatable.js")  << "./translatable.js" << "One" << "En";
    QTest::newRow("../translatable.js")  << "../translatable.js" << "One" << "En";
    QTest::newRow("foo/translatable.js")  << "foo/translatable.js" << "One" << "En";
    QTest::newRow("file:///home/qt/translatable.js")  << "file:///home/qt/translatable.js" << "One" << "En";
    QTest::newRow(":/resources/translatable.js")  << ":/resources/translatable.js" << "One" << "En";
    QTest::newRow("/translatable.js.foo")  << "/translatable.js.foo" << "One" << "En";
    QTest::newRow("/translatable.txt")  << "/translatable.txt" << "One" << "En";
    QTest::newRow("translatable")  << "translatable" << "One" << "En";
    QTest::newRow("foo/translatable")  << "foo/translatable" << "One" << "En";

    QTest::newRow("native separators")
        << (QDir::toNativeSeparators(QDir::currentPath()) + QDir::separator() + "translatable.js")
        << "One" << "En";

    QTest::newRow("translatable.js/")  << "translatable.js/" << "One" << "One";
    QTest::newRow("nosuchscript.js")  << "" << "One" << "One";
    QTest::newRow("(empty)")  << "" << "One" << "One";
}

void tst_QScriptEngine::translationContext()
{
    TranslationScope tranScope(":/translations/translatable_la");

    QScriptEngine engine;
    engine.installTranslatorFunctions();

    QFETCH(QString, path);
    QFETCH(QString, text);
    QFETCH(QString, expectedTranslation);
    QScriptValue ret = engine.evaluate(QString::fromLatin1("qsTr('%0')").arg(text), path);
    QVERIFY(ret.isString());
    QCOMPARE(ret.toString(), expectedTranslation);
}

void tst_QScriptEngine::translateScriptIdBased()
{
    QScriptEngine engine;

    TranslationScope tranScope(":/translations/idtranslatable_la");
    engine.installTranslatorFunctions();

    QString fileName = QString::fromLatin1("idtranslatable.js");

    QHash<QString, QString> expectedTranslations;
    expectedTranslations["qtn_foo_bar"] = "First string";
    expectedTranslations["qtn_needle"] = "Second string";
    expectedTranslations["qtn_haystack"] = "Third string";
    expectedTranslations["qtn_bar_baz"] = "Fourth string";

    QHash<QString, QString>::const_iterator it;
    for (it = expectedTranslations.constBegin(); it != expectedTranslations.constEnd(); ++it) {
        for (int x = 0; x < 2; ++x) {
            QString fn;
            if (x)
                fn = fileName;
            // Top-level
            QCOMPARE(engine.evaluate(QString::fromLatin1("qsTrId('%0')")
                                     .arg(it.key()), fn).toString(),
                     it.value());
            QCOMPARE(engine.evaluate(QString::fromLatin1("QT_TRID_NOOP('%0')")
                                     .arg(it.key()), fn).toString(),
                     it.key());
            // From function
            QCOMPARE(engine.evaluate(QString::fromLatin1("(function() { return qsTrId('%0'); })()")
                                     .arg(it.key()), fn).toString(),
                     it.value());
            QCOMPARE(engine.evaluate(QString::fromLatin1("(function() { return QT_TRID_NOOP('%0'); })()")
                                     .arg(it.key()), fn).toString(),
                     it.key());
        }
    }

    // Plural form
    QCOMPARE(engine.evaluate("qsTrId('qtn_bar_baz', 10)").toString(),
             QString::fromLatin1("10 fooish bar(s) found"));
    QCOMPARE(engine.evaluate("qsTrId('qtn_foo_bar', 10)").toString(),
             QString::fromLatin1("qtn_foo_bar")); // Doesn't have plural
}

// How to add a new test row:
// - Find a nice list of Unicode characters to choose from
// - Write source string/context/comment in .js using Unicode escape sequences (\uABCD)
// - Update corresponding .ts file (e.g. lupdate foo.js -ts foo.ts -codecfortr UTF-8)
// - Enter translation in Linguist
// - Update corresponding .qm file (e.g. lrelease foo.ts)
// - Evaluate script that performs translation; make sure the correct result is returned
//   (e.g. by setting the resulting string as the text of a QLabel and visually verifying
//   that it looks the same as what you entered in Linguist :-) )
// - Generate the expectedTranslation column data using toUtf8().toHex()
void tst_QScriptEngine::translateScriptUnicode_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("expectedTranslation");

    QString fileName = QString::fromLatin1("translatable-unicode.js");
    QTest::newRow("qsTr('H\\u2082O')@translatable-unicode.js")
            << QString::fromLatin1("qsTr('H\\u2082O')") << fileName << QString::fromUtf8("\xcd\xbb\xcd\xbc\xcd\xbd");
    QTest::newRow("qsTranslate('\\u010C\\u0101\\u011F\\u0115', 'CO\\u2082')@translatable-unicode.js")
            << QString::fromLatin1("qsTranslate('\\u010C\\u0101\\u011F\\u0115', 'CO\\u2082')") << fileName << QString::fromUtf8("\xd7\x91\xd7\x9a\xd7\xa2");
    QTest::newRow("qsTr('\\u0391\\u0392\\u0393')@translatable-unicode.js")
            << QString::fromLatin1("qsTr('\\u0391\\u0392\\u0393')") << fileName << QString::fromUtf8("\xd3\x9c\xd2\xb4\xd1\xbc");
    QTest::newRow("qsTranslate('\\u010C\\u0101\\u011F\\u0115', '\\u0414\\u0415\\u0416')@translatable-unicode.js")
            << QString::fromLatin1("qsTranslate('\\u010C\\u0101\\u011F\\u0115', '\\u0414\\u0415\\u0416')") << fileName << QString::fromUtf8("\xd8\xae\xd8\xb3\xd8\xb3");
    QTest::newRow("qsTr('H\\u2082O', 'not the same H\\u2082O')@translatable-unicode.js")
            << QString::fromLatin1("qsTr('H\\u2082O', 'not the same H\\u2082O')") << fileName << QString::fromUtf8("\xd4\xb6\xd5\x8a\xd5\x92");
    QTest::newRow("qsTr('H\\u2082O')")
            << QString::fromLatin1("qsTr('H\\u2082O')") << QString() << QString::fromUtf8("\x48\xe2\x82\x82\x4f");
    QTest::newRow("qsTranslate('\\u010C\\u0101\\u011F\\u0115', 'CO\\u2082')")
            << QString::fromLatin1("qsTranslate('\\u010C\\u0101\\u011F\\u0115', 'CO\\u2082')") << QString() << QString::fromUtf8("\xd7\x91\xd7\x9a\xd7\xa2");
}

void tst_QScriptEngine::translateScriptUnicode()
{
    QFETCH(QString, expression);
    QFETCH(QString, fileName);
    QFETCH(QString, expectedTranslation);

    QScriptEngine engine;

    TranslationScope tranScope(":/translations/translatable-unicode");
    engine.installTranslatorFunctions();

    QCOMPARE(engine.evaluate(expression, fileName).toString(), expectedTranslation);
    QVERIFY(!engine.hasUncaughtException());
}

void tst_QScriptEngine::translateScriptUnicodeIdBased_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QString>("expectedTranslation");

    QTest::newRow("qsTrId('\\u01F8\\u01D2\\u0199\\u01D0\\u01E1'')")
            << QString::fromLatin1("qsTrId('\\u01F8\\u01D2\\u0199\\u01D0\\u01E1')") << QString::fromUtf8("\xc6\xa7\xc6\xb0\xc6\x88\xc8\xbc\xc8\x9d\xc8\xbf\xc8\x99");
    QTest::newRow("qsTrId('\\u0191\\u01CE\\u0211\\u0229\\u019C\\u018E\\u019A\\u01D0')")
            << QString::fromLatin1("qsTrId('\\u0191\\u01CE\\u0211\\u0229\\u019C\\u018E\\u019A\\u01D0')") << QString::fromUtf8("\xc7\xa0\xc8\xa1\xc8\x8b\xc8\x85\xc8\x95");
    QTest::newRow("qsTrId('\\u0181\\u01A1\\u0213\\u018F\\u018C', 10)")
            << QString::fromLatin1("qsTrId('\\u0181\\u01A1\\u0213\\u018F\\u018C', 10)") << QString::fromUtf8("\x31\x30\x20\xc6\x92\xc6\xa1\xc7\x92\x28\xc8\x99\x29");
    QTest::newRow("qsTrId('\\u0181\\u01A1\\u0213\\u018F\\u018C')")
            << QString::fromLatin1("qsTrId('\\u0181\\u01A1\\u0213\\u018F\\u018C')") << QString::fromUtf8("\xc6\x91\xc6\xb0\xc7\xb9");
    QTest::newRow("qsTrId('\\u01CD\\u0180\\u01A8\\u0190\\u019E\\u01AB')")
            << QString::fromLatin1("qsTrId('\\u01CD\\u0180\\u01A8\\u0190\\u019E\\u01AB')") << QString::fromUtf8("\xc7\x8d\xc6\x80\xc6\xa8\xc6\x90\xc6\x9e\xc6\xab");
}

void tst_QScriptEngine::translateScriptUnicodeIdBased()
{
    QFETCH(QString, expression);
    QFETCH(QString, expectedTranslation);

    QScriptEngine engine;

    TranslationScope tranScope(":/translations/idtranslatable-unicode");
    engine.installTranslatorFunctions();

    QCOMPARE(engine.evaluate(expression).toString(), expectedTranslation);
    QVERIFY(!engine.hasUncaughtException());
}

void tst_QScriptEngine::translateFromBuiltinCallback()
{
    QScriptEngine eng;
    eng.installTranslatorFunctions();

    // Callback has no translation context.
    eng.evaluate("function foo() { qsTr('foo'); }");

    // Stack at translation time will be:
    // qsTr, foo, forEach, global
    // qsTr() needs to walk to the outer-most (global) frame before it finds
    // a translation context, and this should not crash.
    eng.evaluate("[10,20].forEach(foo)", "script.js");
}

void tst_QScriptEngine::functionScopes()
{
    QScriptEngine eng;
    {
        // top-level functions have only the global object in their scope
        QScriptValue fun = eng.evaluate("(function() {})");
        QVERIFY(fun.isFunction());
        QEXPECT_FAIL("", "QScriptValue::scope() is internal, not implemented", Abort);
        QVERIFY(fun.scope().isObject());
        QVERIFY(fun.scope().strictlyEquals(eng.globalObject()));
        QVERIFY(!eng.globalObject().scope().isValid());
    }
    {
        QScriptValue fun = eng.globalObject().property("Object");
        QVERIFY(fun.isFunction());
        // native built-in functions don't have scope
        QVERIFY(!fun.scope().isValid());
    }
    {
        // closure
        QScriptValue fun = eng.evaluate("(function(arg) { var foo = arg; return function() { return foo; }; })(123)");
        QVERIFY(fun.isFunction());
        {
            QScriptValue ret = fun.call();
            QVERIFY(ret.isNumber());
            QCOMPARE(ret.toInt32(), 123);
        }
        QScriptValue scope = fun.scope();
        QVERIFY(scope.isObject());
        {
            QScriptValue ret = scope.property("foo");
            QVERIFY(ret.isNumber());
            QCOMPARE(ret.toInt32(), 123);
            QCOMPARE(scope.propertyFlags("foo"), QScriptValue::Undeletable);
        }
        {
            QScriptValue ret = scope.property("arg");
            QVERIFY(ret.isNumber());
            QCOMPARE(ret.toInt32(), 123);
            QCOMPARE(scope.propertyFlags("arg"), QScriptValue::Undeletable | QScriptValue::SkipInEnumeration);
        }

        scope.setProperty("foo", 456);
        {
            QScriptValue ret = fun.call();
            QVERIFY(ret.isNumber());
            QCOMPARE(ret.toInt32(), 456);
        }

        scope = scope.scope();
        QVERIFY(scope.isObject());
        QVERIFY(scope.strictlyEquals(eng.globalObject()));
    }
}

static QScriptValue counter_inner(QScriptContext *ctx, QScriptEngine *)
{
     QScriptValue outerAct = ctx->callee().scope();
     double count = outerAct.property("count").toNumber();
     outerAct.setProperty("count", count+1);
     return count;
}

static QScriptValue counter(QScriptContext *ctx, QScriptEngine *eng)
{
     QScriptValue act = ctx->activationObject();
     act.setProperty("count", ctx->argument(0).toInt32());
     QScriptValue result = eng->newFunction(counter_inner);
     result.setScope(act);
     return result;
}

static QScriptValue counter_hybrid(QScriptContext *ctx, QScriptEngine *eng)
{
     QScriptValue act = ctx->activationObject();
     act.setProperty("count", ctx->argument(0).toInt32());
     return eng->evaluate("(function() { return count++; })");
}

void tst_QScriptEngine::nativeFunctionScopes()
{
    QScriptEngine eng;
    {
        QScriptValue fun = eng.newFunction(counter);
        QScriptValue cnt = fun.call(QScriptValue(), QScriptValueList() << 123);
        QVERIFY(cnt.isFunction());
        {
            QScriptValue ret = cnt.call();
            QVERIFY(ret.isNumber());
            QCOMPARE(ret.toInt32(), 123);
        }
    }
    {
        QScriptValue fun = eng.newFunction(counter_hybrid);
        QScriptValue cnt = fun.call(QScriptValue(), QScriptValueList() << 123);
        QVERIFY(cnt.isFunction());
        {
            QScriptValue ret = cnt.call();
            QVERIFY(ret.isNumber());
            QCOMPARE(ret.toInt32(), 123);
        }
    }

    //from http://doc.trolltech.com/latest/qtscript.html#nested-functions-and-the-scope-chain
    {
        QScriptEngine eng;
        eng.evaluate("function counter() { var count = 0; return function() { return count++; } }\n"
                     "var c1 = counter();  var c2 = counter(); ");
        QCOMPARE(eng.evaluate("c1()").toString(), QString::fromLatin1("0"));
        QCOMPARE(eng.evaluate("c1()").toString(), QString::fromLatin1("1"));
        QCOMPARE(eng.evaluate("c2()").toString(), QString::fromLatin1("0"));
        QCOMPARE(eng.evaluate("c2()").toString(), QString::fromLatin1("1"));
        QVERIFY(!eng.hasUncaughtException());
    }
    {
        QScriptEngine eng;
        eng.globalObject().setProperty("counter", eng.newFunction(counter));
        eng.evaluate("var c1 = counter();  var c2 = counter(); ");
        QCOMPARE(eng.evaluate("c1()").toString(), QString::fromLatin1("0"));
        QCOMPARE(eng.evaluate("c1()").toString(), QString::fromLatin1("1"));
        QCOMPARE(eng.evaluate("c2()").toString(), QString::fromLatin1("0"));
        QCOMPARE(eng.evaluate("c2()").toString(), QString::fromLatin1("1"));
        QVERIFY(!eng.hasUncaughtException());
    }
    {
        QScriptEngine eng;
        eng.globalObject().setProperty("counter", eng.newFunction(counter_hybrid));
        eng.evaluate("var c1 = counter();  var c2 = counter(); ");
        QCOMPARE(eng.evaluate("c1()").toString(), QString::fromLatin1("0"));
        QCOMPARE(eng.evaluate("c1()").toString(), QString::fromLatin1("1"));
        QCOMPARE(eng.evaluate("c2()").toString(), QString::fromLatin1("0"));
        QCOMPARE(eng.evaluate("c2()").toString(), QString::fromLatin1("1"));
        QVERIFY(!eng.hasUncaughtException());
    }
}

static QScriptValue createProgram(QScriptContext *ctx, QScriptEngine *eng)
{
    QString code = ctx->argument(0).toString();
    QScriptProgram result(code);
    return qScriptValueFromValue(eng, result);
}

void tst_QScriptEngine::evaluateProgram()
{
    QScriptEngine eng;

    {
        QString code("1 + 2");
        QString fileName("hello.js");
        int lineNumber(123);
        QScriptProgram program(code, fileName, lineNumber);
        QVERIFY(!program.isNull());
        QCOMPARE(program.sourceCode(), code);
        QCOMPARE(program.fileName(), fileName);
        QCOMPARE(program.firstLineNumber(), lineNumber);

        QScriptValue expected = eng.evaluate(code);
        for (int x = 0; x < 10; ++x) {
            QScriptValue ret = eng.evaluate(program);
            QVERIFY(ret.equals(expected));
        }

        // operator=
        QScriptProgram sameProgram = program;
        QVERIFY(sameProgram == program);
        QVERIFY(eng.evaluate(sameProgram).equals(expected));

        // copy constructor
        QScriptProgram sameProgram2(program);
        QVERIFY(sameProgram2 == program);
        QVERIFY(eng.evaluate(sameProgram2).equals(expected));

        QScriptProgram differentProgram("2 + 3");
        QVERIFY(differentProgram != program);
        QVERIFY(!eng.evaluate(differentProgram).equals(expected));
    }
}

void tst_QScriptEngine::evaluateProgram_customScope()
{
    QScriptEngine eng;
    {
        QScriptProgram program("a");
        QVERIFY(!program.isNull());
        {
            QScriptValue ret = eng.evaluate(program);
            QVERIFY(ret.isError());
            QCOMPARE(ret.toString(), QString::fromLatin1("ReferenceError: Can't find variable: a"));
        }

        QScriptValue obj = eng.newObject();
        obj.setProperty("a", 123);
        QScriptContext *ctx = eng.currentContext();
        ctx->pushScope(obj);
        {
            QScriptValue ret = eng.evaluate(program);
            QVERIFY(!ret.isError());
            QVERIFY(ret.equals(obj.property("a")));
        }

        obj.setProperty("a", QScriptValue());
        {
            QScriptValue ret = eng.evaluate(program);
            QVERIFY(ret.isError());
        }

        QScriptValue obj2 = eng.newObject();
        obj2.setProperty("a", 456);
        ctx->pushScope(obj2);
        {
            QScriptValue ret = eng.evaluate(program);
            QVERIFY(!ret.isError());
            QVERIFY(ret.equals(obj2.property("a")));
        }

        ctx->popScope();
    }
}

void tst_QScriptEngine::evaluateProgram_closure()
{
    QScriptEngine eng;
    {
        QScriptProgram program("(function() { var count = 0; return function() { return count++; }; })");
        QVERIFY(!program.isNull());
        QScriptValue createCounter = eng.evaluate(program);
        QVERIFY(createCounter.isFunction());
        QScriptValue counter = createCounter.call();
        QVERIFY(counter.isFunction());
        {
            QScriptValue ret = counter.call();
            QVERIFY(ret.isNumber());
        }
        QScriptValue counter2 = createCounter.call();
        QVERIFY(counter2.isFunction());
        QVERIFY(!counter2.equals(counter));
        {
            QScriptValue ret = counter2.call();
            QVERIFY(ret.isNumber());
        }
    }
}

void tst_QScriptEngine::evaluateProgram_executeLater()
{
    QScriptEngine eng;
    // Program created in a function call, then executed later
    {
        QScriptValue fun = eng.newFunction(createProgram);
        QScriptProgram program = qscriptvalue_cast<QScriptProgram>(
            fun.call(QScriptValue(), QScriptValueList() << "a + 1"));
        QVERIFY(!program.isNull());
        eng.globalObject().setProperty("a", QScriptValue());
        {
            QScriptValue ret = eng.evaluate(program);
            QVERIFY(ret.isError());
            QCOMPARE(ret.toString(), QString::fromLatin1("ReferenceError: Can't find variable: a"));
        }
        eng.globalObject().setProperty("a", 122);
        {
            QScriptValue ret = eng.evaluate(program);
            QVERIFY(!ret.isError());
            QVERIFY(ret.isNumber());
            QCOMPARE(ret.toInt32(), 123);
        }
    }
}

void tst_QScriptEngine::evaluateProgram_multipleEngines()
{
    QScriptEngine eng;
    {
        QString code("1 + 2");
        QScriptProgram program(code);
        QVERIFY(!program.isNull());
        double expected = eng.evaluate(program).toNumber();
        for (int x = 0; x < 2; ++x) {
            QScriptEngine eng2;
            for (int y = 0; y < 2; ++y) {
                double ret = eng2.evaluate(program).toNumber();
                QCOMPARE(ret, expected);
            }
        }
    }
}

void tst_QScriptEngine::evaluateProgram_empty()
{
    QScriptEngine eng;
    {
        QScriptProgram program;
        QVERIFY(program.isNull());
        QScriptValue ret = eng.evaluate(program);
        QVERIFY(!ret.isValid());
    }
}

void tst_QScriptEngine::collectGarbageAfterConnect()
{
    // QTBUG-6366
    QScriptEngine engine;
    QPointer<QWidget> widget = new QWidget;
    engine.globalObject().setProperty(
        "widget", engine.newQObject(widget, QScriptEngine::ScriptOwnership));
    QVERIFY(engine.evaluate("widget.customContextMenuRequested.connect(\n"
                            "  function() { print('hello'); }\n"
                            ");")
            .isUndefined());
    QVERIFY(widget != 0);
    engine.evaluate("widget = null;");
    // The connection should not keep the widget alive.
    collectGarbage_helper(engine);
    QVERIFY(widget == 0);
}

void tst_QScriptEngine::collectGarbageAfterNativeArguments()
{
    // QTBUG-17788
    QScriptEngine eng;
    QScriptContext *ctx = eng.pushContext();
    QScriptValue arguments = ctx->argumentsObject();
    // Shouldn't crash when marking the arguments object.
    collectGarbage_helper(eng);
}

static QScriptValue constructQObjectFromThisObject(QScriptContext *ctx, QScriptEngine *eng)
{
    if (!ctx->isCalledAsConstructor()) {
        qWarning("%s: ctx->isCalledAsConstructor() returned false", Q_FUNC_INFO);
        return QScriptValue();
    }
    return eng->newQObject(ctx->thisObject(), new QObject, QScriptEngine::ScriptOwnership);
}

void tst_QScriptEngine::promoteThisObjectToQObjectInConstructor()
{
    QScriptEngine engine;
    QScriptValue ctor = engine.newFunction(constructQObjectFromThisObject);
    engine.globalObject().setProperty("Ctor", ctor);
    QScriptValue object = engine.evaluate("new Ctor");
    QVERIFY(!object.isError());
    QVERIFY(object.isQObject());
    QVERIFY(object.toQObject() != 0);
    QVERIFY(object.property("objectName").isString());
    QVERIFY(object.property("deleteLater").isFunction());
}

static QRegExp minimal(QRegExp r) { r.setMinimal(true); return r; }

void tst_QScriptEngine::qRegExpInport_data()
{
    QTest::addColumn<QRegExp>("rx");
    QTest::addColumn<QString>("string");
    QTest::addColumn<QString>("matched");

    QTest::newRow("normal")  << QRegExp("(test|foo)") << "test _ foo _ test _ Foo";
    QTest::newRow("normal2")  << QRegExp("(Test|Foo)") << "test _ foo _ test _ Foo";
    QTest::newRow("case insensitive)")  << QRegExp("(test|foo)", Qt::CaseInsensitive) << "test _ foo _ test _ Foo";
    QTest::newRow("case insensitive2)")  << QRegExp("(Test|Foo)", Qt::CaseInsensitive) << "test _ foo _ test _ Foo";
    QTest::newRow("b(a*)(b*)")  << QRegExp("b(a*)(b*)", Qt::CaseInsensitive) << "aaabbBbaAabaAaababaaabbaaab";
    QTest::newRow("greedy")  << QRegExp("a*(a*)", Qt::CaseInsensitive, QRegExp::RegExp2) << "aaaabaaba";
    // this one will fail because we do not support the QRegExp::RegExp in JSC
    //QTest::newRow("not_greedy")  << QRegExp("a*(a*)", Qt::CaseInsensitive, QRegExp::RegExp) << "aaaabaaba";
    QTest::newRow("willcard")  << QRegExp("*.txt", Qt::CaseSensitive, QRegExp::Wildcard) << "file.txt";
    QTest::newRow("willcard 2")  << QRegExp("a?b.txt", Qt::CaseSensitive, QRegExp::Wildcard) << "ab.txt abb.rtc acb.txt";
    QTest::newRow("slash")  << QRegExp("g/.*/s", Qt::CaseInsensitive, QRegExp::RegExp2) << "string/string/string";
    QTest::newRow("slash2")  << QRegExp("g / .* / s", Qt::CaseInsensitive, QRegExp::RegExp2) << "string / string / string";
    QTest::newRow("fixed")  << QRegExp("a*aa.a(ba)*a\\ba", Qt::CaseInsensitive, QRegExp::FixedString) << "aa*aa.a(ba)*a\\ba";
    QTest::newRow("fixed insensitive")  << QRegExp("A*A", Qt::CaseInsensitive, QRegExp::FixedString) << "a*A A*a A*A a*a";
    QTest::newRow("fixed sensitive")  << QRegExp("A*A", Qt::CaseSensitive, QRegExp::FixedString) << "a*A A*a A*A a*a";
    QTest::newRow("html")  << QRegExp("<b>(.*)</b>", Qt::CaseSensitive, QRegExp::RegExp2) << "<b>bold</b><i>italic</i><b>bold</b>";
    QTest::newRow("html minimal")  << minimal(QRegExp("<b>(.*)</b>", Qt::CaseSensitive, QRegExp::RegExp2)) << "<b>bold</b><i>italic</i><b>bold</b>";
    QTest::newRow("aaa")  << QRegExp("a{2,5}") << "aAaAaaaaaAa";
    QTest::newRow("aaa minimal")  << minimal(QRegExp("a{2,5}")) << "aAaAaaaaaAa";
    QTest::newRow("minimal")  << minimal(QRegExp(".*\\} [*8]")) << "}?} ?} *";
    QTest::newRow(".? minimal")  << minimal(QRegExp(".?")) << ".?";
    QTest::newRow(".+ minimal")  << minimal(QRegExp(".+")) << ".+";
    QTest::newRow("[.?] minimal")  << minimal(QRegExp("[.?]")) << ".?";
    QTest::newRow("[.+] minimal")  << minimal(QRegExp("[.+]")) << ".+";
}

void tst_QScriptEngine::qRegExpInport()
{
    QFETCH(QRegExp, rx);
    QFETCH(QString, string);

    QScriptEngine eng;
    QScriptValue rexp;
    rexp = eng.newRegExp(rx);

    QCOMPARE(rexp.isValid(), true);
    QCOMPARE(rexp.isRegExp(), true);
    QVERIFY(rexp.isFunction());

    QScriptValue func = eng.evaluate("(function(string, regexp) { return string.match(regexp); })");
    QScriptValue result = func.call(QScriptValue(),  QScriptValueList() << string << rexp);

    rx.indexIn(string);
    for (int i = 0; i <= rx.captureCount(); i++)  {
        QCOMPARE(result.property(i).toString(), rx.cap(i));
    }
}

static QByteArray msgDateRoundtripJSQtJS(int i, uint secs,
                                         const QScriptValue &jsDate2,
                                         const QScriptValue &jsDate)
{
    QString result;
    const qsreal diff = jsDate.toNumber() - jsDate2.toNumber();
    QTextStream(&result) << "jsDate2=\"" << jsDate2.toString()
        << "\" (" << jsDate2.toNumber() << "), jsDate=\""
        << jsDate.toString() << "\" (" << jsDate.toNumber()
        << "), diff=" << diff << "\", i=" << i << ", secs=" << secs
        << ", TZ=\"" << qgetenv("TZ") << '"';
    return result.toLocal8Bit();
}

// QScriptValue::toDateTime() returns a local time, whereas JS dates
// are always stored as UTC. Qt Script must respect the current time
// zone, and correctly adjust for daylight saving time that may be in
// effect at a given date (QTBUG-9770).
void tst_QScriptEngine::dateRoundtripJSQtJS()
{
    uint secs = QDateTime(QDate(2009, 1, 1)).toUTC().toTime_t();
    QScriptEngine eng;
    for (int i = 0; i < 8000; ++i) {
        QScriptValue jsDate = eng.evaluate(QString::fromLatin1("new Date(%0 * 1000.0)").arg(secs));
        QDateTime qtDate = jsDate.toDateTime();
        QScriptValue jsDate2 = eng.newDate(qtDate);
        QVERIFY2(jsDate2.toNumber() == jsDate.toNumber(),
                 msgDateRoundtripJSQtJS(i, secs, jsDate2, jsDate));
        secs += 2*60*60;
    }
}

void tst_QScriptEngine::dateRoundtripQtJSQt()
{
    QDateTime qtDate = QDateTime(QDate(2009, 1, 1));
    QScriptEngine eng;
    for (int i = 0; i < 8000; ++i) {
        QScriptValue jsDate = eng.newDate(qtDate);
        QDateTime qtDate2 = jsDate.toDateTime();
        if (qtDate2 != qtDate)
            QFAIL(qPrintable(qtDate.toString()));
        qtDate = qtDate.addSecs(2*60*60);
    }
}

static QByteArray msgDateConversionJSQt(int i, uint secs,
                                        const QString &qtUTCDateStr,
                                        const QString &jsUTCDateStr,
                                        const QScriptValue &jsDate)
{
    QString result;
    QTextStream(&result) << "qtUTCDateStr=\"" << qtUTCDateStr
        << "\", jsUTCDateStr=\"" << jsUTCDateStr << "\", jsDate=\""
        << jsDate.toString() << "\", i=" << i << ", secs=" << secs
        << ", TZ=\"" << qgetenv("TZ") << '"';
    return result.toLocal8Bit();
}

void tst_QScriptEngine::dateConversionJSQt()
{
    uint secs = QDateTime(QDate(2009, 1, 1)).toUTC().toTime_t();
    QScriptEngine eng;
    for (int i = 0; i < 8000; ++i) {
        QScriptValue jsDate = eng.evaluate(QString::fromLatin1("new Date(%0 * 1000.0)").arg(secs));
        QDateTime qtDate = jsDate.toDateTime();
        QString qtUTCDateStr = qtDate.toUTC().toString(Qt::ISODate);
        QString jsUTCDateStr = jsDate.property("toISOString").call(jsDate).toString();
        jsUTCDateStr.remove(jsUTCDateStr.length() - 5, 4); // get rid of milliseconds (".000")
        QVERIFY2(qtUTCDateStr == jsUTCDateStr,
                 msgDateConversionJSQt(i, secs, qtUTCDateStr, jsUTCDateStr, jsDate));
        secs += 2*60*60;
    }
}

void tst_QScriptEngine::dateConversionQtJS()
{
    QDateTime qtDate = QDateTime(QDate(2009, 1, 1));
    QScriptEngine eng;
    for (int i = 0; i < 8000; ++i) {
        QScriptValue jsDate = eng.newDate(qtDate);
        QString jsUTCDateStr = jsDate.property("toISOString").call(jsDate).toString();
        jsUTCDateStr.remove(jsUTCDateStr.length() - 5, 4); // get rid of milliseconds (".000")
        QString qtUTCDateStr = qtDate.toUTC().toString(Qt::ISODate);
        if (jsUTCDateStr != qtUTCDateStr)
            QFAIL(qPrintable(qtDate.toString()));
        qtDate = qtDate.addSecs(2*60*60);
    }
}

static QScriptValue createAnotherEngine(QScriptContext *, QScriptEngine *)
{
    QScriptEngine eng;
    eng.evaluate("function foo(x, y) { return x + y; }" );
    eng.evaluate("hello = 5; world = 6" );
    return eng.evaluate("foo(hello,world)").toInt32();
}


void tst_QScriptEngine::reentrency()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("foo", eng.newFunction(createAnotherEngine));
    eng.evaluate("function bar() { return foo(); }  hello = 9; function getHello() { return hello; }");
    QCOMPARE(eng.evaluate("foo() + getHello() + foo()").toInt32(), 5+6 + 9 + 5+6);
    QCOMPARE(eng.evaluate("foo").call().toInt32(), 5+6);
    QCOMPARE(eng.evaluate("hello").toInt32(), 9);
    QCOMPARE(eng.evaluate("foo() + hello").toInt32(), 5+6+9);
}

void tst_QScriptEngine::newFixedStaticScopeObject()
{
    // "Static scope objects" is an optimization we do for QML.
    // It enables the creation of JS objects that can guarantee to the
    // compiler that no properties will be added or removed. This enables
    // the compiler to generate a very simple (fast) property access, as
    // opposed to a full virtual lookup. Due to the inherent use of scope
    // chains in QML, this can make a huge difference (10x improvement for
    // benchmark in QTBUG-8576).
    // Ideally we would not need a special object type for this, and the
    // VM would dynamically optimize it to be fast...
    // See also QScriptEngine benchmark.

    QScriptEngine eng;
    static const int propertyCount = 4;
    QString names[] = { "foo", "bar", "baz", "Math" };
    QScriptValue values[] = { 123, "ciao", true, false };
    QScriptValue::PropertyFlags flags[] = { QScriptValue::Undeletable,
                                            QScriptValue::ReadOnly | QScriptValue::Undeletable,
                                            QScriptValue::SkipInEnumeration | QScriptValue::Undeletable,
                                            QScriptValue::Undeletable };
    QScriptValue scope = QScriptDeclarativeClass::newStaticScopeObject(&eng, propertyCount, names, values, flags);

    // Query property.
    for (int i = 0; i < propertyCount; ++i) {
        for (int x = 0; x < 2; ++x) {
            if (x) {
                // Properties can't be deleted.
                scope.setProperty(names[i], QScriptValue());
            }
            QVERIFY(scope.property(names[i]).equals(values[i]));
            QCOMPARE(scope.propertyFlags(names[i]), flags[i]);
        }
    }

    // Property that doesn't exist.
    QVERIFY(!scope.property("noSuchProperty").isValid());
    QCOMPARE(scope.propertyFlags("noSuchProperty"), QScriptValue::PropertyFlags());

    // Write to writable property.
    {
        QScriptValue oldValue = scope.property("foo");
        QVERIFY(oldValue.isNumber());
        QScriptValue newValue = oldValue.toNumber() * 2;
        scope.setProperty("foo", newValue);
        QVERIFY(scope.property("foo").equals(newValue));
        scope.setProperty("foo", oldValue);
        QVERIFY(scope.property("foo").equals(oldValue));
    }

    // Write to read-only property.
    scope.setProperty("bar", 456);
    QVERIFY(scope.property("bar").equals("ciao"));

    // Iterate.
    {
        QScriptValueIterator it(scope);
        QSet<QString> iteratedNames;
        while (it.hasNext()) {
            it.next();
            iteratedNames.insert(it.name());
        }
        for (int i = 0; i < propertyCount; ++i)
            QVERIFY(iteratedNames.contains(names[i]));
    }

    // Push it on the scope chain of a new context.
    QScriptContext *ctx = eng.pushContext();
    ctx->pushScope(scope);
    QCOMPARE(ctx->scopeChain().size(), 3); // Global Object, native activation, custom scope
    QVERIFY(ctx->activationObject().equals(scope));

    // Read property from JS.
    for (int i = 0; i < propertyCount; ++i) {
        for (int x = 0; x < 2; ++x) {
            if (x) {
                // Property can't be deleted from JS.
                QScriptValue ret = eng.evaluate(QString::fromLatin1("delete %0").arg(names[i]));
                QVERIFY(ret.equals(false));
            }
            QVERIFY(eng.evaluate(names[i]).equals(values[i]));
        }
    }

    // Property that doesn't exist.
    QVERIFY(eng.evaluate("noSuchProperty").equals("ReferenceError: Can't find variable: noSuchProperty"));

    // Write property from JS.
    {
        QScriptValue oldValue = eng.evaluate("foo");
        QVERIFY(oldValue.isNumber());
        QScriptValue newValue = oldValue.toNumber() * 2;
        QVERIFY(eng.evaluate("foo = foo * 2; foo").equals(newValue));
        scope.setProperty("foo", oldValue);
        QVERIFY(eng.evaluate("foo").equals(oldValue));
    }

    // Write to read-only property.
    QVERIFY(eng.evaluate("bar = 456; bar").equals("ciao"));

    // Create a closure and return properties from there.
    {
        QScriptValue props = eng.evaluate("(function() { var baz = 'shadow'; return [foo, bar, baz, Math, Array]; })()");
        QVERIFY(props.isArray());
        // "foo" and "bar" come from scope object.
        QVERIFY(props.property(0).equals(scope.property("foo")));
        QVERIFY(props.property(1).equals(scope.property("bar")));
        // "baz" shadows property in scope object.
        QVERIFY(props.property(2).equals("shadow"));
        // "Math" comes from scope object, and shadows Global Object's "Math".
        QVERIFY(props.property(3).equals(scope.property("Math")));
        QVERIFY(!props.property(3).equals(eng.globalObject().property("Math")));
        // "Array" comes from Global Object.
        QVERIFY(props.property(4).equals(eng.globalObject().property("Array")));
    }

    // As with normal JS, assigning to an undefined variable will create
    // the property on the Global Object, not the inner scope.
    QVERIFY(!eng.globalObject().property("newProperty").isValid());
    QVERIFY(eng.evaluate("(function() { newProperty = 789; })()").isUndefined());
    QVERIFY(!scope.property("newProperty").isValid());
    QVERIFY(eng.globalObject().property("newProperty").isNumber());

    // Nested static scope.
    {
        static const int propertyCount2 = 2;
        QString names2[] = { "foo", "hum" };
        QScriptValue values2[] = { 321, "hello" };
        QScriptValue::PropertyFlags flags2[] = { QScriptValue::Undeletable,
                                                 QScriptValue::ReadOnly | QScriptValue::Undeletable };
        QScriptValue scope2 = QScriptDeclarativeClass::newStaticScopeObject(&eng, propertyCount2, names2, values2, flags2);
        ctx->pushScope(scope2);

        // "foo" shadows scope.foo.
        QVERIFY(eng.evaluate("foo").equals(scope2.property("foo")));
        QVERIFY(!eng.evaluate("foo").equals(scope.property("foo")));
        // "hum" comes from scope2.
        QVERIFY(eng.evaluate("hum").equals(scope2.property("hum")));
        // "Array" comes from Global Object.
        QVERIFY(eng.evaluate("Array").equals(eng.globalObject().property("Array")));

        ctx->popScope();
    }

    QScriptValue fun = eng.evaluate("(function() { return foo; })");
    QVERIFY(fun.isFunction());
    eng.popContext();
    // Function's scope chain persists after popContext().
    QVERIFY(fun.call().equals(scope.property("foo")));
}

void tst_QScriptEngine::newGrowingStaticScopeObject()
{
    // The main use case for a growing static scope object is to set it as
    // the activation object of a QScriptContext, so that all JS variable
    // declarations end up in that object. It needs to be "growable" since
    // we don't know in advance how many variables a script will declare.

    QScriptEngine eng;
    QScriptValue scope = QScriptDeclarativeClass::newStaticScopeObject(&eng);

    // Initially empty.
    QVERIFY(!QScriptValueIterator(scope).hasNext());
    QVERIFY(!scope.property("foo").isValid());

    // Add a static property.
    scope.setProperty("foo", 123);
    QVERIFY(scope.property("foo").equals(123));
    QCOMPARE(scope.propertyFlags("foo"), QScriptValue::Undeletable);

    // Modify existing property.
    scope.setProperty("foo", 456);
    QVERIFY(scope.property("foo").equals(456));

    // Add a read-only property.
    scope.setProperty("bar", "ciao", QScriptValue::ReadOnly);
    QVERIFY(scope.property("bar").equals("ciao"));
    QCOMPARE(scope.propertyFlags("bar"), QScriptValue::ReadOnly | QScriptValue::Undeletable);

    // Attempt to modify read-only property.
    scope.setProperty("bar", "hello");
    QVERIFY(scope.property("bar").equals("ciao"));

    // Properties can't be deleted.
    scope.setProperty("foo", QScriptValue());
    QVERIFY(scope.property("foo").equals(456));
    scope.setProperty("bar", QScriptValue());
    QVERIFY(scope.property("bar").equals("ciao"));

    // Iterate.
    {
        QScriptValueIterator it(scope);
        QSet<QString> iteratedNames;
        while (it.hasNext()) {
            it.next();
            iteratedNames.insert(it.name());
        }
        QCOMPARE(iteratedNames.size(), 2);
        QVERIFY(iteratedNames.contains("foo"));
        QVERIFY(iteratedNames.contains("bar"));
    }

    // Push it on the scope chain of a new context.
    QScriptContext *ctx = eng.pushContext();
    ctx->pushScope(scope);
    QCOMPARE(ctx->scopeChain().size(), 3); // Global Object, native activation, custom scope
    QVERIFY(ctx->activationObject().equals(scope));

    // Read property from JS.
    QVERIFY(eng.evaluate("foo").equals(scope.property("foo")));
    QVERIFY(eng.evaluate("bar").equals(scope.property("bar")));

    // Write property from JS.
    {
        QScriptValue oldValue = eng.evaluate("foo");
        QVERIFY(oldValue.isNumber());
        QScriptValue newValue = oldValue.toNumber() * 2;
        QVERIFY(eng.evaluate("foo = foo * 2; foo").equals(newValue));
        scope.setProperty("foo", oldValue);
        QVERIFY(eng.evaluate("foo").equals(oldValue));
    }

    // Write to read-only property.
    QVERIFY(eng.evaluate("bar = 456; bar").equals("ciao"));

    // Shadow property.
    QVERIFY(eng.evaluate("Math").equals(eng.globalObject().property("Math")));
    scope.setProperty("Math", "fake Math");
    QVERIFY(eng.evaluate("Math").equals(scope.property("Math")));

    // Variable declarations will create properties on the scope.
    eng.evaluate("var baz = 456");
    QVERIFY(scope.property("baz").equals(456));

    // Function declarations will create properties on the scope.
    eng.evaluate("function fun() { return baz; }");
    QVERIFY(scope.property("fun").isFunction());
    QVERIFY(scope.property("fun").call().equals(scope.property("baz")));

    // Demonstrate the limitation of a growable static scope: Once a function that
    // uses the scope has been compiled, it won't pick up properties that are added
    // to the scope later.
    {
        QScriptValue fun = eng.evaluate("(function() { return futureProperty; })");
        QVERIFY(fun.isFunction());
        QVERIFY(fun.call().toString().contains(QString::fromLatin1("ReferenceError")));
        scope.setProperty("futureProperty", "added after the function was compiled");
        // If scope were dynamic, this would return the new property.
        QVERIFY(fun.call().toString().contains(QString::fromLatin1("ReferenceError")));
    }

    eng.popContext();
}

QT_BEGIN_NAMESPACE
Q_SCRIPT_DECLARE_QMETAOBJECT(QStandardItemModel, QObject*)
QT_END_NAMESPACE

void tst_QScriptEngine::scriptValueFromQMetaObject()
{
    QScriptEngine eng;
    {
        QScriptValue meta = eng.scriptValueFromQMetaObject<QScriptEngine>();
        QVERIFY(meta.isQMetaObject());
        QCOMPARE(meta.toQMetaObject(), &QScriptEngine::staticMetaObject);
        // Because of missing Q_SCRIPT_DECLARE_QMETAOBJECT() for QScriptEngine.
        QVERIFY(!meta.construct().isValid());
    }
    {
        QScriptValue meta = eng.scriptValueFromQMetaObject<QStandardItemModel>();
        QVERIFY(meta.isQMetaObject());
        QCOMPARE(meta.toQMetaObject(), &QStandardItemModel::staticMetaObject);
        QScriptValue obj = meta.construct(QScriptValueList() << eng.newQObject(&eng));
        QVERIFY(obj.isQObject());
        QStandardItemModel *model = qobject_cast<QStandardItemModel*>(obj.toQObject());
        QVERIFY(model != 0);
        QCOMPARE(model->parent(), (QObject*)&eng);
    }
}

// QTBUG-21896
void tst_QScriptEngine::stringListFromArrayWithEmptyElement()
{
    QScriptEngine eng;
    QCOMPARE(qscriptvalue_cast<QStringList>(eng.evaluate("[,'hello']")),
             QStringList() << "" << "hello");
}

// QTBUG-21993
void tst_QScriptEngine::collectQObjectWithCachedWrapper_data()
{
    QTest::addColumn<QString>("program");
    QTest::addColumn<QString>("ownership");
    QTest::addColumn<bool>("giveParent");
    QTest::addColumn<bool>("shouldBeCollected");

    QString prog1 = QString::fromLatin1("newQObject(ownership, parent)");
    QTest::newRow("unassigned,cpp,no-parent") << prog1 << "cpp" << false << false;
    QTest::newRow("unassigned,cpp,parent") << prog1 << "cpp" << true << false;
    QTest::newRow("unassigned,auto,no-parent") << prog1 << "auto" << false << true;
    QTest::newRow("unassigned,auto,parent") << prog1 << "auto" << true << false;
    QTest::newRow("unassigned,script,no-parent") << prog1 << "script" << false << true;
    QTest::newRow("unassigned,script,parent") << prog1 << "script" << true << true;

    QString prog2 = QString::fromLatin1("myObject = { foo: newQObject(ownership, parent) }; myObject.foo");
    QTest::newRow("global-property-property,cpp,no-parent") << prog2 << "cpp" << false << false;
    QTest::newRow("global-property-property,cpp,parent") << prog2 << "cpp" << true << false;
    QTest::newRow("global-property-property,auto,no-parent") << prog2 << "auto" << false << false;
    QTest::newRow("global-property-property,auto,parent") << prog2 << "auto" << true << false;
    QTest::newRow("global-property-property,script,no-parent") << prog2 << "script" << false << false;
    QTest::newRow("global-property-property,script,parent") << prog2 << "script" << true << false;

}

void tst_QScriptEngine::collectQObjectWithCachedWrapper()
{
    struct Functions {
        static QScriptValue newQObject(QScriptContext *ctx, QScriptEngine *eng)
        {
            QString ownershipString = ctx->argument(0).toString();
            QScriptEngine::ValueOwnership ownership;
            if (ownershipString == "cpp")
                ownership = QScriptEngine::QtOwnership;
            else if (ownershipString == "auto")
                ownership = QScriptEngine::AutoOwnership;
            else if (ownershipString == "script")
                ownership = QScriptEngine::ScriptOwnership;
            else
                return ctx->throwError("Ownership specifier 'cpp', 'auto' or 'script' expected");

            QObject *parent = ctx->argument(1).toQObject();
            return eng->newQObject(new QObject(parent), ownership,
                                   QScriptEngine::PreferExistingWrapperObject);
        }
    };

    QFETCH(QString, program);
    QFETCH(QString, ownership);
    QFETCH(bool, giveParent);
    QFETCH(bool, shouldBeCollected);

    QScriptEngine eng;
    eng.globalObject().setProperty("ownership", ownership);
    eng.globalObject().setProperty("newQObject",
                                   eng.newFunction(Functions::newQObject));

    QObject parent;
    eng.globalObject().setProperty("parent",
                                   giveParent ? eng.newQObject(&parent)
                                              : QScriptValue(QScriptValue::NullValue));

    QPointer<QObject> ptr = eng.evaluate(program).toQObject();
    QVERIFY(ptr != 0);
    QVERIFY(ptr->parent() == (giveParent ? &parent : 0));

    collectGarbage_helper(eng);

    if (ptr && shouldBeCollected)
        QEXPECT_FAIL("", "Test can fail because the garbage collector is conservative", Continue);
    QCOMPARE(ptr == 0, shouldBeCollected);
}

// QTBUG-18188
void tst_QScriptEngine::pushContext_noInheritedScope()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("foo", 123);

    QScriptContext *ctx1 = eng.pushContext();
    QCOMPARE(ctx1->scopeChain().size(), 2);
    ctx1->activationObject().setProperty("foo", 456);
    QCOMPARE(eng.evaluate("foo").toInt32(), 456);

    QScriptContext *ctx2 = eng.pushContext();
    // The parent context's scope should not be inherited.
    QCOMPARE(ctx2->scopeChain().size(), 2);
    QCOMPARE(eng.evaluate("foo").toInt32(), 123);

    ctx2->activationObject().setProperty("foo", 789);
    QCOMPARE(eng.evaluate("foo").toInt32(), 789);

    eng.popContext();
    QCOMPARE(eng.evaluate("foo").toInt32(), 456);

    eng.popContext();
    QCOMPARE(eng.evaluate("foo").toInt32(), 123);
}

QTEST_MAIN(tst_QScriptEngine)
#include "tst_qscriptengine.moc"
