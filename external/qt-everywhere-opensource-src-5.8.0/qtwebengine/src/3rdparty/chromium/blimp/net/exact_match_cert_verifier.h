// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_EXACT_MATCH_CERT_VERIFIER_H_
#define BLIMP_NET_EXACT_MATCH_CERT_VERIFIER_H_

#include <string>
#include <vector>

#include "blimp/net/blimp_net_export.h"
#include "net/cert/cert_verifier.h"

namespace net {
class HashValue;
}  // namespace net

namespace blimp {

// Checks if the peer certificate is an exact match to the X.509 certificate
// |engine_cert|, which is specified at class construction time.
class BLIMP_NET_EXPORT ExactMatchCertVerifier : public net::CertVerifier {
 public:
  // |engine_cert|: The single allowable certificate.
  explicit ExactMatchCertVerifier(
      scoped_refptr<net::X509Certificate> engine_cert);

  ~ExactMatchCertVerifier() override;

  // net::CertVerifier implementation.
  int Verify(const RequestParams& params,
             net::CRLSet* crl_set,
             net::CertVerifyResult* verify_result,
             const net::CompletionCallback& callback,
             std::unique_ptr<Request>* out_req,
             const net::BoundNetLog& net_log) override;

 private:
  scoped_refptr<net::X509Certificate> engine_cert_;
  std::vector<net::HashValue> engine_cert_hashes_;

  DISALLOW_COPY_AND_ASSIGN(ExactMatchCertVerifier);
};

}  // namespace blimp

#endif  // BLIMP_NET_EXACT_MATCH_CERT_VERIFIER_H_
