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

#include <QtScript/qscriptengineagent.h>
#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptprogram.h>
#include <qscriptvalueiterator.h>

QT_BEGIN_NAMESPACE
extern bool qt_script_isJITEnabled();
QT_END_NAMESPACE

class tst_QScriptEngineAgent : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double testProperty READ testProperty WRITE setTestProperty)

public:
    tst_QScriptEngineAgent();
    virtual ~tst_QScriptEngineAgent();

    double testProperty() const { return m_testProperty; }
    void setTestProperty(double val) { m_testProperty = val; }

public slots:
    double testSlot(double arg) { return arg; }

signals:
    void testSignal(double arg);

private slots:
    void unloadRecursion();
    void scriptLoadAndUnload_statement();
    void scriptLoadAndUnload();
    void scriptLoadAndUnload_eval();
    void contextPushAndPop();
    void functionEntryAndExit_semicolon();
    void functionEntryAndExit_expression();
    void functionEntryAndExit_functionCall();
    void functionEntryAndExit_functionCallWithoutReturn();
    void functionEntryAndExit_functionDefinition();
    void functionEntryAndExit_native();
    void functionEntryAndExit_native2();
    void functionEntryAndExit_nativeThrowing();
    void functionEntryAndExit_builtin_data();
    void functionEntryAndExit_builtin();
    void functionEntryAndExit_objects();
    void functionEntryAndExit_slots();
    void functionEntryAndExit_property_set();
    void functionEntryAndExit_property_get();
    void functionEntryAndExit_call();
    void functionEntryAndExit_functionReturn_construct();
    void functionEntryAndExit_functionReturn_call();
    void functionEntryAndExit_objectCall();
    void positionChange_1();
    void positionChange_2();
    void positionChange_3();
    void exceptionThrowAndCatch();
    void eventOrder_assigment();
    void eventOrder_functionDefinition();
    void eventOrder_throwError();
    void eventOrder_throwAndCatch();
    void eventOrder_functions();
    void eventOrder_throwCatchFinally();
    void eventOrder_signalsHandling();
    void recursiveObserve();
    void multipleAgents();
    void syntaxError();
    void extension_invoctaion();
    void extension();
    void isEvaluatingInExtension();
    void hasUncaughtException();
    void evaluateProgram();
    void evaluateProgram_SyntaxError();
    void evaluateNullProgram();
    void QTBUG6108();
    void backtraces_data();
    void backtraces();

private:
    double m_testProperty;
};

tst_QScriptEngineAgent::tst_QScriptEngineAgent()
{
}

tst_QScriptEngineAgent::~tst_QScriptEngineAgent()
{
}

struct ScriptEngineEvent
{
    enum Type {
        ScriptLoad,
        ScriptUnload,//1
        ContextPush,
        ContextPop,  //3
        FunctionEntry, //4
        FunctionExit, //5
        PositionChange,
        ExceptionThrow,//7
        ExceptionCatch,
        DebuggerInvocationRequest
    };

    Type type;

    qint64 scriptId;
    QString script;
    QString fileName;
    int lineNumber;
    int columnNumber;
    QScriptValue value;
    bool hasExceptionHandler;

    ScriptEngineEvent(qint64 scriptId,
                      const QString &script, const QString &fileName,
                      int lineNumber)
        : type(ScriptLoad), scriptId(scriptId),
          script(script), fileName(fileName),
          lineNumber(lineNumber)
        { }

    ScriptEngineEvent(Type type, qint64 scriptId = -777)
        : type(type), scriptId(scriptId)
        { }

    ScriptEngineEvent(Type type, qint64 scriptId,
                      const QScriptValue &value)
        : type(type), scriptId(scriptId),
          value(value)
        { }

    ScriptEngineEvent(qint64 scriptId,
                      int lineNumber, int columnNumber)
        : type(PositionChange), scriptId(scriptId),
          lineNumber(lineNumber), columnNumber(columnNumber)
        { }

    ScriptEngineEvent(qint64 scriptId,
                      const QScriptValue &exception, bool hasHandler)
        : type(ExceptionThrow), scriptId(scriptId),
          value(exception), hasExceptionHandler(hasHandler)
        { }

    static QString typeToQString(Type t)
    {
        switch (t) {
            case ScriptEngineEvent::ScriptLoad: return "ScriptLoad";
            case ScriptEngineEvent::ScriptUnload: return "ScriptUnload";
            case ScriptEngineEvent::ContextPush: return "ContextPush";
            case ScriptEngineEvent::ContextPop: return "ContextPop";
            case ScriptEngineEvent::FunctionEntry: return "FunctionEntry";
            case ScriptEngineEvent::FunctionExit: return "FunctionExit";
            case ScriptEngineEvent::PositionChange: return "PositionChange";
            case ScriptEngineEvent::ExceptionThrow: return "ExceptionThrow";
            case ScriptEngineEvent::ExceptionCatch: return "ExceptionCatch";
            case ScriptEngineEvent::DebuggerInvocationRequest: return "DebuggerInvocationRequest";
            }
    }
};

class ScriptEngineSpy : public QScriptEngineAgent, public QList<ScriptEngineEvent>
{
public:
    enum IgnoreFlag {
        IgnoreScriptLoad = 0x001,
        IgnoreScriptUnload = 0x002,
        IgnoreFunctionEntry = 0x004,
        IgnoreFunctionExit = 0x008,
        IgnorePositionChange = 0x010,
        IgnoreExceptionThrow = 0x020,
        IgnoreExceptionCatch = 0x040,
        IgnoreContextPush = 0x0100,
        IgnoreContextPop = 0x0200,
        IgnoreDebuggerInvocationRequest = 0x0400
    };

    ScriptEngineSpy(QScriptEngine *engine, int ignores = 0);
    ~ScriptEngineSpy();

    void enableIgnoreFlags(int flags)
        { m_ignores |= flags; }
    void disableIgnoreFlags(int flags)
        { m_ignores &= ~flags; }

protected:
    void scriptLoad(qint64 id, const QString &script,
                    const QString &fileName, int lineNumber);
    void scriptUnload(qint64 id);

    void contextPush();
    void contextPop();

    void functionEntry(qint64 scriptId);
    void functionExit(qint64 scriptId, const QScriptValue &returnValue);

    void positionChange(qint64 scriptId,
                        int lineNumber, int columnNumber);

    void exceptionThrow(qint64 scriptId, const QScriptValue &exception,
                        bool hasHandler);
    void exceptionCatch(qint64 scriptId, const QScriptValue &exception);

    bool supportsExtension(Extension ext) const;
    QVariant extension(Extension ext, const QVariant &arg);

private:
    int m_ignores;
};

ScriptEngineSpy::ScriptEngineSpy(QScriptEngine *engine, int ignores)
    : QScriptEngineAgent(engine)
{
    m_ignores = ignores;
    engine->setAgent(this);
}

ScriptEngineSpy::~ScriptEngineSpy()
{
}

void ScriptEngineSpy::scriptLoad(qint64 id, const QString &script,
                                 const QString &fileName, int lineNumber)
{
    if (!(m_ignores & IgnoreScriptLoad))
        append(ScriptEngineEvent(id, script, fileName, lineNumber));
}

void ScriptEngineSpy::scriptUnload(qint64 id)
{
    if (!(m_ignores & IgnoreScriptUnload))
        append(ScriptEngineEvent(ScriptEngineEvent::ScriptUnload, id));
}

void ScriptEngineSpy::contextPush()
{
    if (!(m_ignores & IgnoreContextPush))
        append(ScriptEngineEvent(ScriptEngineEvent::ContextPush));
}

void ScriptEngineSpy::contextPop()
{
    if (!(m_ignores & IgnoreContextPop))
        append(ScriptEngineEvent(ScriptEngineEvent::ContextPop));
}

void ScriptEngineSpy::functionEntry(qint64 scriptId)
{
    if (!(m_ignores & IgnoreFunctionEntry))
        append(ScriptEngineEvent(ScriptEngineEvent::FunctionEntry, scriptId));
}

void ScriptEngineSpy::functionExit(qint64 scriptId,
                                   const QScriptValue &returnValue)
{
    if (!(m_ignores & IgnoreFunctionExit))
        append(ScriptEngineEvent(ScriptEngineEvent::FunctionExit, scriptId, returnValue));
}

void ScriptEngineSpy::positionChange(qint64 scriptId,
                                     int lineNumber, int columnNumber)
{
    if (!(m_ignores & IgnorePositionChange))
        append(ScriptEngineEvent(scriptId, lineNumber, columnNumber));
}

void ScriptEngineSpy::exceptionThrow(qint64 scriptId,
                                     const QScriptValue &exception, bool hasHandler)
{
    if (!(m_ignores & IgnoreExceptionThrow))
        append(ScriptEngineEvent(scriptId, exception, hasHandler));
}

