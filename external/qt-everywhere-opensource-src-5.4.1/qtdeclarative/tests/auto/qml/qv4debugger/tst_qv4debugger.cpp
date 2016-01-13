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
#include <QtTest/QtTest>

#include <QJSEngine>
#include <QQmlEngine>
#include <QQmlComponent>
#include <private/qv4engine_p.h>
#include <private/qv4debugging_p.h>
#include <private/qv8engine_p.h>

using namespace QV4;
using namespace QV4::Debugging;

typedef QV4::ReturnedValue (*InjectedFunction)(QV4::CallContext*);
Q_DECLARE_METATYPE(InjectedFunction)

static bool waitForSignal(QObject* obj, const char* signal, int timeout = 10000)
{
    QEventLoop loop;
    QObject::connect(obj, signal, &loop, SLOT(quit()));
    QTimer timer;
    QSignalSpy timeoutSpy(&timer, SIGNAL(timeout()));
    if (timeout > 0) {
        QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
        timer.setSingleShot(true);
        timer.start(timeout);
    }
    loop.exec();
    return timeoutSpy.isEmpty();
}

class TestEngine : public QJSEngine
{
    Q_OBJECT
public:
    TestEngine()
    {
        qMetaTypeId<InjectedFunction>();
    }

    Q_INVOKABLE void evaluate(const QString &script, const QString &fileName, int lineNumber = 1)
    {
        QJSEngine::evaluate(script, fileName, lineNumber);
        emit evaluateFinished();
    }

    QV4::ExecutionEngine *v4Engine() { return QV8Engine::getV4(this); }

    Q_INVOKABLE void injectFunction(const QString &functionName, InjectedFunction injectedFunction)
    {
        QV4::ExecutionEngine *v4 = v4Engine();
        QV4::Scope scope(v4);

        QV4::Scoped<QV4::String> name(scope, v4->newString(functionName));
        QV4::ScopedValue function(scope, BuiltinFunction::create(v4->rootContext, name, injectedFunction));
        v4->globalObject->put(name, function);
    }

signals:
    void evaluateFinished();
};


namespace {
class TestCollector: public QV4::Debugging::Debugger::Collector
{
public:
    TestCollector(QV4::ExecutionEngine *engine)
        : Collector(engine)
        , destination(0)
    {}

    virtual ~TestCollector() {}

    void setDestination(QVariantMap *dest)
    { destination = dest; }

protected:
    virtual void addUndefined(const QString &name)
    {
        destination->insert(name, QStringLiteral("undefined")); // TODO: add a user-defined type for this
    }

    virtual void addNull(const QString &name)
    {
        destination->insert(name, QStringLiteral("null")); // TODO: add a user-defined type for this
    }

    virtual void addBoolean(const QString &name, bool value)
    {
        destination->insert(name, value);
    }

    virtual void addString(const QString &name, const QString &value)
    {
        destination->insert(name, value);
    }

    virtual void addObject(const QString &name, QV4::ValueRef value)
    {
        QV4::Scope scope(engine());
        QV4::ScopedObject obj(scope, value->asObject());

        QVariantMap props, *prev = &props;
        qSwap(destination, prev);
        collect(obj);
        qSwap(destination, prev);

        destination->insert(name, props);
    }

    virtual void addInteger(const QString &name, int value)
    {
        destination->insert(name, QVariant::fromValue<double>(static_cast<double>(value)));
    }

    virtual void addDouble(const QString &name, double value)
    {
        destination->insert(name, QVariant::fromValue<double>(value));
    }

private:
    QVariantMap *destination;
};
}

class TestAgent : public QV4::Debugging::DebuggerAgent
{
    Q_OBJECT
public:
    TestAgent()
        : m_wasPaused(false)
        , m_captureContextInfo(false)
    {
    }

