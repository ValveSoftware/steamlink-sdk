/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qv4debugging_p.h"
#include "qv4object_p.h"
#include "qv4functionobject_p.h"
#include "qv4function_p.h"
#include "qv4instr_moth_p.h"
#include "qv4runtime_p.h"
#include "qv4script_p.h"
#include <iostream>

#include <algorithm>

using namespace QV4;
using namespace QV4::Debugging;

namespace {
class JavaScriptJob: public Debugger::Job
{
    QV4::ExecutionEngine *engine;
    int frameNr;
    const QString &script;

public:
    JavaScriptJob(QV4::ExecutionEngine *engine, int frameNr, const QString &script)
        : engine(engine)
        , frameNr(frameNr)
        , script(script)
    {}

    void run()
    {
        Scope scope(engine);

        ExecutionContextSaver saver(engine->currentContext());

        if (frameNr > 0) {
            Value *savedContexts = scope.alloc(frameNr);
            for (int i = 0; i < frameNr; ++i) {
                savedContexts[i] = engine->currentContext();
                engine->popContext();
            }
        }

        ExecutionContext *ctx = engine->currentContext();
        QV4::Script script(ctx, this->script);
        script.strictMode = ctx->d()->strictMode;
        // In order for property lookups in QML to work, we need to disable fast v4 lookups. That
        // is a side-effect of inheritContext.
        script.inheritContext = true;
        script.parse();
        QV4::ScopedValue result(scope);
        if (!scope.engine->hasException)
            result = script.run();
        if (scope.engine->hasException)
            result = ctx->catchException();
        handleResult(result);
    }

protected:
    virtual void handleResult(QV4::ScopedValue &result) = 0;
};

class EvalJob: public JavaScriptJob
{
    bool result;

public:
    EvalJob(QV4::ExecutionEngine *engine, const QString &script)
        : JavaScriptJob(engine, /*frameNr*/-1, script)
        , result(false)
    {}

    virtual void handleResult(QV4::ScopedValue &result)
    {
        this->result = result->toBoolean();
    }

    bool resultAsBoolean() const
    {
        return result;
    }
};

class ExpressionEvalJob: public JavaScriptJob
{
    Debugger::Collector *collector;

public:
    ExpressionEvalJob(ExecutionEngine *engine, int frameNr, const QString &expression, Debugger::Collector *collector)
        : JavaScriptJob(engine, frameNr, expression)
        , collector(collector)
    {
    }

    virtual void handleResult(QV4::ScopedValue &result)
    {
        collector->collect(QStringLiteral("body"), result);
    }
};

class GatherSourcesJob: public Debugger::Job
{
    QV4::ExecutionEngine *engine;
    const int seq;

public:
    GatherSourcesJob(QV4::ExecutionEngine *engine, int seq)
        : engine(engine)
        , seq(seq)
    {}

    ~GatherSourcesJob() {}

    void run()
    {
        QStringList sources;

        foreach (QV4::CompiledData::CompilationUnit *unit, engine->compilationUnits) {
            QString fileName = unit->fileName();
            if (!fileName.isEmpty())
                sources.append(fileName);
        }

        Debugger *debugger = engine->debugger;
        QMetaObject::invokeMethod(debugger->agent(), "sourcesCollected", Qt::QueuedConnection,
                                  Q_ARG(QV4::Debugging::Debugger*, debugger),
                                  Q_ARG(QStringList, sources),
                                  Q_ARG(int, seq));
    }
};
}

Debugger::Debugger(QV4::ExecutionEngine *engine)
    : m_engine(engine)
    , m_currentContext(0)
    , m_agent(0)
    , m_state(Running)
    , m_stepping(NotStepping)
    , m_pauseRequested(false)
    , m_haveBreakPoints(false)
    , m_breakOnThrow(false)
    , m_returnedValue(Primitive::undefinedValue())
    , m_gatherSources(0)
    , m_runningJob(0)
{
    qMetaTypeId<Debugger*>();
    qMetaTypeId<PauseReason>();
}

Debugger::~Debugger()
{
    detachFromAgent();
}

void Debugger::attachToAgent(DebuggerAgent *agent)
{
    Q_ASSERT(!m_agent);
    m_agent = agent;
}