void ScriptEngineSpy::exceptionCatch(qint64 scriptId,
                                     const QScriptValue &exception)
{
    if (!(m_ignores & IgnoreExceptionCatch))
        append(ScriptEngineEvent(ScriptEngineEvent::ExceptionCatch, scriptId, exception));
}

bool ScriptEngineSpy::supportsExtension(Extension ext) const
{
    if (ext == DebuggerInvocationRequest)
        return !(m_ignores & IgnoreDebuggerInvocationRequest);
    return false;
}

QVariant ScriptEngineSpy::extension(Extension ext, const QVariant &arg)
{
    if (ext == DebuggerInvocationRequest) {
        QVariantList lst = arg.toList();
        qint64 scriptId = lst.at(0).toLongLong();
        int lineNumber = lst.at(1).toInt();
        int columnNumber = lst.at(2).toInt();
        ScriptEngineEvent evt(scriptId, lineNumber, columnNumber);
        evt.type = ScriptEngineEvent::DebuggerInvocationRequest;
        append(evt);
        return QString::fromLatin1("extension(DebuggerInvocationRequest)");
    }
    return QVariant();
}

static void collectScriptObjects(QScriptEngine *engine)
{
    // We call garbage collection few times to collect objects that
    // are unreferenced after first gc. We try to force full gc.
    engine->collectGarbage();
    engine->collectGarbage();
    engine->collectGarbage();
}

class EvaluatingAgent : public QScriptEngineAgent {
public:
    EvaluatingAgent(QScriptEngine *engine)
        : QScriptEngineAgent(engine)
        , count(0)
    {}

    virtual void scriptUnload(qint64)
    {
        if (++count > 10) // recursion breaker.
            return;
        // check if recursive evaluation works
        engine()->evaluate(";");
        collectScriptObjects(engine());
    }

    bool isOk() const { return count > 10; }
private:
    int count;
};

void tst_QScriptEngineAgent::unloadRecursion()
{
    QScriptEngine engine;
    EvaluatingAgent *agent = new EvaluatingAgent(&engine);
    engine.setAgent(agent);
    engine.evaluate(";");
    collectScriptObjects(&engine);
    QVERIFY(agent->isOk());
}

void tst_QScriptEngineAgent::scriptLoadAndUnload_statement()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreScriptLoad
                                                       | ScriptEngineSpy::IgnoreScriptUnload));
    QCOMPARE(eng.agent(), (QScriptEngineAgent*)spy);
    {
        spy->clear();
        QString code = ";";
        QString fileName = "foo.qs";
        int lineNumber = 123;
        eng.evaluate(code, fileName, lineNumber);

        // Script object have to be garbage collected first.
        collectScriptObjects(&eng);
        QCOMPARE(spy->count(), 2);

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).script, code);
        QCOMPARE(spy->at(0).fileName, fileName);
        QCOMPARE(spy->at(0).lineNumber, lineNumber);

        QCOMPARE(spy->at(1).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
    }

    {
        spy->clear();
        QString code = ";";
        QString fileName = "bar.qs";
        int lineNumber = 456;
        eng.evaluate(code, fileName, lineNumber);

        // Script object have to be garbage collected first.
        collectScriptObjects(&eng);
        QCOMPARE(spy->count(), 2);

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).script, code);
        QCOMPARE(spy->at(0).fileName, fileName);
        QCOMPARE(spy->at(0).lineNumber, lineNumber);

        QCOMPARE(spy->at(1).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
    }
    delete spy;
}

void tst_QScriptEngineAgent::scriptLoadAndUnload()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreScriptLoad
                                                       | ScriptEngineSpy::IgnoreScriptUnload));
    QCOMPARE(eng.agent(), (QScriptEngineAgent*)spy);
    {
        spy->clear();
        QString code = "function foo() { print('ciao'); }";
        QString fileName = "baz.qs";
        int lineNumber = 789;
        eng.evaluate(code, fileName, lineNumber);

        QCOMPARE(spy->count(), 1);

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).script, code);
        QCOMPARE(spy->at(0).fileName, fileName);
        QCOMPARE(spy->at(0).lineNumber, lineNumber);

        code = "foo = null";
        eng.evaluate(code);
        collectScriptObjects(&eng); // foo() is GC'ed
        QCOMPARE(spy->count(), 4);

        QCOMPARE(spy->at(1).type, ScriptEngineEvent::ScriptLoad);
        QVERIFY(spy->at(1).scriptId != -1);
        QVERIFY(spy->at(1).scriptId != spy->at(0).scriptId);
        QCOMPARE(spy->at(1).script, code);
        QCOMPARE(spy->at(1).lineNumber, 1);

        QCOMPARE(spy->at(2).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(2).scriptId, spy->at(1).scriptId);

        QCOMPARE(spy->at(3).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(3).scriptId, spy->at(0).scriptId);
    }

    {
        spy->clear();
        QString code = "function foo() { return function() { print('ciao'); } }";
        QString fileName = "foo.qs";
        int lineNumber = 123;
        eng.evaluate(code, fileName, lineNumber);

        QCOMPARE(spy->count(), 1);

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).script, code);
        QCOMPARE(spy->at(0).fileName, fileName);
        QCOMPARE(spy->at(0).lineNumber, lineNumber);

        code = "bar = foo(); foo = null";
        eng.evaluate(code);
        collectScriptObjects(&eng);
        QCOMPARE(spy->count(), 3);

        QCOMPARE(spy->at(1).type, ScriptEngineEvent::ScriptLoad);
        QVERIFY(spy->at(1).scriptId != -1);
        QVERIFY(spy->at(1).scriptId != spy->at(0).scriptId);
        QCOMPARE(spy->at(1).script, code);

        QCOMPARE(spy->at(2).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(2).scriptId, spy->at(1).scriptId);

        collectScriptObjects(&eng); // foo() is not GC'ed
        QCOMPARE(spy->count(), 3);

        code = "bar = null";
        eng.evaluate(code);
        collectScriptObjects(&eng); // foo() is GC'ed
        QCOMPARE(spy->count(), 6);
    }
    delete spy;
}

void tst_QScriptEngineAgent::scriptLoadAndUnload_eval()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreScriptLoad
                                                       | ScriptEngineSpy::IgnoreScriptUnload));
    {
        spy->clear();
        eng.evaluate("eval('function foo() { print(123); }')");

        QEXPECT_FAIL("","QTBUG-6140 Eval is threaded in different way that in old backend", Abort);
        QCOMPARE(spy->count(), 3);

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        QVERIFY(spy->at(0).scriptId != -1);

        QCOMPARE(spy->at(1).type, ScriptEngineEvent::ScriptLoad);
        QVERIFY(spy->at(1).scriptId != -1);
        QVERIFY(spy->at(1).scriptId != spy->at(0).scriptId);

        QCOMPARE(spy->at(2).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);
    }
    delete spy;
}

void tst_QScriptEngineAgent::contextPushAndPop()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreContextPush
                                                       | ScriptEngineSpy::IgnoreContextPop));

    {
        spy->clear();
        eng.pushContext();
        eng.popContext();
        QCOMPARE(spy->count(), 2);

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ContextPush);
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::ContextPop);
    }
}

static QScriptValue nativeFunctionReturningArg(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->argument(0);
}

static QScriptValue nativeFunctionThrowingError(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->throwError(ctx->argument(0).toString());
}

static QScriptValue nativeFunctionCallingArg(QScriptContext *ctx, QScriptEngine *)
{
    return ctx->argument(0).call();
}

/** check behaiviour of ';' */
void tst_QScriptEngineAgent::functionEntryAndExit_semicolon()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    {
        spy->clear();
        eng.evaluate(";");

        QCOMPARE(spy->count(), 2);

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        QVERIFY(spy->at(0).scriptId != -1);

        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(1).value.isUndefined());
    }
    delete spy;
}

/** check behaiviour of expression */
void tst_QScriptEngineAgent::functionEntryAndExit_expression()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    {
        spy->clear();
        eng.evaluate("1 + 2");

        QCOMPARE(spy->count(), 2);

        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        QVERIFY(spy->at(0).scriptId != -1);

        // evaluate() exit
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(1).value.isNumber());
        QCOMPARE(spy->at(1).value.toNumber(), qsreal(3));
    }
    delete spy;
}

/** check behaiviour of standard function call */
void tst_QScriptEngineAgent::functionEntryAndExit_functionCall()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    {
        spy->clear();
        QVERIFY(eng.evaluate("(function() { return 123; } )()").toNumber()==123);

        QCOMPARE(spy->count(), 4);

        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        QVERIFY(spy->at(0).scriptId != -1);

        // anonymous function entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);

        // anonymous function exit
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(2).value.isNumber());
        QCOMPARE(spy->at(2).value.toNumber(), qsreal(123));

        // evaluate() exit
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(3).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(3).value.isNumber());
        QCOMPARE(spy->at(3).value.toNumber(), qsreal(123));
    }
    delete spy;
}