    virtual void debuggerPaused(Debugger *debugger, PauseReason reason)
    {
        Q_ASSERT(m_debuggers.count() == 1 && m_debuggers.first() == debugger);
        m_wasPaused = true;
        m_pauseReason = reason;
        m_statesWhenPaused << debugger->currentExecutionState();

        TestCollector collector(debugger->engine());
        QVariantMap tmp;
        collector.setDestination(&tmp);
        debugger->collectThrownValue(&collector);
        m_thrownValue = tmp["exception"];

        foreach (const TestBreakPoint &bp, m_breakPointsToAddWhenPaused)
            debugger->addBreakPoint(bp.fileName, bp.lineNumber);
        m_breakPointsToAddWhenPaused.clear();

        m_stackTrace = debugger->stackTrace();

        while (!m_expressionRequests.isEmpty()) {
            ExpressionRequest request = m_expressionRequests.takeFirst();
            QVariantMap result;
            collector.setDestination(&result);
            debugger->evaluateExpression(request.frameNr, request.expression, &collector);
            m_expressionResults << result[QString::fromLatin1("body")];
        }

        if (m_captureContextInfo)
            captureContextInfo(debugger);

        debugger->resume(Debugger::FullThrottle);
    }

    virtual void sourcesCollected(Debugger *debugger, QStringList sources, int requestSequenceNr)
    {
        Q_UNUSED(debugger);
        Q_UNUSED(sources);
        Q_UNUSED(requestSequenceNr);
    }

    int debuggerCount() const { return m_debuggers.count(); }

    struct TestBreakPoint
    {
        TestBreakPoint() : lineNumber(-1) {}
        TestBreakPoint(const QString &fileName, int lineNumber)
            : fileName(fileName), lineNumber(lineNumber) {}
        QString fileName;
        int lineNumber;
    };

    void captureContextInfo(Debugger *debugger)
    {
        TestCollector collector(debugger->engine());

        for (int i = 0, ei = m_stackTrace.size(); i != ei; ++i) {
            QVariantMap args;
            collector.setDestination(&args);
            debugger->collectArgumentsInContext(&collector, i);
            m_capturedArguments.append(args);

            QVariantMap locals;
            collector.setDestination(&locals);
            debugger->collectLocalsInContext(&collector, i);
            m_capturedLocals.append(locals);
        }
    }

    bool m_wasPaused;
    PauseReason m_pauseReason;
    bool m_captureContextInfo;
    QList<Debugger::ExecutionState> m_statesWhenPaused;
    QList<TestBreakPoint> m_breakPointsToAddWhenPaused;
    QVector<QV4::StackFrame> m_stackTrace;
    QList<QVariantMap> m_capturedArguments;
    QList<QVariantMap> m_capturedLocals;
    QVariant m_thrownValue;

    struct ExpressionRequest {
        QString expression;
        int frameNr;
    };
    QVector<ExpressionRequest> m_expressionRequests;
    QVector<QVariant> m_expressionResults;

    // Utility methods:
    void dumpStackTrace() const
    {
        qDebug() << "Stack depth:" << m_stackTrace.size();
        foreach (const QV4::StackFrame &frame, m_stackTrace)
            qDebug("\t%s (%s:%d:%d)", qPrintable(frame.function), qPrintable(frame.source),
                   frame.line, frame.column);
    }
};

class tst_qv4debugger : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // breakpoints:
    void breakAnywhere();
    void pendingBreakpoint();
    void liveBreakPoint();
    void removePendingBreakPoint();
    void addBreakPointWhilePaused();
    void removeBreakPointForNextInstruction();
    void conditionalBreakPoint();
    void conditionalBreakPointInQml();

    // context access:
    void readArguments();
    void readLocals();
    void readObject();
    void readContextInAllFrames();

    // exceptions:
    void pauseOnThrow();
    void breakInCatch();
    void breakInWith();

    void evaluateExpression();