void Debugger::detachFromAgent()
{
    DebuggerAgent *agent = 0;
    {
        QMutexLocker locker(&m_lock);
        agent = m_agent;
        m_agent = 0;
    }
    if (agent)
        agent->removeDebugger(this);
}

void Debugger::gatherSources(int requestSequenceNr)
{
    QMutexLocker locker(&m_lock);

    m_gatherSources = new GatherSourcesJob(m_engine, requestSequenceNr);
    if (m_state == Paused) {
        runInEngine_havingLock(m_gatherSources);
        delete m_gatherSources;
        m_gatherSources = 0;
    }
}

void Debugger::pause()
{
    QMutexLocker locker(&m_lock);
    if (m_state == Paused)
        return;
    m_pauseRequested = true;
}

void Debugger::resume(Speed speed)
{
    QMutexLocker locker(&m_lock);
    if (m_state != Paused)
        return;

    if (!m_returnedValue.isUndefined())
        m_returnedValue = Encode::undefined();

    m_currentContext = m_engine->currentContext();
    m_stepping = speed;
    m_runningCondition.wakeAll();
}

void Debugger::addBreakPoint(const QString &fileName, int lineNumber, const QString &condition)
{
    QMutexLocker locker(&m_lock);
    m_breakPoints.insert(DebuggerBreakPoint(fileName.mid(fileName.lastIndexOf('/') + 1), lineNumber), condition);
    m_haveBreakPoints = true;
}

void Debugger::removeBreakPoint(const QString &fileName, int lineNumber)
{
    QMutexLocker locker(&m_lock);
    m_breakPoints.remove(DebuggerBreakPoint(fileName.mid(fileName.lastIndexOf('/') + 1), lineNumber));
    m_haveBreakPoints = !m_breakPoints.isEmpty();
}

void Debugger::setBreakOnThrow(bool onoff)
{
    QMutexLocker locker(&m_lock);

    m_breakOnThrow = onoff;
}

Debugger::ExecutionState Debugger::currentExecutionState() const
{
    ExecutionState state;
    state.fileName = getFunction()->sourceFile();
    state.lineNumber = engine()->currentContext()->d()->lineNumber;

    return state;
}

QVector<StackFrame> Debugger::stackTrace(int frameLimit) const
{
    return m_engine->stackTrace(frameLimit);
}

static inline CallContext *findContext(ExecutionContext *ctxt, int frame)
{
    while (ctxt) {
        CallContext *cCtxt = ctxt->asCallContext();
        if (cCtxt && cCtxt->d()->function) {
            if (frame < 1)
                return cCtxt;
            --frame;
        }
        ctxt = ctxt->d()->parent;
    }

    return 0;
}

static inline CallContext *findScope(ExecutionContext *ctxt, int scope)
{
    for (; scope > 0 && ctxt; --scope)
        ctxt = ctxt->d()->outer;

    return ctxt ? ctxt->asCallContext() : 0;
}

void Debugger::collectArgumentsInContext(Collector *collector, int frameNr, int scopeNr)
{
    if (state() != Paused)
        return;

    class ArgumentCollectJob: public Job
    {
        QV4::ExecutionEngine *engine;
        Collector *collector;
        int frameNr;
        int scopeNr;

    public:
        ArgumentCollectJob(QV4::ExecutionEngine *engine, Collector *collector, int frameNr, int scopeNr)
            : engine(engine)
            , collector(collector)
            , frameNr(frameNr)
            , scopeNr(scopeNr)
        {}

        ~ArgumentCollectJob() {}

        void run()
        {
            if (frameNr < 0)
                return;

            CallContext *ctxt = findScope(findContext(engine->currentContext(), frameNr), scopeNr);
            if (!ctxt)
                return;

            Scope scope(engine);
            ScopedValue v(scope);
            int nFormals = ctxt->formalCount();
            for (unsigned i = 0, ei = nFormals; i != ei; ++i) {
                QString qName;
                if (String *name = ctxt->formals()[nFormals - i - 1])
                    qName = name->toQString();
                v = ctxt->argument(i);
                collector->collect(qName, v);
            }
        }
    };

    ArgumentCollectJob job(m_engine, collector, frameNr, scopeNr);
    runInEngine(&job);
}