/** check behaiviour of standard function call */
void tst_QScriptEngineAgent::functionEntryAndExit_functionCallWithoutReturn()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    {
        spy->clear();
        eng.evaluate("(function() { var a = 123; } )()");

        QCOMPARE(spy->count(), 4);

        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        QVERIFY(spy->at(0).scriptId != -1);

        // anonymous function entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);

        // anonymous function exit
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);

        // evaluate() exit
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(3).scriptId, spy->at(0).scriptId);
    }
    delete spy;
}

/** check behaiviour of function definition */
void tst_QScriptEngineAgent::functionEntryAndExit_functionDefinition()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    {
        spy->clear();
        eng.evaluate("function foo() { return 456; }");
        QCOMPARE(spy->count(), 2);

        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        QVERIFY(spy->at(0).scriptId != -1);

        // evaluate() exit
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(1).value.isUndefined());

        eng.evaluate("foo()");
        QCOMPARE(spy->count(), 6);

        // evaluate() entry
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionEntry);
        QVERIFY(spy->at(2).scriptId != spy->at(0).scriptId);

        // foo() entry
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(3).scriptId, spy->at(0).scriptId);

        // foo() exit
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(4).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(4).value.isNumber());
        QCOMPARE(spy->at(4).value.toNumber(), qsreal(456));

        // evaluate() exit
        QCOMPARE(spy->at(5).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(5).scriptId, spy->at(2).scriptId);
        QVERIFY(spy->at(5).value.isNumber());
        QCOMPARE(spy->at(5).value.toNumber(), qsreal(456));
    }
    delete spy;
}

/** check behaiviour of native function */
void tst_QScriptEngineAgent::functionEntryAndExit_native()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    // native functions
    {
        QScriptValue fun = eng.newFunction(nativeFunctionReturningArg);
        eng.globalObject().setProperty("nativeFunctionReturningArg", fun);

        spy->clear();
        eng.evaluate("nativeFunctionReturningArg(123)");

        QCOMPARE(spy->count(), 4);

        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);

        // native function entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(1).scriptId, qint64(-1));

        // native function exit
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(2).scriptId, qint64(-1));
        QVERIFY(spy->at(2).value.isNumber());
        QCOMPARE(spy->at(2).value.toNumber(), qsreal(123));

        // evaluate() exit
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(3).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(3).value.isNumber());
        QCOMPARE(spy->at(3).value.toNumber(), qsreal(123));
    }
    delete spy;
}

/** check behaiviour of native function */
void tst_QScriptEngineAgent::functionEntryAndExit_native2()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    {
        QScriptValue fun = eng.newFunction(nativeFunctionCallingArg);
        eng.globalObject().setProperty("nativeFunctionCallingArg", fun);

        spy->clear();
        eng.evaluate("nativeFunctionCallingArg(function() { return 123; })");
        QCOMPARE(spy->count(), 6);

        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);

        // native function entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(1).scriptId, qint64(-1));

        // script function entry
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);

        // script function exit
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(3).scriptId, spy->at(0).scriptId);

        // native function exit
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(4).scriptId, qint64(-1));
        QVERIFY(spy->at(4).value.isNumber());
        QCOMPARE(spy->at(4).value.toNumber(), qsreal(123));

        // evaluate() exit
        QCOMPARE(spy->at(5).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(5).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(5).value.isNumber());
        QCOMPARE(spy->at(5).value.toNumber(), qsreal(123));
    }
    delete spy;
}

/** check behavior of native function throwing error*/
void tst_QScriptEngineAgent::functionEntryAndExit_nativeThrowing()
{
    /* This function was changed from old backend. JSC return more Entrys / Exits, (exactly +1)
       in exception creation time */

    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    {
        QScriptValue fun = eng.newFunction(nativeFunctionThrowingError);
        eng.globalObject().setProperty("nativeFunctionThrowingError", fun);

        spy->clear();
        eng.evaluate("nativeFunctionThrowingError('ciao')");
        QCOMPARE(spy->count(), 6);

        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);

        // native function entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(1).scriptId, qint64(-1));

        // Exception constructor entry
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(2).scriptId, qint64(-1));

        // Exception constructor exit
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(3).scriptId, qint64(-1));
        QVERIFY(spy->at(3).value.isError());

        // native function exit
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(4).scriptId, qint64(-1));
        QVERIFY(spy->at(4).value.isError());

        // evaluate() exit
        QCOMPARE(spy->at(5).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(5).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(5).value.isError());
    }
    delete spy;
}

void tst_QScriptEngineAgent::functionEntryAndExit_builtin_data()
{
  QTest::addColumn<QString>("script");
  QTest::addColumn<QString>("result");

  QTest::newRow("string native") << "'ciao'.toString()" << "ciao";
  QTest::newRow("string object") << "String('ciao').toString()" << "ciao";
  QTest::newRow("number native") << "(123).toString()" << "123";
  QTest::newRow("number object") << "Number(123).toString()" << "123";
  QTest::newRow("array native") << "['s','a'].toString()" << "s, a";
  QTest::newRow("array object") << "Array('s', 'a').toString()" << "s,a";
  QTest::newRow("boolean native") << "false.toString()" << "false";
  QTest::newRow("boolean object") << "Boolean(true).toString()" << "true";
  QTest::newRow("regexp native") << "/a/.toString()" << "/a/";
  QTest::newRow("regexp object") << "RegExp('a').toString()" << "/a/";
}

/** check behaiviour of built-in function */
void tst_QScriptEngineAgent::functionEntryAndExit_builtin()
{
    QSKIP("The test fails on platforms others than Linux. The issue will be fixed with next JSC update");
    QFETCH(QString, script);
    QFETCH(QString, result);
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    {
        spy->clear();
        eng.evaluate(script);

        if (qt_script_isJITEnabled()) {
            QEXPECT_FAIL("string native", "QTBUG-6187 Some events are missing when JIT is enabled", Abort);
            QEXPECT_FAIL("number native", "QTBUG-6187 Some events are missing when JIT is enabled", Abort);
            QEXPECT_FAIL("array native", "QTBUG-6187 Some events are missing when JIT is enabled", Abort);
            QEXPECT_FAIL("boolean native", "QTBUG-6187 Some events are missing when JIT is enabled", Abort);
            QEXPECT_FAIL("regexp native", "QTBUG-6187 Some events are missing when JIT is enabled", Abort);
        }
        QCOMPARE(spy->count(), 4);

        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);

        // built-in native function entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(1).scriptId, qint64(-1));

        // built-in native function exit
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(2).scriptId, qint64(-1));
        QCOMPARE(spy->at(2).value.toString(), QString(result));

        // evaluate() exit
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(3).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(3).value.isString());
        QCOMPARE(spy->at(3).value.toString(), QString(result));
    }
    delete spy;
}

/** check behaiviour of object creation*/
void tst_QScriptEngineAgent::functionEntryAndExit_objects()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    {
        spy->clear();
        eng.evaluate("Array(); Boolean(); Date(); Function(); Number(); Object(); RegExp(); String()");
        QCOMPARE(spy->count(), 18);

        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);

        // Array constructor entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(1).scriptId, qint64(-1));

        // Array constructor exit
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(2).scriptId, qint64(-1));
        QVERIFY(spy->at(2).value.isArray());

        // Boolean constructor entry
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(3).scriptId, qint64(-1));

        // Boolean constructor exit
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(4).scriptId, qint64(-1));
        QVERIFY(spy->at(4).value.isBoolean());

        // Date constructor entry
        QCOMPARE(spy->at(5).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(5).scriptId, qint64(-1));

        // Date constructor exit
        QCOMPARE(spy->at(6).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(6).scriptId, qint64(-1));
        QVERIFY(spy->at(6).value.isString());

        // Function constructor entry
        QCOMPARE(spy->at(7).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(7).scriptId, qint64(-1));

        // Function constructor exit
        QCOMPARE(spy->at(8).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(8).scriptId, qint64(-1));
        QVERIFY(spy->at(8).value.isFunction());

        // Number constructor entry
        QCOMPARE(spy->at(9).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(9).scriptId, qint64(-1));

        // Number constructor exit
        QCOMPARE(spy->at(10).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(10).scriptId, qint64(-1));
        QVERIFY(spy->at(10).value.isNumber());

        // Object constructor entry
        QCOMPARE(spy->at(11).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(11).scriptId, qint64(-1));

        // Object constructor exit
        QCOMPARE(spy->at(12).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(12).scriptId, qint64(-1));
        QVERIFY(spy->at(12).value.isObject());

        // RegExp constructor entry
        QCOMPARE(spy->at(13).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(13).scriptId, qint64(-1));

        // RegExp constructor exit
        QCOMPARE(spy->at(14).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(14).scriptId, qint64(-1));
        QVERIFY(spy->at(14).value.isRegExp());

        // String constructor entry
        QCOMPARE(spy->at(15).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(15).scriptId, qint64(-1));

        // String constructor exit
        QCOMPARE(spy->at(16).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(16).scriptId, qint64(-1));
        QVERIFY(spy->at(16).value.isString());

        // evaluate() exit
        QCOMPARE(spy->at(17).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(17).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(17).value.isString());
        QCOMPARE(spy->at(17).value.toString(), QString());
    }
    delete spy;
}

