// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintTiming.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "platform/TraceEvent.h"

namespace blink {

static const char kSupplementName[] = "PaintTiming";

PaintTiming& PaintTiming::from(Document& document)
{
    PaintTiming* timing = static_cast<PaintTiming*>(Supplement<Document>::from(document, kSupplementName));
    if (!timing) {
        timing = new PaintTiming(document);
        Supplement<Document>::provideTo(document, kSupplementName, timing);
    }
    return *timing;
}

void PaintTiming::markFirstPaint()
{
    // Test that m_firstPaint is non-zero here, as well as in setFirstPaint, so
    // we avoid invoking monotonicallyIncreasingTime() on every call to
    // markFirstPaint().
    if (m_firstPaint != 0.0)
        return;
    setFirstPaint(monotonicallyIncreasingTime());
    notifyPaintTimingChanged();
}

void PaintTiming::markFirstContentfulPaint()
{
    // Test that m_firstContentfulPaint is non-zero here, as well as in
    // setFirstContentfulPaint, so we avoid invoking
    // monotonicallyIncreasingTime() on every call to
    // markFirstContentfulPaint().
    if (m_firstContentfulPaint != 0.0)
        return;
    setFirstContentfulPaint(monotonicallyIncreasingTime());
    notifyPaintTimingChanged();
}

void PaintTiming::markFirstTextPaint()
{
    if (m_firstTextPaint != 0.0)
        return;
    m_firstTextPaint = monotonicallyIncreasingTime();
    setFirstContentfulPaint(m_firstTextPaint);
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "firstTextPaint", m_firstTextPaint, "frame", frame());
    notifyPaintTimingChanged();
}

void PaintTiming::markFirstImagePaint()
{
    if (m_firstImagePaint != 0.0)
        return;
    m_firstImagePaint = monotonicallyIncreasingTime();
    setFirstContentfulPaint(m_firstImagePaint);
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "firstImagePaint", m_firstImagePaint, "frame", frame());
    notifyPaintTimingChanged();
}

DEFINE_TRACE(PaintTiming)
{
    visitor->trace(m_document);
}

PaintTiming::PaintTiming(Document& document)
    : m_document(document)
{
}

LocalFrame* PaintTiming::frame() const
{
    return m_document ? m_document->frame() : nullptr;
}

void PaintTiming::notifyPaintTimingChanged()
{
    if (m_document && m_document->loader())
        m_document->loader()->didChangePerformanceTiming();
}

void PaintTiming::setFirstPaint(double stamp)
{
    if (m_firstPaint != 0.0)
        return;
    m_firstPaint = stamp;
    TRACE_EVENT_INSTANT1("blink.user_timing", "firstPaint", TRACE_EVENT_SCOPE_PROCESS, "frame", frame());
}

void PaintTiming::setFirstContentfulPaint(double stamp)
{
    if (m_firstContentfulPaint != 0.0)
        return;
    setFirstPaint(stamp);
    m_firstContentfulPaint = stamp;
    TRACE_EVENT_INSTANT1("blink.user_timing", "firstContentfulPaint", TRACE_EVENT_SCOPE_PROCESS, "frame", frame());
}

} // namespace blink
