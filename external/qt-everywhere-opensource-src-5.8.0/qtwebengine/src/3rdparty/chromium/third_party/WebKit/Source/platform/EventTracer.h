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

#ifndef EventTracer_h
#define EventTracer_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/text/WTFString.h"
#include <memory>
#include <stdint.h>

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
class TraceEventMemoryOverhead;
}
}

// This will mark the trace event as disabled by default. The user will need
// to explicitly enable the event.
#define TRACE_DISABLED_BY_DEFAULT(name) "disabled-by-default-" name

namespace blink {

class TracedValue;

namespace TraceEvent {
typedef uint64_t TraceEventHandle;
typedef intptr_t TraceEventAPIAtomicWord;
} // namespace TraceEvent

// FIXME: Make these global variables thread-safe. Make a value update atomic.
PLATFORM_EXPORT extern TraceEvent::TraceEventAPIAtomicWord* traceSamplingState[3];

class PLATFORM_EXPORT EventTracer {
    STATIC_ONLY(EventTracer);
public:
    static void initialize();
    static const unsigned char* getTraceCategoryEnabledFlag(const char*);
    static TraceEvent::TraceEventHandle addTraceEvent(char phase,
        const unsigned char* categoryEnabledFlag,
        const char* name,
        const char* scope,
        unsigned long long id,
        unsigned long long bindId,
        double timestamp,
        int numArgs,
        const char* argNames[],
        const unsigned char argTypes[],
        const unsigned long long argValues[],
        std::unique_ptr<TracedValue>,
        std::unique_ptr<TracedValue>,
        unsigned flags);
    static TraceEvent::TraceEventHandle addTraceEvent(char phase,
        const unsigned char* categoryEnabledFlag,
        const char* name,
        const char* scope,
        unsigned long long id,
        unsigned long long bindId,
        double timestamp,
        int numArgs,
        const char* argNames[],
        const unsigned char argTypes[],
        const unsigned long long argValues[],
        unsigned flags);
    static void updateTraceEventDuration(const unsigned char* categoryEnabledFlag, const char* name, TraceEvent::TraceEventHandle);
    static double systemTraceTime();

private:
    static TraceEvent::TraceEventHandle addTraceEvent(char phase,
        const unsigned char* categoryEnabledFlag,
        const char* name,
        const char* scope,
        unsigned long long id,
        unsigned long long bindId,
        double timestamp,
        int numArgs,
        const char* argNames[],
        const unsigned char argTypes[],
        const unsigned long long argValues[],
        std::unique_ptr<base::trace_event::ConvertableToTraceFormat>* convertables,
        unsigned flags);
};

} // namespace blink

#endif // EventTracer_h
