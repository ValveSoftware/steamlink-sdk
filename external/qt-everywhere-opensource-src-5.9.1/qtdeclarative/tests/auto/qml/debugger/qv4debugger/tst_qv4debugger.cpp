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
#include <QtTest/QtTest>

#include "qv4datacollector.h"
#include "qv4debugger.h"
#include "qv4debugjob.h"

#include <QJSEngine>
#include <QQmlEngine>
#include <QQmlComponent>
#include <private/qv4engine_p.h>
#include <private/qv4debugging_p.h>
#include <private/qv8engine_p.h>
#include <private/qv4objectiterator_p.h>
#include <private/qv4isel_moth_p.h>
#include <private/qv4string_p.h>
#include <private/qqmlbuiltinfunctions_p.h>
#include <private/qqmldebugservice_p.h>

using namespace QV4;
using namespace QV4::Debugging;

typedef void (*InjectedFunction)(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *callData);
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

        QV4::ScopedString name(scope, v4->newString(functionName));
        QV4::ScopedContext ctx(scope, v4->rootContext());
        QV4::ScopedValue function(scope, BuiltinFunction::create(ctx, name, injectedFunction));
        v4->globalObject->put(name, function);
    }

signals:
    void evaluateFinished();
};

class ExceptionCollectJob: public CollectJob
{
    QV4DataCollector::Ref exception;

public:
    ExceptionCollectJob(QV4DataCollector *collector) :
        CollectJob(collector), exception(-1) {}

    void run() {
        QV4::Scope scope(collector->engine());
        QV4::ScopedValue v(scope, *collector->engine()->exceptionValue);
        exception = collector->collect(v);
    }

    QV4DataCollector::Ref exceptionValue() const { return exception; }
};

class TestAgent : public QObject
{
    Q_OBJECT
public:
    typedef QV4DataCollector::Refs Refs;
    typedef QV4DataCollector::Ref Ref;
    struct NamedRefs {
        QJsonObject scope;

        int size() const {
            return scope.size();
        }

        bool contains(const QString &name) const {
            return scope.contains(name);
        }

#define DUMP_JSON(x) {\
    QJsonDocument doc(x);\
    qDebug() << #x << "=" << doc.toJson(QJsonDocument::Indented);\
}

        QJsonObject rawValue(const QString &name) const {
            Q_ASSERT(contains(name));
            return scope[name].toObject();
        }

        QJsonValue value(const QString &name) const {
            return rawValue(name).value(QStringLiteral("value"));
        }

        QString type(const QString &name) const {
            return rawValue(name).value(QStringLiteral("type")).toString();
        }

        void dump(const QString &name) const {
            if (!contains(name)) {
                qDebug() << "no" << name;
                return;
            }

            QJsonObject o = scope[name].toObject();
            QJsonDocument d;
            d.setObject(o);
            qDebug() << name << "=" << d.toJson(QJsonDocument::Indented);
        }
    };

    TestAgent(QV4::ExecutionEngine *engine)
        : m_wasPaused(false)
        , m_captureContextInfo(false)
        , m_thrownValue(-1)
        , collector(engine)
        , m_resumeSpeed(QV4Debugger::FullThrottle)
        , m_debugger(0)
    {
    }

public slots:
    void debuggerPaused(QV4Debugger *debugger, QV4Debugger::PauseReason reason)
    {
        Q_ASSERT(debugger == m_debugger);
        Q_ASSERT(debugger->engine() == collector.engine());
        m_wasPaused = true;
        m_pauseReason = reason;
        m_statesWhenPaused << debugger->currentExecutionState();

        if (debugger->state() == QV4Debugger::Paused && debugger->engine()->hasException) {
            ExceptionCollectJob job(&collector);
            debugger->runInEngine(&job);
            m_thrownValue = job.exceptionValue();
        }

        foreach (const TestBreakPoint &bp, m_breakPointsToAddWhenPaused)
            debugger->addBreakPoint(bp.fileName, bp.lineNumber);
        m_breakPointsToAddWhenPaused.clear();

        m_stackTrace = debugger->stackTrace();

        while (!m_expressionRequests.isEmpty()) {
            Q_ASSERT(debugger->state() == QV4Debugger::Paused);
            ExpressionRequest request = m_expressionRequests.takeFirst();
            ExpressionEvalJob job(debugger->engine(), request.frameNr, request.context,
                                  request.expression, &collector);
            debugger->runInEngine(&job);
            m_expressionResults << job.returnValue();
        }

        if (m_captureContextInfo)
            captureContextInfo(debugger);

        debugger->resume(m_resumeSpeed);
    }

