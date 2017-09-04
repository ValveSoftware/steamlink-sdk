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

#include <qtest.h>
#include <QtScript>

#include <QtScript/private/qscriptdeclarativeclass_p.h>

Q_DECLARE_METATYPE(QScriptValue)

class tst_QScriptEngine : public QObject
{
    Q_OBJECT

public:
    tst_QScriptEngine();
    virtual ~tst_QScriptEngine();

public slots:
    void init();
    void cleanup();

private slots:
    void constructor();
    void defaultPrototype();
    void setDefaultPrototype();
    void evaluate_data();
    void evaluate();
    void evaluateProgram_data();
    void evaluateProgram();
    void connectAndDisconnect();
    void globalObject();
    void hasUncaughtException();
    void isEvaluating();
    void newArray_data();
    void newArray();
    void newDate();
    void newDateFromMs();
    void newObject();
    void newObjectWithScriptClass();
    void newQMetaObject();
    void newQObject();
    void newFunction();
    void newRegExp();
    void newRegExpFromString();
    void newVariant();
    void nullValue();
    void undefinedValue();
    void collectGarbage();
    void currentContext();
    void pushAndPopContext();
    void availableExtensions();
    void importedExtensions();
    void toObject_data();
    void toObject();
    void toStringHandle();
    void castValueToQreal();
    void nativeCall();
    void installTranslatorFunctions();
    void translation_data();
    void translation();
    void readScopeProperty_data();
    void readScopeProperty();

private:
    void defineStandardTestValues();
    void newEngine()
    {
        delete m_engine;
        m_engine = new QScriptEngine;
    }

    QScriptEngine *m_engine;
};

tst_QScriptEngine::tst_QScriptEngine()
    : m_engine(0)
{
}

tst_QScriptEngine::~tst_QScriptEngine()
{
    delete m_engine;
}

void tst_QScriptEngine::init()
{
}

void tst_QScriptEngine::cleanup()
{
}

void tst_QScriptEngine::constructor()
{
    QBENCHMARK {
        QScriptEngine engine;
        (void)engine.parent();
    }
}

void tst_QScriptEngine::defaultPrototype()
{
    newEngine();
    int type = qMetaTypeId<int>();
    m_engine->setDefaultPrototype(type, m_engine->newObject());
    QBENCHMARK {
        m_engine->defaultPrototype(type);
    }
}

void tst_QScriptEngine::setDefaultPrototype()
{
    newEngine();
    int type = qMetaTypeId<int>();
    QScriptValue proto = m_engine->newObject();
    QBENCHMARK {
        m_engine->setDefaultPrototype(type, proto);
    }
}

void tst_QScriptEngine::evaluate_data()
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

void tst_QScriptEngine::evaluate()
{
    QFETCH(QString, code);
    newEngine();

    QBENCHMARK {
        (void)m_engine->evaluate(code);
    }
}

void tst_QScriptEngine::connectAndDisconnect()
{
    newEngine();
    QScriptValue fun = m_engine->evaluate("(function() { })");
    QBENCHMARK {
        qScriptConnect(m_engine, SIGNAL(destroyed()), QScriptValue(), fun);
        qScriptDisconnect(m_engine, SIGNAL(destroyed()), QScriptValue(), fun);
    }
}

void tst_QScriptEngine::evaluateProgram_data()
{
    evaluate_data();
}

void tst_QScriptEngine::evaluateProgram()
{
    QFETCH(QString, code);
    QScriptProgram program(code);
    newEngine();

    QBENCHMARK {
        (void)m_engine->evaluate(program);
    }
}

void tst_QScriptEngine::globalObject()
{
    newEngine();
    QBENCHMARK {
        m_engine->globalObject();
    }
}

void tst_QScriptEngine::hasUncaughtException()
{
    newEngine();
    QBENCHMARK {
        m_engine->hasUncaughtException();
    }
}

void tst_QScriptEngine::isEvaluating()
{
    newEngine();
    QBENCHMARK {
        m_engine->isEvaluating();
    }
}

void tst_QScriptEngine::newArray_data()
{
    QTest::addColumn<int>("size");
    QTest::newRow("size=0") << 0;
    QTest::newRow("size=10") << 10;
    QTest::newRow("size=100") << 0;
    QTest::newRow("size=1000") << 0;
    QTest::newRow("size=10000") << 0;
    QTest::newRow("size=50000") << 0;
}

