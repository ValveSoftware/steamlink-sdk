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

#ifndef RTCStatsRequestImpl_h
#define RTCStatsRequestImpl_h

#include "core/dom/ActiveDOMObject.h"
#include "modules/mediastream/RTCStatsResponse.h"
#include "platform/heap/Handle.h"
#include "platform/mediastream/RTCStatsRequest.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class MediaStreamTrack;
class RTCPeerConnection;
class RTCStatsCallback;

class RTCStatsRequestImpl FINAL : public RTCStatsRequest, public ActiveDOMObject {
public:
    static PassRefPtr<RTCStatsRequestImpl> create(ExecutionContext*, PassRefPtrWillBeRawPtr<RTCPeerConnection>, PassOwnPtr<RTCStatsCallback>, PassRefPtr<MediaStreamTrack>);
    virtual ~RTCStatsRequestImpl();

    virtual PassRefPtrWillBeRawPtr<RTCStatsResponseBase> createResponse() OVERRIDE;
    virtual bool hasSelector() OVERRIDE;
    virtual MediaStreamComponent* component() OVERRIDE;

    virtual void requestSucceeded(PassRefPtrWillBeRawPtr<RTCStatsResponseBase>) OVERRIDE;

    // ActiveDOMObject
    virtual void stop() OVERRIDE;

private:
    RTCStatsRequestImpl(ExecutionContext*, PassRefPtrWillBeRawPtr<RTCPeerConnection>, PassOwnPtr<RTCStatsCallback>, PassRefPtr<MediaStreamTrack>);

    void clear();

    OwnPtr<RTCStatsCallback> m_successCallback;
    RefPtr<MediaStreamComponent> m_component;

    RefPtrWillBePersistent<RTCPeerConnection> m_requester;
};

} // namespace WebCore

#endif // RTCStatsRequestImpl_h