public:
    struct TestBreakPoint
    {
        TestBreakPoint() : lineNumber(-1) {}
        TestBreakPoint(const QString &fileName, int lineNumber)
            : fileName(fileName), lineNumber(lineNumber) {}
        QString fileName;
        int lineNumber;
    };

    void captureContextInfo(QV4Debugger *debugger)
    {
        for (int i = 0, ei = m_stackTrace.size(); i != ei; ++i) {
            m_capturedScope.append(NamedRefs());
            ScopeJob job(&collector, i, 0);
            debugger->runInEngine(&job);
            NamedRefs &refs = m_capturedScope.last();
            QJsonObject object = job.returnValue();
            object = object.value(QLatin1String("object")).toObject();
            if (object.contains("ref") && !object.contains("properties")) {
                QVERIFY(collector.redundantRefs());
                object = collector.lookupRef(object.value("ref").toInt(), true);
                QVERIFY(object.contains("properties"));
            }
            foreach (const QJsonValue &value, object.value(QLatin1String("properties")).toArray()) {
                QJsonObject property = value.toObject();
                QString name = property.value(QLatin1String("name")).toString();
                property.remove(QLatin1String("name"));
                refs.scope.insert(name, property);
            }
        }
    }

    void addDebugger(QV4Debugger *debugger)
    {
        Q_ASSERT(!m_debugger);
        m_debugger = debugger;
        connect(m_debugger, &QV4Debugger::debuggerPaused, this, &TestAgent::debuggerPaused);
    }

    bool m_wasPaused;
    QV4Debugger::PauseReason m_pauseReason;
    bool m_captureContextInfo;
    QList<QV4Debugger::ExecutionState> m_statesWhenPaused;
    QList<TestBreakPoint> m_breakPointsToAddWhenPaused;
    QVector<QV4::StackFrame> m_stackTrace;
    QVector<NamedRefs> m_capturedScope;
    qint64 m_thrownValue;
    QV4DataCollector collector;

    struct ExpressionRequest {
        QString expression;
        int frameNr;
        int context;
    };
    QVector<ExpressionRequest> m_expressionRequests;
    QV4Debugger::Speed m_resumeSpeed;
    QList<QJsonObject> m_expressionResults;
    QV4Debugger *m_debugger;

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
    void readArguments_data() { redundancy_data(); }
    void readArguments();
    void readLocals_data() { redundancy_data(); }
    void readLocals();
    void readObject_data() { redundancy_data(); }
    void readObject();
    void readContextInAllFrames_data() { redundancy_data(); }
    void readContextInAllFrames();

    // exceptions:
    void pauseOnThrow();
    void breakInCatch();
    void breakInWith();

    void evaluateExpression_data() { redundancy_data(); }
    void evaluateExpression();
    void stepToEndOfScript_data() { redundancy_data(); }
    void stepToEndOfScript();

    void lastLineOfConditional_data();
    void lastLineOfConditional();
private:
    QV4Debugger *debugger() const
    {
        return static_cast<QV4Debugger *>(m_v4->debugger());
    }
    void evaluateJavaScript(const QString &script, const QString &fileName, int lineNumber = 1)
    {
        QMetaObject::invokeMethod(m_engine, "evaluate", Qt::QueuedConnection,
                                  Q_ARG(QString, script), Q_ARG(QString, fileName),
                                  Q_ARG(int, lineNumber));
        waitForSignal(m_engine, SIGNAL(evaluateFinished()), /*timeout*/0);
    }

    void redundancy_data();

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
    m_v4->iselFactory.reset(new QV4::Moth::ISelFactory);
    m_v4->setDebugger(new QV4Debugger(m_v4));
    m_engine->moveToThread(m_javaScriptThread);
    m_javaScriptThread->start();
    m_debuggerAgent = new TestAgent(m_v4);
    m_debuggerAgent->addDebugger(debugger());
}

