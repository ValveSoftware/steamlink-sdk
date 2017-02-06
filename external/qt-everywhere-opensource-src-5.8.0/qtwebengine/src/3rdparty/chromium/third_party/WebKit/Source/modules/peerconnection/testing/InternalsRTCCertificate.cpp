// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/testing/InternalsRTCCertificate.h"

namespace blink {

bool InternalsRTCCertificate::rtcCertificateEquals(Internals& internals, RTCCertificate* a, RTCCertificate* b)
{
    return a->certificate().equals(b->certificate());
}

} // namespace blink
