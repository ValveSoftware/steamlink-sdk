// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCOfferOptions_h
#define WebRTCOfferOptions_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"

namespace blink {

class RTCOfferOptionsPlatform;

class BLINK_PLATFORM_EXPORT WebRTCOfferOptions {
 public:
  WebRTCOfferOptions(int32_t offerToReceiveAudio,
                     int32_t offerToReceiveVideo,
                     bool voiceActivityDetection,
                     bool iceRestart);
  WebRTCOfferOptions(const WebRTCOfferOptions& other) { assign(other); }
  ~WebRTCOfferOptions() { reset(); }

  WebRTCOfferOptions& operator=(const WebRTCOfferOptions& other) {
    assign(other);
    return *this;
  }

  void assign(const WebRTCOfferOptions&);

  void reset();
  bool isNull() const { return m_private.isNull(); }

  int32_t offerToReceiveVideo() const;
  int32_t offerToReceiveAudio() const;
  bool voiceActivityDetection() const;
  bool iceRestart() const;

#if INSIDE_BLINK
  WebRTCOfferOptions(RTCOfferOptionsPlatform*);
#endif

 private:
  WebPrivatePtr<RTCOfferOptionsPlatform> m_private;
};

}  // namespace blink

#endif  // WebRTCOfferOptions_h