void tst_qv4debugger::cleanup()
{
    m_javaScriptThread->exit();
    m_javaScriptThread->wait();
    delete m_engine;
    delete m_javaScriptThread;
    m_engine = 0;
    m_v4 = 0;
    delete m_debuggerAgent;
    m_debuggerAgent = 0;
}

void tst_qv4debugger::breakAnywhere()
{
    QString script =
            "var i = 42;\n"
            "var j = i + 1\n"
            "var k = i\n";
    debugger()->pause();
    evaluateJavaScript(script, "testFile");
    QVERIFY(m_debuggerAgent->m_wasPaused);
}

void tst_qv4debugger::pendingBreakpoint()
{
    QString script =
            "var i = 42;\n"
            "var j = i + 1\n"
            "var k = i\n";
    debugger()->addBreakPoint("testfile", 2);
    evaluateJavaScript(script, "testfile");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 1);
    QV4Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.first();
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
    debugger()->pause();
    evaluateJavaScript(script, "liveBreakPoint");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 2);
    QV4Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.at(1);
    QCOMPARE(state.fileName, QString("liveBreakPoint"));
    QCOMPARE(state.lineNumber, 3);
}

void tst_qv4debugger::removePendingBreakPoint()
{
    QString script =
            "var i = 42;\n"
            "var j = i + 1\n"
            "var k = i\n";
    debugger()->addBreakPoint("removePendingBreakPoint", 2);
    debugger()->removeBreakPoint("removePendingBreakPoint", 2);
    evaluateJavaScript(script, "removePendingBreakPoint");
    QVERIFY(!m_debuggerAgent->m_wasPaused);
}

void tst_qv4debugger::addBreakPointWhilePaused()
{
    QString script =
            "var i = 42;\n"
            "var j = i + 1\n"
            "var k = i\n";
    debugger()->addBreakPoint("addBreakPointWhilePaused", 1);
    m_debuggerAgent->m_breakPointsToAddWhenPaused << TestAgent::TestBreakPoint("addBreakPointWhilePaused", 2);
    evaluateJavaScript(script, "addBreakPointWhilePaused");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 2);

    QV4Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.at(0);
    QCOMPARE(state.fileName, QString("addBreakPointWhilePaused"));
    QCOMPARE(state.lineNumber, 1);

    state = m_debuggerAgent->m_statesWhenPaused.at(1);
    QCOMPARE(state.fileName, QString("addBreakPointWhilePaused"));
    QCOMPARE(state.lineNumber, 2);
}

static void someCall(const QV4::BuiltinFunction *, QV4::Scope &scope, QV4::CallData *)
{
    static_cast<QV4Debugger *>(scope.engine->debugger())
            ->removeBreakPoint("removeBreakPointForNextInstruction", 2);
    RETURN_UNDEFINED();
}

void tst_qv4debugger::removeBreakPointForNextInstruction()
{
    QString script =
            "someCall();\n"
            "var i = 42;";

    QMetaObject::invokeMethod(m_engine, "injectFunction", Qt::BlockingQueuedConnection,
                              Q_ARG(QString, "someCall"), Q_ARG(InjectedFunction, someCall));

    debugger()->addBreakPoint("removeBreakPointForNextInstruction", 2);

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

    debugger()->addBreakPoint("conditionalBreakPoint", 3, QStringLiteral("i > 10"));
    evaluateJavaScript(script, "conditionalBreakPoint");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 4);
    QV4Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.first();
    QCOMPARE(state.fileName, QString("conditionalBreakPoint"));
    QCOMPARE(state.lineNumber, 3);

    QVERIFY(m_debuggerAgent->m_capturedScope.size() > 1);
    const TestAgent::NamedRefs &frame0 = m_debuggerAgent->m_capturedScope.at(0);
    QCOMPARE(frame0.size(), 2);
    QVERIFY(frame0.contains("i"));
    QCOMPARE(frame0.value("i").toInt(), 11);
}