/** check behaiviour of slots*/
void tst_QScriptEngineAgent::functionEntryAndExit_slots()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    // slots
    {
        eng.globalObject().setProperty("qobj", eng.newQObject(this));
        spy->clear();
        eng.evaluate("qobj.testSlot(123)");
        QCOMPARE(spy->count(), 4);
        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        // testSlot() entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(1).scriptId, qint64(-1));
        // testSlot() exit
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(2).scriptId, qint64(-1));
        QVERIFY(spy->at(2).value.isNumber());
        QCOMPARE(spy->at(2).value.toNumber(), qsreal(123));
        // evaluate() exit
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit);
    }
    delete spy;
}

/** check behaiviour of property accessors*/
void tst_QScriptEngineAgent::functionEntryAndExit_property_set()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    // property accessors
    {
        eng.globalObject().setProperty("qobj", eng.newQObject(this));
        // set
        spy->clear();
        eng.evaluate("qobj.testProperty = 456");
        QCOMPARE(spy->count(), 4);
        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        // setTestProperty() entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(1).scriptId, qint64(-1));
        // setTestProperty() exit
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(2).scriptId, qint64(-1));
        QVERIFY(spy->at(2).value.isNumber());
        QCOMPARE(spy->at(2).value.toNumber(), testProperty());
        // evaluate() exit
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit);
        QVERIFY(spy->at(3).value.strictlyEquals(spy->at(2).value));
    }
    delete spy;
}

/** check behaiviour of property accessors*/
void tst_QScriptEngineAgent::functionEntryAndExit_property_get()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    // property accessors
    {
        eng.globalObject().setProperty("qobj", eng.newQObject(this));
        // set
        eng.evaluate("qobj.testProperty = 456");
        // get
        spy->clear();
        eng.evaluate("qobj.testProperty");
        QCOMPARE(spy->count(), 4);
        // evaluate() entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        // testProperty() entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(1).scriptId, qint64(-1));
        // testProperty() exit
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(2).scriptId, qint64(-1));
        QVERIFY(spy->at(2).value.isNumber());
        QCOMPARE(spy->at(2).value.toNumber(), testProperty());
        // evaluate() exit
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit);
        QVERIFY(spy->at(3).value.strictlyEquals(spy->at(2).value));
    }
    delete spy;
}


/** check behaiviour of calling script functions from c++*/
void tst_QScriptEngineAgent::functionEntryAndExit_call()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    // calling script functions from C++

    {
        QScriptValue fun = eng.evaluate("function foo() { return 123; }; foo");
        QVERIFY(fun.isFunction());

        spy->clear();
        fun.call();
        QCOMPARE(spy->count(), 2);

        // entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        QVERIFY(spy->at(0).scriptId != -1);

        // exit
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(1).value.isNumber());
        QCOMPARE(spy->at(1).value.toNumber(), qsreal(123));
    }
    delete spy;
}

/** check behaiviour of native function returnning arg*/
void tst_QScriptEngineAgent::functionEntryAndExit_functionReturn_call()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    {
        QScriptValue fun = eng.newFunction(nativeFunctionReturningArg);

        spy->clear();
        QScriptValueList args;
        args << QScriptValue(&eng, 123);
        fun.call(QScriptValue(), args);
        QCOMPARE(spy->count(), 2);

        // entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        QVERIFY(spy->at(0).scriptId == -1);

        // exit
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(1).value.strictlyEquals(args.at(0)));
    }
    delete spy;
}

void tst_QScriptEngineAgent::functionEntryAndExit_functionReturn_construct()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    {
        QScriptValue fun = eng.newFunction(nativeFunctionReturningArg);

        spy->clear();
        QScriptValueList args;
        args << QScriptValue(&eng, 123);
        QScriptValue obj = fun.construct(args);

        QVERIFY(args.at(0).isValid());
        QVERIFY(args.at(0).isNumber());
        QVERIFY(args.at(0).toNumber() == 123);

        QCOMPARE(spy->count(), 2);

        // entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        QVERIFY(spy->at(0).scriptId == -1);

        // exit
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);

        QVERIFY(spy->at(1).value.strictlyEquals(args.at(0)));
    }

    delete spy;
}

/** check behaiviour of object creation with args (?)*/
void tst_QScriptEngineAgent::functionEntryAndExit_objectCall()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreFunctionEntry
                                                       | ScriptEngineSpy::IgnoreFunctionExit));
    for (int x = 0; x < 2; ++x) {
        QScriptValue fun = eng.evaluate("Boolean");

        QVERIFY(!fun.isError());

        spy->clear();
        QScriptValueList args;
        args << QScriptValue(&eng, true);
        if (x)
            fun.construct(args);
        else
            fun.call(QScriptValue(), args);
        QCOMPARE(spy->count(), 2);

        // entry
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry);
        QVERIFY(spy->at(0).scriptId == -1);

        // exit
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(1).value.equals(args.at(0)));
    }
    delete spy;
}

void tst_QScriptEngineAgent::positionChange_1()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnorePositionChange));
    {
        spy->clear();
        eng.evaluate(";");
        QEXPECT_FAIL("","QTBUG-6142 JSC do not evaluate ';' to statemant",Continue);
        QCOMPARE(spy->count(), 1);
        if (spy->count()) {
            QEXPECT_FAIL("","QTBUG-6142 JSC do not evaluate ';' to statemant",Continue);
            QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
            QEXPECT_FAIL("","QTBUG-6142 JSC do not evaluate ';' to statemant",Continue);
            QVERIFY(spy->at(0).scriptId != -1);
            QEXPECT_FAIL("","QTBUG-6142 JSC do not evaluate ';' to statemant",Continue);
            QCOMPARE(spy->at(0).lineNumber, 1);
            QEXPECT_FAIL("","QTBUG-6142 JSC do not evaluate ';' to statemant",Continue);
            QCOMPARE(spy->at(0).columnNumber, 1);
        }
    }

    {
        spy->clear();
        int lineNumber = 123;
        eng.evaluate("1 + 2", "foo.qs", lineNumber);
        QCOMPARE(spy->count(), 1);

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, lineNumber);
        QCOMPARE(spy->at(0).columnNumber, 1);
    }

    {
        spy->clear();
        int lineNumber = 123;
        eng.evaluate("var i = 0", "foo.qs", lineNumber);
        QCOMPARE(spy->count(), 1);

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, lineNumber);
        QCOMPARE(spy->at(0).columnNumber, 1);
    }

    QStringList lineTerminators;
    lineTerminators << "\n" << "\r" << "\n\r" << "\r\n";
    for (int i = 0; i < lineTerminators.size(); ++i) {
        spy->clear();
        int lineNumber = 456;
        QString code = "1 + 2; 3 + 4;";
        code.append(lineTerminators.at(i));
        code.append("5 + 6");
        eng.evaluate(code, "foo.qs", lineNumber);
        QCOMPARE(spy->count(), 3);

        // 1 + 2
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, lineNumber);
        QCOMPARE(spy->at(0).columnNumber, 1);

        // 3 + 4
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, lineNumber);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 8);

        // 5 + 6
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(2).lineNumber, lineNumber + 1);
        QCOMPARE(spy->at(2).columnNumber, 1);
    }
    delete spy;
}

