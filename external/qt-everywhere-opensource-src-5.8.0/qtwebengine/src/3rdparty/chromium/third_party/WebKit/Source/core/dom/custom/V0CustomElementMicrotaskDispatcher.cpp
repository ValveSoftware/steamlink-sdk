// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/V0CustomElementMicrotaskDispatcher.h"

#include "bindings/core/v8/Microtask.h"
#include "core/dom/custom/V0CustomElementCallbackQueue.h"
#include "core/dom/custom/V0CustomElementMicrotaskImportStep.h"
#include "core/dom/custom/V0CustomElementProcessingStack.h"
#include "core/dom/custom/V0CustomElementScheduler.h"

namespace blink {

static const V0CustomElementCallbackQueue::ElementQueueId kMicrotaskQueueId = 0;

V0CustomElementMicrotaskDispatcher::V0CustomElementMicrotaskDispatcher()
    : m_hasScheduledMicrotask(false)
    , m_phase(Quiescent)
{
}

V0CustomElementMicrotaskDispatcher& V0CustomElementMicrotaskDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(V0CustomElementMicrotaskDispatcher, instance, (new V0CustomElementMicrotaskDispatcher));
    return instance;
}

void V0CustomElementMicrotaskDispatcher::enqueue(V0CustomElementCallbackQueue* queue)
{
    ensureMicrotaskScheduledForElementQueue();
    queue->setOwner(kMicrotaskQueueId);
    m_elements.append(queue);
}

void V0CustomElementMicrotaskDispatcher::ensureMicrotaskScheduledForElementQueue()
{
    DCHECK(m_phase == Quiescent || m_phase == Resolving);
    ensureMicrotaskScheduled();
}

void V0CustomElementMicrotaskDispatcher::ensureMicrotaskScheduled()
{
    if (!m_hasScheduledMicrotask) {
        Microtask::enqueueMicrotask(WTF::bind(&dispatch));
        m_hasScheduledMicrotask = true;
    }
}

void V0CustomElementMicrotaskDispatcher::dispatch()
{
    instance().doDispatch();
}

void V0CustomElementMicrotaskDispatcher::doDispatch()
{
    DCHECK(isMainThread());

    DCHECK(m_phase == Quiescent);
    DCHECK(m_hasScheduledMicrotask);
    m_hasScheduledMicrotask = false;

    // Finishing microtask work deletes all
    // V0CustomElementCallbackQueues. Being in a callback delivery scope
    // implies those queues could still be in use.
    ASSERT_WITH_SECURITY_IMPLICATION(!V0CustomElementProcessingStack::inCallbackDeliveryScope());

    m_phase = Resolving;

    m_phase = DispatchingCallbacks;
    for (const auto& element : m_elements) {
        // Created callback may enqueue an attached callback.
        V0CustomElementProcessingStack::CallbackDeliveryScope scope;
        element->processInElementQueue(kMicrotaskQueueId);
    }

    m_elements.clear();
    V0CustomElementScheduler::microtaskDispatcherDidFinish();
    m_phase = Quiescent;
}

DEFINE_TRACE(V0CustomElementMicrotaskDispatcher)
{
    visitor->trace(m_elements);
}

} // namespace blink
