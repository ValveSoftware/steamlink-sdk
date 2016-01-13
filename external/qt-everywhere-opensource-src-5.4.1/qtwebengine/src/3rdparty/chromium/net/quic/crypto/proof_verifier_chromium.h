// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CRYPTO_PROOF_VERIFIER_CHROMIUM_H_
#define NET_QUIC_CRYPTO_PROOF_VERIFIER_CHROMIUM_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_export.h"
#include "net/base/net_log.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/quic/crypto/proof_verifier.h"

namespace net {

class CertVerifier;
class SingleRequestCertVerifier;
class TransportSecurityState;

// ProofVerifyDetailsChromium is the implementation-specific information that a
// ProofVerifierChromium returns about a certificate verification.
struct ProofVerifyDetailsChromium : public ProofVerifyDetails {
 public:
  CertVerifyResult cert_verify_result;

  // pinning_failure_log contains a message produced by
  // TransportSecurityState::DomainState::CheckPublicKeyPins in the event of a
  // pinning failure. It is a (somewhat) human-readable string.
  std::string pinning_failure_log;
};

// ProofVerifyContextChromium is the implementation-specific information that a
// ProofVerifierChromium needs in order to log correctly.
struct ProofVerifyContextChromium : public ProofVerifyContext {
 public:
  explicit ProofVerifyContextChromium(const BoundNetLog& net_log)
      : net_log(net_log) {}

  BoundNetLog net_log;
};

// ProofVerifierChromium implements the QUIC ProofVerifier interface.  It is
// capable of handling multiple simultaneous requests.
class NET_EXPORT_PRIVATE ProofVerifierChromium : public ProofVerifier {
 public:
  ProofVerifierChromium(CertVerifier* cert_verifier,
                        TransportSecurityState* transport_security_state);
  virtual ~ProofVerifierChromium();

  // ProofVerifier interface
  virtual QuicAsyncStatus VerifyProof(
      const std::string& hostname,
      const std::string& server_config,
      const std::vector<std::string>& certs,
      const std::string& signature,
      const ProofVerifyContext* verify_context,
      std::string* error_details,
      scoped_ptr<ProofVerifyDetails>* verify_details,
      ProofVerifierCallback* callback) OVERRIDE;

 private:
  class Job;

  void OnJobComplete(Job* job);

  // Set owning pointers to active jobs.
  typedef std::set<Job*> JobSet;
  JobSet active_jobs_;

  // Underlying verifier used to verify certificates.
  CertVerifier* const cert_verifier_;

  TransportSecurityState* transport_security_state_;

  DISALLOW_COPY_AND_ASSIGN(ProofVerifierChromium);
};

}  // namespace net

#endif  // NET_QUIC_CRYPTO_PROOF_VERIFIER_CHROMIUM_H_