void tst_qv4debugger::conditionalBreakPointInQml()
{
    QQmlEngine engine;
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(&engine);
    QV4Debugger *v4Debugger = new QV4Debugger(v4);
    v4->iselFactory.reset(new QV4::Moth::ISelFactory);
    v4->setDebugger(v4Debugger);

    QScopedPointer<QThread> debugThread(new QThread);
    debugThread->start();
    QScopedPointer<TestAgent> debuggerAgent(new TestAgent(v4));
    debuggerAgent->addDebugger(v4Debugger);
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

    v4Debugger->addBreakPoint("test.qml", 7, "root.foo == 42");

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
    QFETCH(bool, redundantRefs);
    m_debuggerAgent->collector.setRedundantRefs(redundantRefs);

    m_debuggerAgent->m_captureContextInfo = true;
    QString script =
            "function f(a, b, c, d) {\n"
            "  return a === b\n"
            "}\n"
            "var four;\n"
            "f(1, 'two', null, four);\n";
    debugger()->addBreakPoint("readArguments", 2);
    evaluateJavaScript(script, "readArguments");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QVERIFY(m_debuggerAgent->m_capturedScope.size() > 1);
    const TestAgent::NamedRefs &frame0 = m_debuggerAgent->m_capturedScope.at(0);
    QCOMPARE(frame0.size(), 4);
    QVERIFY(frame0.contains(QStringLiteral("a")));
    QCOMPARE(frame0.type(QStringLiteral("a")), QStringLiteral("number"));
    QCOMPARE(frame0.value(QStringLiteral("a")).toDouble(), 1.0);
    QVERIFY(frame0.scope.contains("b"));
    QCOMPARE(frame0.type(QStringLiteral("b")), QStringLiteral("string"));
    QCOMPARE(frame0.value(QStringLiteral("b")).toString(), QStringLiteral("two"));
}

void tst_qv4debugger::readLocals()
{
    QFETCH(bool, redundantRefs);
    m_debuggerAgent->collector.setRedundantRefs(redundantRefs);

    m_debuggerAgent->m_captureContextInfo = true;
    QString script =
            "function f(a, b) {\n"
            "  var c = a + b\n"
            "  var d = a - b\n" // breakpoint, c should be set, d should be undefined
            "  return c === d\n"
            "}\n"
            "f(1, 2, 3);\n";
    debugger()->addBreakPoint("readLocals", 3);
    evaluateJavaScript(script, "readLocals");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QVERIFY(m_debuggerAgent->m_capturedScope.size() > 1);
    const TestAgent::NamedRefs &frame0 = m_debuggerAgent->m_capturedScope.at(0);
    QCOMPARE(frame0.size(), 4); // locals and parameters
    QVERIFY(frame0.contains("c"));
    QCOMPARE(frame0.type("c"), QStringLiteral("number"));
    QCOMPARE(frame0.value("c").toDouble(), 3.0);
    QVERIFY(frame0.contains("d"));
    QCOMPARE(frame0.type("d"), QStringLiteral("undefined"));
}