void tst_QScriptEngine::newArray()
{
    QFETCH(int, size);
    newEngine();
    QBENCHMARK {
        m_engine->newArray(size);
    }
}

void tst_QScriptEngine::newDate()
{
    newEngine();
    QDateTime dt = QDateTime::currentDateTime();
    QBENCHMARK {
        m_engine->newDate(dt);
    }
}

void tst_QScriptEngine::newDateFromMs()
{
    newEngine();
    QBENCHMARK {
        m_engine->newDate(0);
    }
}

void tst_QScriptEngine::newObject()
{
    newEngine();
    QBENCHMARK {
        (void)m_engine->newObject();
    }
}

void tst_QScriptEngine::newObjectWithScriptClass()
{
    newEngine();
    QScriptClass cls(m_engine);
    QBENCHMARK {
        m_engine->newObject(&cls);
    }
}

void tst_QScriptEngine::newQMetaObject()
{
    newEngine();
    QBENCHMARK {
        m_engine->newQMetaObject(&QScriptEngine::staticMetaObject);
    }
}

void tst_QScriptEngine::newQObject()
{
    newEngine();
    QBENCHMARK {
        (void)m_engine->newQObject(QCoreApplication::instance());
    }
}

static QScriptValue testFunction(QScriptContext *, QScriptEngine *)
{
    return 0;
}

void tst_QScriptEngine::newFunction()
{
    newEngine();
    QBENCHMARK {
        (void)m_engine->newFunction(testFunction);
    }
}

void tst_QScriptEngine::newRegExp()
{
    newEngine();
    QRegExp re = QRegExp("foo");
    QBENCHMARK {
        m_engine->newRegExp(re);
    }
}

void tst_QScriptEngine::newRegExpFromString()
{
    newEngine();
    QString pattern("foo");
    QString flags("gim");
    QBENCHMARK {
        m_engine->newRegExp(pattern, flags);
    }
}

void tst_QScriptEngine::newVariant()
{
    newEngine();
    QVariant var(123);
    QBENCHMARK {
        (void)m_engine->newVariant(var);
    }
}

void tst_QScriptEngine::nullValue()
{
    newEngine();
    QBENCHMARK {
        m_engine->nullValue();
    }
}

void tst_QScriptEngine::undefinedValue()
{
    newEngine();
    QBENCHMARK {
        m_engine->undefinedValue();
    }
}

void tst_QScriptEngine::collectGarbage()
{
    newEngine();
    QBENCHMARK {
        m_engine->collectGarbage();
    }
}

void tst_QScriptEngine::availableExtensions()
{
    newEngine();
    QBENCHMARK {
        m_engine->availableExtensions();
    }
}

void tst_QScriptEngine::importedExtensions()
{
    newEngine();
    QBENCHMARK {
        m_engine->importedExtensions();
    }
}

void tst_QScriptEngine::currentContext()
{
    newEngine();
    QBENCHMARK {
        m_engine->currentContext();
    }
}

void tst_QScriptEngine::pushAndPopContext()
{
    newEngine();
    QBENCHMARK {
        (void)m_engine->pushContext();
        m_engine->popContext();
    }
}

