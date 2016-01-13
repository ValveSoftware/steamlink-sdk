/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef RTCSessionDescriptionRequestImpl_h
#define RTCSessionDescriptionRequestImpl_h

#include "core/dom/ActiveDOMObject.h"
#include "platform/heap/Handle.h"
#include "platform/mediastream/RTCSessionDescriptionRequest.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"

namespace blink {
class WebRTCSessionDescription;
}

namespace WebCore {

class RTCErrorCallback;
class RTCPeerConnection;
class RTCSessionDescriptionCallback;

class RTCSessionDescriptionRequestImpl FINAL : public RTCSessionDescriptionRequest, public ActiveDOMObject {
public:
    static PassRefPtr<RTCSessionDescriptionRequestImpl> create(ExecutionContext*, PassRefPtrWillBeRawPtr<RTCPeerConnection>, PassOwnPtr<RTCSessionDescriptionCallback>, PassOwnPtr<RTCErrorCallback>);
    virtual ~RTCSessionDescriptionRequestImpl();

    virtual void requestSucceeded(const blink::WebRTCSessionDescription&) OVERRIDE;
    virtual void requestFailed(const String& error) OVERRIDE;

    // ActiveDOMObject
    virtual void stop() OVERRIDE;

private:
    RTCSessionDescriptionRequestImpl(ExecutionContext*, PassRefPtrWillBeRawPtr<RTCPeerConnection>, PassOwnPtr<RTCSessionDescriptionCallback>, PassOwnPtr<RTCErrorCallback>);

    void clear();

    OwnPtr<RTCSessionDescriptionCallback> m_successCallback;
    OwnPtr<RTCErrorCallback> m_errorCallback;

    RefPtrWillBePersistent<RTCPeerConnection> m_requester;
};

} // namespace WebCore

#endif // RTCSessionDescriptionRequestImpl_h
