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
#include <QtQml/qjsvalue.h>
#include <QtQml/qjsengine.h>

class tst_QJSEngine : public QObject
{
    Q_OBJECT

public:
    tst_QJSEngine();
    virtual ~tst_QJSEngine();

public slots:
    void init();
    void cleanup();

private slots:
    void constructor();
#if 0 // No defaultPrototype for now
    void defaultPrototype();
    void setDefaultPrototype();
#endif
    void evaluate_data();
    void evaluate();
#if 0 // No program
    void evaluateProgram_data();
    void evaluateProgram();
#endif
#if 0 // no connections for now
    void connectAndDisconnect();
#endif
    void globalObject();
#if 0  // no is Evaluating for now
    void isEvaluating();
#endif
    void newArray_data();
    void newArray();
    void newDate();
    void newObject();
#if 0 // No ScriptClass
    void newObjectWithScriptClass();
#endif
#if 0 // no qmetaobject
    void newQMetaObject();
#endif
    void newQObject();
#if 0 // no native functions for now
    void newFunction();
#endif
    void newRegExp();
    void newVariant();
    void undefinedValue();
    void collectGarbage();
#if 0 // No extensions
    void availableExtensions();
    void importedExtensions();
#endif
#if 0 // no context
    void currentContext();
    void pushAndPopContext();
#endif
#if 0 // no stringhandle
    void toStringHandle();
#endif
    void castValueToQreal();
#if 0 // no native functions for now
    void nativeCall();
#endif
#if 0 // no translations
    void installTranslatorFunctions();
    void translation_data();
    void translation();
#endif
#if 0 // no declarative class
    void readScopeProperty_data();
    void readScopeProperty();
#endif
#if 0 // no context
    void evaluateInNewContext();
    void evaluateInNewContextWithScope();
#endif
#if 0 // no pushScope
    void evaluateBindingExpression();
#endif

private:
    void defineStandardTestValues();
    void newEngine()
    {
        delete m_engine;
        m_engine = new QJSEngine;
    }

    QJSEngine *m_engine;
};

tst_QJSEngine::tst_QJSEngine()
    : m_engine(0)
{
}

tst_QJSEngine::~tst_QJSEngine()
{
    delete m_engine;
}

void tst_QJSEngine::init()
{
}

void tst_QJSEngine::cleanup()
{
}

void tst_QJSEngine::constructor()
{
    QBENCHMARK {
        QJSEngine engine;
        (void)engine.parent();
    }
}

#if 0 // No defaultPrototype for now
void tst_QJSEngine::defaultPrototype()
{
    newEngine();
    int type = qMetaTypeId<int>();
    m_engine->setDefaultPrototype(type, m_engine->newObject());
    QBENCHMARK {
        m_engine->defaultPrototype(type);
    }
}

void tst_QJSEngine::setDefaultPrototype()
{
    newEngine();
    int type = qMetaTypeId<int>();
    QJSValue proto = m_engine->newObject();
    QBENCHMARK {
        m_engine->setDefaultPrototype(type, proto);
    }
}

#endif

