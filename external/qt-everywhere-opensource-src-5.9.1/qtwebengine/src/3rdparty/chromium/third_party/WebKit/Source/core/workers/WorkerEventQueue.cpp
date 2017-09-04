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

namespace blink {

WorkerEventQueue* WorkerEventQueue::create(ExecutionContext* context) {
  return new WorkerEventQueue(context);
}

WorkerEventQueue::WorkerEventQueue(ExecutionContext* context)
    : m_executionContext(context), m_isClosed(false) {}

WorkerEventQueue::~WorkerEventQueue() {
  DCHECK(m_pendingEvents.isEmpty());
}

DEFINE_TRACE(WorkerEventQueue) {
  visitor->trace(m_executionContext);
  visitor->trace(m_pendingEvents);
  EventQueue::trace(visitor);
}

bool WorkerEventQueue::enqueueEvent(Event* event) {
  if (m_isClosed)
    return false;
  InspectorInstrumentation::asyncTaskScheduled(
      event->target()->getExecutionContext(), event->type(), event);
  m_pendingEvents.add(event);
  m_executionContext->postTask(
      BLINK_FROM_HERE,
      createSameThreadTask(&WorkerEventQueue::dispatchEvent,
                           wrapPersistent(this), wrapWeakPersistent(event)));
  return true;
}

bool WorkerEventQueue::cancelEvent(Event* event) {
  if (!removeEvent(event))
    return false;
  InspectorInstrumentation::asyncTaskCanceled(
      event->target()->getExecutionContext(), event);
  return true;
}

void WorkerEventQueue::close() {
  m_isClosed = true;
  for (const auto& event : m_pendingEvents)
    InspectorInstrumentation::asyncTaskCanceled(
        event->target()->getExecutionContext(), event);
  m_pendingEvents.clear();
}

bool WorkerEventQueue::removeEvent(Event* event) {
  auto found = m_pendingEvents.find(event);
  if (found == m_pendingEvents.end())
    return false;
  m_pendingEvents.remove(found);
  return true;
}

void WorkerEventQueue::dispatchEvent(Event* event,
                                     ExecutionContext* executionContext) {
  if (!event || !removeEvent(event))
    return;

  InspectorInstrumentation::AsyncTask asyncTask(executionContext, event);
  event->target()->dispatchEvent(event);
}

}  // namespace blink
