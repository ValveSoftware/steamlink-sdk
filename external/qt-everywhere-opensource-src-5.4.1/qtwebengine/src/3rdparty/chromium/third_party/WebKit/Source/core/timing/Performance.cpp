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

#include "config.h"
#include "core/timing/Performance.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/DocumentLoader.h"
#include "core/timing/ResourceTimingInfo.h"
#include "core/timing/PerformanceResourceTiming.h"
#include "core/timing/PerformanceUserTiming.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

static const size_t defaultResourceTimingBufferSize = 150;

Performance::Performance(LocalFrame* frame)
    : DOMWindowProperty(frame)
    , m_resourceTimingBufferSize(defaultResourceTimingBufferSize)
    , m_referenceTime(frame && frame->host() ? frame->document()->loader()->timing()->referenceMonotonicTime() : 0.0)
    , m_userTiming(nullptr)
{
    ScriptWrappable::init(this);
}

Performance::~Performance()
{
}

const AtomicString& Performance::interfaceName() const
{
    return EventTargetNames::Performance;
}

ExecutionContext* Performance::executionContext() const
{
    if (!frame())
        return 0;
    return frame()->document();
}

PassRefPtrWillBeRawPtr<MemoryInfo> Performance::memory() const
{
    return MemoryInfo::create();
}

PerformanceNavigation* Performance::navigation() const
{
    if (!m_navigation)
        m_navigation = PerformanceNavigation::create(m_frame);

    return m_navigation.get();
}

PerformanceTiming* Performance::timing() const
{
    if (!m_timing)
        m_timing = PerformanceTiming::create(m_frame);

    return m_timing.get();
}

