// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCSessionDescriptionRequestPromiseImpl_h
#define RTCSessionDescriptionRequestPromiseImpl_h

#include "platform/peerconnection/RTCSessionDescriptionRequest.h"
#include "wtf/text/WTFString.h"

namespace blink {

class RTCPeerConnection;
class ScriptPromiseResolver;
class WebRTCSessionDescription;

class RTCSessionDescriptionRequestPromiseImpl final : public RTCSessionDescriptionRequest {
public:
    static RTCSessionDescriptionRequestPromiseImpl* create(RTCPeerConnection*, ScriptPromiseResolver*);
    ~RTCSessionDescriptionRequestPromiseImpl() override;

    // RTCSessionDescriptionRequest
    void requestSucceeded(const WebRTCSessionDescription&) override;
    void requestFailed(const String& error) override;

    DECLARE_VIRTUAL_TRACE();

private:
    RTCSessionDescriptionRequestPromiseImpl(RTCPeerConnection*, ScriptPromiseResolver*);

    void clear();

    Member<RTCPeerConnection> m_requester;
    Member<ScriptPromiseResolver> m_resolver;
};

} // namespace blink

#endif // RTCSessionDescriptionRequestPromiseImpl_h
