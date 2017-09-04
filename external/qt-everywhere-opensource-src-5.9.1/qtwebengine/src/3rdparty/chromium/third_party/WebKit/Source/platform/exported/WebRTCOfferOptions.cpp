// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebRTCOfferOptions.h"

#include "platform/peerconnection/RTCOfferOptionsPlatform.h"

namespace blink {

WebRTCOfferOptions::WebRTCOfferOptions(RTCOfferOptionsPlatform* options)
    : m_private(options) {}

WebRTCOfferOptions::WebRTCOfferOptions(int32_t offerToReceiveAudio,
                                       int32_t offerToReceiveVideo,
                                       bool voiceActivityDetection,
                                       bool iceRestart)
    : m_private(RTCOfferOptionsPlatform::create(offerToReceiveAudio,
                                                offerToReceiveVideo,
                                                voiceActivityDetection,
                                                iceRestart)) {}

void WebRTCOfferOptions::assign(const WebRTCOfferOptions& other) {
  m_private = other.m_private;
}

void WebRTCOfferOptions::reset() {
  m_private.reset();
}

int32_t WebRTCOfferOptions::offerToReceiveVideo() const {
  ASSERT(!isNull());
  return m_private->offerToReceiveVideo();
}

int32_t WebRTCOfferOptions::offerToReceiveAudio() const {
  ASSERT(!isNull());
  return m_private->offerToReceiveAudio();
}

bool WebRTCOfferOptions::voiceActivityDetection() const {
  ASSERT(!isNull());
  return m_private->voiceActivityDetection();
}

bool WebRTCOfferOptions::iceRestart() const {
  ASSERT(!isNull());
  return m_private->iceRestart();
}

}  // namespace blink