void tst_QJSEngine::evaluate_data()
{
    QTest::addColumn<QString>("code");
    QTest::newRow("empty script") << QString::fromLatin1("");
    QTest::newRow("number literal") << QString::fromLatin1("123");
    QTest::newRow("string literal") << QString::fromLatin1("'ciao'");
    QTest::newRow("regexp literal") << QString::fromLatin1("/foo/gim");
    QTest::newRow("null literal") << QString::fromLatin1("null");
    QTest::newRow("undefined literal") << QString::fromLatin1("undefined");
    QTest::newRow("null literal") << QString::fromLatin1("null");
    QTest::newRow("empty object literal") << QString::fromLatin1("{}");
    QTest::newRow("this") << QString::fromLatin1("this");
    QTest::newRow("object literal with one property") << QString::fromLatin1("{ foo: 123 }");
    QTest::newRow("object literal with two properties") << QString::fromLatin1("{ foo: 123, bar: 456 }");
    QTest::newRow("object literal with many properties") << QString::fromLatin1("{ a: 1, b: 2, c: 3, d: 4, e: 5, f: 6, g: 7, h: 8, i: 9, j: 10 }");
    QTest::newRow("empty array literal") << QString::fromLatin1("[]");
    QTest::newRow("array literal with one element") << QString::fromLatin1("[1]");
    QTest::newRow("array literal with two elements") << QString::fromLatin1("[1,2]");
    QTest::newRow("array literal with many elements") << QString::fromLatin1("[1,2,3,4,5,6,7,8,9,10,9,8,7,6,5,4,3,2,1]");
    QTest::newRow("empty function definition") << QString::fromLatin1("function foo() { }");
    QTest::newRow("function definition") << QString::fromLatin1("function foo() { return 123; }");
    QTest::newRow("for loop with empty body (1000 iterations)") << QString::fromLatin1("for (i = 0; i < 1000; ++i) {}");
    QTest::newRow("for loop with empty body (10000 iterations)") << QString::fromLatin1("for (i = 0; i < 10000; ++i) {}");
    QTest::newRow("for loop with empty body (100000 iterations)") << QString::fromLatin1("for (i = 0; i < 100000; ++i) {}");
    QTest::newRow("for loop with empty body (1000000 iterations)") << QString::fromLatin1("for (i = 0; i < 1000000; ++i) {}");
    QTest::newRow("for loop (1000 iterations)") << QString::fromLatin1("j = 0; for (i = 0; i < 1000; ++i) { j += i; }; j");
    QTest::newRow("for loop (10000 iterations)") << QString::fromLatin1("j = 0; for (i = 0; i < 10000; ++i) { j += i; }; j");
    QTest::newRow("for loop (100000 iterations)") << QString::fromLatin1("j = 0; for (i = 0; i < 100000; ++i) { j += i; }; j");
    QTest::newRow("for loop (1000000 iterations)") << QString::fromLatin1("j = 0; for (i = 0; i < 1000000; ++i) { j += i; }; j");
    QTest::newRow("assignments") << QString::fromLatin1("a = 1; b = 2; c = 3; d = 4");
    QTest::newRow("while loop (1000 iterations)") << QString::fromLatin1("i = 0; while (i < 1000) { ++i; }; i");
    QTest::newRow("while loop (10000 iterations)") << QString::fromLatin1("i = 0; while (i < 10000) { ++i; }; i");
    QTest::newRow("while loop (100000 iterations)") << QString::fromLatin1("i = 0; while (i < 100000) { ++i; }; i");
    QTest::newRow("while loop (1000000 iterations)") << QString::fromLatin1("i = 0; while (i < 1000000) { ++i; }; i");
    QTest::newRow("function expression") << QString::fromLatin1("(function(a, b, c){ return a + b + c; })(1, 2, 3)");
}

void tst_QJSEngine::evaluate()
{
    QFETCH(QString, code);
    newEngine();

    QBENCHMARK {
        (void)m_engine->evaluate(code);
    }
}

#if 0
void tst_QJSEngine::connectAndDisconnect()
{
    newEngine();
    QJSValue fun = m_engine->evaluate("(function() { })");
    QBENCHMARK {
        qScriptConnect(m_engine, SIGNAL(destroyed()), QJSValue(), fun);
        qScriptDisconnect(m_engine, SIGNAL(destroyed()), QJSValue(), fun);
    }
}

void tst_QJSEngine::evaluateProgram_data()
{
    evaluate_data();
}

void tst_QJSEngine::evaluateProgram()
{
    QFETCH(QString, code);
    QScriptProgram program(code);
    newEngine();

    QBENCHMARK {
        (void)m_engine->evaluate(program);
    }
}
#endif

void tst_QJSEngine::globalObject()
{
    newEngine();
    QBENCHMARK {
        m_engine->globalObject();
    }
}

