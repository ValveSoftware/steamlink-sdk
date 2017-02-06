/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/MainThreadTaskRunner.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "platform/CrossThreadFunctional.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"

namespace blink {

MainThreadTaskRunner::MainThreadTaskRunner(ExecutionContext* context)
    : m_context(context)
    , m_pendingTasksTimer(this, &MainThreadTaskRunner::pendingTasksTimerFired)
    , m_suspended(false)
    , m_weakFactory(this)
{
}

MainThreadTaskRunner::~MainThreadTaskRunner()
{
}

void MainThreadTaskRunner::postTaskInternal(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task, bool isInspectorTask, bool instrumenting)
{
    Platform::current()->mainThread()->getWebTaskRunner()->postTask(location, crossThreadBind(
        &MainThreadTaskRunner::perform,
        m_weakFactory.createWeakPtr(),
        passed(std::move(task)),
        isInspectorTask,
        instrumenting));
}

void MainThreadTaskRunner::postTask(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task, const String& taskNameForInstrumentation)
{
    if (!taskNameForInstrumentation.isEmpty())
        InspectorInstrumentation::asyncTaskScheduled(m_context, taskNameForInstrumentation, task.get());
    const bool instrumenting = !taskNameForInstrumentation.isEmpty();
    postTaskInternal(location, std::move(task), false, instrumenting);
}

void MainThreadTaskRunner::postInspectorTask(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task)
{
    postTaskInternal(location, std::move(task), true, false);
}

void MainThreadTaskRunner::perform(std::unique_ptr<ExecutionContextTask> task, bool isInspectorTask, bool instrumenting)
{
    // If the owner m_context is about to be swept then it
    // is no longer safe to access.
    if (ThreadHeap::willObjectBeLazilySwept(m_context.get()))
        return;

    if (!isInspectorTask && (m_context->tasksNeedSuspension() || !m_pendingTasks.isEmpty())) {
        m_pendingTasks.append(make_pair(std::move(task), instrumenting));
        return;
    }

    InspectorInstrumentation::AsyncTask asyncTask(m_context, task.get(), !isInspectorTask);
    task->performTask(m_context);
}

void MainThreadTaskRunner::suspend()
{
    DCHECK(!m_suspended);
    m_pendingTasksTimer.stop();
    m_suspended = true;
}

void MainThreadTaskRunner::resume()
{
    DCHECK(m_suspended);
    if (!m_pendingTasks.isEmpty())
        m_pendingTasksTimer.startOneShot(0, BLINK_FROM_HERE);

    m_suspended = false;
}

void MainThreadTaskRunner::pendingTasksTimerFired(Timer<MainThreadTaskRunner>*)
{
    // If the owner m_context is about to be swept then it
    // is no longer safe to access.
    if (ThreadHeap::willObjectBeLazilySwept(m_context.get()))
        return;

    while (!m_pendingTasks.isEmpty()) {
        std::unique_ptr<ExecutionContextTask> task = std::move(m_pendingTasks[0].first);
        const bool instrumenting = m_pendingTasks[0].second;
        m_pendingTasks.remove(0);
        InspectorInstrumentation::AsyncTask asyncTask(m_context, task.get(), instrumenting);
        task->performTask(m_context);
    }
}

} // namespace blink