void tst_qv4debugger::readObject()
{
    QFETCH(bool, redundantRefs);
    m_debuggerAgent->collector.setRedundantRefs(redundantRefs);

    m_debuggerAgent->m_captureContextInfo = true;
    QString script =
            "function f(a) {\n"
            "  var b = a\n"
            "  return b\n"
            "}\n"
            "f({head: 1, tail: { head: 'asdf', tail: null }});\n";
    debugger()->addBreakPoint("readObject", 3);
    evaluateJavaScript(script, "readObject");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QVERIFY(m_debuggerAgent->m_capturedScope.size() > 1);
    const TestAgent::NamedRefs &frame0 = m_debuggerAgent->m_capturedScope.at(0);
    QCOMPARE(frame0.size(), 2);
    QVERIFY(frame0.contains("b"));
    QCOMPARE(frame0.type("b"), QStringLiteral("object"));
    QJsonObject b = frame0.rawValue("b");
    QVERIFY(b.contains(QStringLiteral("ref")));
    QVERIFY(b.contains(QStringLiteral("value")));
    QVERIFY(!b.contains(QStringLiteral("properties")));
    QVERIFY(b.value("value").isDouble());
    QCOMPARE(b.value("value").toInt(), 2);
    b = m_debuggerAgent->collector.lookupRef(b.value("ref").toInt(), true);
    QVERIFY(b.contains(QStringLiteral("properties")));
    QVERIFY(b.value("properties").isArray());
    QJsonArray b_props = b.value("properties").toArray();
    QCOMPARE(b_props.size(), 2);

    QVERIFY(b_props.at(0).isObject());
    QJsonObject b_head = b_props.at(0).toObject();
    QCOMPARE(b_head.value("name").toString(), QStringLiteral("head"));
    QCOMPARE(b_head.value("type").toString(), QStringLiteral("number"));
    QCOMPARE(b_head.value("value").toDouble(), 1.0);
    QVERIFY(b_props.at(1).isObject());
    QJsonObject b_tail = b_props.at(1).toObject();
    QCOMPARE(b_tail.value("name").toString(), QStringLiteral("tail"));
    QVERIFY(b_tail.contains("ref"));

    QJsonObject b_tail_value = m_debuggerAgent->collector.lookupRef(b_tail.value("ref").toInt(),
                                                                    true);
    QCOMPARE(b_tail_value.value("type").toString(), QStringLiteral("object"));
    QVERIFY(b_tail_value.contains("properties"));
    QJsonArray b_tail_props = b_tail_value.value("properties").toArray();
    QCOMPARE(b_tail_props.size(), 2);
    QJsonObject b_tail_head = b_tail_props.at(0).toObject();
    QCOMPARE(b_tail_head.value("name").toString(), QStringLiteral("head"));
    QCOMPARE(b_tail_head.value("type").toString(), QStringLiteral("string"));
    QCOMPARE(b_tail_head.value("value").toString(), QStringLiteral("asdf"));
    QJsonObject b_tail_tail = b_tail_props.at(1).toObject();
    QCOMPARE(b_tail_tail.value("name").toString(), QStringLiteral("tail"));
    QCOMPARE(b_tail_tail.value("type").toString(), QStringLiteral("null"));
    QVERIFY(b_tail_tail.value("value").isNull());
}

void tst_qv4debugger::readContextInAllFrames()
{
    QFETCH(bool, redundantRefs);
    m_debuggerAgent->collector.setRedundantRefs(redundantRefs);

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
    debugger()->addBreakPoint("readFormalsInAllFrames", 7);
    evaluateJavaScript(script, "readFormalsInAllFrames");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_stackTrace.size(), 13);
    QCOMPARE(m_debuggerAgent->m_capturedScope.size(), 13);

    for (int i = 0; i < 12; ++i) {
        const TestAgent::NamedRefs &scope = m_debuggerAgent->m_capturedScope.at(i);
        QCOMPARE(scope.size(), 2);
        QVERIFY(scope.contains("n"));
        QCOMPARE(scope.type("n"), QStringLiteral("number"));
        QCOMPARE(scope.value("n").toDouble(), i + 1.0);
        QVERIFY(scope.contains("n_1"));
        if (i == 0) {
            QCOMPARE(scope.type("n_1"), QStringLiteral("undefined"));
        } else {
            QCOMPARE(scope.type("n_1"), QStringLiteral("number"));
            QCOMPARE(scope.value("n_1").toInt(), i);
        }
    }
    QCOMPARE(m_debuggerAgent->m_capturedScope[12].size(), 0);
}

