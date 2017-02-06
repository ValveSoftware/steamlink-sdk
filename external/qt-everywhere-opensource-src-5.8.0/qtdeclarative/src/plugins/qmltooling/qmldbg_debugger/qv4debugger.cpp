/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qv4debugger.h"
#include "qv4debugjob.h"
#include "qv4datacollector.h"

#include <private/qv4scopedvalue_p.h>
#include <private/qv4script_p.h>
#include <private/qqmlcontext_p.h>
#include <private/qqmlengine_p.h>

QT_BEGIN_NAMESPACE

QV4Debugger::BreakPoint::BreakPoint(const QString &fileName, int line)
    : fileName(fileName), lineNumber(line)
{}

inline uint qHash(const QV4Debugger::BreakPoint &b, uint seed = 0) Q_DECL_NOTHROW
{
    return qHash(b.fileName, seed) ^ b.lineNumber;
}

inline bool operator==(const QV4Debugger::BreakPoint &a,
                       const QV4Debugger::BreakPoint &b)
{
    return a.lineNumber == b.lineNumber && a.fileName == b.fileName;
}

QV4Debugger::QV4Debugger(QV4::ExecutionEngine *engine)
    : m_engine(engine)
    , m_state(Running)
    , m_stepping(NotStepping)
    , m_pauseRequested(false)
    , m_haveBreakPoints(false)
    , m_breakOnThrow(false)
    , m_returnedValue(engine, QV4::Primitive::undefinedValue())
    , m_gatherSources(0)
    , m_runningJob(0)
    , m_collector(engine)
{
    static int debuggerId = qRegisterMetaType<QV4Debugger*>();
    static int pauseReasonId = qRegisterMetaType<QV4Debugger::PauseReason>();
    Q_UNUSED(debuggerId);
    Q_UNUSED(pauseReasonId);
    connect(this, &QV4Debugger::scheduleJob,
            this, &QV4Debugger::runJobUnpaused, Qt::QueuedConnection);
}

QV4::ExecutionEngine *QV4Debugger::engine() const
{
    return m_engine;
}

const QV4DataCollector *QV4Debugger::collector() const
{
    return &m_collector;
}

QV4DataCollector *QV4Debugger::collector()
{
    return &m_collector;
}

void QV4Debugger::pause()
{
    QMutexLocker locker(&m_lock);
    if (m_state == Paused)
        return;
    m_pauseRequested = true;
}

void QV4Debugger::resume(Speed speed)
{
    QMutexLocker locker(&m_lock);
    if (m_state != Paused)
        return;

    if (!m_returnedValue.isUndefined())
        m_returnedValue.set(m_engine, QV4::Encode::undefined());

    m_currentContext.set(m_engine, *m_engine->currentContext);
    m_stepping = speed;
    m_runningCondition.wakeAll();
}

QV4Debugger::State QV4Debugger::state() const
{
    return m_state;
}

void QV4Debugger::addBreakPoint(const QString &fileName, int lineNumber, const QString &condition)
{
    QMutexLocker locker(&m_lock);
    m_breakPoints.insert(BreakPoint(fileName.mid(fileName.lastIndexOf('/') + 1),
                                            lineNumber), condition);
    m_haveBreakPoints = true;
}

void QV4Debugger::removeBreakPoint(const QString &fileName, int lineNumber)
{
    QMutexLocker locker(&m_lock);
    m_breakPoints.remove(BreakPoint(fileName.mid(fileName.lastIndexOf('/') + 1),
                                            lineNumber));
    m_haveBreakPoints = !m_breakPoints.isEmpty();
}

void QV4Debugger::setBreakOnThrow(bool onoff)
{
    QMutexLocker locker(&m_lock);

    m_breakOnThrow = onoff;
}

void QV4Debugger::clearPauseRequest()
{
    QMutexLocker locker(&m_lock);
    m_pauseRequested = false;
}

QV4Debugger::ExecutionState QV4Debugger::currentExecutionState() const
{
    ExecutionState state;
    state.fileName = getFunction()->sourceFile();
    state.lineNumber = engine()->current->lineNumber;

    return state;
}

bool QV4Debugger::pauseAtNextOpportunity() const {
    return m_pauseRequested || m_haveBreakPoints || m_gatherSources || m_stepping >= StepOver;
}

QVector<QV4::StackFrame> QV4Debugger::stackTrace(int frameLimit) const
{
    return m_engine->stackTrace(frameLimit);
}

void QV4Debugger::maybeBreakAtInstruction()
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
        if (m_currentContext.asManaged()->d() != m_engine->current)
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
        if (QV4::Function *f = getFunction()) {
            const int lineNumber = engine()->current->lineNumber;
            if (reallyHitTheBreakPoint(f->sourceFile(), lineNumber))
                pauseAndWait(BreakPointHit);
        }
    }
}

void QV4Debugger::enteringFunction()
{
    if (m_runningJob)
        return;
    QMutexLocker locker(&m_lock);

    if (m_stepping == StepIn) {
        m_currentContext.set(m_engine, *m_engine->currentContext);
    }
}

void QV4Debugger::leavingFunction(const QV4::ReturnedValue &retVal)
{
    if (m_runningJob)
        return;
    Q_UNUSED(retVal); // TODO

    QMutexLocker locker(&m_lock);

    if (m_stepping != NotStepping && m_currentContext.asManaged()->d() == m_engine->current) {
        m_currentContext.set(m_engine, *m_engine->parentContext(m_engine->currentContext));
        m_stepping = StepOver;
        m_returnedValue.set(m_engine, retVal);
    }
}

void QV4Debugger::aboutToThrow()
{
    if (!m_breakOnThrow)
        return;

    if (m_runningJob) // do not re-enter when we're doing a job for the debugger.
        return;

    QMutexLocker locker(&m_lock);
    pauseAndWait(Throwing);
}

QV4::Function *QV4Debugger::getFunction() const
{
    QV4::Scope scope(m_engine);
    QV4::ExecutionContext *context = m_engine->currentContext;
    QV4::ScopedFunctionObject function(scope, context->getFunctionObject());
    if (function)
        return function->function();
    else
        return context->d()->engine->globalCode;
}

void QV4Debugger::runJobUnpaused()
{
    QMutexLocker locker(&m_lock);
    if (m_runningJob)
        m_runningJob->run();
    m_jobIsRunning.wakeAll();
}

void QV4Debugger::pauseAndWait(PauseReason reason)
{
    if (m_runningJob)
        return;

    m_state = Paused;
    emit debuggerPaused(this, reason);

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

bool QV4Debugger::reallyHitTheBreakPoint(const QString &filename, int linenr)
{
    QHash<BreakPoint, QString>::iterator it = m_breakPoints.find(
                BreakPoint(filename.mid(filename.lastIndexOf('/') + 1), linenr));
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

void QV4Debugger::runInEngine(QV4DebugJob *job)
{
    QMutexLocker locker(&m_lock);
    runInEngine_havingLock(job);
}

void QV4Debugger::runInEngine_havingLock(QV4DebugJob *job)
{
    Q_ASSERT(job);
    Q_ASSERT(m_runningJob == 0);

    m_runningJob = job;
    if (state() == Paused)
        m_runningCondition.wakeAll();
    else
        emit scheduleJob();
    m_jobIsRunning.wait(&m_lock);
    m_runningJob = 0;
}

QT_END_NAMESPACE
