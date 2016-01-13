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

#ifndef PerformanceResourceTiming_h
#define PerformanceResourceTiming_h

#include "core/timing/PerformanceEntry.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class Document;
class KURL;
class ResourceLoadTiming;
class ResourceRequest;
class ResourceResponse;
class ResourceTimingInfo;

class PerformanceResourceTiming FINAL : public PerformanceEntry {
public:
    static PassRefPtrWillBeRawPtr<PerformanceResourceTiming> create(const ResourceTimingInfo& info, Document* requestingDocument, double startTime, double lastRedirectEndTime, bool m_allowTimingDetails, bool m_allowRedirectDetails)
    {
        return adoptRefWillBeNoop(new PerformanceResourceTiming(info, requestingDocument, startTime, lastRedirectEndTime, m_allowTimingDetails, m_allowRedirectDetails));
    }

    static PassRefPtrWillBeRawPtr<PerformanceResourceTiming> create(const ResourceTimingInfo& info, Document* requestingDocument, double startTime, bool m_allowTimingDetails)
    {
        return adoptRefWillBeNoop(new PerformanceResourceTiming(info, requestingDocument, startTime, 0.0, m_allowTimingDetails, false));
    }

    AtomicString initiatorType() const;

    double redirectStart() const;
    double redirectEnd() const;
    double fetchStart() const;
    double domainLookupStart() const;
    double domainLookupEnd() const;
    double connectStart() const;
    double connectEnd() const;
    double secureConnectionStart() const;
    double requestStart() const;
    double responseStart() const;
    double responseEnd() const;

    virtual bool isResource() OVERRIDE { return true; }

    virtual void trace(Visitor*) OVERRIDE;

private:
    PerformanceResourceTiming(const ResourceTimingInfo&, Document* requestingDocument, double startTime, double lastRedirectEndTime, bool m_allowTimingDetails, bool m_allowRedirectDetails);
    virtual ~PerformanceResourceTiming();

    AtomicString m_initiatorType;
    RefPtr<ResourceLoadTiming> m_timing;
    double m_lastRedirectEndTime;
    double m_finishTime;
    bool m_didReuseConnection;
    bool m_allowTimingDetails;
    bool m_allowRedirectDetails;
    RefPtrWillBeMember<Document> m_requestingDocument;
};

}

#endif // !defined(PerformanceResourceTiming_h)