private:
    void evaluateJavaScript(const QString &script, const QString &fileName, int lineNumber = 1)
    {
        QMetaObject::invokeMethod(m_engine, "evaluate", Qt::QueuedConnection,
                                  Q_ARG(QString, script), Q_ARG(QString, fileName),
                                  Q_ARG(int, lineNumber));
        waitForSignal(m_engine, SIGNAL(evaluateFinished()), /*timeout*/0);
    }

    TestEngine *m_engine;
    QV4::ExecutionEngine *m_v4;
    TestAgent *m_debuggerAgent;
    QThread *m_javaScriptThread;
};

void tst_qv4debugger::init()
{
    m_javaScriptThread = new QThread;
    m_engine = new TestEngine;
    m_v4 = m_engine->v4Engine();
    m_v4->enableDebugger();
    m_engine->moveToThread(m_javaScriptThread);
    m_javaScriptThread->start();
    m_debuggerAgent = new TestAgent;
    m_debuggerAgent->addDebugger(m_v4->debugger);
}

void tst_qv4debugger::cleanup()
{
    m_javaScriptThread->exit();
    m_javaScriptThread->wait();
    delete m_engine;
    delete m_javaScriptThread;
    m_engine = 0;
    m_v4 = 0;
    QCOMPARE(m_debuggerAgent->debuggerCount(), 0);
    delete m_debuggerAgent;
    m_debuggerAgent = 0;
}

void tst_qv4debugger::breakAnywhere()
{
    QString script =
            "var i = 42;\n"
            "var j = i + 1\n"
            "var k = i\n";
    m_debuggerAgent->pauseAll();
    evaluateJavaScript(script, "testFile");
    QVERIFY(m_debuggerAgent->m_wasPaused);
}

void tst_qv4debugger::pendingBreakpoint()
{
    QString script =
            "var i = 42;\n"
            "var j = i + 1\n"
            "var k = i\n";
    m_debuggerAgent->addBreakPoint("testfile", 2);
    evaluateJavaScript(script, "testfile");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 1);
    QV4::Debugging::Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.first();
    QCOMPARE(state.fileName, QString("testfile"));
    QCOMPARE(state.lineNumber, 2);
}

void tst_qv4debugger::liveBreakPoint()
{
    QString script =
            "var i = 42;\n"
            "var j = i + 1\n"
            "var k = i\n";
    m_debuggerAgent->m_breakPointsToAddWhenPaused << TestAgent::TestBreakPoint("liveBreakPoint", 3);
    m_debuggerAgent->pauseAll();
    evaluateJavaScript(script, "liveBreakPoint");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 2);
    QV4::Debugging::Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.at(1);
    QCOMPARE(state.fileName, QString("liveBreakPoint"));
    QCOMPARE(state.lineNumber, 3);
}

void tst_qv4debugger::removePendingBreakPoint()
{
    QString script =
            "var i = 42;\n"
            "var j = i + 1\n"
            "var k = i\n";
    int id = m_debuggerAgent->addBreakPoint("removePendingBreakPoint", 2);
    m_debuggerAgent->removeBreakPoint(id);
    evaluateJavaScript(script, "removePendingBreakPoint");
    QVERIFY(!m_debuggerAgent->m_wasPaused);
}

void tst_qv4debugger::addBreakPointWhilePaused()
{
    QString script =
            "var i = 42;\n"
            "var j = i + 1\n"
            "var k = i\n";
    m_debuggerAgent->addBreakPoint("addBreakPointWhilePaused", 1);
    m_debuggerAgent->m_breakPointsToAddWhenPaused << TestAgent::TestBreakPoint("addBreakPointWhilePaused", 2);
    evaluateJavaScript(script, "addBreakPointWhilePaused");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 2);

    QV4::Debugging::Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.at(0);
    QCOMPARE(state.fileName, QString("addBreakPointWhilePaused"));
    QCOMPARE(state.lineNumber, 1);

    state = m_debuggerAgent->m_statesWhenPaused.at(1);
    QCOMPARE(state.fileName, QString("addBreakPointWhilePaused"));
    QCOMPARE(state.lineNumber, 2);
}