void tst_QScriptEngineAgent::positionChange_2()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnorePositionChange));
    {
        spy->clear();
        int lineNumber = 789;
        eng.evaluate("function foo() { return 123; }", "foo.qs", lineNumber);
        QCOMPARE(spy->count(), 0);

        eng.evaluate("foo()");
        QCOMPARE(spy->count(), 2);

        // foo()
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);

        // return 123
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(1).scriptId != spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, lineNumber);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 18);
    }

    {
        spy->clear();
        eng.evaluate("if (true) i = 1; else i = 0;");
        QCOMPARE(spy->count(), 2);

        // if
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);

        // i = 1
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 11);
    }

    {
        spy->clear();
        eng.evaluate("for (var i = 0; i < 2; ++i) { }");
        QCOMPARE(spy->count(), 1);

        // for
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);
    }

    {
        spy->clear();
        eng.evaluate("for (var i = 0; i < 2; ++i) { void(i); }");
        QCOMPARE(spy->count(), 3);

        // for
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);

        // void(i)
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 31);

        // void(i)
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(2).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(2).columnNumber, 31);
    }

    {
        spy->clear();
        eng.evaluate("var i = 0; while (i < 2) { ++i; }");
        QCOMPARE(spy->count(), 4);

        // i = 0
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);

        // while
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 12);

        // ++i
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(2).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(2).columnNumber, 28);

        // ++i
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(3).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(3).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(3).columnNumber, 28);
    }

    {
        spy->clear();
        eng.evaluate("var i = 0; do { ++i; } while (i < 2)");
        QCOMPARE(spy->count(), 5);

        // i = 0
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);

        // do
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 12);

        // ++i
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(2).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(2).columnNumber, 17);

        // do
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(3).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(3).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(3).columnNumber, 12);

        // ++i
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(4).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(4).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(4).columnNumber, 17);
    }

    {
        spy->clear();
        eng.evaluate("for (var i in { }) { void(i); }");
        QCOMPARE(spy->count(), 1);

        // for
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);
    }

    {
        spy->clear();
        eng.evaluate("for ( ; ; ) { break; }");
        QCOMPARE(spy->count(), 2);

        // for
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);

        // break
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 15);
    }

    {
        spy->clear();
        eng.evaluate("for (var i = 0 ; i < 2; ++i) { continue; }");
        QCOMPARE(spy->count(), 3);

        // for
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);

        // continue
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 32);

        // continue
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(2).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(2).columnNumber, 32);
    }

    {
        spy->clear();
        eng.evaluate("with (this) { }");
        QCOMPARE(spy->count(), 1);

        // with
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);
    }

    {
        spy->clear();
        eng.evaluate("switch (undefined) { }");
        QCOMPARE(spy->count(), 1);

        // switch
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);
    }

    {
        spy->clear();
        eng.evaluate("switch (undefined) { default: i = 5; }");
        QCOMPARE(spy->count(), 2);

        // switch
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);

        // i = 5
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 31);
    }

    {
        spy->clear();
        eng.evaluate("switch (undefined) { case undefined: i = 5; break; }");
        QCOMPARE(spy->count(), 3);

        // switch
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);

        // i = 5
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 38);

        // break
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(2).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(2).columnNumber, 45);
    }

    {
        spy->clear();
        eng.evaluate("throw 1");
        QCOMPARE(spy->count(), 1);

        // throw
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);
    }

    {
        spy->clear();
        eng.evaluate("try { throw 1; } catch(e) { i = e; } finally { i = 2; }");
        QCOMPARE(spy->count(), 3);

        // throw 1
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(0).columnNumber, 7);

        // i = e
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 29);

        // i = 2
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(2).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(2).columnNumber, 48);
    }

    {
        spy->clear();
        eng.evaluate("try { i = 1; } catch(e) { i = 2; } finally { i = 3; }");
        QCOMPARE(spy->count(), 2);

        // i = 1
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(0).columnNumber, 7);

        // i = 3
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 46);
    }

    {
        QEXPECT_FAIL("","QTBUG-6142 I believe the test is wrong. Expressions shouldn't call positionChange "
                     "because statement '1+2' will call it at least twice, why debugger have to "
                     "stop here so many times?", Abort);
        spy->clear();
        eng.evaluate("c = {a: 10, b: 20}");
        QCOMPARE(spy->count(), 2);

        // a: 10
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);
        QCOMPARE(spy->at(0).columnNumber, 1);

        // b: 20
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 1);
        QEXPECT_FAIL("", "QTBUG-17609: With JSC-based back-end, column number is always reported as 1", Continue);
        QCOMPARE(spy->at(1).columnNumber, 20);
    }
    delete spy;
}

void tst_QScriptEngineAgent::positionChange_3()
{
    QScriptEngine eng;
    eng.evaluate("function some_function1(a) {\n a++; \n return a + 12; } \n some_function1(42);", "function1.qs", 12);
    QScriptValue some_function2 = eng.evaluate("(function (b) {\n b--; \n return b + 11; })", "function2.qs", 21);
    some_function2.call(QScriptValue(), QScriptValueList() << 2 );

    // Test that the agent work, even if installed after the function has been evaluated.
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnorePositionChange));
    {
        spy->clear();
        QScriptValue v = eng.evaluate("some_function1(15)");
        QCOMPARE(v.toInt32(), (15+1+12));
        QCOMPARE(spy->count(), 3);

        // some_function1()
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 1);

        // a++
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(1).scriptId != spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 13);
        // return a + 12
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(2).scriptId == spy->at(1).scriptId);
        QCOMPARE(spy->at(2).lineNumber, 14);
    }

    {
        spy->clear();
        QScriptValue v = some_function2.call(QScriptValue(), QScriptValueList() << 89 );
        QCOMPARE(v.toInt32(), (89-1+11));
        QCOMPARE(spy->count(), 2);

        // b--
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(0).scriptId != -1);
        QCOMPARE(spy->at(0).lineNumber, 22);
        // return b + 11
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QVERIFY(spy->at(1).scriptId == spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, 23);
    }

    QVERIFY(!eng.hasUncaughtException());
}


void tst_QScriptEngineAgent::exceptionThrowAndCatch()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreExceptionThrow
                                                       | ScriptEngineSpy::IgnoreExceptionCatch));
    {
        spy->clear();
        eng.evaluate(";");
        QCOMPARE(spy->count(), 0);
    }

    {
        spy->clear();
        eng.evaluate("try { i = 5; } catch (e) { }");
        QCOMPARE(spy->count(), 0);
    }

    {
        spy->clear();
        eng.evaluate("throw new Error('ciao');");
        QCOMPARE(spy->count(), 1);

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ExceptionThrow);
        QVERIFY(spy->at(0).scriptId != -1);
        QVERIFY(!spy->at(0).hasExceptionHandler);
        QVERIFY(spy->at(0).value.isError());
        QCOMPARE(spy->at(0).value.toString(), QString("Error: ciao"));
    }

    {
        spy->clear();
        eng.evaluate("try { throw new Error('ciao'); } catch (e) { }");
        QCOMPARE(spy->count(), 2);

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ExceptionThrow);
        QVERIFY(spy->at(0).scriptId != -1);
        QVERIFY(spy->at(0).hasExceptionHandler);
        QVERIFY(spy->at(0).value.isError());
        QCOMPARE(spy->at(0).value.toString(), QString("Error: ciao"));
        QVERIFY(spy->at(0).scriptId != -1);

        QCOMPARE(spy->at(1).type, ScriptEngineEvent::ExceptionCatch);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(1).value.strictlyEquals(spy->at(0).value));
    }
}

void tst_QScriptEngineAgent::eventOrder_assigment()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    {
        spy->clear();
        eng.evaluate("i = 3; i = 5");
        QCOMPARE(spy->count(), 6);
        // load
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        // evaluate() entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        // i = 3
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);
        // i = 5
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(3).scriptId, spy->at(0).scriptId);
        // evaluate() exit
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(4).scriptId, spy->at(0).scriptId);
        // unload
        QCOMPARE(spy->at(5).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(5).scriptId, spy->at(0).scriptId);
    }
    delete spy;
}

void tst_QScriptEngineAgent::eventOrder_functionDefinition()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    {
        spy->clear();
        eng.evaluate("function foo(arg) { void(arg); }");
        QCOMPARE(spy->count(), 3);
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionExit);

        eng.evaluate("foo(123)");
        QCOMPARE(spy->count(), 13);
        // load
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::ScriptLoad);
        QVERIFY(spy->at(3).scriptId != spy->at(0).scriptId);
        // evaluate() entry
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(4).scriptId, spy->at(3).scriptId);
        // foo()
        QCOMPARE(spy->at(5).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(5).scriptId, spy->at(3).scriptId);
        // new context
        QCOMPARE(spy->at(6).type, ScriptEngineEvent::ContextPush);
        // foo() entry
        QCOMPARE(spy->at(7).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(7).scriptId, spy->at(0).scriptId);
        // void(arg)
        QCOMPARE(spy->at(8).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(8).scriptId, spy->at(0).scriptId);
        // foo() exit
        QCOMPARE(spy->at(9).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(9).scriptId, spy->at(0).scriptId);
        // restore context
        QCOMPARE(spy->at(10).type, ScriptEngineEvent::ContextPop);
        // evaluate() exit
        QCOMPARE(spy->at(11).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(11).scriptId, spy->at(3).scriptId);
        // unload
        QCOMPARE(spy->at(12).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(12).scriptId, spy->at(3).scriptId);

        eng.evaluate("foo = null");
        eng.collectGarbage();
    }
    delete spy;
}

