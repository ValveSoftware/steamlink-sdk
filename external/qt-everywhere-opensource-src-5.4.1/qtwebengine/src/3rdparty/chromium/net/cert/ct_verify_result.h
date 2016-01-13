// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_VERIFY_RESULT_H_
#define NET_CERT_CT_VERIFY_RESULT_H_

#include <vector>

#include "net/cert/signed_certificate_timestamp.h"

namespace net {

namespace ct {

typedef std::vector<scoped_refptr<SignedCertificateTimestamp> > SCTList;

// Holds Signed Certificate Timestamps, depending on their verification results.
// More information could be tracked here about SCTs, but for the current UI
// this categorization is enough.
struct NET_EXPORT CTVerifyResult {
  CTVerifyResult();
  ~CTVerifyResult();

  // SCTs from known logs where the signature verified correctly.
  SCTList verified_scts;
  // SCTs from known logs where the signature failed to verify.
  SCTList invalid_scts;
  // SCTs from unknown logs and as such are unverifiable.
  SCTList unknown_logs_scts;
};

}  // namespace ct

}  // namespace net

#endif  // NET_CERT_CT_VERIFY_RESULT_H_