#if 0
void tst_QJSEngine::isEvaluating()
{
    newEngine();
    QBENCHMARK {
        m_engine->isEvaluating();
    }
}
#endif

void tst_QJSEngine::newArray_data()
{
    QTest::addColumn<int>("size");
    QTest::newRow("size=0") << 0;
    QTest::newRow("size=10") << 10;
    QTest::newRow("size=100") << 0;
    QTest::newRow("size=1000") << 0;
    QTest::newRow("size=10000") << 0;
    QTest::newRow("size=50000") << 0;
}

void tst_QJSEngine::newArray()
{
    QFETCH(int, size);
    newEngine();
    QBENCHMARK {
        m_engine->newArray(size);
    }
}

void tst_QJSEngine::newDate()
{
    newEngine();
    QDateTime dt = QDateTime::currentDateTime();
    QBENCHMARK {
        m_engine->toScriptValue(dt);
    }
}

void tst_QJSEngine::newObject()
{
    newEngine();
    QBENCHMARK {
        (void)m_engine->newObject();
    }
}

#if 0
void tst_QJSEngine::newObjectWithScriptClass()
{
    newEngine();
    QScriptClass cls(m_engine);
    QBENCHMARK {
        m_engine->newObject(&cls);
    }
}

void tst_QJSEngine::newQMetaObject()
{
    newEngine();
    QBENCHMARK {
        m_engine->newQMetaObject(&QJSEngine::staticMetaObject);
    }
}
#endif

void tst_QJSEngine::newQObject()
{
    newEngine();
    QBENCHMARK {
        (void)m_engine->newQObject(QCoreApplication::instance());
    }
}

#if 0
static QJSValue testFunction(QScriptContext *, QJSEngine *)
{
    return 0;
}

void tst_QJSEngine::newFunction()
{
    newEngine();
    QBENCHMARK {
        (void)m_engine->newFunction(testFunction);
    }
}
#endif

void tst_QJSEngine::newRegExp()
{
    newEngine();
    QRegExp re = QRegExp("foo");
    QBENCHMARK {
        m_engine->toScriptValue(re);
    }
}

void tst_QJSEngine::newVariant()
{
    newEngine();
    QVariant var(QPoint(10, 20));
    QBENCHMARK {
        (void)m_engine->toScriptValue(var);
    }
}

void tst_QJSEngine::undefinedValue()
{
    newEngine();
    QVariant var;
    QBENCHMARK {
        m_engine->toScriptValue(var);
    }
}

void tst_QJSEngine::collectGarbage()
{
    newEngine();
    QBENCHMARK {
        m_engine->collectGarbage();
    }
}

#if 0
void tst_QJSEngine::availableExtensions()
{
    newEngine();
    QBENCHMARK {
        m_engine->availableExtensions();
    }
}

void tst_QJSEngine::importedExtensions()
{
    newEngine();
    QBENCHMARK {
        m_engine->importedExtensions();
    }
}

void tst_QJSEngine::currentContext()
{
    newEngine();
    QBENCHMARK {
        m_engine->currentContext();
    }
}

void tst_QJSEngine::pushAndPopContext()
{
    newEngine();
    QBENCHMARK {
        (void)m_engine->pushContext();
        m_engine->popContext();
    }
}
#endif

#if 0
void tst_QJSEngine::toStringHandle()
{
    newEngine();
    QString str = QString::fromLatin1("foobarbaz");
    QBENCHMARK {
        (void)m_engine->toStringHandle(str);
    }
}
#endif

void tst_QJSEngine::castValueToQreal()
{
    QJSValue val(123);
    QBENCHMARK {
        (void)qjsvalue_cast<qreal>(val);
    }
}

#if 0
static QJSValue native_function(QScriptContext *, QJSEngine *)
{
    return 42;
}

void tst_QJSEngine::nativeCall()
{
    newEngine();
    m_engine->globalObject().setProperty("fun", m_engine->newFunction(native_function));
    QBENCHMARK{
        m_engine->evaluate("var w = 0; for (i = 0; i < 100000; ++i) {\n"
                     "  w += fun() + fun(); w -= fun(); fun(); w -= fun(); }");
    }
}

