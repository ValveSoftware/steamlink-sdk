// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_certificate.h"

#include "base/memory/ptr_util.h"
#include "url/gurl.h"

namespace content {

RTCCertificate::RTCCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate)
    : certificate_(certificate) {
  DCHECK(certificate_);
}

RTCCertificate::~RTCCertificate() {
}

std::unique_ptr<blink::WebRTCCertificate> RTCCertificate::shallowCopy() const {
  return base::WrapUnique(new RTCCertificate(certificate_));
}

uint64_t RTCCertificate::expires() const {
  return certificate_->Expires();
}

blink::WebRTCCertificatePEM RTCCertificate::toPEM() const {
  rtc::RTCCertificatePEM pem = certificate_->ToPEM();
  return blink::WebRTCCertificatePEM(
      blink::WebString::fromUTF8(pem.private_key()),
          blink::WebString::fromUTF8(pem.certificate()));
}

bool RTCCertificate::equals(const blink::WebRTCCertificate& other) const {
  return *certificate_ ==
         *static_cast<const RTCCertificate&>(other).certificate_;
}

const rtc::scoped_refptr<rtc::RTCCertificate>&
RTCCertificate::rtcCertificate() const {
  return certificate_;
}

}  // namespace content
