/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "core/timing/PerformanceResourceTiming.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/timing/ResourceTimingInfo.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"

namespace WebCore {

static double monotonicTimeToDocumentMilliseconds(Document* document, double seconds)
{
    ASSERT(seconds >= 0.0);
    return document->loader()->timing()->monotonicTimeToZeroBasedDocumentTime(seconds) * 1000.0;
}

PerformanceResourceTiming::PerformanceResourceTiming(const ResourceTimingInfo& info, Document* requestingDocument, double startTime, double lastRedirectEndTime, bool allowTimingDetails, bool allowRedirectDetails)
    : PerformanceEntry(info.initialRequest().url().string(), "resource", monotonicTimeToDocumentMilliseconds(requestingDocument, startTime), monotonicTimeToDocumentMilliseconds(requestingDocument, info.loadFinishTime()))
    , m_initiatorType(info.initiatorType())
    , m_timing(info.finalResponse().resourceLoadTiming())
    , m_lastRedirectEndTime(lastRedirectEndTime)
    , m_finishTime(info.loadFinishTime())
    , m_didReuseConnection(info.finalResponse().connectionReused())
    , m_allowTimingDetails(allowTimingDetails)
    , m_allowRedirectDetails(allowRedirectDetails)
    , m_requestingDocument(requestingDocument)
{
    ScriptWrappable::init(this);
}

PerformanceResourceTiming::~PerformanceResourceTiming()
{
}

AtomicString PerformanceResourceTiming::initiatorType() const
{
    return m_initiatorType;
}

double PerformanceResourceTiming::redirectStart() const
{
    if (!m_lastRedirectEndTime || !m_allowRedirectDetails)
        return 0.0;

    return PerformanceEntry::startTime();
}

double PerformanceResourceTiming::redirectEnd() const
{
    if (!m_lastRedirectEndTime || !m_allowRedirectDetails)
        return 0.0;

    return monotonicTimeToDocumentMilliseconds(m_requestingDocument.get(), m_lastRedirectEndTime);
}

double PerformanceResourceTiming::fetchStart() const
{
    if (m_lastRedirectEndTime) {
        // FIXME: ASSERT(m_timing) should be in constructor once timeticks of
        // AppCache is exposed from chrome network stack, crbug/251100
        ASSERT(m_timing);
        return monotonicTimeToDocumentMilliseconds(m_requestingDocument.get(), m_timing->requestTime);
    }

    return PerformanceEntry::startTime();
}

double PerformanceResourceTiming::domainLookupStart() const
{
    if (!m_allowTimingDetails)
        return 0.0;

    if (!m_timing || m_timing->dnsStart == 0.0)
        return fetchStart();

    return monotonicTimeToDocumentMilliseconds(m_requestingDocument.get(), m_timing->dnsStart);
}

double PerformanceResourceTiming::domainLookupEnd() const
{
    if (!m_allowTimingDetails)
        return 0.0;

    if (!m_timing || m_timing->dnsEnd == 0.0)
        return domainLookupStart();

    return monotonicTimeToDocumentMilliseconds(m_requestingDocument.get(), m_timing->dnsEnd);
}

double PerformanceResourceTiming::connectStart() const
{
    if (!m_allowTimingDetails)
        return 0.0;

    // connectStart will be zero when a network request is not made.
    if (!m_timing || m_timing->connectStart == 0.0 || m_didReuseConnection)
        return domainLookupEnd();

    // connectStart includes any DNS time, so we may need to trim that off.
    double connectStart = m_timing->connectStart;
    if (m_timing->dnsEnd > 0.0)
        connectStart = m_timing->dnsEnd;

    return monotonicTimeToDocumentMilliseconds(m_requestingDocument.get(), connectStart);
}

double PerformanceResourceTiming::connectEnd() const
{
    if (!m_allowTimingDetails)
        return 0.0;

    // connectStart will be zero when a network request is not made.
    if (!m_timing || m_timing->connectEnd == 0.0 || m_didReuseConnection)
        return connectStart();

    return monotonicTimeToDocumentMilliseconds(m_requestingDocument.get(), m_timing->connectEnd);
}

double PerformanceResourceTiming::secureConnectionStart() const
{
    if (!m_allowTimingDetails)
        return 0.0;

    if (!m_timing || m_timing->sslStart == 0.0) // Secure connection not negotiated.
        return 0.0;

    return monotonicTimeToDocumentMilliseconds(m_requestingDocument.get(), m_timing->sslStart);
}

double PerformanceResourceTiming::requestStart() const
{
    if (!m_allowTimingDetails)
        return 0.0;

    if (!m_timing)
        return connectEnd();

    return monotonicTimeToDocumentMilliseconds(m_requestingDocument.get(), m_timing->sendStart);
}

double PerformanceResourceTiming::responseStart() const
{
    if (!m_allowTimingDetails)
        return 0.0;

    if (!m_timing)
        return requestStart();

    // FIXME: This number isn't exactly correct. See the notes in PerformanceTiming::responseStart().
    return monotonicTimeToDocumentMilliseconds(m_requestingDocument.get(), m_timing->receiveHeadersEnd);
}

double PerformanceResourceTiming::responseEnd() const
{
    if (!m_finishTime)
        return responseStart();

    return monotonicTimeToDocumentMilliseconds(m_requestingDocument.get(), m_finishTime);
}

void PerformanceResourceTiming::trace(Visitor* visitor)
{
    visitor->trace(m_requestingDocument);
    PerformanceEntry::trace(visitor);
}

} // namespace WebCore
