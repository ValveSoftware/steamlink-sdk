// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/proof_verifier_chromium.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "crypto/signature_verifier.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/single_request_cert_verifier.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/http/transport_security_state.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/ssl/ssl_config_service.h"

using base::StringPiece;
using base::StringPrintf;
using std::string;
using std::vector;

namespace net {

// A Job handles the verification of a single proof.  It is owned by the
// ProofVerifier. If the verification can not complete synchronously, it
// will notify the ProofVerifier upon completion.
class ProofVerifierChromium::Job {
 public:
  Job(ProofVerifierChromium* proof_verifier,
      CertVerifier* cert_verifier,
      TransportSecurityState* transport_security_state,
      const BoundNetLog& net_log);

  // Starts the proof verification.  If |QUIC_PENDING| is returned, then
  // |callback| will be invoked asynchronously when the verification completes.
  QuicAsyncStatus VerifyProof(const std::string& hostname,
                              const std::string& server_config,
                              const std::vector<std::string>& certs,
                              const std::string& signature,
                              std::string* error_details,
                              scoped_ptr<ProofVerifyDetails>* verify_details,
                              ProofVerifierCallback* callback);

 private:
  enum State {
    STATE_NONE,
    STATE_VERIFY_CERT,
    STATE_VERIFY_CERT_COMPLETE,
  };

  int DoLoop(int last_io_result);
  void OnIOComplete(int result);
  int DoVerifyCert(int result);
  int DoVerifyCertComplete(int result);

  bool VerifySignature(const std::string& signed_data,
                       const std::string& signature,
                       const std::string& cert);

  // Proof verifier to notify when this jobs completes.
  ProofVerifierChromium* proof_verifier_;

  // The underlying verifier used for verifying certificates.
  scoped_ptr<SingleRequestCertVerifier> verifier_;

  TransportSecurityState* transport_security_state_;

  // |hostname| specifies the hostname for which |certs| is a valid chain.
  std::string hostname_;

  scoped_ptr<ProofVerifierCallback> callback_;
  scoped_ptr<ProofVerifyDetailsChromium> verify_details_;
  std::string error_details_;

  // X509Certificate from a chain of DER encoded certificates.
  scoped_refptr<X509Certificate> cert_;

  State next_state_;

  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(Job);
};

ProofVerifierChromium::Job::Job(
    ProofVerifierChromium* proof_verifier,
    CertVerifier* cert_verifier,
    TransportSecurityState* transport_security_state,
    const BoundNetLog& net_log)
    : proof_verifier_(proof_verifier),
      verifier_(new SingleRequestCertVerifier(cert_verifier)),
      transport_security_state_(transport_security_state),
      next_state_(STATE_NONE),
      net_log_(net_log) {
}

QuicAsyncStatus ProofVerifierChromium::Job::VerifyProof(
    const string& hostname,
    const string& server_config,
    const vector<string>& certs,
    const string& signature,
    std::string* error_details,
    scoped_ptr<ProofVerifyDetails>* verify_details,
    ProofVerifierCallback* callback) {
  DCHECK(error_details);
  DCHECK(verify_details);
  DCHECK(callback);

  error_details->clear();

  if (STATE_NONE != next_state_) {
    *error_details = "Certificate is already set and VerifyProof has begun";
    DLOG(DFATAL) << *error_details;
    return QUIC_FAILURE;
  }

  verify_details_.reset(new ProofVerifyDetailsChromium);

  if (certs.empty()) {
    *error_details = "Failed to create certificate chain. Certs are empty.";
    DLOG(WARNING) << *error_details;
    verify_details_->cert_verify_result.cert_status = CERT_STATUS_INVALID;
    verify_details->reset(verify_details_.release());
    return QUIC_FAILURE;
  }

  // Convert certs to X509Certificate.
  vector<StringPiece> cert_pieces(certs.size());
  for (unsigned i = 0; i < certs.size(); i++) {
    cert_pieces[i] = base::StringPiece(certs[i]);
  }
  cert_ = X509Certificate::CreateFromDERCertChain(cert_pieces);
  if (!cert_.get()) {
    *error_details = "Failed to create certificate chain";
    DLOG(WARNING) << *error_details;
    verify_details_->cert_verify_result.cert_status = CERT_STATUS_INVALID;
    verify_details->reset(verify_details_.release());
    return QUIC_FAILURE;
  }

  // We call VerifySignature first to avoid copying of server_config and
  // signature.
  if (!VerifySignature(server_config, signature, certs[0])) {
    *error_details = "Failed to verify signature of server config";
    DLOG(WARNING) << *error_details;
    verify_details_->cert_verify_result.cert_status = CERT_STATUS_INVALID;
    verify_details->reset(verify_details_.release());
    return QUIC_FAILURE;
  }

  hostname_ = hostname;

  next_state_ = STATE_VERIFY_CERT;
  switch (DoLoop(OK)) {
    case OK:
      verify_details->reset(verify_details_.release());
      return QUIC_SUCCESS;
    case ERR_IO_PENDING:
      callback_.reset(callback);
      return QUIC_PENDING;
    default:
      *error_details = error_details_;
      verify_details->reset(verify_details_.release());
      return QUIC_FAILURE;
  }
}

int ProofVerifierChromium::Job::DoLoop(int last_result) {
  int rv = last_result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_VERIFY_CERT:
        DCHECK(rv == OK);
        rv = DoVerifyCert(rv);
        break;
      case STATE_VERIFY_CERT_COMPLETE:
        rv = DoVerifyCertComplete(rv);
        break;
      case STATE_NONE:
      default:
        rv = ERR_UNEXPECTED;
        LOG(DFATAL) << "unexpected state " << state;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

void ProofVerifierChromium::Job::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    scoped_ptr<ProofVerifierCallback> callback(callback_.release());
    // Callback expects ProofVerifyDetails not ProofVerifyDetailsChromium.
    scoped_ptr<ProofVerifyDetails> verify_details(verify_details_.release());
    callback->Run(rv == OK, error_details_, &verify_details);
    // Will delete |this|.
    proof_verifier_->OnJobComplete(this);
  }
}

