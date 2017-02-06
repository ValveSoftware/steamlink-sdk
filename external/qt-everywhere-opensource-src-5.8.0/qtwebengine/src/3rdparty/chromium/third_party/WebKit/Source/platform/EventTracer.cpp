/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "platform/EventTracer.h"

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "platform/TracedValue.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include <memory>
#include <stdio.h>

namespace blink {

static_assert(sizeof(TraceEvent::TraceEventHandle) == sizeof(base::trace_event::TraceEventHandle), "TraceEventHandle types must be the same");
static_assert(sizeof(TraceEvent::TraceEventAPIAtomicWord) == sizeof(const char*), "TraceEventAPIAtomicWord must be pointer-sized.");

// The dummy variable is needed to avoid a crash when someone updates the state variables
// before EventTracer::initialize() is called.
TraceEvent::TraceEventAPIAtomicWord dummyTraceSamplingState = 0;
TraceEvent::TraceEventAPIAtomicWord* traceSamplingState[3] = {&dummyTraceSamplingState, &dummyTraceSamplingState, &dummyTraceSamplingState };

void EventTracer::initialize()
{
    traceSamplingState[0] = reinterpret_cast<TraceEvent::TraceEventAPIAtomicWord*>(&TRACE_EVENT_API_THREAD_BUCKET(0));
    // FIXME: traceSamplingState[0] can be 0 in split-dll build. http://crbug.com/256965
    if (!traceSamplingState[0])
        traceSamplingState[0] = &dummyTraceSamplingState;
    traceSamplingState[1] = reinterpret_cast<TraceEvent::TraceEventAPIAtomicWord*>(&TRACE_EVENT_API_THREAD_BUCKET(1));
    if (!traceSamplingState[1])
        traceSamplingState[1] = &dummyTraceSamplingState;
    traceSamplingState[2] = reinterpret_cast<TraceEvent::TraceEventAPIAtomicWord*>(&TRACE_EVENT_API_THREAD_BUCKET(2));
    if (!traceSamplingState[2])
        traceSamplingState[2] = &dummyTraceSamplingState;
}

const unsigned char* EventTracer::getTraceCategoryEnabledFlag(const char* categoryName)
{
    return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(categoryName);
}

TraceEvent::TraceEventHandle EventTracer::addTraceEvent(char phase, const unsigned char* categoryEnabledFlag,
    const char* name, const char* scope, unsigned long long id, unsigned long long bindId, double timestamp,
    int numArgs, const char* argNames[], const unsigned char argTypes[],
    const unsigned long long argValues[],
    std::unique_ptr<TracedValue> tracedValue1,
    std::unique_ptr<TracedValue> tracedValue2,
    unsigned flags)
{
    std::unique_ptr<base::trace_event::ConvertableToTraceFormat> convertables[2];
    ASSERT(numArgs <= 2);
    // We move m_tracedValues from TracedValues for thread safety.
    // https://crbug.com/478149
    if (numArgs >= 1 && argTypes[0] == TRACE_VALUE_TYPE_CONVERTABLE)
        convertables[0] = std::move(tracedValue1->m_tracedValue);
    if (numArgs >= 2 && argTypes[1] == TRACE_VALUE_TYPE_CONVERTABLE)
        convertables[1] = std::move(tracedValue2->m_tracedValue);
    return addTraceEvent(phase, categoryEnabledFlag, name, scope, id, bindId, timestamp, numArgs, argNames, argTypes, argValues, convertables, flags);
}

TraceEvent::TraceEventHandle EventTracer::addTraceEvent(char phase, const unsigned char* categoryEnabledFlag,
    const char* name, const char* scope, unsigned long long id, unsigned long long bindId, double timestamp,
    int numArgs, const char** argNames, const unsigned char* argTypes,
    const unsigned long long* argValues, unsigned flags)
{
    return addTraceEvent(phase, categoryEnabledFlag, name, scope, id, bindId, timestamp, numArgs, argNames, argTypes, argValues, nullptr, flags);
}

TraceEvent::TraceEventHandle EventTracer::addTraceEvent(char phase, const unsigned char* categoryEnabledFlag,
    const char* name, const char* scope, unsigned long long id, unsigned long long bindId, double timestamp,
    int numArgs, const char** argNames, const unsigned char* argTypes, const unsigned long long* argValues,
    std::unique_ptr<base::trace_event::ConvertableToTraceFormat>* convertables, unsigned flags)
{
    base::TimeTicks timestampTimeTicks = base::TimeTicks() + base::TimeDelta::FromSecondsD(timestamp);
    base::trace_event::TraceEventHandle handle = TRACE_EVENT_API_ADD_TRACE_EVENT_WITH_THREAD_ID_AND_TIMESTAMP(phase, categoryEnabledFlag, name, scope, id, bindId, base::PlatformThread::CurrentId(), timestampTimeTicks, numArgs, argNames, argTypes, argValues, convertables, flags);
    TraceEvent::TraceEventHandle result;
    memcpy(&result, &handle, sizeof(result));
    return result;
}

void EventTracer::updateTraceEventDuration(const unsigned char* categoryEnabledFlag, const char* name, TraceEvent::TraceEventHandle handle)
{
    base::trace_event::TraceEventHandle traceEventHandle;
    memcpy(&traceEventHandle, &handle, sizeof(handle));
    TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(categoryEnabledFlag, name, traceEventHandle);
}

double EventTracer::systemTraceTime()
{
    return (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
}

} // namespace blink
