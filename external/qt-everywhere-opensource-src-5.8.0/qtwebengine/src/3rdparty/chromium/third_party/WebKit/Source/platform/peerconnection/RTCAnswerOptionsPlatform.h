// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCAnswerOptionsPlatform_h
#define RTCAnswerOptionsPlatform_h

#include "platform/heap/Handle.h"

namespace blink {

class RTCAnswerOptionsPlatform final : public GarbageCollected<RTCAnswerOptionsPlatform> {
public:
    static RTCAnswerOptionsPlatform* create(bool voiceActivityDetection)
    {
        return new RTCAnswerOptionsPlatform(voiceActivityDetection);
    }

    bool voiceActivityDetection() const { return m_voiceActivityDetection; }

    DEFINE_INLINE_TRACE() {}

private:
    explicit RTCAnswerOptionsPlatform(bool voiceActivityDetection)
        : m_voiceActivityDetection(voiceActivityDetection)
    {
    }

    bool m_voiceActivityDetection;
};

} // namespace blink

#endif // RTCAnswerOptionsPlatform_h