PerformanceEntryVector Performance::getEntries() const
{
    PerformanceEntryVector entries;

    entries.appendVector(m_resourceTimingBuffer);

    if (m_userTiming) {
        entries.appendVector(m_userTiming->getMarks());
        entries.appendVector(m_userTiming->getMeasures());
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

PerformanceEntryVector Performance::getEntriesByType(const String& entryType)
{
    PerformanceEntryVector entries;

    if (equalIgnoringCase(entryType, "resource"))
        for (PerformanceEntryVector::const_iterator resource = m_resourceTimingBuffer.begin(); resource != m_resourceTimingBuffer.end(); ++resource)
            entries.append(*resource);

    if (m_userTiming) {
        if (equalIgnoringCase(entryType, "mark"))
            entries.appendVector(m_userTiming->getMarks());
        else if (equalIgnoringCase(entryType, "measure"))
            entries.appendVector(m_userTiming->getMeasures());
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

PerformanceEntryVector Performance::getEntriesByName(const String& name, const String& entryType)
{
    PerformanceEntryVector entries;

    if (entryType.isNull() || equalIgnoringCase(entryType, "resource"))
        for (PerformanceEntryVector::const_iterator resource = m_resourceTimingBuffer.begin(); resource != m_resourceTimingBuffer.end(); ++resource)
            if ((*resource)->name() == name)
                entries.append(*resource);

    if (m_userTiming) {
        if (entryType.isNull() || equalIgnoringCase(entryType, "mark"))
            entries.appendVector(m_userTiming->getMarks(name));
        if (entryType.isNull() || equalIgnoringCase(entryType, "measure"))
            entries.appendVector(m_userTiming->getMeasures(name));
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

void Performance::webkitClearResourceTimings()
{
    m_resourceTimingBuffer.clear();
}

void Performance::webkitSetResourceTimingBufferSize(unsigned size)
{
    m_resourceTimingBufferSize = size;
    if (isResourceTimingBufferFull())
        dispatchEvent(Event::create(EventTypeNames::webkitresourcetimingbufferfull));
}

static bool passesTimingAllowCheck(const ResourceResponse& response, Document* requestingDocument, const AtomicString& originalTimingAllowOrigin)
{
    AtomicallyInitializedStatic(AtomicString&, timingAllowOrigin = *new AtomicString("timing-allow-origin"));

    RefPtr<SecurityOrigin> resourceOrigin = SecurityOrigin::create(response.url());
    if (resourceOrigin->isSameSchemeHostPort(requestingDocument->securityOrigin()))
        return true;

    const AtomicString& timingAllowOriginString = originalTimingAllowOrigin.isEmpty() ? response.httpHeaderField(timingAllowOrigin) : originalTimingAllowOrigin;
    if (timingAllowOriginString.isEmpty() || equalIgnoringCase(timingAllowOriginString, "null"))
        return false;

    if (timingAllowOriginString == starAtom)
        return true;

    const String& securityOrigin = requestingDocument->securityOrigin()->toString();
    Vector<String> timingAllowOrigins;
    timingAllowOriginString.string().split(" ", timingAllowOrigins);
    for (size_t i = 0; i < timingAllowOrigins.size(); ++i) {
        if (timingAllowOrigins[i] == securityOrigin)
            return true;
    }

    return false;
}

static bool allowsTimingRedirect(const Vector<ResourceResponse>& redirectChain, const ResourceResponse& finalResponse, Document* initiatorDocument)
{
    if (!passesTimingAllowCheck(finalResponse, initiatorDocument, emptyAtom))
        return false;

    for (size_t i = 0; i < redirectChain.size(); i++) {
        if (!passesTimingAllowCheck(redirectChain[i], initiatorDocument, emptyAtom))
            return false;
    }

    return true;
}

void Performance::addResourceTiming(const ResourceTimingInfo& info, Document* initiatorDocument)
{
    if (isResourceTimingBufferFull())
        return;

    const ResourceResponse& finalResponse = info.finalResponse();
    bool allowTimingDetails = passesTimingAllowCheck(finalResponse, initiatorDocument, info.originalTimingAllowOrigin());
    double startTime = info.initialTime();

    if (info.redirectChain().isEmpty()) {
        RefPtrWillBeRawPtr<PerformanceEntry> entry = PerformanceResourceTiming::create(info, initiatorDocument, startTime, allowTimingDetails);
        addResourceTimingBuffer(entry);
        return;
    }

    const Vector<ResourceResponse>& redirectChain = info.redirectChain();
    bool allowRedirectDetails = allowsTimingRedirect(redirectChain, finalResponse, initiatorDocument);

    if (!allowRedirectDetails) {
        ResourceLoadTiming* finalTiming = finalResponse.resourceLoadTiming();
        ASSERT(finalTiming);
        if (finalTiming)
            startTime = finalTiming->requestTime;
    }

    ResourceLoadTiming* lastRedirectTiming = redirectChain.last().resourceLoadTiming();
    ASSERT(lastRedirectTiming);
    double lastRedirectEndTime = lastRedirectTiming->receiveHeadersEnd;

    RefPtrWillBeRawPtr<PerformanceEntry> entry = PerformanceResourceTiming::create(info, initiatorDocument, startTime, lastRedirectEndTime, allowTimingDetails, allowRedirectDetails);
    addResourceTimingBuffer(entry);
}

void Performance::addResourceTimingBuffer(PassRefPtrWillBeRawPtr<PerformanceEntry> entry)
{
    m_resourceTimingBuffer.append(entry);

    if (isResourceTimingBufferFull())
        dispatchEvent(Event::create(EventTypeNames::webkitresourcetimingbufferfull));
}

bool Performance::isResourceTimingBufferFull()
{
    return m_resourceTimingBuffer.size() >= m_resourceTimingBufferSize;
}

void Performance::mark(const String& markName, ExceptionState& exceptionState)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->mark(markName, exceptionState);
}

void Performance::clearMarks(const String& markName)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->clearMarks(markName);
}

void Performance::measure(const String& measureName, const String& startMark, const String& endMark, ExceptionState& exceptionState)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->measure(measureName, startMark, endMark, exceptionState);
}

void Performance::clearMeasures(const String& measureName)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(this);
    m_userTiming->clearMeasures(measureName);
}

double Performance::now() const
{
    return 1000.0 * (monotonicallyIncreasingTime() - m_referenceTime);
}

void Performance::trace(Visitor* visitor)
{
    visitor->trace(m_navigation);
    visitor->trace(m_timing);
    visitor->trace(m_resourceTimingBuffer);
    visitor->trace(m_userTiming);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace WebCore
