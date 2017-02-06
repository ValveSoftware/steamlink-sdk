// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceObserver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExecutionContext.h"
#include "core/timing/PerformanceBase.h"
#include "core/timing/PerformanceEntry.h"
#include "core/timing/PerformanceObserverCallback.h"
#include "core/timing/PerformanceObserverEntryList.h"
#include "core/timing/PerformanceObserverInit.h"
#include "platform/Timer.h"
#include <algorithm>

namespace blink {

PerformanceObserver* PerformanceObserver::create(PerformanceBase* performance, PerformanceObserverCallback* callback)
{
    ASSERT(isMainThread());
    return new PerformanceObserver(performance, callback);
}

PerformanceObserver::PerformanceObserver(PerformanceBase* performance, PerformanceObserverCallback* callback)
    : m_callback(callback)
    , m_performance(performance)
    , m_filterOptions(PerformanceEntry::Invalid)
    , m_isRegistered(false)
{
}

void PerformanceObserver::observe(const PerformanceObserverInit& observerInit, ExceptionState& exceptionState)
{
    if (!m_performance) {
        exceptionState.throwTypeError("Window may be destroyed? Performance target is invalid.");
        return;
    }

    PerformanceEntryTypeMask entryTypes = PerformanceEntry::Invalid;
    if (observerInit.hasEntryTypes() && observerInit.entryTypes().size()) {
        const Vector<String>& sequence = observerInit.entryTypes();
        for (const auto& entryTypeString : sequence)
            entryTypes |= PerformanceEntry::toEntryTypeEnum(entryTypeString);
    }
    if (entryTypes == PerformanceEntry::Invalid) {
        exceptionState.throwTypeError("A Performance Observer MUST have a non-empty entryTypes attribute.");
        return;
    }
    m_filterOptions = entryTypes;
    if (m_isRegistered)
        m_performance->updatePerformanceObserverFilterOptions();
    else
        m_performance->registerPerformanceObserver(*this);
    m_isRegistered = true;
}

void PerformanceObserver::disconnect()
{
    if (m_performance)
        m_performance->unregisterPerformanceObserver(*this);

    m_performanceEntries.clear();
    m_isRegistered = false;
}

void PerformanceObserver::enqueuePerformanceEntry(PerformanceEntry& entry)
{
    ASSERT(isMainThread());
    m_performanceEntries.append(&entry);
    if (m_performance)
        m_performance->activateObserver(*this);
}

bool PerformanceObserver::shouldBeSuspended() const
{
    return m_callback->getExecutionContext() && m_callback->getExecutionContext()->activeDOMObjectsAreSuspended();
}

void PerformanceObserver::deliver()
{
    ASSERT(!shouldBeSuspended());

    if (m_performanceEntries.isEmpty())
        return;

    PerformanceEntryVector performanceEntries;
    performanceEntries.swap(m_performanceEntries);
    Member<PerformanceObserverEntryList> entryList(new PerformanceObserverEntryList(performanceEntries));

    m_callback->handleEvent(entryList, this);
}

DEFINE_TRACE(PerformanceObserver)
{
    visitor->trace(m_callback);
    visitor->trace(m_performance);
    visitor->trace(m_performanceEntries);
}

} // namespace blink
