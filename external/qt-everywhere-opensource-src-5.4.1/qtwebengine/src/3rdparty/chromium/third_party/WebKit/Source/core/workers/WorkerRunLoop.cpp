/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/workers/WorkerRunLoop.h"

#include "core/inspector/InspectorInstrumentation.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerThread.h"
#include "platform/PlatformThreadData.h"
#include "platform/SharedTimer.h"
#include "platform/ThreadTimers.h"
#include "platform/heap/ThreadState.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

class WorkerRunLoopTask : public blink::WebThread::Task {
    WTF_MAKE_NONCOPYABLE(WorkerRunLoopTask); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<WorkerRunLoopTask> create(const WorkerRunLoop& runLoop, PassOwnPtr<ExecutionContextTask> task)
    {
        return adoptPtr(new WorkerRunLoopTask(runLoop, task));
    }

    virtual ~WorkerRunLoopTask() { }

    virtual void run() OVERRIDE
    {
        WorkerGlobalScope* workerGlobalScope = m_runLoop.context();
        if ((!workerGlobalScope->isClosing() && !m_runLoop.terminated()) || m_task->isCleanupTask())
            m_task->performTask(workerGlobalScope);
    }

private:
    WorkerRunLoopTask(const WorkerRunLoop& runLoop, PassOwnPtr<ExecutionContextTask> task)
        : m_runLoop(runLoop)
        , m_task(task)
    {
    }

    const WorkerRunLoop& m_runLoop;
    OwnPtr<ExecutionContextTask> m_task;
};

class TickleDebuggerQueueTask FINAL : public ExecutionContextTask {
public:
    static PassOwnPtr<TickleDebuggerQueueTask> create(WorkerRunLoop* loop)
    {
        return adoptPtr(new TickleDebuggerQueueTask(loop));
    }
    virtual void performTask(ExecutionContext* context) OVERRIDE
    {
        ASSERT(context->isWorkerGlobalScope());
        m_loop->runDebuggerTask(WorkerRunLoop::DontWaitForMessage);
    }

private:
    explicit TickleDebuggerQueueTask(WorkerRunLoop* loop) : m_loop(loop) { }

    WorkerRunLoop* m_loop;
};

class WorkerSharedTimer : public SharedTimer {
public:
    WorkerSharedTimer()
        : m_sharedTimerFunction(0)
        , m_nextFireTime(0)
    {
    }

    // SharedTimer interface.
    virtual void setFiredFunction(void (*function)()) { m_sharedTimerFunction = function; }
    virtual void setFireInterval(double interval) { m_nextFireTime = interval + currentTime(); }
    virtual void stop() { m_nextFireTime = 0; }

    bool isActive() { return m_sharedTimerFunction && m_nextFireTime; }
    double fireTime() { return m_nextFireTime; }
    void fire() { m_sharedTimerFunction(); }

private:
    void (*m_sharedTimerFunction)();
    double m_nextFireTime;
};

WorkerRunLoop::WorkerRunLoop()
    : m_sharedTimer(adoptPtr(new WorkerSharedTimer))
    , m_context(0)
    , m_nestedCount(0)
{
}

WorkerRunLoop::~WorkerRunLoop()
{
    ASSERT(!m_nestedCount);
}

class RunLoopSetup {
    WTF_MAKE_NONCOPYABLE(RunLoopSetup);
public:
    RunLoopSetup(WorkerRunLoop& runLoop, WorkerGlobalScope* context)
        : m_runLoop(runLoop)
        , m_context(context)
    {
        if (!m_runLoop.m_nestedCount)
            PlatformThreadData::current().threadTimers().setSharedTimer(m_runLoop.m_sharedTimer.get());
        m_runLoop.m_nestedCount++;
        InspectorInstrumentation::willEnterNestedRunLoop(m_context);
    }

    ~RunLoopSetup()
    {
        m_runLoop.m_nestedCount--;
        if (!m_runLoop.m_nestedCount)
            PlatformThreadData::current().threadTimers().setSharedTimer(0);
        InspectorInstrumentation::didLeaveNestedRunLoop(m_context);
    }
private:
    WorkerRunLoop& m_runLoop;
    WorkerGlobalScope* m_context;
};

void WorkerRunLoop::setWorkerGlobalScope(WorkerGlobalScope* context)
{
    ASSERT(!m_context);
    ASSERT(context);
    m_context = context;
}