/// Same as \c retrieveArgumentsFromContext, but now for locals.
void Debugger::collectLocalsInContext(Collector *collector, int frameNr, int scopeNr)
{
    if (state() != Paused)
        return;

    class LocalCollectJob: public Job
    {
        QV4::ExecutionEngine *engine;
        Collector *collector;
        int frameNr;
        int scopeNr;

    public:
        LocalCollectJob(QV4::ExecutionEngine *engine, Collector *collector, int frameNr, int scopeNr)
            : engine(engine)
            , collector(collector)
            , frameNr(frameNr)
            , scopeNr(scopeNr)
        {}

        void run()
        {
            if (frameNr < 0)
                return;

            CallContext *ctxt = findScope(findContext(engine->currentContext(), frameNr), scopeNr);
            if (!ctxt)
                return;

            Scope scope(engine);
            ScopedValue v(scope);
            for (unsigned i = 0, ei = ctxt->variableCount(); i != ei; ++i) {
                QString qName;
                if (String *name = ctxt->variables()[i])
                    qName = name->toQString();
                v = ctxt->d()->locals[i];
                collector->collect(qName, v);
            }
        }
    };

    LocalCollectJob job(m_engine, collector, frameNr, scopeNr);
    runInEngine(&job);
}

bool Debugger::collectThisInContext(Debugger::Collector *collector, int frame)
{
    if (state() != Paused)
        return false;

    class ThisCollectJob: public Job
    {
        QV4::ExecutionEngine *engine;
        Collector *collector;
        int frameNr;
        bool *foundThis;

    public:
        ThisCollectJob(QV4::ExecutionEngine *engine, Collector *collector, int frameNr, bool *foundThis)
            : engine(engine)
            , collector(collector)
            , frameNr(frameNr)
            , foundThis(foundThis)
        {}

        void run()
        {
            *foundThis = myRun();
        }

        bool myRun()
        {
            ExecutionContext *ctxt = findContext(engine->currentContext(), frameNr);
            while (ctxt) {
                if (CallContext *cCtxt = ctxt->asCallContext())
                    if (cCtxt->d()->activation)
                        break;
                ctxt = ctxt->d()->outer;
            }

            if (!ctxt)
                return false;

            Scope scope(engine);
            ScopedObject o(scope, ctxt->asCallContext()->d()->activation);
            collector->collect(o);
            return true;
        }
    };

    bool foundThis = false;
    ThisCollectJob job(m_engine, collector, frame, &foundThis);
    runInEngine(&job);
    return foundThis;
}

void Debugger::collectThrownValue(Collector *collector)
{
    if (state() != Paused || !m_engine->hasException)
        return;

    class ThisCollectJob: public Job
    {
        QV4::ExecutionEngine *engine;
        Collector *collector;

    public:
        ThisCollectJob(QV4::ExecutionEngine *engine, Collector *collector)
            : engine(engine)
            , collector(collector)
        {}

        void run()
        {
            Scope scope(engine);
            ScopedValue v(scope, engine->exceptionValue);
            collector->collect(QStringLiteral("exception"), v);
        }
    };

    ThisCollectJob job(m_engine, collector);
    runInEngine(&job);
}

void Debugger::collectReturnedValue(Collector *collector) const
{
    if (state() != Paused)
        return;

    Scope scope(m_engine);
    ScopedObject o(scope, m_returnedValue);
    collector->collect(o);
}

QVector<ExecutionContext::ContextType> Debugger::getScopeTypes(int frame) const
{
    QVector<ExecutionContext::ContextType> types;

    if (state() != Paused)
        return types;

    CallContext *sctxt = findContext(m_engine->currentContext(), frame);
    if (!sctxt || sctxt->d()->type < ExecutionContext::Type_SimpleCallContext)
        return types;
    CallContext *ctxt = static_cast<CallContext *>(sctxt);

    for (ExecutionContext *it = ctxt; it; it = it->d()->outer)
        types.append(it->d()->type);

    return types;
}


void Debugger::evaluateExpression(int frameNr, const QString &expression, Debugger::Collector *resultsCollector)
{
    Q_ASSERT(state() == Paused);

    Q_ASSERT(m_runningJob == 0);
    ExpressionEvalJob job(m_engine, frameNr, expression, resultsCollector);
    runInEngine(&job);
}