void tst_QScriptEngine::toObject_data()
{
    newEngine();
    QTest::addColumn<QScriptValue>("val");
    QTest::newRow("bool") << m_engine->evaluate("true");
    QTest::newRow("number") << m_engine->evaluate("123");
    QTest::newRow("string") << m_engine->evaluate("'ciao'");
    QTest::newRow("null") << m_engine->evaluate("null");
    QTest::newRow("undefined") << m_engine->evaluate("undefined");
    QTest::newRow("object") << m_engine->evaluate("({foo:123})");
    QTest::newRow("array") << m_engine->evaluate("[10,20,30]");
    QTest::newRow("function") << m_engine->evaluate("(function foo(a, b, c) { return a + b + c; })");
    QTest::newRow("date") << m_engine->evaluate("new Date");
    QTest::newRow("regexp") << m_engine->evaluate("new RegExp('foo')");
    QTest::newRow("error") << m_engine->evaluate("new Error");

    QTest::newRow("qobject") << m_engine->newQObject(this);
    QTest::newRow("qmetaobject") << m_engine->newQMetaObject(&QScriptEngine::staticMetaObject);
    QTest::newRow("variant") << m_engine->newVariant(123);
    QTest::newRow("qscriptclassobject") << m_engine->newObject(new QScriptClass(m_engine));

    QTest::newRow("invalid") << QScriptValue();
    QTest::newRow("bool-no-engine") << QScriptValue(true);
    QTest::newRow("number-no-engine") << QScriptValue(123.0);
    QTest::newRow("string-no-engine") << QScriptValue(QString::fromLatin1("hello"));
    QTest::newRow("null-no-engine") << QScriptValue(QScriptValue::NullValue);
    QTest::newRow("undefined-no-engine") << QScriptValue(QScriptValue::UndefinedValue);
}

void tst_QScriptEngine::toObject()
{
    QFETCH(QScriptValue, val);
    QBENCHMARK {
        m_engine->toObject(val);
    }
}

void tst_QScriptEngine::toStringHandle()
{
    newEngine();
    QString str = QString::fromLatin1("foobarbaz");
    QBENCHMARK {
        (void)m_engine->toStringHandle(str);
    }
}

void tst_QScriptEngine::castValueToQreal()
{
    QScriptValue val(123);
    QBENCHMARK {
        (void)qscriptvalue_cast<qreal>(val);
    }
}

static QScriptValue native_function(QScriptContext *, QScriptEngine *)
{
    return 42;
}

void tst_QScriptEngine::nativeCall()
{
    newEngine();
    m_engine->globalObject().setProperty("fun", m_engine->newFunction(native_function));
    QBENCHMARK{
        m_engine->evaluate("var w = 0; for (i = 0; i < 100000; ++i) {\n"
                     "  w += fun() + fun(); w -= fun(); fun(); w -= fun(); }");
    }
}

void tst_QScriptEngine::installTranslatorFunctions()
{
    newEngine();
    QBENCHMARK {
        m_engine->installTranslatorFunctions();
    }
}

void tst_QScriptEngine::translation_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("fileName");
    QTest::newRow("no translation") << "\"hello world\"" << "";
    QTest::newRow("qsTr") << "qsTr(\"hello world\")" << "";
    QTest::newRow("qsTranslate") << "qsTranslate(\"\", \"hello world\")" << "";
    QTest::newRow("qsTr:script.js") << "qsTr(\"hello world\")" << "script.js";
}

void tst_QScriptEngine::translation()
{
    QFETCH(QString, text);
    QFETCH(QString, fileName);
    newEngine();
    m_engine->installTranslatorFunctions();

    QBENCHMARK {
        (void)m_engine->evaluate(text, fileName);
    }
}

void tst_QScriptEngine::readScopeProperty_data()
{
    QTest::addColumn<bool>("staticScope");
    QTest::addColumn<bool>("nestedScope");
    QTest::newRow("single dynamic scope") << false << false;
    QTest::newRow("single static scope") << true << false;
    QTest::newRow("double dynamic scope") << false << true;
    QTest::newRow("double static scope") << true << true;
}

void tst_QScriptEngine::readScopeProperty()
{
    QFETCH(bool, staticScope);
    QFETCH(bool, nestedScope);

    newEngine();
    QScriptContext *ctx = m_engine->pushContext();

    QScriptValue scope;
    if (staticScope)
        scope = QScriptDeclarativeClass::newStaticScopeObject(m_engine);
    else
        scope = m_engine->newObject();
    scope.setProperty("foo", 123);
    ctx->pushScope(scope);

    if (nestedScope) {
        QScriptValue scope2;
        if (staticScope)
            scope2 = QScriptDeclarativeClass::newStaticScopeObject(m_engine);
        else
            scope2 = m_engine->newObject();
        scope2.setProperty("bar", 456); // ensure a miss in inner scope
        ctx->pushScope(scope2);
    }

    QScriptValue fun = m_engine->evaluate("(function() {\n"
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

QTEST_MAIN(tst_QScriptEngine)
#include "tst_qscriptengine.moc"