int ProofVerifierChromium::Job::DoVerifyCert(int result) {
  next_state_ = STATE_VERIFY_CERT_COMPLETE;

  int flags = 0;
  return verifier_->Verify(
      cert_.get(),
      hostname_,
      flags,
      SSLConfigService::GetCRLSet().get(),
      &verify_details_->cert_verify_result,
      base::Bind(&ProofVerifierChromium::Job::OnIOComplete,
                 base::Unretained(this)),
      net_log_);
}

int ProofVerifierChromium::Job::DoVerifyCertComplete(int result) {
  verifier_.reset();

#if defined(OFFICIAL_BUILD) && !defined(OS_ANDROID) && !defined(OS_IOS)
  // TODO(wtc): The following code was copied from ssl_client_socket_nss.cc.
  // Convert it to a new function that can be called by both files. These
  // variables simulate the arguments to the new function.
  const CertVerifyResult& cert_verify_result =
      verify_details_->cert_verify_result;
  bool sni_available = true;
  const std::string& host = hostname_;
  TransportSecurityState* transport_security_state = transport_security_state_;
  std::string* pinning_failure_log = &verify_details_->pinning_failure_log;

  // Take care of any mandates for public key pinning.
  //
  // Pinning is only enabled for official builds to make sure that others don't
  // end up with pins that cannot be easily updated.
  //
  // TODO(agl): We might have an issue here where a request for foo.example.com
  // merges into a SPDY connection to www.example.com, and gets a different
  // certificate.

  // Perform pin validation if, and only if, all these conditions obtain:
  //
  // * a TransportSecurityState object is available;
  // * the server's certificate chain is valid (or suffers from only a minor
  //   error);
  // * the server's certificate chain chains up to a known root (i.e. not a
  //   user-installed trust anchor); and
  // * the build is recent (very old builds should fail open so that users
  //   have some chance to recover).
  //
  const CertStatus cert_status = cert_verify_result.cert_status;
  if (transport_security_state &&
      (result == OK ||
       (IsCertificateError(result) && IsCertStatusMinorError(cert_status))) &&
      cert_verify_result.is_issued_by_known_root &&
      TransportSecurityState::IsBuildTimely()) {
    if (transport_security_state->HasPublicKeyPins(host, sni_available)) {
      if (!transport_security_state->CheckPublicKeyPins(
              host,
              sni_available,
              cert_verify_result.public_key_hashes,
              pinning_failure_log)) {
        LOG(ERROR) << *pinning_failure_log;
        result = ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN;
        UMA_HISTOGRAM_BOOLEAN("Net.PublicKeyPinSuccess", false);
        TransportSecurityState::ReportUMAOnPinFailure(host);
      } else {
        UMA_HISTOGRAM_BOOLEAN("Net.PublicKeyPinSuccess", true);
      }
    }
  }
#endif

  if (result <= ERR_FAILED) {
    error_details_ = StringPrintf("Failed to verify certificate chain: %s",
                                  ErrorToString(result));
    DLOG(WARNING) << error_details_;
    result = ERR_FAILED;
  }

  // Exit DoLoop and return the result to the caller to VerifyProof.
  DCHECK_EQ(STATE_NONE, next_state_);
  return result;
}

