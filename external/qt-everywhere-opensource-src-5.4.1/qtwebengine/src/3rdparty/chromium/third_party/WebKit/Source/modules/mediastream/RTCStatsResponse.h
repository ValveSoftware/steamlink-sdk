/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RTCStatsResponse_h
#define RTCStatsResponse_h

#include "bindings/v8/ScriptWrappable.h"
#include "modules/mediastream/RTCStatsReport.h"
#include "platform/heap/Handle.h"
#include "platform/mediastream/RTCStatsResponseBase.h"
#include "wtf/HashMap.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class RTCStatsResponse FINAL : public RTCStatsResponseBase, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<RTCStatsResponse> create();

    const WillBeHeapVector<RefPtrWillBeMember<RTCStatsReport> >& result() const { return m_result; }

    PassRefPtrWillBeRawPtr<RTCStatsReport> namedItem(const AtomicString& name);

    virtual size_t addReport(String id, String type, double timestamp) OVERRIDE;
    virtual void addStatistic(size_t report, String name, String value) OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

private:
    RTCStatsResponse();
    WillBeHeapVector<RefPtrWillBeMember<RTCStatsReport> > m_result;
    HashMap<String, int> m_idmap;
};

} // namespace WebCore

#endif // RTCStatsResponse_h