void tst_QScriptEngineAgent::eventOrder_throwError()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    {
        spy->clear();
        eng.evaluate("throw new Error('ciao')");
        QCOMPARE(spy->count(), 10);
        // load
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        // evaluate() entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        // throw
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        // new context
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::ContextPush);
        // Error constructor entry
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::FunctionEntry);
        // Error constructor exit
        QCOMPARE(spy->at(5).type, ScriptEngineEvent::FunctionExit);
        // restore context
        QCOMPARE(spy->at(6).type, ScriptEngineEvent::ContextPop);
        // exception
        QCOMPARE(spy->at(7).type, ScriptEngineEvent::ExceptionThrow);
        QVERIFY(!spy->at(7).hasExceptionHandler);
        // evaluate() exit
        QCOMPARE(spy->at(8).type, ScriptEngineEvent::FunctionExit);
        // unload
        QCOMPARE(spy->at(9).type, ScriptEngineEvent::ScriptUnload);
    }
    delete spy;
}

void tst_QScriptEngineAgent::eventOrder_throwAndCatch()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    {
        spy->clear();
        eng.evaluate("try { throw new Error('ciao') } catch (e) { void(e); }");
        QCOMPARE(spy->count(), 12);
        // load
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        // evaluate() entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        // throw
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        // new context
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::ContextPush);
        // Error constructor entry
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::FunctionEntry);
        // Error constructor exit
        QCOMPARE(spy->at(5).type, ScriptEngineEvent::FunctionExit);
        // restore context
        QCOMPARE(spy->at(6).type, ScriptEngineEvent::ContextPop);
        // exception
        QCOMPARE(spy->at(7).type, ScriptEngineEvent::ExceptionThrow);
        QVERIFY(spy->at(7).value.isError());
        QVERIFY(spy->at(7).hasExceptionHandler);
        // catch
        QCOMPARE(spy->at(8).type, ScriptEngineEvent::ExceptionCatch);
        QVERIFY(spy->at(8).value.isError());
        // void(e)
        QCOMPARE(spy->at(9).type, ScriptEngineEvent::PositionChange);
        // evaluate() exit
        QCOMPARE(spy->at(10).type, ScriptEngineEvent::FunctionExit);
        QVERIFY(spy->at(10).value.isUndefined());
        // unload
        QCOMPARE(spy->at(11).type, ScriptEngineEvent::ScriptUnload);
    }
    delete spy;
}

void tst_QScriptEngineAgent::eventOrder_functions()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    {
        spy->clear();
        eng.evaluate("function foo(arg) { return bar(arg); }");
        eng.evaluate("function bar(arg) { return arg; }");
        QCOMPARE(spy->count(), 6);

        eng.evaluate("foo(123)");
        QCOMPARE(spy->count(), 21);

        // load
        QCOMPARE(spy->at(6).type, ScriptEngineEvent::ScriptLoad);
        // evaluate() entry
        QCOMPARE(spy->at(7).type, ScriptEngineEvent::FunctionEntry);
        // foo(123)
        QCOMPARE(spy->at(8).type, ScriptEngineEvent::PositionChange);
        // new context
        QCOMPARE(spy->at(9).type, ScriptEngineEvent::ContextPush);
        // foo() entry
        QCOMPARE(spy->at(10).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(10).scriptId, spy->at(0).scriptId);
        // return bar(arg)
        QCOMPARE(spy->at(11).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(11).scriptId, spy->at(0).scriptId);
        // new context
        QCOMPARE(spy->at(12).type, ScriptEngineEvent::ContextPush);
        // bar() entry
        QCOMPARE(spy->at(13).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(13).scriptId, spy->at(3).scriptId);
        // return arg
        QCOMPARE(spy->at(14).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(14).scriptId, spy->at(3).scriptId);
        // bar() exit
        QCOMPARE(spy->at(15).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(15).scriptId, spy->at(3).scriptId);
        QVERIFY(spy->at(15).value.isNumber());
        // restore context
        QCOMPARE(spy->at(16).type, ScriptEngineEvent::ContextPop);
        // foo() exit
        QCOMPARE(spy->at(17).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(17).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(17).value.isNumber());
        // restore context
        QCOMPARE(spy->at(18).type, ScriptEngineEvent::ContextPop);
        // evaluate() exit
        QCOMPARE(spy->at(19).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(19).scriptId, spy->at(6).scriptId);
        QVERIFY(spy->at(19).value.isNumber());
        // unload
        QCOMPARE(spy->at(20).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(20).scriptId, spy->at(6).scriptId);

        // redefine bar()
        eng.evaluate("function bar(arg) { throw new Error(arg); }");
        eng.collectGarbage();
        QCOMPARE(spy->count(), 25);
        QCOMPARE(spy->at(21).type, ScriptEngineEvent::ScriptLoad);
        QCOMPARE(spy->at(22).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(23).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(24).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(24).scriptId, spy->at(3).scriptId);

        eng.evaluate("foo('ciao')");

        QCOMPARE(spy->count(), 45);

        // load
        QCOMPARE(spy->at(25).type, ScriptEngineEvent::ScriptLoad);
        // evaluate() entry
        QCOMPARE(spy->at(26).type, ScriptEngineEvent::FunctionEntry);
        // foo('ciao')
        QCOMPARE(spy->at(27).type, ScriptEngineEvent::PositionChange);
        // new context
        QCOMPARE(spy->at(28).type, ScriptEngineEvent::ContextPush);
        // foo() entry
        QCOMPARE(spy->at(29).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(29).scriptId, spy->at(0).scriptId);
        // return bar(arg)
        QCOMPARE(spy->at(30).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(30).scriptId, spy->at(0).scriptId);
        // new context
        QCOMPARE(spy->at(31).type, ScriptEngineEvent::ContextPush);
        // bar() entry
        QCOMPARE(spy->at(32).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(32).scriptId, spy->at(21).scriptId);
        // throw
        QCOMPARE(spy->at(33).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(33).scriptId, spy->at(21).scriptId);
        // new context
        QCOMPARE(spy->at(34).type, ScriptEngineEvent::ContextPush);
        // Error constructor entry
        QCOMPARE(spy->at(35).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(35).scriptId, qint64(-1));
        // Error constructor exit
        QCOMPARE(spy->at(36).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(36).scriptId, qint64(-1));
        // restore context
        QCOMPARE(spy->at(37).type, ScriptEngineEvent::ContextPop);
        // exception
        QCOMPARE(spy->at(38).type, ScriptEngineEvent::ExceptionThrow);
        QCOMPARE(spy->at(38).scriptId, spy->at(21).scriptId);
        QVERIFY(!spy->at(38).hasExceptionHandler);
        // bar() exit
        QCOMPARE(spy->at(39).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(39).scriptId, spy->at(21).scriptId);
        QVERIFY(spy->at(39).value.isError());
        // restore context
        QCOMPARE(spy->at(40).type, ScriptEngineEvent::ContextPop);
        // foo() exit
        QCOMPARE(spy->at(41).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(41).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(41).value.isError());
        // restore context
        QCOMPARE(spy->at(42).type, ScriptEngineEvent::ContextPop);
        // evaluate() exit
        QCOMPARE(spy->at(43).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(43).scriptId, spy->at(26).scriptId);
        QVERIFY(spy->at(43).value.isError());
        // unload
        QCOMPARE(spy->at(44).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(44).scriptId, spy->at(25).scriptId);
    }
    delete spy;
}

void tst_QScriptEngineAgent::eventOrder_throwCatchFinally()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    {
        spy->clear();
        eng.evaluate("try { throw 1; } catch(e) { i = e; } finally { i = 2; }");
        QCOMPARE(spy->count(), 9);

        // load
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        // evaluate() entry
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        // throw 1
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        // i = e
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::ExceptionThrow);
        // catch
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::ExceptionCatch);
        // i = e
        QCOMPARE(spy->at(5).type, ScriptEngineEvent::PositionChange);
        // i = 2
        QCOMPARE(spy->at(6).type, ScriptEngineEvent::PositionChange);
        // evaluate() exit
        QCOMPARE(spy->at(7).type, ScriptEngineEvent::FunctionExit);
        // unload
        QCOMPARE(spy->at(8).type, ScriptEngineEvent::ScriptUnload);
    }
    delete spy;
}