void Debugger::maybeBreakAtInstruction()
{
    if (m_runningJob) // do not re-enter when we're doing a job for the debugger.
        return;

    QMutexLocker locker(&m_lock);

    if (m_gatherSources) {
        m_gatherSources->run();
        delete m_gatherSources;
        m_gatherSources = 0;
    }

    switch (m_stepping) {
    case StepOver:
        if (m_currentContext != m_engine->currentContext())
            break;
        // fall through
    case StepIn:
        pauseAndWait(Step);
        return;
    case StepOut:
    case NotStepping:
        break;
    }

    if (m_pauseRequested) { // Serve debugging requests from the agent
        m_pauseRequested = false;
        pauseAndWait(PauseRequest);
    } else if (m_haveBreakPoints) {
        if (Function *f = getFunction()) {
            const int lineNumber = engine()->currentContext()->d()->lineNumber;
            if (reallyHitTheBreakPoint(f->sourceFile(), lineNumber))
                pauseAndWait(BreakPoint);
        }
    }
}

void Debugger::enteringFunction()
{
    if (m_runningJob)
        return;
    QMutexLocker locker(&m_lock);

    if (m_stepping == StepIn) {
        m_currentContext = m_engine->currentContext();
    }
}

void Debugger::leavingFunction(const ReturnedValue &retVal)
{
    if (m_runningJob)
        return;
    Q_UNUSED(retVal); // TODO

    QMutexLocker locker(&m_lock);

    if (m_stepping != NotStepping && m_currentContext == m_engine->currentContext()) {
        m_currentContext = m_engine->currentContext()->d()->parent;
        m_stepping = StepOver;
        m_returnedValue = retVal;
    }
}

void Debugger::aboutToThrow()
{
    if (!m_breakOnThrow)
        return;

    if (m_runningJob) // do not re-enter when we're doing a job for the debugger.
        return;

    QMutexLocker locker(&m_lock);
    pauseAndWait(Throwing);
}

Function *Debugger::getFunction() const
{
    ExecutionContext *context = m_engine->currentContext();
    if (const FunctionObject *function = context->getFunctionObject())
        return function->function();
    else
        return context->d()->engine->globalCode;
}

void Debugger::pauseAndWait(PauseReason reason)
{
    if (m_runningJob)
        return;

    m_state = Paused;
    QMetaObject::invokeMethod(m_agent, "debuggerPaused", Qt::QueuedConnection,
                              Q_ARG(QV4::Debugging::Debugger*, this),
                              Q_ARG(QV4::Debugging::PauseReason, reason));

    while (true) {
        m_runningCondition.wait(&m_lock);
        if (m_runningJob) {
            m_runningJob->run();
            m_jobIsRunning.wakeAll();
        } else {
            break;
        }
    }

    m_state = Running;
}

bool Debugger::reallyHitTheBreakPoint(const QString &filename, int linenr)
{
    BreakPoints::iterator it = m_breakPoints.find(DebuggerBreakPoint(filename.mid(filename.lastIndexOf('/') + 1), linenr));
    if (it == m_breakPoints.end())
        return false;
    QString condition = it.value();
    if (condition.isEmpty())
        return true;

    Q_ASSERT(m_runningJob == 0);
    EvalJob evilJob(m_engine, condition);
    m_runningJob = &evilJob;
    m_runningJob->run();
    m_runningJob = 0;

    return evilJob.resultAsBoolean();
}

void Debugger::runInEngine(Debugger::Job *job)
{
    QMutexLocker locker(&m_lock);
    runInEngine_havingLock(job);
}

void Debugger::runInEngine_havingLock(Debugger::Job *job)
{
    Q_ASSERT(job);
    Q_ASSERT(m_runningJob == 0);

    m_runningJob = job;
    m_runningCondition.wakeAll();
    m_jobIsRunning.wait(&m_lock);
    m_runningJob = 0;
}

void DebuggerAgent::addDebugger(Debugger *debugger)
{
    Q_ASSERT(!m_debuggers.contains(debugger));
    m_debuggers << debugger;
    debugger->attachToAgent(this);

    debugger->setBreakOnThrow(m_breakOnThrow);

    foreach (const BreakPoint &breakPoint, m_breakPoints.values())
        if (breakPoint.enabled)
            debugger->addBreakPoint(breakPoint.fileName, breakPoint.lineNr, breakPoint.condition);
}

void DebuggerAgent::removeDebugger(Debugger *debugger)
{
    m_debuggers.removeAll(debugger);
    debugger->detachFromAgent();
}

