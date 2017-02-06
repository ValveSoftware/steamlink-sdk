// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DocumentTiming.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "platform/TraceEvent.h"

namespace blink {

DocumentTiming::DocumentTiming(Document& document)
    : m_document(document)
{
}

DEFINE_TRACE(DocumentTiming)
{
    visitor->trace(m_document);
}

LocalFrame* DocumentTiming::frame() const
{
    return m_document ? m_document->frame() : nullptr;
}

void DocumentTiming::notifyDocumentTimingChanged()
{
    if (m_document && m_document->loader())
        m_document->loader()->didChangePerformanceTiming();
}

void DocumentTiming::markDomLoading()
{
    m_domLoading = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "domLoading", m_domLoading, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentTiming::markDomInteractive()
{
    m_domInteractive = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "domInteractive", m_domInteractive, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentTiming::markDomContentLoadedEventStart()
{
    m_domContentLoadedEventStart = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "domContentLoadedEventStart", m_domContentLoadedEventStart, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentTiming::markDomContentLoadedEventEnd()
{
    m_domContentLoadedEventEnd = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "domContentLoadedEventEnd", m_domContentLoadedEventEnd, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentTiming::markDomComplete()
{
    m_domComplete = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "domComplete", m_domComplete, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentTiming::markFirstLayout()
{
    m_firstLayout = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "firstLayout", m_firstLayout, "frame", frame());
    notifyDocumentTimingChanged();
}

} // namespace blink