void tst_QScriptEngineAgent::eventOrder_signalsHandling()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    // signal handling
    {
        spy->clear();
        QScriptValue fun = eng.evaluate("(function(arg) { throw Error(arg); })");
        QVERIFY(fun.isFunction());
        QCOMPARE(spy->count(), 4);
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit);

        qScriptConnect(this, SIGNAL(testSignal(double)),
                       QScriptValue(), fun);

        emit testSignal(123);

        QCOMPARE(spy->count(), 14);
        // new context
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::ContextPush);
        // anonymous function entry
        QCOMPARE(spy->at(5).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(5).scriptId, spy->at(0).scriptId);
        // throw statement
        QCOMPARE(spy->at(6).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(6).scriptId, spy->at(0).scriptId);
        // new context
        QCOMPARE(spy->at(7).type, ScriptEngineEvent::ContextPush);
        // Error constructor entry
        QCOMPARE(spy->at(8).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(8).scriptId, qint64(-1));
        // Error constructor exit
        QCOMPARE(spy->at(9).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(9).scriptId, qint64(-1));
        // restore context
        QCOMPARE(spy->at(10).type, ScriptEngineEvent::ContextPop);
        // exception
        QCOMPARE(spy->at(11).type, ScriptEngineEvent::ExceptionThrow);
        QCOMPARE(spy->at(11).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(11).value.isError());
        QVERIFY(!spy->at(11).hasExceptionHandler);
        // anonymous function exit
        QCOMPARE(spy->at(12).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(12).scriptId, spy->at(0).scriptId);
        QVERIFY(spy->at(12).value.isError());
        // restore context
        QCOMPARE(spy->at(13).type, ScriptEngineEvent::ContextPop);
    }
    delete spy;
}

class DoubleAgent : public ScriptEngineSpy
{
public:
    DoubleAgent(QScriptEngine *engine) : ScriptEngineSpy(engine) { }
    ~DoubleAgent() { }

    void positionChange(qint64 scriptId, int lineNumber, int columnNumber)
    {
        if (lineNumber == 123)
            engine()->evaluate("1 + 2");
        ScriptEngineSpy::positionChange(scriptId, lineNumber, columnNumber);
    }
};

void tst_QScriptEngineAgent::recursiveObserve()
{
    QScriptEngine eng;
    DoubleAgent *spy = new DoubleAgent(&eng);

    eng.evaluate("3 + 4", "foo.qs", 123);

    QCOMPARE(spy->count(), 10);

    int i = 0;
    // load "3 + 4"    
    QCOMPARE(spy->at(i).type, ScriptEngineEvent::ScriptLoad);
    i++;
    // evaluate() entry
    QCOMPARE(spy->at(i).type, ScriptEngineEvent::FunctionEntry);
    i++;
    // load "1 + 2"
    QCOMPARE(spy->at(i).type, ScriptEngineEvent::ScriptLoad);
    i++;
    // evaluate() entry
    QCOMPARE(spy->at(i).type, ScriptEngineEvent::FunctionEntry);
    i++;
    // 1 + 2
    QCOMPARE(spy->at(i).type, ScriptEngineEvent::PositionChange);
    QCOMPARE(spy->at(i).scriptId, spy->at(2).scriptId);
    i++;
    // evaluate() exit
    QCOMPARE(spy->at(i).type, ScriptEngineEvent::FunctionExit);
    i++;
    // unload "1 + 2"
    QCOMPARE(spy->at(i).type, ScriptEngineEvent::ScriptUnload);
    QCOMPARE(spy->at(i).scriptId, spy->at(2).scriptId);
    i++;
    // 3 + 4
    QCOMPARE(spy->at(i).type, ScriptEngineEvent::PositionChange);
    QCOMPARE(spy->at(i).scriptId, spy->at(0).scriptId);
    i++;
    // evaluate() exit
    QCOMPARE(spy->at(i).type, ScriptEngineEvent::FunctionExit);
    i++;
    // unload "3 + 4"
    QCOMPARE(spy->at(i).type, ScriptEngineEvent::ScriptUnload);
    QCOMPARE(spy->at(i).scriptId, spy->at(0).scriptId);
}


/** When second agent is attached to Engine the first one should be deatached */
void tst_QScriptEngineAgent::multipleAgents()
{
    QScriptEngine eng;
    QCOMPARE(eng.agent(), (QScriptEngineAgent *)0);
    ScriptEngineSpy *spy1 = new ScriptEngineSpy(&eng);
    QCOMPARE(eng.agent(), (QScriptEngineAgent*)spy1);
    ScriptEngineSpy *spy2 = new ScriptEngineSpy(&eng);
    QCOMPARE(eng.agent(), (QScriptEngineAgent*)spy2);

    eng.evaluate("1 + 2");
    QCOMPARE(spy1->count(), 0);
    QCOMPARE(spy2->count(), 5);

    spy2->clear();
    eng.setAgent(spy1);
    eng.evaluate("1 + 2");
    QCOMPARE(spy2->count(), 0);
    QCOMPARE(spy1->count(), 5);
}

void tst_QScriptEngineAgent::syntaxError()
{
    /* This test was changed. Old backend didn't generate events in exception objects creation time
        JSC does */
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    {
        int i = 0;
        spy->clear();
        eng.evaluate("{");
        
        QCOMPARE(spy->count(), 9);

        QCOMPARE(spy->at(i).type, ScriptEngineEvent::ScriptLoad);
        QVERIFY(spy->at(i).scriptId != -1);
        i = 1;
        QCOMPARE(spy->at(i).type, ScriptEngineEvent::FunctionEntry);
        QCOMPARE(spy->at(i).scriptId, spy->at(0).scriptId);

        //create exception

        i = 2;
        QCOMPARE(spy->at(i).type, ScriptEngineEvent::ContextPush);
        i = 3;
        QCOMPARE(spy->at(i).type, ScriptEngineEvent::FunctionEntry);
        QVERIFY(spy->at(i).scriptId == -1);
        i = 4;
        QCOMPARE(spy->at(i).type, ScriptEngineEvent::FunctionExit);
        QVERIFY(spy->at(i).scriptId == -1);
        i = 5;
        QCOMPARE(spy->at(i).type, ScriptEngineEvent::ContextPop);
        i = 6;
        QCOMPARE(spy->at(i).type, ScriptEngineEvent::ExceptionThrow);
        QCOMPARE(spy->at(i).scriptId, spy->at(0).scriptId);
        QVERIFY(!spy->at(i).hasExceptionHandler);
        QVERIFY(spy->at(i).value.isError());
        QVERIFY(spy->at(i).value.toString().contains(QLatin1String("SyntaxError")));
        QCOMPARE(spy->at(i).scriptId, spy->at(0).scriptId);
        i = 7;
        //exit script
        QCOMPARE(spy->at(i).type, ScriptEngineEvent::FunctionExit);
        QCOMPARE(spy->at(i).scriptId, spy->at(0).scriptId);
        i = 8;
        QCOMPARE(spy->at(i).type, ScriptEngineEvent::ScriptUnload);
        QCOMPARE(spy->at(i).scriptId, spy->at(0).scriptId);
    }
}

void tst_QScriptEngineAgent::extension_invoctaion()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreDebuggerInvocationRequest
                                                   | ScriptEngineSpy::IgnoreScriptLoad));
    // DebuggerInvocationRequest
    {
        spy->clear();

        QString fileName = "foo.qs";
        int lineNumber = 123;
        QScriptValue ret = eng.evaluate("debugger", fileName, lineNumber);

        QCOMPARE(spy->count(), 2);
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        QCOMPARE(spy->at(1).type, ScriptEngineEvent::DebuggerInvocationRequest);
        QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);
        QCOMPARE(spy->at(1).lineNumber, lineNumber);
        QCOMPARE(spy->at(1).columnNumber, 1);

        QEXPECT_FAIL("","QTBUG-6135 In JSC Eval('debugger') returns undefined",Abort);
        QVERIFY(ret.isString());
        QCOMPARE(ret.toString(), QString::fromLatin1("extension(DebuggerInvocationRequest)"));
    }
    delete spy;
}

void tst_QScriptEngineAgent::extension()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng, ~(ScriptEngineSpy::IgnoreDebuggerInvocationRequest
                                                   | ScriptEngineSpy::IgnoreScriptLoad));

    {
        spy->clear();
        spy->enableIgnoreFlags(ScriptEngineSpy::IgnoreDebuggerInvocationRequest);

        QScriptValue ret = eng.evaluate("debugger");

        QCOMPARE(spy->count(), 1);
        QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
        QVERIFY(ret.isUndefined());
    }
    delete spy;
}

class TestIsEvaluatingAgent : public QScriptEngineAgent
{
public:
    TestIsEvaluatingAgent(QScriptEngine *engine)
        : QScriptEngineAgent(engine), wasEvaluating(false)
    { engine->setAgent(this); }
    bool supportsExtension(Extension ext) const
    { return ext == DebuggerInvocationRequest; }
    QVariant extension(Extension, const QVariant &)
    { wasEvaluating = engine()->isEvaluating(); return QVariant(); }

    bool wasEvaluating;
};

