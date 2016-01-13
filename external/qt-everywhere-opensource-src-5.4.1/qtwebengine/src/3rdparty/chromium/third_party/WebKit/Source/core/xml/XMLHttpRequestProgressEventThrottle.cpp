/*
 * Copyright (C) 2010 Julien Chaffraix <jchaffraix@webkit.org>  All right reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
#include "core/xml/XMLHttpRequestProgressEventThrottle.h"

#include "core/events/EventTarget.h"
#include "core/xml/XMLHttpRequestProgressEvent.h"

namespace WebCore {

const double XMLHttpRequestProgressEventThrottle::minimumProgressEventDispatchingIntervalInSeconds = .05; // 50 ms per specification.

XMLHttpRequestProgressEventThrottle::XMLHttpRequestProgressEventThrottle(EventTarget* target)
    : m_target(target)
    , m_loaded(0)
    , m_total(0)
    , m_deferEvents(false)
    , m_dispatchDeferredEventsTimer(this, &XMLHttpRequestProgressEventThrottle::dispatchDeferredEvents)
{
    ASSERT(target);
}

XMLHttpRequestProgressEventThrottle::~XMLHttpRequestProgressEventThrottle()
{
}

void XMLHttpRequestProgressEventThrottle::dispatchProgressEvent(const AtomicString& type, bool lengthComputable, unsigned long long loaded, unsigned long long total)
{
    RefPtrWillBeRawPtr<XMLHttpRequestProgressEvent> progressEvent = XMLHttpRequestProgressEvent::create(type, lengthComputable, loaded, total);

    if (type != EventTypeNames::progress) {
        dispatchEvent(progressEvent);
        return;
    }

    if (m_deferEvents) {
        // Only store the latest progress event while suspended.
        m_deferredProgressEvent = progressEvent;
        return;
    }

    if (!isActive()) {
        // The timer is not active so the least frequent event for now is every byte.
        // Just go ahead and dispatch the event.

        // We should not have any pending loaded & total information from a previous run.
        ASSERT(!m_loaded);
        ASSERT(!m_total);

        dispatchEvent(progressEvent);
        startRepeating(minimumProgressEventDispatchingIntervalInSeconds, FROM_HERE);
        return;
    }

    // The timer is already active so minimumProgressEventDispatchingIntervalInSeconds is the least frequent event.
    m_lengthComputable = lengthComputable;
    m_loaded = loaded;
    m_total = total;
}

void XMLHttpRequestProgressEventThrottle::dispatchReadyStateChangeEvent(PassRefPtrWillBeRawPtr<Event> event, ProgressEventAction progressEventAction)
{
    if (progressEventAction == FlushProgressEvent || progressEventAction == FlushDeferredProgressEvent) {
        if (!flushDeferredProgressEvent() && progressEventAction == FlushProgressEvent)
            deliverProgressEvent();
    }

    dispatchEvent(event);
}

void XMLHttpRequestProgressEventThrottle::dispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    ASSERT(event);
    if (m_deferEvents) {
        if (m_deferredEvents.size() > 1 && event->type() == EventTypeNames::readystatechange && event->type() == m_deferredEvents.last()->type()) {
            // Readystatechange events are state-less so avoid repeating two identical events in a row on resume.
            return;
        }
        m_deferredEvents.append(event);
    } else {
        m_target->dispatchEvent(event);
    }
}

bool XMLHttpRequestProgressEventThrottle::flushDeferredProgressEvent()
{
    if (m_deferEvents && m_deferredProgressEvent) {
        // Move the progress event to the queue, to get it in the right order on resume.
        m_deferredEvents.append(m_deferredProgressEvent);
        m_deferredProgressEvent = nullptr;
        return true;
    }
    return false;
}

void XMLHttpRequestProgressEventThrottle::deliverProgressEvent()
{
    if (!hasEventToDispatch())
        return;

    RefPtrWillBeRawPtr<Event> event = XMLHttpRequestProgressEvent::create(EventTypeNames::progress, m_lengthComputable, m_loaded, m_total);
    m_loaded = 0;
    m_total = 0;

    // We stop the timer as this is called when no more events are supposed to occur.
    stop();

    dispatchEvent(event);
}

void XMLHttpRequestProgressEventThrottle::dispatchDeferredEvents(Timer<XMLHttpRequestProgressEventThrottle>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_dispatchDeferredEventsTimer);
    ASSERT(m_deferEvents);
    m_deferEvents = false;

    // Take over the deferred events before dispatching them which can potentially add more.
    WillBeHeapVector<RefPtrWillBeMember<Event> > deferredEvents;
    m_deferredEvents.swap(deferredEvents);

    RefPtrWillBeRawPtr<Event> deferredProgressEvent = m_deferredProgressEvent;
    m_deferredProgressEvent = nullptr;

    WillBeHeapVector<RefPtrWillBeMember<Event> >::const_iterator it = deferredEvents.begin();
    const WillBeHeapVector<RefPtrWillBeMember<Event> >::const_iterator end = deferredEvents.end();
    for (; it != end; ++it)
        dispatchEvent(*it);

    // The progress event will be in the m_deferredEvents vector if the load was finished while suspended.
    // If not, just send the most up-to-date progress on resume.
    if (deferredProgressEvent)
        dispatchEvent(deferredProgressEvent);
}

void XMLHttpRequestProgressEventThrottle::fired()
{
    ASSERT(isActive());
    if (!hasEventToDispatch()) {
        // No progress event was queued since the previous dispatch, we can safely stop the timer.
        stop();
        return;
    }

    dispatchEvent(XMLHttpRequestProgressEvent::create(EventTypeNames::progress, m_lengthComputable, m_loaded, m_total));
    m_total = 0;
    m_loaded = 0;
}

bool XMLHttpRequestProgressEventThrottle::hasEventToDispatch() const
{
    return (m_total || m_loaded) && isActive();
}

void XMLHttpRequestProgressEventThrottle::suspend()
{
    // If re-suspended before deferred events have been dispatched, just stop the dispatch
    // and continue the last suspend.
    if (m_dispatchDeferredEventsTimer.isActive()) {
        ASSERT(m_deferEvents);
        m_dispatchDeferredEventsTimer.stop();
        return;
    }
    ASSERT(!m_deferredProgressEvent);
    ASSERT(m_deferredEvents.isEmpty());
    ASSERT(!m_deferEvents);

    m_deferEvents = true;
    // If we have a progress event waiting to be dispatched,
    // just defer it.
    if (hasEventToDispatch()) {
        m_deferredProgressEvent = XMLHttpRequestProgressEvent::create(EventTypeNames::progress, m_lengthComputable, m_loaded, m_total);
        m_total = 0;
        m_loaded = 0;
    }
    stop();
}

void XMLHttpRequestProgressEventThrottle::resume()
{
    ASSERT(!m_loaded);
    ASSERT(!m_total);

    if (m_deferredEvents.isEmpty() && !m_deferredProgressEvent) {
        m_deferEvents = false;
        return;
    }

    // Do not dispatch events inline here, since ExecutionContext is iterating over
    // the list of active DOM objects to resume them, and any activated JS event-handler
    // could insert new active DOM objects to the list.
    // m_deferEvents is kept true until all deferred events have been dispatched.
    m_dispatchDeferredEventsTimer.startOneShot(0, FROM_HERE);
}

void XMLHttpRequestProgressEventThrottle::trace(Visitor* visitor)
{
    visitor->trace(m_target);
    visitor->trace(m_deferredProgressEvent);
    visitor->trace(m_deferredEvents);
}

} // namespace WebCore
