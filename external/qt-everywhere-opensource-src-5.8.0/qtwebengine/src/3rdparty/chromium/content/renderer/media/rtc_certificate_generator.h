// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_CERTIFICATE_GENERATOR_H_
#define CONTENT_RENDERER_MEDIA_RTC_CERTIFICATE_GENERATOR_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebRTCCertificate.h"
#include "third_party/WebKit/public/platform/WebRTCCertificateGenerator.h"
#include "third_party/WebKit/public/platform/WebRTCKeyParams.h"
#include "third_party/webrtc/base/optional.h"

namespace content {

// Chromium's WebRTCCertificateGenerator implementation; uses the
// PeerConnectionIdentityStore/SSLIdentity::Generate to generate the identity,
// rtc::RTCCertificate and content::RTCCertificate.
class RTCCertificateGenerator : public blink::WebRTCCertificateGenerator {
 public:
  RTCCertificateGenerator() {}
  ~RTCCertificateGenerator() override {}

  // blink::WebRTCCertificateGenerator implementation.
  void generateCertificate(
      const blink::WebRTCKeyParams& key_params,
      std::unique_ptr<blink::WebRTCCertificateCallback> observer) override;
  void generateCertificateWithExpiration(
      const blink::WebRTCKeyParams& key_params,
      uint64_t expires_ms,
      std::unique_ptr<blink::WebRTCCertificateCallback> observer) override;
  bool isSupportedKeyParams(const blink::WebRTCKeyParams& key_params) override;
  std::unique_ptr<blink::WebRTCCertificate> fromPEM(
      const std::string& pem_private_key,
      const std::string& pem_certificate) override;

 private:
  void generateCertificateWithOptionalExpiration(
      const blink::WebRTCKeyParams& key_params,
      const rtc::Optional<uint64_t>& expires_ms,
      std::unique_ptr<blink::WebRTCCertificateCallback> observer);

  DISALLOW_COPY_AND_ASSIGN(RTCCertificateGenerator);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_CERTIFICATE_GENERATOR_H_