void tst_QScriptEngineAgent::isEvaluatingInExtension()
{
    QScriptEngine eng;
    TestIsEvaluatingAgent *spy = new TestIsEvaluatingAgent(&eng);
    QVERIFY(!spy->wasEvaluating);
    eng.evaluate("debugger");
    QVERIFY(spy->wasEvaluating);
}

class NewSpy :public QScriptEngineAgent
{
    bool m_result;
public:
  NewSpy(QScriptEngine* eng) : QScriptEngineAgent(eng), m_result(false) {}
  void functionExit (qint64, const QScriptValue & /* scriptValue */)
  {
      if (engine()->hasUncaughtException()) m_result = true;
  }

  bool isPass() { return m_result; }
  void reset() { m_result =  false; }
};

void tst_QScriptEngineAgent::hasUncaughtException()
{
  QScriptEngine eng;
  NewSpy* spy = new NewSpy(&eng);
  eng.setAgent(spy);
  QScriptValue scriptValue;

  // Check unhandled exception.
  eng.evaluate("function init () {Unknown.doSth ();}");
  scriptValue = QScriptValue(eng.globalObject().property("init")).call();
  QVERIFY(eng.hasUncaughtException());
  QVERIFY2(spy->isPass(), "At least one of a functionExit event should set hasUncaughtException flag.");
  spy->reset();

  // Check caught exception.
  eng.evaluate("function innerFoo() { throw new Error('ciao') }");
  eng.evaluate("function foo() {try { innerFoo() } catch (e) {} }");
  scriptValue = QScriptValue(eng.globalObject().property("foo")).call();
  QVERIFY(!eng.hasUncaughtException());
  QVERIFY2(spy->isPass(), "At least one of a functionExit event should set hasUncaughtException flag.");
}

void tst_QScriptEngineAgent::evaluateProgram()
{
    QScriptEngine eng;
    QScriptProgram program("1 + 2", "foo.js", 123);
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    qint64 scriptId = -1;
    for (int x = 0; x < 10; ++x) {
        spy->clear();
        (void)eng.evaluate(program);
        QCOMPARE(spy->count(), (x == 0) ? 4 : 3);

        if (x == 0) {
            // script is only loaded on first execution
            QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
            scriptId = spy->at(0).scriptId;
            QVERIFY(scriptId != -1);
            QCOMPARE(spy->at(0).script, program.sourceCode());
            QCOMPARE(spy->at(0).fileName, program.fileName());
            QCOMPARE(spy->at(0).lineNumber, program.firstLineNumber());
            spy->removeFirst();
        }

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry); // evaluate()
        QCOMPARE(spy->at(0).scriptId, scriptId);

        QCOMPARE(spy->at(1).type, ScriptEngineEvent::PositionChange);
        QCOMPARE(spy->at(1).scriptId, scriptId);
        QCOMPARE(spy->at(1).lineNumber, program.firstLineNumber());

        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionExit); // evaluate()
        QCOMPARE(spy->at(2).scriptId, scriptId);
        QVERIFY(spy->at(2).value.isNumber());
        QCOMPARE(spy->at(2).value.toNumber(), qsreal(3));
    }
}

void tst_QScriptEngineAgent::evaluateProgram_SyntaxError()
{
    QScriptEngine eng;
    QScriptProgram program("this is not valid syntax", "foo.js", 123);
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    qint64 scriptId = -1;
    for (int x = 0; x < 10; ++x) {
        spy->clear();
        (void)eng.evaluate(program);
        QCOMPARE(spy->count(), (x == 0) ? 8 : 7);

        if (x == 0) {
            // script is only loaded on first execution
            QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
            scriptId = spy->at(0).scriptId;
            QVERIFY(scriptId != -1);
            QCOMPARE(spy->at(0).script, program.sourceCode());
            QCOMPARE(spy->at(0).fileName, program.fileName());
            QCOMPARE(spy->at(0).lineNumber, program.firstLineNumber());
            spy->removeFirst();
        }

        QCOMPARE(spy->at(0).type, ScriptEngineEvent::FunctionEntry); // evaluate()
        QCOMPARE(spy->at(0).scriptId, scriptId);

        QCOMPARE(spy->at(1).type, ScriptEngineEvent::ContextPush); // SyntaxError constructor
        QCOMPARE(spy->at(2).type, ScriptEngineEvent::FunctionEntry); // SyntaxError constructor
        QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit); // SyntaxError constructor
        QCOMPARE(spy->at(4).type, ScriptEngineEvent::ContextPop); // SyntaxError constructor

        QCOMPARE(spy->at(5).type, ScriptEngineEvent::ExceptionThrow);
        QVERIFY(spy->at(5).value.isError());
        QCOMPARE(spy->at(5).value.toString(), QString::fromLatin1("SyntaxError: Parse error"));

        QCOMPARE(spy->at(6).type, ScriptEngineEvent::FunctionExit); // evaluate()
        QCOMPARE(spy->at(6).scriptId, scriptId);
    }
}

void tst_QScriptEngineAgent::evaluateNullProgram()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    (void)eng.evaluate(QScriptProgram());
    QCOMPARE(spy->count(), 0);
}

void tst_QScriptEngineAgent::QTBUG6108()
{
    QScriptEngine eng;
    ScriptEngineSpy *spy = new ScriptEngineSpy(&eng);
    eng.evaluate("eval('a = 1')");
    QCOMPARE(spy->count(), 5);

    QCOMPARE(spy->at(0).type, ScriptEngineEvent::ScriptLoad);
    QVERIFY(spy->at(0).scriptId != -1);

    QCOMPARE(spy->at(1).type, ScriptEngineEvent::FunctionEntry);
    QVERIFY(spy->at(1).scriptId != -1);
    QCOMPARE(spy->at(1).scriptId, spy->at(0).scriptId);

    QCOMPARE(spy->at(2).type, ScriptEngineEvent::PositionChange);
    QVERIFY(spy->at(2).scriptId != -1);
    QCOMPARE(spy->at(2).scriptId, spy->at(0).scriptId);
    QCOMPARE(spy->at(2).lineNumber, 1);

    QCOMPARE(spy->at(3).type, ScriptEngineEvent::FunctionExit);
    QVERIFY(spy->at(3).scriptId != -1);
    QCOMPARE(spy->at(3).scriptId, spy->at(0).scriptId);

    QCOMPARE(spy->at(4).type, ScriptEngineEvent::ScriptUnload);
    QCOMPARE(spy->at(4).scriptId, spy->at(0).scriptId);
}

class BacktraceSpy : public QScriptEngineAgent
{
public:
    BacktraceSpy(QScriptEngine *engine, const QStringList &expectedbacktrace, int breakpoint)
        : QScriptEngineAgent(engine), expectedbacktrace(expectedbacktrace), breakpoint(breakpoint), ok(false) {}

    QStringList expectedbacktrace;
    int breakpoint;
    bool ok;

protected:

    void exceptionThrow(qint64 , const QScriptValue &, bool)
    {  check();  }

    void positionChange(qint64 , int lineNumber, int )
    {
        if (lineNumber == breakpoint)
            check();
    }

private:
    void check()
    {
        QCOMPARE(engine()->currentContext()->backtrace(), expectedbacktrace);
        ok = true;
    }
};


void tst_QScriptEngineAgent::backtraces_data()
{
    QTest::addColumn<QString>("code");
    QTest::addColumn<int>("breakpoint");
    QTest::addColumn<QStringList>("expectedbacktrace");

    {
        QString source(
            "function foo() {\n"
            "  var a = 5\n"
            "}\n"
            "foo('hello', { })\n"
            "var r = 0;");

        QStringList expected;
        expected
            << "foo('hello', [object Object]) at filename.js:2"
            << "<global>() at filename.js:4";
        QTest::newRow("simple breakpoint") << source <<  2 << expected;
    }

    {
        QString source(
            "function foo() {\n"
            "  error = err\n" //this must throw
            "}\n"
            "foo('hello', { })\n"
            "var r = 0;");

        QStringList expected;
        expected
            << "foo('hello', [object Object]) at filename.js:2"
            << "<global>() at filename.js:4";
        QTest::newRow("throw because of error") << source <<  -100 << expected;
    }
}

void tst_QScriptEngineAgent::backtraces()
{
    QFETCH(QString, code);
    QFETCH(int, breakpoint);
    QFETCH(QStringList, expectedbacktrace);

    QScriptEngine eng;
    BacktraceSpy *spy = new BacktraceSpy(&eng, expectedbacktrace, breakpoint);
    eng.setAgent(spy);
    QLatin1String filename("filename.js");
    eng.evaluate(code, filename);
    QVERIFY(spy->ok);
}

QTEST_MAIN(tst_QScriptEngineAgent)
#include "tst_qscriptengineagent.moc"
