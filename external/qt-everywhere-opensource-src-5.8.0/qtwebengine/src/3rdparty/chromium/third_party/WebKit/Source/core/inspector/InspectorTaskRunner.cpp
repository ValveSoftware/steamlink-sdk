// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/InspectorTaskRunner.h"

namespace blink {

InspectorTaskRunner::IgnoreInterruptsScope::IgnoreInterruptsScope(InspectorTaskRunner* taskRunner)
    : m_wasIgnoring(taskRunner->m_ignoreInterrupts)
    , m_taskRunner(taskRunner)
{
    // There may be nested scopes e.g. when tasks are being executed on XHR breakpoint.
    m_taskRunner->m_ignoreInterrupts = true;
}

InspectorTaskRunner::IgnoreInterruptsScope::~IgnoreInterruptsScope()
{
    m_taskRunner->m_ignoreInterrupts = m_wasIgnoring;
}

InspectorTaskRunner::InspectorTaskRunner()
    : m_ignoreInterrupts(false)
    , m_killed(false)
{
}

InspectorTaskRunner::~InspectorTaskRunner()
{
}

void InspectorTaskRunner::appendTask(std::unique_ptr<Task> task)
{
    MutexLocker lock(m_mutex);
    if (m_killed)
        return;
    m_queue.append(std::move(task));
    m_condition.signal();
}

std::unique_ptr<InspectorTaskRunner::Task> InspectorTaskRunner::takeNextTask(InspectorTaskRunner::WaitMode waitMode)
{
    MutexLocker lock(m_mutex);
    bool timedOut = false;

    static double infiniteTime = std::numeric_limits<double>::max();
    double absoluteTime = waitMode == WaitForTask ? infiniteTime : 0.0;
    while (!m_killed && !timedOut && m_queue.isEmpty())
        timedOut = !m_condition.timedWait(m_mutex, absoluteTime);
    ASSERT(!timedOut || absoluteTime != infiniteTime);

    if (m_killed || timedOut)
        return nullptr;

    ASSERT_WITH_SECURITY_IMPLICATION(!m_queue.isEmpty());
    return m_queue.takeFirst();
}

void InspectorTaskRunner::kill()
{
    MutexLocker lock(m_mutex);
    m_killed = true;
    m_condition.broadcast();
}

void InspectorTaskRunner::interruptAndRunAllTasksDontWait(v8::Isolate* isolate)
{
    isolate->RequestInterrupt(&v8InterruptCallback, this);
}

void InspectorTaskRunner::runAllTasksDontWait()
{
    while (true) {
        std::unique_ptr<Task> task = takeNextTask(DontWaitForTask);
        if (!task)
            return;
        (*task)();
    }
}

void InspectorTaskRunner::v8InterruptCallback(v8::Isolate*, void* data)
{
    InspectorTaskRunner* runner = static_cast<InspectorTaskRunner*>(data);
    if (runner->m_ignoreInterrupts)
        return;
    runner->runAllTasksDontWait();
}

} // namespace blink
