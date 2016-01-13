/*
* Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/inspector/TraceEventDispatcher.h"

#include "core/inspector/InspectorClient.h"
#include "wtf/CurrentTime.h"
#include "wtf/Functional.h"
#include "wtf/MainThread.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

void TraceEventDispatcher::dispatchEventOnAnyThread(char phase, const unsigned char*, const char* name, unsigned long long id,
    int numArgs, const char* const* argNames, const unsigned char* argTypes, const unsigned long long* argValues,
    unsigned char flags, double timestamp)
{
    if (phase == TRACE_EVENT_PHASE_INSTANT_WITH_SCOPE)
        phase = TRACE_EVENT_PHASE_INSTANT;
    TraceEventDispatcher* self = instance();
    {
        MutexLocker locker(self->m_mutex);
        if (self->m_handlers.find(std::make_pair(name, phase)) == self->m_handlers.end())
            return;
    }
    self->enqueueEvent(TraceEvent(timestamp, phase, name, id, currentThread(), numArgs, argNames, argTypes, argValues));
    if (isMainThread())
        self->processBackgroundEvents();
}

void TraceEventDispatcher::enqueueEvent(const TraceEvent& event)
{
    const float eventProcessingThresholdInSeconds = 0.1;
    {
        MutexLocker locker(m_mutex);
        m_backgroundEvents.append(event);
        if (m_processEventsTaskInFlight || event.timestamp() - m_lastEventProcessingTime <= eventProcessingThresholdInSeconds)
            return;
    }
    m_processEventsTaskInFlight = true;
    callOnMainThread(bind(&TraceEventDispatcher::processBackgroundEventsTask, this));
}

void TraceEventDispatcher::processBackgroundEventsTask()
{
    m_processEventsTaskInFlight = false;
    processBackgroundEvents();
}

void TraceEventDispatcher::processBackgroundEvents()
{
    ASSERT(isMainThread());
    Vector<TraceEvent> events;
    {
        MutexLocker locker(m_mutex);
        m_lastEventProcessingTime = WTF::monotonicallyIncreasingTime();
        if (m_backgroundEvents.isEmpty())
            return;
        events.reserveCapacity(m_backgroundEvents.capacity());
        m_backgroundEvents.swap(events);
    }
    for (size_t eventIndex = 0, size = events.size(); eventIndex < size; ++eventIndex) {
        const TraceEvent& event = events[eventIndex];
        HandlersMap::iterator it = m_handlers.find(std::make_pair(event.name(), event.phase()));
        if (it == m_handlers.end())
            continue;
        Vector<BoundTraceEventHandler>& handlers = it->value;
        for (size_t handlerIndex = 0; handlerIndex < handlers.size(); ++handlerIndex)
            (handlers[handlerIndex].instance->*(handlers[handlerIndex].method))(event);
    }
}

void TraceEventDispatcher::innerAddListener(const char* name, char phase, TraceEventTargetBase* instance, TraceEventHandlerMethod method, InspectorClient* client)
{
    static const char CategoryFilter[] = "-*," TRACE_DISABLED_BY_DEFAULT("devtools.timeline") "," TRACE_DISABLED_BY_DEFAULT("devtools.timeline.frame");

    ASSERT(isMainThread());
    MutexLocker locker(m_mutex);
    if (m_handlers.isEmpty())
        client->setTraceEventCallback(CategoryFilter, dispatchEventOnAnyThread);
    HandlersMap::iterator it = m_handlers.find(std::make_pair(name, phase));
    if (it == m_handlers.end())
        m_handlers.add(std::make_pair(name, phase), Vector<BoundTraceEventHandler>()).storedValue->value.append(BoundTraceEventHandler(instance, method));
    else
        it->value.append(BoundTraceEventHandler(instance, method));
}

void TraceEventDispatcher::removeAllListeners(TraceEventTargetBase* instance, InspectorClient* client)
{
    ASSERT(isMainThread());
    processBackgroundEvents();
    {
        MutexLocker locker(m_mutex);

        HandlersMap remainingHandlers;
        for (HandlersMap::iterator it = m_handlers.begin(); it != m_handlers.end(); ++it) {
            Vector<BoundTraceEventHandler>& handlers = it->value;
            for (size_t j = 0; j < handlers.size();) {
                if (handlers[j].instance == instance)
                    handlers.remove(j);
                else
                    ++j;
            }
            if (!handlers.isEmpty())
                remainingHandlers.add(it->key, it->value);
        }
        m_handlers.swap(remainingHandlers);
    }
    if (m_handlers.isEmpty())
        client->resetTraceEventCallback();
}

size_t TraceEventDispatcher::TraceEvent::findParameter(const char* name) const
{
    for (int i = 0; i < m_argumentCount; ++i) {
        if (!strcmp(name, m_argumentNames[i]))
            return i;
    }
    return kNotFound;
}

const TraceEvent::TraceValueUnion& TraceEventDispatcher::TraceEvent::parameter(const char* name, unsigned char expectedType) const
{
    static WebCore::TraceEvent::TraceValueUnion missingValue;
    size_t index = findParameter(name);
    ASSERT(isMainThread());
    if (index == kNotFound || m_argumentTypes[index] != expectedType) {
        ASSERT_NOT_REACHED();
        return missingValue;
    }
    return m_argumentValues[index];
}

} // namespace WebCore

