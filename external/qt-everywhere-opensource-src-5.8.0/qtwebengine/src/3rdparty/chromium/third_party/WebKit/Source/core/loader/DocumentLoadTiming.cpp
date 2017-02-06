/*
 * Copyright (C) 2011 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/loader/DocumentLoadTiming.h"

#include "core/loader/DocumentLoader.h"
#include "platform/TraceEvent.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/RefPtr.h"

namespace blink {

DocumentLoadTiming::DocumentLoadTiming(DocumentLoader& documentLoader)
    : m_referenceMonotonicTime(0.0)
    , m_referenceWallTime(0.0)
    , m_navigationStart(0.0)
    , m_unloadEventStart(0.0)
    , m_unloadEventEnd(0.0)
    , m_redirectStart(0.0)
    , m_redirectEnd(0.0)
    , m_redirectCount(0)
    , m_fetchStart(0.0)
    , m_responseEnd(0.0)
    , m_loadEventStart(0.0)
    , m_loadEventEnd(0.0)
    , m_hasCrossOriginRedirect(false)
    , m_hasSameOriginAsPreviousDocument(false)
    , m_documentLoader(documentLoader)
{
}

DEFINE_TRACE(DocumentLoadTiming)
{
    visitor->trace(m_documentLoader);
}

// TODO(csharrison): Remove the null checking logic in a later patch.
LocalFrame* DocumentLoadTiming::frame() const
{
    return m_documentLoader ? m_documentLoader->frame() : nullptr;
}

void DocumentLoadTiming::notifyDocumentTimingChanged()
{
    if (m_documentLoader)
        m_documentLoader->didChangePerformanceTiming();
}

void DocumentLoadTiming::ensureReferenceTimesSet()
{
    if (!m_referenceWallTime)
        m_referenceWallTime = currentTime();
    if (!m_referenceMonotonicTime)
        m_referenceMonotonicTime = monotonicallyIncreasingTime();
}

double DocumentLoadTiming::monotonicTimeToZeroBasedDocumentTime(double monotonicTime) const
{
    if (!monotonicTime || !m_referenceMonotonicTime)
        return 0.0;
    return monotonicTime - m_referenceMonotonicTime;
}

double DocumentLoadTiming::monotonicTimeToPseudoWallTime(double monotonicTime) const
{
    if (!monotonicTime || !m_referenceMonotonicTime)
        return 0.0;
    return m_referenceWallTime + monotonicTime - m_referenceMonotonicTime;
}

double DocumentLoadTiming::pseudoWallTimeToMonotonicTime(double pseudoWallTime) const
{
    if (!pseudoWallTime)
        return 0.0;
    return m_referenceMonotonicTime + pseudoWallTime - m_referenceWallTime;
}

void DocumentLoadTiming::markNavigationStart()
{
    // Allow the embedder to override navigationStart before we record it if
    // they have a more accurate timestamp.
    if (m_navigationStart) {
        ASSERT(m_referenceMonotonicTime && m_referenceWallTime);
        return;
    }
    ASSERT(!m_navigationStart && !m_referenceMonotonicTime && !m_referenceWallTime);
    ensureReferenceTimesSet();
    m_navigationStart = m_referenceMonotonicTime;
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "navigationStart", m_navigationStart, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentLoadTiming::setNavigationStart(double navigationStart)
{
    // |m_referenceMonotonicTime| and |m_referenceWallTime| represent
    // navigationStart. We must set these to the current time if they haven't
    // been set yet in order to have a valid reference time in both units.
    ensureReferenceTimesSet();
    m_navigationStart = navigationStart;
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "navigationStart", m_navigationStart, "frame", frame());

    // The reference times are adjusted based on the embedder's navigationStart.
    ASSERT(m_referenceMonotonicTime && m_referenceWallTime);
    m_referenceWallTime = monotonicTimeToPseudoWallTime(navigationStart);
    m_referenceMonotonicTime = navigationStart;
    notifyDocumentTimingChanged();
}

void DocumentLoadTiming::addRedirect(const KURL& redirectingUrl, const KURL& redirectedUrl)
{
    m_redirectCount++;
    if (!m_redirectStart) {
        setRedirectStart(m_fetchStart);
    }
    markRedirectEnd();
    markFetchStart();

    // Check if the redirected url is allowed to access the redirecting url's timing information.
    RefPtr<SecurityOrigin> redirectedSecurityOrigin = SecurityOrigin::create(redirectedUrl);
    m_hasCrossOriginRedirect |= !redirectedSecurityOrigin->canRequest(redirectingUrl);
}

void DocumentLoadTiming::markUnloadEventStart()
{
    m_unloadEventStart = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "unloadEventStart", m_unloadEventStart, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentLoadTiming::markUnloadEventEnd()
{
    m_unloadEventEnd = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "unloadEventEnd", m_unloadEventEnd, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentLoadTiming::markFetchStart()
{
    m_fetchStart = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "fetchStart", m_fetchStart, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentLoadTiming::setResponseEnd(double responseEnd)
{
    m_responseEnd = responseEnd;
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "responseEnd", m_responseEnd, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentLoadTiming::markLoadEventStart()
{
    m_loadEventStart = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "loadEventStart", m_loadEventStart, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentLoadTiming::markLoadEventEnd()
{
    m_loadEventEnd = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "loadEventEnd", m_loadEventEnd, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentLoadTiming::setRedirectStart(double redirectStart)
{
    m_redirectStart = redirectStart;
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "redirectStart", m_redirectStart, "frame", frame());
    notifyDocumentTimingChanged();
}

void DocumentLoadTiming::markRedirectEnd()
{
    m_redirectEnd = monotonicallyIncreasingTime();
    TRACE_EVENT_MARK_WITH_TIMESTAMP1("blink.user_timing", "redirectEnd", m_redirectEnd, "frame", frame());
    notifyDocumentTimingChanged();
}

} // namespace blink