static QV4::ReturnedValue someCall(QV4::CallContext *ctx)
{
    ctx->d()->engine->debugger->removeBreakPoint("removeBreakPointForNextInstruction", 2);
    return QV4::Encode::undefined();
}

void tst_qv4debugger::removeBreakPointForNextInstruction()
{
    QString script =
            "someCall();\n"
            "var i = 42;";

    QMetaObject::invokeMethod(m_engine, "injectFunction", Qt::BlockingQueuedConnection,
                              Q_ARG(QString, "someCall"), Q_ARG(InjectedFunction, someCall));

    m_debuggerAgent->addBreakPoint("removeBreakPointForNextInstruction", 2);

    evaluateJavaScript(script, "removeBreakPointForNextInstruction");
    QVERIFY(!m_debuggerAgent->m_wasPaused);
}

void tst_qv4debugger::conditionalBreakPoint()
{
    m_debuggerAgent->m_captureContextInfo = true;
    QString script =
            "function test() {\n"
            "    for (var i = 0; i < 15; ++i) {\n"
            "        var x = i;\n"
            "    }\n"
            "}\n"
            "test()\n";

    m_debuggerAgent->addBreakPoint("conditionalBreakPoint", 3, /*enabled*/true, QStringLiteral("i > 10"));
    evaluateJavaScript(script, "conditionalBreakPoint");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 4);
    QV4::Debugging::Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.first();
    QCOMPARE(state.fileName, QString("conditionalBreakPoint"));
    QCOMPARE(state.lineNumber, 3);
    QCOMPARE(m_debuggerAgent->m_capturedLocals[0].size(), 2);
    QVERIFY(m_debuggerAgent->m_capturedLocals[0].contains(QStringLiteral("i")));
    QCOMPARE(m_debuggerAgent->m_capturedLocals[0]["i"].toInt(), 11);
}

void tst_qv4debugger::conditionalBreakPointInQml()
{
    QQmlEngine engine;
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(&engine);
    v4->enableDebugger();

    QScopedPointer<QThread> debugThread(new QThread);
    debugThread->start();
    QScopedPointer<TestAgent> debuggerAgent(new TestAgent);
    debuggerAgent->addDebugger(v4->debugger);
    debuggerAgent->moveToThread(debugThread.data());

    QQmlComponent component(&engine);
    component.setData("import QtQml 2.0\n"
                      "QtObject {\n"
                      "    id: root\n"
                      "    property int foo: 42\n"
                      "    property bool success: false\n"
                      "    Component.onCompleted: {\n"
                      "        success = true;\n" // breakpoint here
                      "    }\n"
                      "}\n", QUrl("test.qml"));

    debuggerAgent->addBreakPoint("test.qml", 7, /*enabled*/true, "root.foo == 42");

    QScopedPointer<QObject> obj(component.create());
    QCOMPARE(obj->property("success").toBool(), true);

    QCOMPARE(debuggerAgent->m_statesWhenPaused.count(), 1);
    QCOMPARE(debuggerAgent->m_statesWhenPaused.at(0).fileName, QStringLiteral("test.qml"));
    QCOMPARE(debuggerAgent->m_statesWhenPaused.at(0).lineNumber, 7);

    debugThread->quit();
    debugThread->wait();
}

