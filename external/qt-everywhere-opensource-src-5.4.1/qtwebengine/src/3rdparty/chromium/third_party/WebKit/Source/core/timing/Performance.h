/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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

#ifndef Performance_h
#define Performance_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowProperty.h"
#include "core/timing/MemoryInfo.h"
#include "core/timing/PerformanceEntry.h"
#include "core/timing/PerformanceNavigation.h"
#include "core/timing/PerformanceTiming.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Document;
class ExceptionState;
class ResourceRequest;
class ResourceResponse;
class ResourceTimingInfo;
class UserTiming;

typedef WillBeHeapVector<RefPtrWillBeMember<PerformanceEntry> > PerformanceEntryVector;

class Performance FINAL : public RefCountedWillBeRefCountedGarbageCollected<Performance>, public ScriptWrappable, public DOMWindowProperty, public EventTargetWithInlineData {
    REFCOUNTED_EVENT_TARGET(Performance);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Performance);
public:
    static PassRefPtrWillBeRawPtr<Performance> create(LocalFrame* frame)
    {
        return adoptRefWillBeRefCountedGarbageCollected(new Performance(frame));
    }
    virtual ~Performance();

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    PassRefPtrWillBeRawPtr<MemoryInfo> memory() const;
    PerformanceNavigation* navigation() const;
    PerformanceTiming* timing() const;
    double now() const;

    PerformanceEntryVector getEntries() const;
    PerformanceEntryVector getEntriesByType(const String& entryType);
    PerformanceEntryVector getEntriesByName(const String& name, const String& entryType);

    void webkitClearResourceTimings();
    void webkitSetResourceTimingBufferSize(unsigned);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitresourcetimingbufferfull);

    void addResourceTiming(const ResourceTimingInfo&, Document*);

    void mark(const String& markName, ExceptionState&);
    void clearMarks(const String& markName);

    void measure(const String& measureName, const String& startMark, const String& endMark, ExceptionState&);
    void clearMeasures(const String& measureName);

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit Performance(LocalFrame*);

    bool isResourceTimingBufferFull();
    void addResourceTimingBuffer(PassRefPtrWillBeRawPtr<PerformanceEntry>);

    mutable RefPtrWillBeMember<PerformanceNavigation> m_navigation;
    mutable RefPtrWillBeMember<PerformanceTiming> m_timing;

    PerformanceEntryVector m_resourceTimingBuffer;
    unsigned m_resourceTimingBufferSize;
    double m_referenceTime;

    RefPtrWillBeMember<UserTiming> m_userTiming;
};

}

#endif // Performance_h
