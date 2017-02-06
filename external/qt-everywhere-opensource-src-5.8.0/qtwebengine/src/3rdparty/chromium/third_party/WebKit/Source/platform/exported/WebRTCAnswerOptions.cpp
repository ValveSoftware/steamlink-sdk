// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebRTCAnswerOptions.h"

#include "platform/peerconnection/RTCAnswerOptionsPlatform.h"

namespace blink {

WebRTCAnswerOptions::WebRTCAnswerOptions(RTCAnswerOptionsPlatform* options)
    : m_private(options)
{
}

void WebRTCAnswerOptions::assign(const WebRTCAnswerOptions& other)
{
    m_private = other.m_private;
}

void WebRTCAnswerOptions::reset()
{
    m_private.reset();
}

bool WebRTCAnswerOptions::voiceActivityDetection() const
{
    ASSERT(!m_private.isNull());
    return m_private->voiceActivityDetection();
}

} // namespace blink
