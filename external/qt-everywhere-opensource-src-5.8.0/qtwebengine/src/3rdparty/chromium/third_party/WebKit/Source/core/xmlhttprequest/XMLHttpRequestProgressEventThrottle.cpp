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

#include "core/xmlhttprequest/XMLHttpRequestProgressEventThrottle.h"

#include "core/EventTypeNames.h"
#include "core/events/ProgressEvent.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/xmlhttprequest/XMLHttpRequest.h"
#include "wtf/Assertions.h"
#include "wtf/text/AtomicString.h"

namespace blink {

static const double kMinimumProgressEventDispatchingIntervalInSeconds = .05; // 50 ms per specification.

XMLHttpRequestProgressEventThrottle::DeferredEvent::DeferredEvent()
{
    clear();
}

void XMLHttpRequestProgressEventThrottle::DeferredEvent::set(bool lengthComputable, unsigned long long loaded, unsigned long long total)
{
    m_isSet = true;

    m_lengthComputable = lengthComputable;
    m_loaded = loaded;
    m_total = total;
}

void XMLHttpRequestProgressEventThrottle::DeferredEvent::clear()
{
    m_isSet = false;

    m_lengthComputable = false;
    m_loaded = 0;
    m_total = 0;
}

Event* XMLHttpRequestProgressEventThrottle::DeferredEvent::take()
{
    ASSERT(m_isSet);

    Event* event = ProgressEvent::create(EventTypeNames::progress, m_lengthComputable, m_loaded, m_total);
    clear();
    return event;
}

XMLHttpRequestProgressEventThrottle::XMLHttpRequestProgressEventThrottle(XMLHttpRequest* target)
    : m_target(target)
    , m_hasDispatchedProgressProgressEvent(false)
{
    ASSERT(target);
}

XMLHttpRequestProgressEventThrottle::~XMLHttpRequestProgressEventThrottle()
{
}

void XMLHttpRequestProgressEventThrottle::dispatchProgressEvent(const AtomicString& type, bool lengthComputable, unsigned long long loaded, unsigned long long total)
{
    // Given that ResourceDispatcher doesn't deliver an event when suspended,
    // we don't have to worry about event dispatching while suspended.
    if (type != EventTypeNames::progress) {
        m_target->dispatchEvent(ProgressEvent::create(type, lengthComputable, loaded, total));
        return;
    }

    if (isActive()) {
        m_deferred.set(lengthComputable, loaded, total);
    } else {
        dispatchProgressProgressEvent(ProgressEvent::create(EventTypeNames::progress, lengthComputable, loaded, total));
        startOneShot(kMinimumProgressEventDispatchingIntervalInSeconds, BLINK_FROM_HERE);
    }
}

void XMLHttpRequestProgressEventThrottle::dispatchReadyStateChangeEvent(Event* event, DeferredEventAction action)
{
    XMLHttpRequest::State state = m_target->readyState();
    // Given that ResourceDispatcher doesn't deliver an event when suspended,
    // we don't have to worry about event dispatching while suspended.
    if (action == Flush) {
        if (m_deferred.isSet())
            dispatchProgressProgressEvent(m_deferred.take());

        stop();
    } else if (action == Clear) {
        m_deferred.clear();
        stop();
    }

    m_hasDispatchedProgressProgressEvent = false;
    if (state == m_target->readyState()) {
        // We don't dispatch the event when an event handler associated with
        // the previously dispatched event changes the readyState (e.g. when
        // the event handler calls xhr.abort()). In such cases a
        // readystatechange should have been already dispatched if necessary.
        InspectorInstrumentation::AsyncTask asyncTask(m_target->getExecutionContext(), m_target, m_target->isAsync());
        m_target->dispatchEvent(event);
    }
}

void XMLHttpRequestProgressEventThrottle::dispatchProgressProgressEvent(Event* progressEvent)
{
    XMLHttpRequest::State state = m_target->readyState();
    if (m_target->readyState() == XMLHttpRequest::LOADING && m_hasDispatchedProgressProgressEvent) {
        TRACE_EVENT1("devtools.timeline", "XHRReadyStateChange", "data", InspectorXhrReadyStateChangeEvent::data(m_target->getExecutionContext(), m_target));
        InspectorInstrumentation::AsyncTask asyncTask(m_target->getExecutionContext(), m_target, m_target->isAsync());
        m_target->dispatchEvent(Event::create(EventTypeNames::readystatechange));
        TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data", InspectorUpdateCountersEvent::data());
    }

    if (m_target->readyState() != state)
        return;

    m_hasDispatchedProgressProgressEvent = true;
    InspectorInstrumentation::AsyncTask asyncTask(m_target->getExecutionContext(), m_target, m_target->isAsync());
    m_target->dispatchEvent(progressEvent);
}

void XMLHttpRequestProgressEventThrottle::fired()
{
    if (!m_deferred.isSet()) {
        // No "progress" event was queued since the previous dispatch, we can
        // safely stop the timer.
        return;
    }

    dispatchProgressProgressEvent(m_deferred.take());

    // Watch if another "progress" ProgressEvent arrives in the next 50ms.
    startOneShot(kMinimumProgressEventDispatchingIntervalInSeconds, BLINK_FROM_HERE);
}

void XMLHttpRequestProgressEventThrottle::suspend()
{
    stop();
}

void XMLHttpRequestProgressEventThrottle::resume()
{
    if (!m_deferred.isSet())
        return;

    // Do not dispatch events inline here, since ExecutionContext is iterating
    // over the list of active DOM objects to resume them, and any activated JS
    // event-handler could insert new active DOM objects to the list.
    startOneShot(0, BLINK_FROM_HERE);
}

DEFINE_TRACE(XMLHttpRequestProgressEventThrottle)
{
    visitor->trace(m_target);
}

} // namespace blink