void tst_qv4debugger::pauseOnThrow()
{
    QString script =
            "function die(n) {\n"
            "  throw n\n"
            "}\n"
            "die('hard');\n";
    debugger()->setBreakOnThrow(true);
    evaluateJavaScript(script, "pauseOnThrow");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_pauseReason, QV4Debugger::Throwing);
    QCOMPARE(m_debuggerAgent->m_stackTrace.size(), 2);
    QVERIFY(m_debuggerAgent->m_thrownValue >= qint64(0));
    QJsonObject exception = m_debuggerAgent->collector.lookupRef(m_debuggerAgent->m_thrownValue,
                                                                 true);
//    DUMP_JSON(exception);
    QCOMPARE(exception.value("type").toString(), QStringLiteral("string"));
    QCOMPARE(exception.value("value").toString(), QStringLiteral("hard"));
}

void tst_qv4debugger::breakInCatch()
{
    QString script =
            "try {\n"
            "    throw 'catch...'\n"
            "} catch (e) {\n"
            "    console.log(e, 'me');\n"
            "}\n";

    debugger()->addBreakPoint("breakInCatch", 4);
    evaluateJavaScript(script, "breakInCatch");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_pauseReason, QV4Debugger::BreakPointHit);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 1);
    QV4Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.first();
    QCOMPARE(state.fileName, QString("breakInCatch"));
    QCOMPARE(state.lineNumber, 4);
}

void tst_qv4debugger::breakInWith()
{
    QString script =
            "with (42) {\n"
            "    console.log('give the answer');\n"
            "}\n";

    debugger()->addBreakPoint("breakInWith", 2);
    evaluateJavaScript(script, "breakInWith");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_pauseReason, QV4Debugger::BreakPointHit);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 1);
    QV4Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.first();
    QCOMPARE(state.fileName, QString("breakInWith"));
    QCOMPARE(state.lineNumber, 2);
}

void tst_qv4debugger::evaluateExpression()
{
    QFETCH(bool, redundantRefs);
    m_debuggerAgent->collector.setRedundantRefs(redundantRefs);

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
    request.context = -1; // no extra context
    m_debuggerAgent->m_expressionRequests << request;
    request.expression = "x";
    request.frameNr = 1;
    m_debuggerAgent->m_expressionRequests << request;

    request.context = 5355; // invalid context object
    m_debuggerAgent->m_expressionRequests << request;

    QObject object; // some object without QML context
    request.context = QQmlDebugService::idForObject(&object);
    m_debuggerAgent->m_expressionRequests << request;

    debugger()->addBreakPoint("evaluateExpression", 3);

    evaluateJavaScript(script, "evaluateExpression");

    QCOMPARE(m_debuggerAgent->m_expressionResults.count(), 4);
    QJsonObject result0 = m_debuggerAgent->m_expressionResults[0];
    QCOMPARE(result0.value("type").toString(), QStringLiteral("number"));
    QCOMPARE(result0.value("value").toInt(), 10);
    for (int i = 1; i < 4; ++i) {
        QJsonObject result1 = m_debuggerAgent->m_expressionResults[i];
        QCOMPARE(result1.value("type").toString(), QStringLiteral("number"));
        QCOMPARE(result1.value("value").toInt(), 20);
    }
}

void tst_qv4debugger::stepToEndOfScript()
{
    QFETCH(bool, redundantRefs);
    m_debuggerAgent->collector.setRedundantRefs(redundantRefs);

    QString script =
            "var ret = 0;\n"
            "ret += 4;\n"
            "ret += 1;\n"
            "ret += 5;\n";

    debugger()->addBreakPoint("toEnd", 1);
    m_debuggerAgent->m_resumeSpeed = QV4Debugger::StepOver;
    evaluateJavaScript(script, "toEnd");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_pauseReason, QV4Debugger::Step);
    QCOMPARE(m_debuggerAgent->m_statesWhenPaused.count(), 5);
    for (int i = 0; i < 4; ++i) {
        QV4Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.at(i);
        QCOMPARE(state.fileName, QString("toEnd"));
        QCOMPARE(state.lineNumber, i + 1);
    }

    QV4Debugger::ExecutionState state = m_debuggerAgent->m_statesWhenPaused.at(4);
    QCOMPARE(state.fileName, QString("toEnd"));
    QCOMPARE(state.lineNumber, -4); // A return instruction without proper line number.
}

