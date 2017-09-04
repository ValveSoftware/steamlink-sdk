// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InternalsRTCCertificate_h
#define InternalsRTCCertificate_h

#include "modules/peerconnection/RTCCertificate.h"
#include "wtf/Allocator.h"

namespace blink {

class Internals;

class InternalsRTCCertificate {
  STATIC_ONLY(InternalsRTCCertificate);

 public:
  static bool rtcCertificateEquals(Internals&,
                                   RTCCertificate*,
                                   RTCCertificate*);
};

}  // blink

#endif  // InternalsRTCCertificate_h