void tst_QJSEngine::installTranslatorFunctions()
{
    newEngine();
    QBENCHMARK {
        m_engine->installTranslatorFunctions();
    }
}

void tst_QJSEngine::translation_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("fileName");
    QTest::newRow("no translation") << "\"hello world\"" << "";
    QTest::newRow("qsTr") << "qsTr(\"hello world\")" << "";
    QTest::newRow("qsTranslate") << "qsTranslate(\"\", \"hello world\")" << "";
    QTest::newRow("qsTr:script.js") << "qsTr(\"hello world\")" << "script.js";
}

void tst_QJSEngine::translation()
{
    QFETCH(QString, text);
    QFETCH(QString, fileName);
    newEngine();
    m_engine->installTranslatorFunctions();

    QBENCHMARK {
        (void)m_engine->evaluate(text, fileName);
    }
}
#endif

#if 0
void tst_QJSEngine::readScopeProperty_data()
{
    QTest::addColumn<bool>("staticScope");
    QTest::addColumn<bool>("nestedScope");
    QTest::newRow("single dynamic scope") << false << false;
    QTest::newRow("single static scope") << true << false;
    QTest::newRow("double dynamic scope") << false << true;
    QTest::newRow("double static scope") << true << true;
}

void tst_QJSEngine::readScopeProperty()
{
    QFETCH(bool, staticScope);
    QFETCH(bool, nestedScope);

    newEngine();
    QScriptContext *ctx = m_engine->pushContext();

    QJSValue scope;
    if (staticScope)
        scope = QScriptDeclarativeClass::newStaticScopeObject(m_engine);
    else
        scope = m_engine->newObject();
    scope.setProperty("foo", 123);
    ctx->pushScope(scope);

    if (nestedScope) {
        QJSValue scope2;
        if (staticScope)
            scope2 = QScriptDeclarativeClass::newStaticScopeObject(m_engine);
        else
            scope2 = m_engine->newObject();
        scope2.setProperty("bar", 456); // ensure a miss in inner scope
        ctx->pushScope(scope2);
    }

    QJSValue fun = m_engine->evaluate("(function() {\n"
                                       "  for (var i = 0; i < 10000; ++i) {\n"
                                       "    foo; foo; foo; foo; foo; foo; foo; foo;\n"
                                       "  }\n"
                                       "})");
    m_engine->popContext();
    QVERIFY(fun.isFunction());
    QBENCHMARK {
        fun.call();
    }
}

void tst_QJSEngine::evaluateInNewContext()
{
    QJSEngine engine;
    QBENCHMARK {
        engine.pushContext();
        engine.evaluate("var a = 10");
        engine.popContext();
    }
}

void tst_QJSEngine::evaluateInNewContextWithScope()
{
    QJSEngine engine;
    QJSValue scope = engine.newObject();
    scope.setProperty("foo", 123);
    QBENCHMARK {
        QScriptContext *ctx = engine.pushContext();
        ctx->pushScope(scope);
        engine.evaluate("foo");
        engine.popContext();
    }
}

// Binding expressions in QML are implemented as anonymous functions
// with custom scopes.
void tst_QJSEngine::evaluateBindingExpression()
{
    QJSEngine engine;
    QScriptContext *ctx = engine.pushContext();
    QJSValue scope = engine.newObject();
    scope.setProperty("foo", 123);
    ctx->pushScope(scope);
    QJSValue fun = engine.evaluate("(function() { return foo; })");
    QVERIFY(fun.isFunction());
    engine.popContext();
    QVERIFY(fun.call().equals(scope.property("foo")));
    QJSValue receiver = engine.globalObject();
    QBENCHMARK {
        fun.call(receiver);
    }
}
#endif

QTEST_MAIN(tst_QJSEngine)
#include "tst_qjsengine.moc"