void WorkerRunLoop::run()
{
    ASSERT(m_context);
    RunLoopSetup setup(*this, m_context);
    MessageQueueWaitResult result;
    do {
        ThreadState::current()->safePoint(ThreadState::NoHeapPointersOnStack);
        result = run(m_messageQueue, WaitForMessage);
    } while (result != MessageQueueTerminated);
    runCleanupTasks();
}

MessageQueueWaitResult WorkerRunLoop::runDebuggerTask(WaitMode waitMode)
{
    ASSERT(m_context);
    RunLoopSetup setup(*this, m_context);
    return run(m_debuggerMessageQueue, waitMode);
}

MessageQueueWaitResult WorkerRunLoop::run(MessageQueue<blink::WebThread::Task>& queue, WaitMode waitMode)
{
    ASSERT(m_context);
    ASSERT(m_context->thread());
    ASSERT(m_context->thread()->isCurrentThread());

    bool isDebuggerQueue = (&queue == &m_debuggerMessageQueue);
    bool nextTimeoutEventIsIdleWatchdog;
    MessageQueueWaitResult result;
    OwnPtr<blink::WebThread::Task> task;
    do {
        double absoluteTime = 0.0;
        nextTimeoutEventIsIdleWatchdog = false;
        if (waitMode == WaitForMessage) {
            absoluteTime = !isDebuggerQueue && m_sharedTimer->isActive() ? m_sharedTimer->fireTime() : MessageQueue<blink::WebThread::Task>::infiniteTime();

            // Do a script engine idle notification if the next event is distant enough.
            const double kMinIdleTimespan = 0.3; // seconds
            if (queue.isEmpty() && absoluteTime > currentTime() + kMinIdleTimespan) {
                bool hasMoreWork = !m_context->idleNotification();
                if (hasMoreWork) {
                    // Schedule a watchdog, so if there are no events within a particular time interval
                    // idle notifications won't stop firing.
                    const double kWatchdogInterval = 3; // seconds
                    double nextWatchdogTime = currentTime() + kWatchdogInterval;
                    if (absoluteTime > nextWatchdogTime) {
                        absoluteTime = nextWatchdogTime;
                        nextTimeoutEventIsIdleWatchdog = true;
                    }
                }
            }
        }

        {
            ThreadState::SafePointScope safePointScope(ThreadState::NoHeapPointersOnStack);
            task = queue.waitForMessageWithTimeout(result, absoluteTime);
        }
    } while (result == MessageQueueTimeout && nextTimeoutEventIsIdleWatchdog);

    // If the context is closing, don't execute any further JavaScript tasks (per section 4.1.1 of the Web Workers spec).
    // However, there may be implementation cleanup tasks in the queue, so keep running through it.

    switch (result) {
    case MessageQueueTerminated:
        break;

    case MessageQueueMessageReceived:
        InspectorInstrumentation::willProcessTask(m_context);
        task->run();
        InspectorInstrumentation::didProcessTask(m_context);
        break;

    case MessageQueueTimeout:
        ASSERT(!isDebuggerQueue || waitMode != WaitForMessage);
        if (!m_context->isClosing())
            m_sharedTimer->fire();
        break;
    }

    return result;
}

void WorkerRunLoop::runCleanupTasks()
{
    ASSERT(m_context);
    ASSERT(m_context->thread());
    ASSERT(m_context->thread()->isCurrentThread());
    ASSERT(m_messageQueue.killed());
    ASSERT(m_debuggerMessageQueue.killed());

    while (true) {
        OwnPtr<blink::WebThread::Task> task = m_debuggerMessageQueue.tryGetMessageIgnoringKilled();
        if (!task)
            task = m_messageQueue.tryGetMessageIgnoringKilled();
        if (!task)
            return;
        task->run();
    }
}

void WorkerRunLoop::terminate()
{
    m_messageQueue.kill();
    m_debuggerMessageQueue.kill();
}

bool WorkerRunLoop::postTask(PassOwnPtr<ExecutionContextTask> task)
{
    return m_messageQueue.append(WorkerRunLoopTask::create(*this, task));
}

void WorkerRunLoop::postTaskAndTerminate(PassOwnPtr<ExecutionContextTask> task)
{
    m_debuggerMessageQueue.kill();
    m_messageQueue.appendAndKill(WorkerRunLoopTask::create(*this, task));
}

bool WorkerRunLoop::postDebuggerTask(PassOwnPtr<ExecutionContextTask> task)
{
    bool posted = m_debuggerMessageQueue.append(WorkerRunLoopTask::create(*this, task));
    if (posted)
        postTask(TickleDebuggerQueueTask::create(this));
    return posted;
}

} // namespace WebCore
