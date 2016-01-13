// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CT_VERIFIER_H_
#define NET_CERT_CT_VERIFIER_H_

#include "net/base/net_export.h"

namespace net {

namespace ct {
struct CTVerifyResult;
}  // namespace ct

class BoundNetLog;
class X509Certificate;

// Interface for verifying Signed Certificate Timestamps over a certificate.
class NET_EXPORT CTVerifier {
 public:
  virtual ~CTVerifier() {}

  // Verifies SCTs embedded in the certificate itself, SCTs embedded in a
  // stapled OCSP response, and SCTs obtained via the
  // signed_certificate_timestamp TLS extension on the given |cert|.
  // A certificate is permitted but not required to use multiple sources for
  // SCTs. It is expected that most certificates will use only one source
  // (embedding, TLS extension or OCSP stapling). If no stapled OCSP response
  // is available, |stapled_ocsp_response| should be an empty string. If no SCT
  // TLS extension was negotiated, |sct_list_from_tls_extension| should be an
  // empty string. |result| will be filled with the SCTs present, divided into
  // categories based on the verification result.
  virtual int Verify(X509Certificate* cert,
                     const std::string& stapled_ocsp_response,
                     const std::string& sct_list_from_tls_extension,
                     ct::CTVerifyResult* result,
                     const BoundNetLog& net_log) = 0;
};

}  // namespace net

#endif  // NET_CERT_CT_VERIFIER_H_
