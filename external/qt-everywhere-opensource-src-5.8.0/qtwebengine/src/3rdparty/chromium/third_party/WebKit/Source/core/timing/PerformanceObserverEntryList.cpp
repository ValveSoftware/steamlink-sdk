// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceObserverEntryList.h"

#include "core/timing/PerformanceEntry.h"
#include "wtf/StdLibExtras.h"
#include <algorithm>

namespace blink {

PerformanceObserverEntryList::PerformanceObserverEntryList(const PerformanceEntryVector& entryVector)
    : m_performanceEntries(entryVector)
{
}

PerformanceObserverEntryList::~PerformanceObserverEntryList()
{
}


PerformanceEntryVector PerformanceObserverEntryList::getEntries() const
{
    PerformanceEntryVector entries;

    entries.appendVector(m_performanceEntries);

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

PerformanceEntryVector PerformanceObserverEntryList::getEntriesByType(const String& entryType)
{
    PerformanceEntryVector entries;
    PerformanceEntry::EntryType type = PerformanceEntry::toEntryTypeEnum(entryType);

    if (type == PerformanceEntry::Invalid)
        return entries;

    for (const auto& entry : m_performanceEntries) {
        if (entry->entryTypeEnum() == type) {
            entries.append(entry);
        }
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

PerformanceEntryVector PerformanceObserverEntryList::getEntriesByName(const String& name, const String& entryType)
{
    PerformanceEntryVector entries;
    PerformanceEntry::EntryType type = PerformanceEntry::toEntryTypeEnum(entryType);

    if (!entryType.isNull() && type == PerformanceEntry::Invalid)
        return entries;

    for (const auto& entry : m_performanceEntries) {
        if (entry->name() == name && (entryType.isNull() || type == entry->entryTypeEnum())) {
            entries.append(entry);
        }
    }

    std::sort(entries.begin(), entries.end(), PerformanceEntry::startTimeCompareLessThan);
    return entries;
}

DEFINE_TRACE(PerformanceObserverEntryList)
{
    visitor->trace(m_performanceEntries);
}

} // namespace blink
