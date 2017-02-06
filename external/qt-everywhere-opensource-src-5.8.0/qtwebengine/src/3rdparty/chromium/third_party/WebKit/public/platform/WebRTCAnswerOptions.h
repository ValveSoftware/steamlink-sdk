// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCAnswerOptions_h
#define WebRTCAnswerOptions_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"

namespace blink {

class RTCAnswerOptionsPlatform;

class BLINK_PLATFORM_EXPORT WebRTCAnswerOptions {
public:
    WebRTCAnswerOptions(const WebRTCAnswerOptions& other) { assign(other); }
    ~WebRTCAnswerOptions() { reset(); }

    WebRTCAnswerOptions& operator=(const WebRTCAnswerOptions& other)
    {
        assign(other);
        return *this;
    }

    void assign(const WebRTCAnswerOptions&);

    void reset();
    bool isNull() const { return m_private.isNull(); }

    bool voiceActivityDetection() const;

#if INSIDE_BLINK
    WebRTCAnswerOptions(RTCAnswerOptionsPlatform*);
#endif

private:
    WebPrivatePtr<RTCAnswerOptionsPlatform> m_private;
};

} // namespace blink

#endif // WebRTCAnswerOptions_h