void tst_qv4debugger::readArguments()
{
    m_debuggerAgent->m_captureContextInfo = true;
    QString script =
            "function f(a, b, c, d) {\n"
            "  return a === b\n"
            "}\n"
            "var four;\n"
            "f(1, 'two', null, four);\n";
    m_debuggerAgent->addBreakPoint("readArguments", 2);
    evaluateJavaScript(script, "readArguments");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_capturedArguments[0].size(), 4);
    QVERIFY(m_debuggerAgent->m_capturedArguments[0].contains(QStringLiteral("a")));
    QCOMPARE(m_debuggerAgent->m_capturedArguments[0]["a"].type(), QVariant::Double);
    QCOMPARE(m_debuggerAgent->m_capturedArguments[0]["a"].toDouble(), 1.0);
    QVERIFY(m_debuggerAgent->m_capturedArguments[0].contains("b"));
    QCOMPARE(m_debuggerAgent->m_capturedArguments[0]["b"].type(), QVariant::String);
    QCOMPARE(m_debuggerAgent->m_capturedArguments[0]["b"].toString(), QLatin1String("two"));
}

void tst_qv4debugger::readLocals()
{
    m_debuggerAgent->m_captureContextInfo = true;
    QString script =
            "function f(a, b) {\n"
            "  var c = a + b\n"
            "  var d = a - b\n" // breakpoint, c should be set, d should be undefined
            "  return c === d\n"
            "}\n"
            "f(1, 2, 3);\n";
    m_debuggerAgent->addBreakPoint("readLocals", 3);
    evaluateJavaScript(script, "readLocals");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_capturedLocals[0].size(), 2);
    QVERIFY(m_debuggerAgent->m_capturedLocals[0].contains("c"));
    QCOMPARE(m_debuggerAgent->m_capturedLocals[0]["c"].type(), QVariant::Double);
    QCOMPARE(m_debuggerAgent->m_capturedLocals[0]["c"].toDouble(), 3.0);
    QVERIFY(m_debuggerAgent->m_capturedLocals[0].contains("d"));
    QCOMPARE(m_debuggerAgent->m_capturedLocals[0]["d"].toString(), QString("undefined"));
}

void tst_qv4debugger::readObject()
{
    m_debuggerAgent->m_captureContextInfo = true;
    QString script =
            "function f(a) {\n"
            "  var b = a\n"
            "  return b\n"
            "}\n"
            "f({head: 1, tail: { head: 'asdf', tail: null }});\n";
    m_debuggerAgent->addBreakPoint("readObject", 3);
    evaluateJavaScript(script, "readObject");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_capturedLocals[0].size(), 1);
    QVERIFY(m_debuggerAgent->m_capturedLocals[0].contains("b"));
    QCOMPARE(m_debuggerAgent->m_capturedLocals[0]["b"].type(), QVariant::Map);

    QVariantMap b = m_debuggerAgent->m_capturedLocals[0]["b"].toMap();
    QCOMPARE(b.size(), 2);
    QVERIFY(b.contains("head"));
    QCOMPARE(b["head"].type(), QVariant::Double);
    QCOMPARE(b["head"].toDouble(), 1.0);
    QVERIFY(b.contains("tail"));
    QCOMPARE(b["tail"].type(), QVariant::Map);

    QVariantMap b_tail = b["tail"].toMap();
    QCOMPARE(b_tail.size(), 2);
    QVERIFY(b_tail.contains("head"));
    QCOMPARE(b_tail["head"].type(), QVariant::String);
    QCOMPARE(b_tail["head"].toString(), QString("asdf"));
}

