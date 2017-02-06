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

#include "core/timing/PerformanceBase.h"

#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/timing/PerformanceCompositeTiming.h"
#include "core/timing/PerformanceObserver.h"
#include "core/timing/PerformanceRenderTiming.h"
#include "core/timing/PerformanceResourceTiming.h"
#include "core/timing/PerformanceUserTiming.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/CurrentTime.h"
#include <algorithm>

namespace blink {

using PerformanceObserverVector = HeapVector<Member<PerformanceObserver>>;

static const size_t defaultResourceTimingBufferSize = 150;
static const size_t defaultFrameTimingBufferSize = 150;

PerformanceBase::PerformanceBase(double timeOrigin)
    : m_frameTimingBufferSize(defaultFrameTimingBufferSize)
    , m_resourceTimingBufferSize(defaultResourceTimingBufferSize)
    , m_timeOrigin(timeOrigin)
    , m_userTiming(nullptr)
    , m_observerFilterOptions(PerformanceEntry::Invalid)
    , m_deliverObservationsTimer(this, &PerformanceBase::deliverObservationsTimerFired)
{
}

PerformanceBase::~PerformanceBase()
{
}

const AtomicString& PerformanceBase::interfaceName() const
{
    return EventTargetNames::Performance;
}

PerformanceTiming* PerformanceBase::timing() const
{
    return nullptr;
}

PerformanceEntryVector PerformanceBase::getEntries() const
{
    PerformanceEntryVector entries;

    entries.appendVector(m_resourceTimingBuffer);
    entries.appendVector(m_frameTimingBuffer);

    if (m_userTiming) {
        entries.appendVector(m_userTiming->getMarks());
        entries.appendVector(m_userTiming->getMeasures());
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

PerformanceEntryVector PerformanceBase::getEntriesByType(const String& entryType)
{
    PerformanceEntryVector entries;
    PerformanceEntry::EntryType type = PerformanceEntry::toEntryTypeEnum(entryType);

    switch (type) {
    case PerformanceEntry::Invalid:
        return entries;
    case PerformanceEntry::Resource:
        for (const auto& resource : m_resourceTimingBuffer)
            entries.append(resource);
        break;
    case PerformanceEntry::Composite:
    case PerformanceEntry::Render:
        for (const auto& frame : m_frameTimingBuffer) {
            if (type == frame->entryTypeEnum()) {
                entries.append(frame);
            }
        }
        break;
    case PerformanceEntry::Mark:
        if (m_userTiming)
            entries.appendVector(m_userTiming->getMarks());
        break;
    case PerformanceEntry::Measure:
        if (m_userTiming)
            entries.appendVector(m_userTiming->getMeasures());
        break;
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

PerformanceEntryVector PerformanceBase::getEntriesByName(const String& name, const String& entryType)
{
    PerformanceEntryVector entries;
    PerformanceEntry::EntryType type = PerformanceEntry::toEntryTypeEnum(entryType);

    if (!entryType.isNull() && type == PerformanceEntry::Invalid)
        return entries;

    if (entryType.isNull() || type == PerformanceEntry::Resource) {
        for (const auto& resource : m_resourceTimingBuffer) {
            if (resource->name() == name)
                entries.append(resource);
        }
    }

    if (entryType.isNull() || type == PerformanceEntry::Composite || type == PerformanceEntry::Render) {
        for (const auto& frame : m_frameTimingBuffer) {
            if (frame->name() == name && (entryType.isNull()
                || equalIgnoringCase(entryType, frame->entryType()))) {
                entries.append(frame);
            }
        }
    }

    if (m_userTiming) {
        if (entryType.isNull() || type == PerformanceEntry::Mark)
            entries.appendVector(m_userTiming->getMarks(name));
        if (entryType.isNull() || type == PerformanceEntry::Measure)
            entries.appendVector(m_userTiming->getMeasures(name));
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

void PerformanceBase::clearResourceTimings()
{
    m_resourceTimingBuffer.clear();
}

void PerformanceBase::setResourceTimingBufferSize(unsigned size)
{
    m_resourceTimingBufferSize = size;
    if (isResourceTimingBufferFull()) {
        dispatchEvent(Event::create(EventTypeNames::resourcetimingbufferfull));
        dispatchEvent(Event::create(EventTypeNames::webkitresourcetimingbufferfull));
    }
}

void PerformanceBase::clearFrameTimings()
{
    m_frameTimingBuffer.clear();
}

void PerformanceBase::setFrameTimingBufferSize(unsigned size)
{
    m_frameTimingBufferSize = size;
    if (isFrameTimingBufferFull())
        dispatchEvent(Event::create(EventTypeNames::frametimingbufferfull));
}

static bool passesTimingAllowCheck(const ResourceResponse& response, const SecurityOrigin& initiatorSecurityOrigin, const AtomicString& originalTimingAllowOrigin)
{
    RefPtr<SecurityOrigin> resourceOrigin = SecurityOrigin::create(response.url());
    if (resourceOrigin->isSameSchemeHostPort(&initiatorSecurityOrigin))
        return true;

    const AtomicString& timingAllowOriginString = originalTimingAllowOrigin.isEmpty() ? response.httpHeaderField(HTTPNames::Timing_Allow_Origin) : originalTimingAllowOrigin;
    if (timingAllowOriginString.isEmpty() || equalIgnoringCase(timingAllowOriginString, "null"))
        return false;

    if (timingAllowOriginString == "*")
        return true;

    const String& securityOrigin = initiatorSecurityOrigin.toString();
    Vector<String> timingAllowOrigins;
    timingAllowOriginString.getString().split(' ', timingAllowOrigins);
    for (const String& allowOrigin : timingAllowOrigins) {
        if (allowOrigin == securityOrigin)
            return true;
    }

    return false;
}

static bool allowsTimingRedirect(const Vector<ResourceResponse>& redirectChain, const ResourceResponse& finalResponse, const SecurityOrigin& initiatorSecurityOrigin)
{
    if (!passesTimingAllowCheck(finalResponse, initiatorSecurityOrigin, AtomicString()))
        return false;

    for (const ResourceResponse& response : redirectChain) {
        if (!passesTimingAllowCheck(response, initiatorSecurityOrigin, AtomicString()))
            return false;
    }

    return true;
}

void PerformanceBase::addResourceTiming(const ResourceTimingInfo& info)
{
    if (isResourceTimingBufferFull() && !hasObserverFor(PerformanceEntry::Resource))
        return;
    SecurityOrigin* securityOrigin = nullptr;
    if (ExecutionContext* context = getExecutionContext())
        securityOrigin = context->getSecurityOrigin();
    if (!securityOrigin)
        return;

    const ResourceResponse& finalResponse = info.finalResponse();
    bool allowTimingDetails = passesTimingAllowCheck(finalResponse, *securityOrigin, info.originalTimingAllowOrigin());
    double startTime = info.initialTime();

    if (info.redirectChain().isEmpty()) {
        PerformanceEntry* entry = PerformanceResourceTiming::create(info, timeOrigin(), startTime, allowTimingDetails);
        notifyObserversOfEntry(*entry);
        if (!isResourceTimingBufferFull())
            addResourceTimingBuffer(*entry);
        return;
    }

    const Vector<ResourceResponse>& redirectChain = info.redirectChain();
    bool allowRedirectDetails = allowsTimingRedirect(redirectChain, finalResponse, *securityOrigin);

    if (!allowRedirectDetails) {
        ResourceLoadTiming* finalTiming = finalResponse.resourceLoadTiming();
        ASSERT(finalTiming);
        if (finalTiming)
            startTime = finalTiming->requestTime();
    }

    ResourceLoadTiming* lastRedirectTiming = redirectChain.last().resourceLoadTiming();
    ASSERT(lastRedirectTiming);
    double lastRedirectEndTime = lastRedirectTiming->receiveHeadersEnd();

    PerformanceEntry* entry = PerformanceResourceTiming::create(info, timeOrigin(), startTime, lastRedirectEndTime, allowTimingDetails, allowRedirectDetails);
    notifyObserversOfEntry(*entry);
    if (!isResourceTimingBufferFull())
        addResourceTimingBuffer(*entry);
}

void PerformanceBase::addResourceTimingBuffer(PerformanceEntry& entry)
{
    m_resourceTimingBuffer.append(&entry);

    if (isResourceTimingBufferFull()) {
        dispatchEvent(Event::create(EventTypeNames::resourcetimingbufferfull));
        dispatchEvent(Event::create(EventTypeNames::webkitresourcetimingbufferfull));
    }
}

bool PerformanceBase::isResourceTimingBufferFull()
{
    return m_resourceTimingBuffer.size() >= m_resourceTimingBufferSize;
}

void PerformanceBase::addRenderTiming(Document* initiatorDocument, unsigned sourceFrame, double startTime, double finishTime)
{
    if (isFrameTimingBufferFull() && !hasObserverFor(PerformanceEntry::Render))
        return;

    PerformanceEntry* entry = PerformanceRenderTiming::create(initiatorDocument, sourceFrame, startTime, finishTime);
    notifyObserversOfEntry(*entry);
    if (!isFrameTimingBufferFull())
        addFrameTimingBuffer(*entry);
}

void PerformanceBase::addCompositeTiming(Document* initiatorDocument, unsigned sourceFrame, double startTime)
{
    if (isFrameTimingBufferFull() && !hasObserverFor(PerformanceEntry::Composite))
        return;

    PerformanceEntry* entry = PerformanceCompositeTiming::create(initiatorDocument, sourceFrame, startTime);
    notifyObserversOfEntry(*entry);
    if (!isFrameTimingBufferFull())
        addFrameTimingBuffer(*entry);
}

void PerformanceBase::addFrameTimingBuffer(PerformanceEntry& entry)
{
    m_frameTimingBuffer.append(&entry);

    if (isFrameTimingBufferFull())
        dispatchEvent(Event::create(EventTypeNames::frametimingbufferfull));
}

bool PerformanceBase::isFrameTimingBufferFull()
{
    return m_frameTimingBuffer.size() >= m_frameTimingBufferSize;
}

void PerformanceBase::mark(const String& markName, ExceptionState& exceptionState)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(*this);
    if (PerformanceEntry* entry = m_userTiming->mark(markName, exceptionState))
        notifyObserversOfEntry(*entry);
}

void PerformanceBase::clearMarks(const String& markName)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(*this);
    m_userTiming->clearMarks(markName);
}

void PerformanceBase::measure(const String& measureName, const String& startMark, const String& endMark, ExceptionState& exceptionState)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(*this);
    if (PerformanceEntry* entry = m_userTiming->measure(measureName, startMark, endMark, exceptionState))
        notifyObserversOfEntry(*entry);
}

void PerformanceBase::clearMeasures(const String& measureName)
{
    if (!m_userTiming)
        m_userTiming = UserTiming::create(*this);
    m_userTiming->clearMeasures(measureName);
}

void PerformanceBase::registerPerformanceObserver(PerformanceObserver& observer)
{
    m_observerFilterOptions |= observer.filterOptions();
    m_observers.add(&observer);
}

void PerformanceBase::unregisterPerformanceObserver(PerformanceObserver& oldObserver)
{
    ASSERT(isMainThread());
    // Deliver any pending observations on this observer before unregistering.
    if (m_activeObservers.contains(&oldObserver) && !oldObserver.shouldBeSuspended()) {
        oldObserver.deliver();
        m_activeObservers.remove(&oldObserver);
    }
    m_observers.remove(&oldObserver);
    updatePerformanceObserverFilterOptions();
}

void PerformanceBase::updatePerformanceObserverFilterOptions()
{
    m_observerFilterOptions = PerformanceEntry::Invalid;
    for (const auto& observer : m_observers) {
        m_observerFilterOptions |= observer->filterOptions();
    }
}

void PerformanceBase::notifyObserversOfEntry(PerformanceEntry& entry)
{
    for (auto& observer : m_observers) {
        if (observer->filterOptions() & entry.entryTypeEnum())
            observer->enqueuePerformanceEntry(entry);
    }
}

bool PerformanceBase::hasObserverFor(PerformanceEntry::EntryType filterType)
{
    return m_observerFilterOptions & filterType;
}

void PerformanceBase::activateObserver(PerformanceObserver& observer)
{
    if (m_activeObservers.isEmpty())
        m_deliverObservationsTimer.startOneShot(0, BLINK_FROM_HERE);

    m_activeObservers.add(&observer);
}

void PerformanceBase::resumeSuspendedObservers()
{
    ASSERT(isMainThread());
    if (m_suspendedObservers.isEmpty())
        return;

    PerformanceObserverVector suspended;
    copyToVector(m_suspendedObservers, suspended);
    for (size_t i = 0; i < suspended.size(); ++i) {
        if (!suspended[i]->shouldBeSuspended()) {
            m_suspendedObservers.remove(suspended[i]);
            activateObserver(*suspended[i]);
        }
    }
}

void PerformanceBase::deliverObservationsTimerFired(Timer<PerformanceBase>*)
{
    ASSERT(isMainThread());
    PerformanceObservers observers;
    m_activeObservers.swap(observers);
    for (const auto& observer : observers) {
        if (observer->shouldBeSuspended())
            m_suspendedObservers.add(observer);
        else
            observer->deliver();
    }
}

// static
double PerformanceBase::clampTimeResolution(double timeSeconds)
{
    const double resolutionSeconds = 0.000005;
    return floor(timeSeconds / resolutionSeconds) * resolutionSeconds;
}

DOMHighResTimeStamp PerformanceBase::monotonicTimeToDOMHighResTimeStamp(double monotonicTime) const
{
    // Avoid exposing raw platform timestamps.
    if (m_timeOrigin == 0.0)
        return 0.0;

    double timeInSeconds = monotonicTime - m_timeOrigin;
    return convertSecondsToDOMHighResTimeStamp(clampTimeResolution(timeInSeconds));
}

DOMHighResTimeStamp PerformanceBase::now() const
{
    return monotonicTimeToDOMHighResTimeStamp(monotonicallyIncreasingTime());
}

DEFINE_TRACE(PerformanceBase)
{
    visitor->trace(m_frameTimingBuffer);
    visitor->trace(m_resourceTimingBuffer);
    visitor->trace(m_userTiming);
    visitor->trace(m_observers);
    visitor->trace(m_activeObservers);
    visitor->trace(m_suspendedObservers);
    EventTargetWithInlineData::trace(visitor);
}

} // namespace blink