void DebuggerAgent::pause(Debugger *debugger) const
{
    debugger->pause();
}

void DebuggerAgent::pauseAll() const
{
    foreach (Debugger *debugger, m_debuggers)
        pause(debugger);
}

void DebuggerAgent::resumeAll() const
{
    foreach (Debugger *debugger, m_debuggers)
        if (debugger->state() == Debugger::Paused)
            debugger->resume(Debugger::FullThrottle);
}

int DebuggerAgent::addBreakPoint(const QString &fileName, int lineNumber, bool enabled, const QString &condition)
{
    if (enabled)
        foreach (Debugger *debugger, m_debuggers)
            debugger->addBreakPoint(fileName, lineNumber, condition);

    int id = m_breakPoints.size();
    m_breakPoints.insert(id, BreakPoint(fileName, lineNumber, enabled, condition));
    return id;
}

void DebuggerAgent::removeBreakPoint(int id)
{
    BreakPoint breakPoint = m_breakPoints.value(id);
    if (!breakPoint.isValid())
        return;

    m_breakPoints.remove(id);

    if (breakPoint.enabled)
        foreach (Debugger *debugger, m_debuggers)
            debugger->removeBreakPoint(breakPoint.fileName, breakPoint.lineNr);
}

void DebuggerAgent::removeAllBreakPoints()
{
    QList<int> ids = m_breakPoints.keys();
    foreach (int id, ids)
        removeBreakPoint(id);
}

void DebuggerAgent::enableBreakPoint(int id, bool onoff)
{
    BreakPoint &breakPoint = m_breakPoints[id];
    if (!breakPoint.isValid() || breakPoint.enabled == onoff)
        return;
    breakPoint.enabled = onoff;

    foreach (Debugger *debugger, m_debuggers) {
        if (onoff)
            debugger->addBreakPoint(breakPoint.fileName, breakPoint.lineNr, breakPoint.condition);
        else
            debugger->removeBreakPoint(breakPoint.fileName, breakPoint.lineNr);
    }
}

QList<int> DebuggerAgent::breakPointIds(const QString &fileName, int lineNumber) const
{
    QList<int> ids;

    for (QHash<int, BreakPoint>::const_iterator i = m_breakPoints.begin(), ei = m_breakPoints.end(); i != ei; ++i)
        if (i->lineNr == lineNumber && fileName.endsWith(i->fileName))
            ids.push_back(i.key());

    return ids;
}

void DebuggerAgent::setBreakOnThrow(bool onoff)
{
    if (onoff != m_breakOnThrow) {
        m_breakOnThrow = onoff;
        foreach (Debugger *debugger, m_debuggers)
            debugger->setBreakOnThrow(onoff);
    }
}

DebuggerAgent::~DebuggerAgent()
{
    foreach (Debugger *debugger, m_debuggers)
        debugger->detachFromAgent();

    Q_ASSERT(m_debuggers.isEmpty());
}

Debugger::Collector::~Collector()
{
}

void Debugger::Collector::collect(const QString &name, const ScopedValue &value)
{
    switch (value->type()) {
    case Value::Empty_Type:
        Q_ASSERT(!"empty Value encountered");
        break;
    case Value::Undefined_Type:
        addUndefined(name);
        break;
    case Value::Null_Type:
        addNull(name);
        break;
    case Value::Boolean_Type:
        addBoolean(name, value->booleanValue());
        break;
    case Value::Managed_Type:
        if (String *s = value->asString())
            addString(name, s->toQString());
        else
            addObject(name, value);
        break;
    case Value::Integer_Type:
        addInteger(name, value->int_32);
        break;
    default: // double
        addDouble(name, value->doubleValue());
        break;
    }
}

void Debugger::Collector::collect(Object *object)
{
    bool property = true;
    qSwap(property, m_isProperty);

    Scope scope(m_engine);
    ObjectIterator it(scope, object, ObjectIterator::EnumerableOnly);
    ScopedValue name(scope);
    ScopedValue value(scope);
    while (true) {
        Value v;
        name = it.nextPropertyNameAsString(&v);
        if (name->isNull())
            break;
        QString key = name->toQStringNoThrow();
        value = v;
        collect(key, value);
    }

    qSwap(property, m_isProperty);
}


Debugger::Job::~Job()
{
}
