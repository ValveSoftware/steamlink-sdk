// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CERTIFICATE_CAST_CERT_VALIDATOR_H_
#define COMPONENTS_CAST_CERTIFICATE_CAST_CERT_VALIDATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"

namespace cast_certificate {

// Describes the policy for a Device certificate.
enum class CastDeviceCertPolicy {
  // The device certificate is unrestricted.
  NONE,

  // The device certificate is for an audio-only device.
  AUDIO_ONLY,
};

// An object of this type is returned by the VerifyDeviceCert function, and can
// be used for additional certificate-related operations, using the verified
// certificate.
class CertVerificationContext {
 public:
  CertVerificationContext() {}
  virtual ~CertVerificationContext() {}

  // Use the public key from the verified certificate to verify a
  // sha1WithRSAEncryption |signature| over arbitrary |data|. Both |signature|
  // and |data| hold raw binary data. Returns true if the signature was
  // correct.
  virtual bool VerifySignatureOverData(const base::StringPiece& signature,
                                       const base::StringPiece& data) const = 0;

  // Retrieve the Common Name attribute of the subject's distinguished name from
  // the verified certificate, if present.  Returns an empty string if no Common
  // Name is found.
  virtual std::string GetCommonName() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CertVerificationContext);
};

// Verifies a cast device certficate given a chain of DER-encoded certificates.
//
// Inputs:
//
// * |certs| is a chain of DER-encoded certificates:
//   * |certs[0]| is the target certificate (i.e. the device certificate)
//   * |certs[i]| is the certificate that issued certs[i-1]
//   * |certs.back()| must be signed by a trust anchor
//
// * |time| is the UTC time to use for determining if the certificate
//   is expired.
//
// Outputs:
//
// Returns true on success, false on failure. On success the output
// parameters are filled with more details:
//
//   * |context| is filled with an object that can be used to verify signatures
//     using the device certificate's public key, as well as to extract other
//     properties from the device certificate (Common Name).
//   * |policy| is filled with an indication of the device certificate's policy
//     (i.e. is it for audio-only devices or is it unrestricted?)
bool VerifyDeviceCert(const std::vector<std::string>& certs,
                      const base::Time::Exploded& time,
                      std::unique_ptr<CertVerificationContext>* context,
                      CastDeviceCertPolicy* policy) WARN_UNUSED_RESULT;

// Exposed only for unit-tests, not for use in production code.
// Production code would get a context from VerifyDeviceCert().
//
// Constructs a VerificationContext that uses the provided public key.
// The common name will be hardcoded to some test value.
std::unique_ptr<CertVerificationContext> CertVerificationContextImplForTest(
    const base::StringPiece& spki);

// Exposed only for testing, not for use in production code.
//
// Injects trusted root certificates into the CastTrustStore.
// |data| must remain valid and not be mutated throughout the lifetime of
// the program.
// Warning: Using this function concurrently with VerifyDeviceCert()
//          is not thread safe.
bool AddTrustAnchorForTest(const uint8_t* data,
                           size_t length) WARN_UNUSED_RESULT;

}  // namespace cast_certificate

#endif  // COMPONENTS_CAST_CERTIFICATE_CAST_CERT_VALIDATOR_H_
