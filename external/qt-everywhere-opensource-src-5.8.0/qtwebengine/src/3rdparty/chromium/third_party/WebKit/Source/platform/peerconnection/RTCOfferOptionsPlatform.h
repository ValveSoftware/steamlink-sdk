// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCOfferOptionsPlatform_h
#define RTCOfferOptionsPlatform_h

#include "platform/heap/Handle.h"

namespace blink {

class RTCOfferOptionsPlatform final : public GarbageCollected<RTCOfferOptionsPlatform> {
public:
    static RTCOfferOptionsPlatform* create(int32_t offerToReceiveVideo, int32_t offerToReceiveAudio, bool voiceActivityDetection, bool iceRestart)
    {
        return new RTCOfferOptionsPlatform(offerToReceiveVideo, offerToReceiveAudio, voiceActivityDetection, iceRestart);
    }

    int32_t offerToReceiveVideo() const { return m_offerToReceiveVideo; }
    int32_t offerToReceiveAudio() const { return m_offerToReceiveAudio; }
    bool voiceActivityDetection() const { return m_voiceActivityDetection; }
    bool iceRestart() const { return m_iceRestart; }

    DEFINE_INLINE_TRACE() {}

private:
    RTCOfferOptionsPlatform(int32_t offerToReceiveVideo, int32_t offerToReceiveAudio, bool voiceActivityDetection, bool iceRestart)
        : m_offerToReceiveVideo(offerToReceiveVideo)
        , m_offerToReceiveAudio(offerToReceiveAudio)
        , m_voiceActivityDetection(voiceActivityDetection)
        , m_iceRestart(iceRestart)
    {
    }

    int32_t m_offerToReceiveVideo;
    int32_t m_offerToReceiveAudio;
    bool m_voiceActivityDetection;
    bool m_iceRestart;
};

} // namespace blink

#endif // RTCOfferOptionsPlatform_h
