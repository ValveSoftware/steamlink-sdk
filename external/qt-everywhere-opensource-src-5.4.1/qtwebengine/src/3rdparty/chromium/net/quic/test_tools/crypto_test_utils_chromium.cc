// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/crypto_test_utils.h"

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/test_data_directory.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_certificate.h"
#include "net/http/transport_security_state.h"
#include "net/quic/crypto/proof_source_chromium.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/test/cert_test_util.h"

namespace net {

namespace test {

class TestProofVerifierChromium : public ProofVerifierChromium {
 public:
  TestProofVerifierChromium(CertVerifier* cert_verifier,
                            TransportSecurityState* transport_security_state,
                            const std::string& cert_file)
      : ProofVerifierChromium(cert_verifier, transport_security_state),
        cert_verifier_(cert_verifier),
        transport_security_state_(transport_security_state) {
    // Load and install the root for the validated chain.
    scoped_refptr<X509Certificate> root_cert =
        ImportCertFromFile(GetTestCertsDirectory(), cert_file);
    scoped_root_.Reset(root_cert.get());
  }
  virtual ~TestProofVerifierChromium() {}

 private:
  ScopedTestRoot scoped_root_;
  scoped_ptr<CertVerifier> cert_verifier_;
  scoped_ptr<TransportSecurityState> transport_security_state_;
};

// static
ProofSource* CryptoTestUtils::ProofSourceForTesting() {
  return new ProofSourceChromium();
}

// static
ProofVerifier* CryptoTestUtils::ProofVerifierForTesting() {
  TestProofVerifierChromium* proof_verifier =
      new TestProofVerifierChromium(CertVerifier::CreateDefault(),
                                    new TransportSecurityState,
                                    "quic_root.crt");
  return proof_verifier;
}

// static
ProofVerifyContext* CryptoTestUtils::ProofVerifyContextForTesting() {
  return new ProofVerifyContextChromium(BoundNetLog());
}

}  // namespace test

}  // namespace net
