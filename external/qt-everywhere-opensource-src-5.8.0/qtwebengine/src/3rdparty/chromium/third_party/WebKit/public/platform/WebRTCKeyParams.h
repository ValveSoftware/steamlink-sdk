/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef WebRTCKeyParams_h
#define WebRTCKeyParams_h

#include "WebCommon.h"
#include "base/logging.h"

namespace blink {

// Corresponds to rtc::KeyType in WebRTC.
enum WebRTCKeyType { WebRTCKeyTypeRSA, WebRTCKeyTypeECDSA, WebRTCKeyTypeNull };

// Corresponds to rtc::RSAParams in WebRTC.
struct WebRTCRSAParams {
    unsigned modLength;
    unsigned pubExp;
};

// Corresponds to rtc::ECCurve in WebRTC.
enum WebRTCECCurve { WebRTCECCurveNistP256 };

// Corresponds to rtc::KeyParams in WebRTC.
class WebRTCKeyParams {
public:
    static WebRTCKeyParams createRSA(unsigned modLength, unsigned pubExp)
    {
        WebRTCKeyParams keyParams(WebRTCKeyTypeRSA);
        keyParams.m_params.rsa.modLength = modLength;
        keyParams.m_params.rsa.pubExp = pubExp;
        return keyParams;
    }
    static WebRTCKeyParams createECDSA(WebRTCECCurve curve)
    {
        WebRTCKeyParams keyParams(WebRTCKeyTypeECDSA);
        keyParams.m_params.ecCurve = curve;
        return keyParams;
    }

    WebRTCKeyParams() : WebRTCKeyParams(WebRTCKeyTypeNull) {}

    WebRTCKeyType keyType() const { return m_keyType; }
    WebRTCRSAParams rsaParams() const
    {
        DCHECK_EQ(m_keyType, WebRTCKeyTypeRSA);
        return m_params.rsa;
    }
    WebRTCECCurve ecCurve() const
    {
        DCHECK_EQ(m_keyType, WebRTCKeyTypeECDSA);
        return m_params.ecCurve;
    }

private:
    WebRTCKeyParams(WebRTCKeyType keyType) : m_keyType(keyType) {}

    WebRTCKeyType m_keyType;
    union {
        WebRTCRSAParams rsa;
        WebRTCECCurve ecCurve;
    } m_params;
};

} // namespace blink

#endif // WebRTCKeyParams_h