void tst_qv4debugger::lastLineOfConditional_data()
{
    QTest::addColumn<QString>("head");
    QTest::addColumn<QString>("tail");
    QTest::addColumn<int>("breakPoint");
    QTest::addColumn<int>("lastLine");

    QTest::newRow("for {block}")       << "for (var i = 0; i < 10; ++i) {\n"   << "}" << 4 << 7;
    QTest::newRow("for..in {block}")   << "for (var i in [0, 1, 2, 3, 4]) {\n" << "}" << 4 << 7;
    QTest::newRow("while {block}")     << "while (ret < 10) {\n"               << "}" << 4 << 7;
    QTest::newRow("do..while {block}") << "do {\n"           << "} while (ret < 10);" << 4 << 7;

    QTest::newRow("if true {block}")       << "if (true) {\n"  << "}"
                                           << 4 << 7;
    QTest::newRow("if false {block}")      << "if (false) {\n" << "}"
                                           << 2 << 8;
    QTest::newRow("if true else {block}")  << "if (true) {\n"  << "} else {\n    ret += 8;\n}"
                                           << 4 << 7;
    QTest::newRow("if false else {block}") << "if (false) {\n" << "} else {\n    ret += 8;\n}"
                                           << 8 << 9;

    QTest::newRow("for statement")       << "for (var i = 0; i < 10; ++i)\n"   << "" << 4 << 2;
    QTest::newRow("for..in statement")   << "for (var i in [0, 1, 2, 3, 4])\n" << "" << 4 << 2;
    QTest::newRow("while statement")     << "while (ret < 10)\n"               << "" << 4 << 2;
    QTest::newRow("do..while statement") << "do\n"            << "while (ret < 10);" << 4 << 7;

    // For two nested if statements without blocks, we need to map the jump from the inner to the
    // outer one on the outer "if". There is just no better place.
    QTest::newRow("if true statement")       << "if (true)\n"  << ""                    << 4 << 2;
    QTest::newRow("if false statement")      << "if (false)\n" << ""                    << 2 << 8;

    // Also two nested ifs without blocks.
    QTest::newRow("if true else statement")  << "if (true)\n"  << "else\n    ret += 8;" << 4 << 2;
    QTest::newRow("if false else statement") << "if (false)\n" << "else\n    ret += 8;" << 8 << 9;
}

void tst_qv4debugger::lastLineOfConditional()
{
    QFETCH(QString, head);
    QFETCH(QString, tail);
    QFETCH(int, breakPoint);
    QFETCH(int, lastLine);

    QString script =
            "var ret = 2;\n"
            + head +
            "    if (ret == 2)\n"
            "        ret += 4;\n" // breakpoint, then step over
            "    else\n"
            "        ret += 1;\n"
            + tail + "\n" +
            "ret -= 5;";

    debugger()->addBreakPoint("trueBranch", breakPoint);
    m_debuggerAgent->m_resumeSpeed = QV4Debugger::StepOver;
    evaluateJavaScript(script, "trueBranch");
    QVERIFY(m_debuggerAgent->m_wasPaused);
    QCOMPARE(m_debuggerAgent->m_pauseReason, QV4Debugger::Step);
    QVERIFY(m_debuggerAgent->m_statesWhenPaused.count() > 1);
    QV4Debugger::ExecutionState firstState = m_debuggerAgent->m_statesWhenPaused.first();
    QCOMPARE(firstState.fileName, QString("trueBranch"));
    QCOMPARE(firstState.lineNumber, breakPoint);
    QV4Debugger::ExecutionState secondState = m_debuggerAgent->m_statesWhenPaused.at(1);
    QCOMPARE(secondState.fileName, QString("trueBranch"));
    QCOMPARE(secondState.lineNumber, lastLine);
}

void tst_qv4debugger::redundancy_data()
{
    QTest::addColumn<bool>("redundantRefs");
    QTest::addRow("redundant") << true;
    QTest::addRow("sparse")    << false;
}

QTEST_MAIN(tst_qv4debugger)

#include "tst_qv4debugger.moc"
