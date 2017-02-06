/*
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
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

#include "core/workers/WorkerEventQueue.h"

#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/events/Event.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "wtf/PtrUtil.h"

namespace blink {

WorkerEventQueue* WorkerEventQueue::create(ExecutionContext* context)
{
    return new WorkerEventQueue(context);
}

WorkerEventQueue::WorkerEventQueue(ExecutionContext* context)
    : m_executionContext(context)
    , m_isClosed(false)
{
}

WorkerEventQueue::~WorkerEventQueue()
{
    DCHECK(m_eventTaskMap.isEmpty());
}

DEFINE_TRACE(WorkerEventQueue)
{
    visitor->trace(m_executionContext);
    visitor->trace(m_eventTaskMap);
    EventQueue::trace(visitor);
}

class WorkerEventQueue::EventDispatcherTask : public ExecutionContextTask {
public:
    static std::unique_ptr<EventDispatcherTask> create(Event* event, WorkerEventQueue* eventQueue)
    {
        return wrapUnique(new EventDispatcherTask(event, eventQueue));
    }

    ~EventDispatcherTask() override
    {
        if (m_event)
            m_eventQueue->removeEvent(m_event.get());
    }

    void dispatchEvent(ExecutionContext* context, Event* event)
    {
        InspectorInstrumentation::AsyncTask asyncTask(context, event);
        event->target()->dispatchEvent(event);
    }

    virtual void performTask(ExecutionContext* context)
    {
        if (m_isCancelled)
            return;
        m_eventQueue->removeEvent(m_event.get());
        dispatchEvent(context, m_event);
        m_event.clear();
    }

    void cancel()
    {
        m_isCancelled = true;
        m_event.clear();
    }

private:
    EventDispatcherTask(Event* event, WorkerEventQueue* eventQueue)
        : m_event(event)
        , m_eventQueue(eventQueue)
        , m_isCancelled(false)
    {
    }

    Persistent<Event> m_event;
    Persistent<WorkerEventQueue> m_eventQueue;
    bool m_isCancelled;
};

void WorkerEventQueue::removeEvent(Event* event)
{
    InspectorInstrumentation::asyncTaskCanceled(event->target()->getExecutionContext(), event);
    m_eventTaskMap.remove(event);
}

bool WorkerEventQueue::enqueueEvent(Event* event)
{
    if (m_isClosed)
        return false;
    InspectorInstrumentation::asyncTaskScheduled(event->target()->getExecutionContext(), event->type(), event);
    std::unique_ptr<EventDispatcherTask> task = EventDispatcherTask::create(event, this);
    m_eventTaskMap.add(event, task.get());
    m_executionContext->postTask(BLINK_FROM_HERE, std::move(task));
    return true;
}

bool WorkerEventQueue::cancelEvent(Event* event)
{
    EventDispatcherTask* task = m_eventTaskMap.get(event);
    if (!task)
        return false;
    task->cancel();
    removeEvent(event);
    return true;
}

void WorkerEventQueue::close()
{
    m_isClosed = true;
    for (const auto& entry : m_eventTaskMap) {
        Event* event = entry.key.get();
        EventDispatcherTask* task = entry.value;
        InspectorInstrumentation::asyncTaskCanceled(event->target()->getExecutionContext(), event);
        task->cancel();
    }
    m_eventTaskMap.clear();
}

} // namespace blink
