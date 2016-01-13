// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/custom/CustomElementMicrotaskDispatcher.h"

#include "core/dom/Microtask.h"
#include "core/dom/custom/CustomElementCallbackDispatcher.h"
#include "core/dom/custom/CustomElementCallbackQueue.h"
#include "core/dom/custom/CustomElementMicrotaskImportStep.h"
#include "core/dom/custom/CustomElementMicrotaskStepDispatcher.h"
#include "core/dom/custom/CustomElementScheduler.h"
#include "wtf/MainThread.h"

namespace WebCore {

static const CustomElementCallbackQueue::ElementQueueId kMicrotaskQueueId = 0;

CustomElementMicrotaskDispatcher::CustomElementMicrotaskDispatcher()
    : m_hasScheduledMicrotask(false)
    , m_phase(Quiescent)
    , m_steps(CustomElementMicrotaskStepDispatcher::create())
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(CustomElementMicrotaskDispatcher)

CustomElementMicrotaskDispatcher& CustomElementMicrotaskDispatcher::instance()
{
    DEFINE_STATIC_LOCAL(OwnPtrWillBePersistent<CustomElementMicrotaskDispatcher>, instance, (adoptPtrWillBeNoop(new CustomElementMicrotaskDispatcher())));
    return *instance;
}

void CustomElementMicrotaskDispatcher::enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskStep> step)
{
    ensureMicrotaskScheduledForMicrotaskSteps();
    m_steps->enqueue(parentLoader, step);
}

void CustomElementMicrotaskDispatcher::enqueue(HTMLImportLoader* parentLoader, PassOwnPtrWillBeRawPtr<CustomElementMicrotaskImportStep> step, bool importIsSync)
{
    ensureMicrotaskScheduledForMicrotaskSteps();
    m_steps->enqueue(parentLoader, step, importIsSync);
}

void CustomElementMicrotaskDispatcher::enqueue(CustomElementCallbackQueue* queue)
{
    ensureMicrotaskScheduledForElementQueue();
    queue->setOwner(kMicrotaskQueueId);
    m_elements.append(queue);
}

void CustomElementMicrotaskDispatcher::importDidFinish(CustomElementMicrotaskImportStep* step)
{
    ensureMicrotaskScheduledForMicrotaskSteps();
}

void CustomElementMicrotaskDispatcher::ensureMicrotaskScheduledForMicrotaskSteps()
{
    ASSERT(m_phase == Quiescent || m_phase == DispatchingCallbacks);
    ensureMicrotaskScheduled();
}

void CustomElementMicrotaskDispatcher::ensureMicrotaskScheduledForElementQueue()
{
    ASSERT(m_phase == Quiescent || m_phase == Resolving);
    ensureMicrotaskScheduled();
}

void CustomElementMicrotaskDispatcher::ensureMicrotaskScheduled()
{
    if (!m_hasScheduledMicrotask) {
        Microtask::enqueueMicrotask(WTF::bind(&dispatch));
        m_hasScheduledMicrotask = true;
    }
}

void CustomElementMicrotaskDispatcher::dispatch()
{
    instance().doDispatch();
}

void CustomElementMicrotaskDispatcher::doDispatch()
{
    ASSERT(isMainThread());

    ASSERT(m_phase == Quiescent && m_hasScheduledMicrotask);
    m_hasScheduledMicrotask = false;

    // Finishing microtask work deletes all
    // CustomElementCallbackQueues. Being in a callback delivery scope
    // implies those queues could still be in use.
    ASSERT_WITH_SECURITY_IMPLICATION(!CustomElementCallbackDispatcher::inCallbackDeliveryScope());

    m_phase = Resolving;
    m_steps->dispatch();

    m_phase = DispatchingCallbacks;
    for (WillBeHeapVector<RawPtrWillBeMember<CustomElementCallbackQueue> >::iterator it = m_elements.begin(); it != m_elements.end(); ++it) {
        // Created callback may enqueue an attached callback.
        CustomElementCallbackDispatcher::CallbackDeliveryScope scope;
        (*it)->processInElementQueue(kMicrotaskQueueId);
    }

    m_elements.clear();
    CustomElementScheduler::microtaskDispatcherDidFinish();
    m_phase = Quiescent;
}

void CustomElementMicrotaskDispatcher::trace(Visitor* visitor)
{
    visitor->trace(m_steps);
#if ENABLE(OILPAN)
    visitor->trace(m_elements);
#endif
}

#if !defined(NDEBUG)
void CustomElementMicrotaskDispatcher::show()
{
    m_steps->show(2);

}
#endif

} // namespace WebCore

#if !defined(NDEBUG)
void showCEMD()
{
    WebCore::CustomElementMicrotaskDispatcher::instance().show();
}
#endif