bool ProofVerifierChromium::Job::VerifySignature(const string& signed_data,
                                            const string& signature,
                                            const string& cert) {
  StringPiece spki;
  if (!asn1::ExtractSPKIFromDERCert(cert, &spki)) {
    DLOG(WARNING) << "ExtractSPKIFromDERCert failed";
    return false;
  }

  crypto::SignatureVerifier verifier;

  size_t size_bits;
  X509Certificate::PublicKeyType type;
  X509Certificate::GetPublicKeyInfo(cert_->os_cert_handle(), &size_bits,
                                    &type);
  if (type == X509Certificate::kPublicKeyTypeRSA) {
    crypto::SignatureVerifier::HashAlgorithm hash_alg =
        crypto::SignatureVerifier::SHA256;
    crypto::SignatureVerifier::HashAlgorithm mask_hash_alg = hash_alg;
    unsigned int hash_len = 32;  // 32 is the length of a SHA-256 hash.

    bool ok = verifier.VerifyInitRSAPSS(
        hash_alg, mask_hash_alg, hash_len,
        reinterpret_cast<const uint8*>(signature.data()), signature.size(),
        reinterpret_cast<const uint8*>(spki.data()), spki.size());
    if (!ok) {
      DLOG(WARNING) << "VerifyInitRSAPSS failed";
      return false;
    }
  } else if (type == X509Certificate::kPublicKeyTypeECDSA) {
    // This is the algorithm ID for ECDSA with SHA-256. Parameters are ABSENT.
    // RFC 5758:
    //   ecdsa-with-SHA256 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
    //        us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 2 }
    //   ...
    //   When the ecdsa-with-SHA224, ecdsa-with-SHA256, ecdsa-with-SHA384, or
    //   ecdsa-with-SHA512 algorithm identifier appears in the algorithm field
    //   as an AlgorithmIdentifier, the encoding MUST omit the parameters
    //   field.  That is, the AlgorithmIdentifier SHALL be a SEQUENCE of one
    //   component, the OID ecdsa-with-SHA224, ecdsa-with-SHA256, ecdsa-with-
    //   SHA384, or ecdsa-with-SHA512.
    // See also RFC 5480, Appendix A.
    static const uint8 kECDSAWithSHA256AlgorithmID[] = {
      0x30, 0x0a,
        0x06, 0x08,
          0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02,
    };

    if (!verifier.VerifyInit(
            kECDSAWithSHA256AlgorithmID, sizeof(kECDSAWithSHA256AlgorithmID),
            reinterpret_cast<const uint8*>(signature.data()),
            signature.size(),
            reinterpret_cast<const uint8*>(spki.data()),
            spki.size())) {
      DLOG(WARNING) << "VerifyInit failed";
      return false;
    }
  } else {
    LOG(ERROR) << "Unsupported public key type " << type;
    return false;
  }

  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(kProofSignatureLabel),
                        sizeof(kProofSignatureLabel));
  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(signed_data.data()),
                        signed_data.size());

  if (!verifier.VerifyFinal()) {
    DLOG(WARNING) << "VerifyFinal failed";
    return false;
  }

  DVLOG(1) << "VerifyFinal success";
  return true;
}

ProofVerifierChromium::ProofVerifierChromium(
    CertVerifier* cert_verifier,
    TransportSecurityState* transport_security_state)
    : cert_verifier_(cert_verifier),
      transport_security_state_(transport_security_state) {
}

ProofVerifierChromium::~ProofVerifierChromium() {
  STLDeleteElements(&active_jobs_);
}

QuicAsyncStatus ProofVerifierChromium::VerifyProof(
    const std::string& hostname,
    const std::string& server_config,
    const std::vector<std::string>& certs,
    const std::string& signature,
    const ProofVerifyContext* verify_context,
    std::string* error_details,
    scoped_ptr<ProofVerifyDetails>* verify_details,
    ProofVerifierCallback* callback) {
  if (!verify_context) {
    *error_details = "Missing context";
    return QUIC_FAILURE;
  }
  const ProofVerifyContextChromium* chromium_context =
      reinterpret_cast<const ProofVerifyContextChromium*>(verify_context);
  scoped_ptr<Job> job(new Job(this,
                              cert_verifier_,
                              transport_security_state_,
                              chromium_context->net_log));
  QuicAsyncStatus status = job->VerifyProof(hostname, server_config, certs,
                                            signature, error_details,
                                            verify_details, callback);
  if (status == QUIC_PENDING) {
    active_jobs_.insert(job.release());
  }
  return status;
}

void ProofVerifierChromium::OnJobComplete(Job* job) {
  active_jobs_.erase(job);
  delete job;
}

}  // namespace net