void tst_qv4debugger::readContextInAllFrames()
{
    m_debuggerAgent->m_captureContextInfo = true;
    QString script =
            "function fact(n) {\n"
            "  if (n > 1) {\n"
            "    var n_1 = n - 1;\n"
            "    n_1 = fact(n_1);\n"
            "    return n * n_1;\n"
            "  } else\n"
            "    return 1;\n" // breakpoint
            "}\n"
            "fact(12);\n";
    m_debuggerAgent->addBreakPoint("readFormalsInAllFrames", 7);
    evaluateJavaScript(script, "readFormalsInAllFrames");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_stackTrace.size(), 13);
    QCOMPARE(m_debuggerAgent->m_capturedArguments.size(), 13);
    QCOMPARE(m_debuggerAgent->m_capturedLocals.size(), 13);

    for (int i = 0; i < 12; ++i) {
        QCOMPARE(m_debuggerAgent->m_capturedArguments[i].size(), 1);
        QVERIFY(m_debuggerAgent->m_capturedArguments[i].contains("n"));
        QCOMPARE(m_debuggerAgent->m_capturedArguments[i]["n"].type(), QVariant::Double);
        QCOMPARE(m_debuggerAgent->m_capturedArguments[i]["n"].toDouble(), i + 1.0);

        QCOMPARE(m_debuggerAgent->m_capturedLocals[i].size(), 1);
        QVERIFY(m_debuggerAgent->m_capturedLocals[i].contains("n_1"));
        if (i == 0) {
            QCOMPARE(m_debuggerAgent->m_capturedLocals[i]["n_1"].toString(), QString("undefined"));
        } else {
            QCOMPARE(m_debuggerAgent->m_capturedLocals[i]["n_1"].type(), QVariant::Double);
            QCOMPARE(m_debuggerAgent->m_capturedLocals[i]["n_1"].toInt(), i);
        }
    }
    QCOMPARE(m_debuggerAgent->m_capturedArguments[12].size(), 0);
    QCOMPARE(m_debuggerAgent->m_capturedLocals[12].size(), 0);
}

void tst_qv4debugger::pauseOnThrow()
{
    QString script =
            "function die(n) {\n"
            "  throw n\n"
            "}\n"
            "die('hard');\n";
    m_debuggerAgent->setBreakOnThrow(true);
    evaluateJavaScript(script, "pauseOnThrow");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_pauseReason, Throwing);
    QCOMPARE(m_debuggerAgent->m_stackTrace.size(), 2);
    QCOMPARE(m_debuggerAgent->m_thrownValue.type(), QVariant::String);
    QCOMPARE(m_debuggerAgent->m_thrownValue.toString(), QString("hard"));
}

void tst_qv4debugger::breakInCatch()
{
    QString script =
            "try {\n"
            "    throw 'catch...'\n"
            "} catch (e) {\n"
            "    console.log(e, 'me');\n"
            "}\n";

    m_debuggerAgent->addBreakPoint("breakInCatch", 4);
    evaluateJavaScript(script, "breakInCatch");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_pauseReason, BreakPoint);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 1);
    QV4::Debugging::Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.first();
    QCOMPARE(state.fileName, QString("breakInCatch"));
    QCOMPARE(state.lineNumber, 4);
}

void tst_qv4debugger::breakInWith()
{
    QString script =
            "with (42) {\n"
            "    console.log('give the answer');\n"
            "}\n";

    m_debuggerAgent->addBreakPoint("breakInWith", 2);
    evaluateJavaScript(script, "breakInWith");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_pauseReason, BreakPoint);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 1);
    QV4::Debugging::Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.first();
    QCOMPARE(state.fileName, QString("breakInWith"));
    QCOMPARE(state.lineNumber, 2);
}

void tst_qv4debugger::evaluateExpression()
{
    QString script =
            "function testFunction() {\n"
            "    var x = 10\n"
            "    return x\n" // breakpoint
            "}\n"
            "var x = 20\n"
            "testFunction()\n";

    TestAgent::ExpressionRequest request;
    request.expression = "x";
    request.frameNr = 0;
    m_debuggerAgent->m_expressionRequests << request;
    request.expression = "x";
    request.frameNr = 1;
    m_debuggerAgent->m_expressionRequests << request;

    m_debuggerAgent->addBreakPoint("evaluateExpression", 3);

    evaluateJavaScript(script, "evaluateExpression");

    QCOMPARE(m_debuggerAgent->m_expressionResults.count(), 2);
    QCOMPARE(m_debuggerAgent->m_expressionResults[0].toInt(), 10);
    QCOMPARE(m_debuggerAgent->m_expressionResults[1].toInt(), 20);
}

QTEST_MAIN(tst_qv4debugger)

#include "tst_qv4debugger.moc"
