// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCVoidRequestPromiseImpl_h
#define RTCVoidRequestPromiseImpl_h

#include "platform/peerconnection/RTCVoidRequest.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptPromiseResolver;
class RTCPeerConnection;

class RTCVoidRequestPromiseImpl final : public RTCVoidRequest {
 public:
  static RTCVoidRequestPromiseImpl* create(RTCPeerConnection*,
                                           ScriptPromiseResolver*);
  ~RTCVoidRequestPromiseImpl() override;

  // RTCVoidRequest
  void requestSucceeded() override;
  void requestFailed(const String& error) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  RTCVoidRequestPromiseImpl(RTCPeerConnection*, ScriptPromiseResolver*);

  void clear();

  Member<RTCPeerConnection> m_requester;
  Member<ScriptPromiseResolver> m_resolver;
};

}  // namespace blink

#endif  // RTCVoidRequestPromiseImpl_h
