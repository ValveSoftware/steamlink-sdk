// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRTCCertificate_h
#define WebRTCCertificate_h

#include "public/platform/WebRTCKeyParams.h"

#include <memory>

namespace blink {

// Corresponds to |rtc::RTCCertificatePEM| in WebRTC.
// See |WebRTCCertificate::toPEM| and |WebRTCCertificateGenerator::fromPEM|.
class WebRTCCertificatePEM {
public:
    WebRTCCertificatePEM(std::string privateKey, std::string certificate)
        : m_privateKey(privateKey), m_certificate(certificate)
    {
    }

    const std::string& privateKey() const
    {
        return m_privateKey;
    }
    const std::string& certificate() const
    {
        return m_certificate;
    }

private:
    std::string m_privateKey;
    std::string m_certificate;
};

// WebRTCCertificate is an interface defining what Blink needs to know about certificates,
// hiding Chromium and WebRTC layer implementation details. It is possible to create
// shallow copies of the WebRTCCertificate. When all copies are destroyed, the
// implementation specific data must be freed. WebRTCCertificate objects thus act as
// references to the reference counted internal data.
class WebRTCCertificate {
public:
    WebRTCCertificate() = default;
    virtual ~WebRTCCertificate() = default;

    // Copies the WebRTCCertificate object without copying the underlying implementation
    // specific (WebRTC layer) certificate. When all copies are destroyed the underlying
    // data is freed.
    virtual std::unique_ptr<WebRTCCertificate> shallowCopy() const = 0;

    // Returns the expiration time in ms relative to epoch, 1970-01-01T00:00:00Z.
    virtual uint64_t expires() const = 0;
    // Creates a PEM strings representation of the certificate. See also
    // |WebRTCCertificateGenerator::fromPEM|.
    virtual WebRTCCertificatePEM toPEM() const = 0;
    // Checks if the two certificate objects represent the same certificate value,
    // as should be the case for a clone and the original.
    virtual bool equals(const WebRTCCertificate& other) const = 0;

private:
    WebRTCCertificate(const WebRTCCertificate&) = delete;
    WebRTCCertificate& operator=(const WebRTCCertificate&) = delete;
};

} // namespace blink

#endif // WebRTCCertificate_h
