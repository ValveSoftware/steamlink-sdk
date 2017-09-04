// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_util.h"

#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/cast_certificate/cast_cert_validator.h"
#include "components/cast_certificate/cast_crl.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "net/cert/x509_certificate.h"
#include "net/der/parse_values.h"

namespace extensions {
namespace api {
namespace cast_channel {
namespace {

const char kParseErrorPrefix[] = "Failed to parse auth message: ";

// The maximum number of days a cert can live for.
const int kMaxSelfSignedCertLifetimeInDays = 4;

// Enforce certificate revocation when enabled.
// If disabled, any revocation failures are ignored.
//
// This flags only controls the enforcement. Revocation is checked regardless.
//
// This flag tracks the changes necessary to fully enforce revocation.
const base::Feature kEnforceRevocationChecking{
    "CastCertificateRevocation", base::FEATURE_DISABLED_BY_DEFAULT};

namespace cast_crypto = ::cast_certificate;

// Extracts an embedded DeviceAuthMessage payload from an auth challenge reply
// message.
AuthResult ParseAuthMessage(const CastMessage& challenge_reply,
                            DeviceAuthMessage* auth_message) {
  if (challenge_reply.payload_type() != CastMessage_PayloadType_BINARY) {
    return AuthResult::CreateWithParseError(
        "Wrong payload type in challenge reply",
        AuthResult::ERROR_WRONG_PAYLOAD_TYPE);
  }
  if (!challenge_reply.has_payload_binary()) {
    return AuthResult::CreateWithParseError(
        "Payload type is binary but payload_binary field not set",
        AuthResult::ERROR_NO_PAYLOAD);
  }
  if (!auth_message->ParseFromString(challenge_reply.payload_binary())) {
    return AuthResult::CreateWithParseError(
        "Cannot parse binary payload into DeviceAuthMessage",
        AuthResult::ERROR_PAYLOAD_PARSING_FAILED);
  }

  VLOG(1) << "Auth message: " << AuthMessageToString(*auth_message);

  if (auth_message->has_error()) {
    return AuthResult::CreateWithParseError(
        "Auth message error: " +
            base::IntToString(auth_message->error().error_type()),
        AuthResult::ERROR_MESSAGE_ERROR);
  }
  if (!auth_message->has_response()) {
    return AuthResult::CreateWithParseError(
        "Auth message has no response field", AuthResult::ERROR_NO_RESPONSE);
  }
  return AuthResult();
}

// Must match with histogram enum CastCertificateStatus.
// This should never be reordered.
enum CertVerificationStatus {
  CERT_STATUS_OK,
  CERT_STATUS_INVALID_CRL,
  CERT_STATUS_VERIFICATION_FAILED,
  CERT_STATUS_REVOKED,
  CERT_STATUS_COUNT,
};

}  // namespace

AuthResult::AuthResult()
    : error_type(ERROR_NONE), channel_policies(POLICY_NONE) {}

AuthResult::AuthResult(const std::string& error_message, ErrorType error_type)
    : error_message(error_message), error_type(error_type) {}

AuthResult::~AuthResult() {
}

// static
AuthResult AuthResult::CreateWithParseError(const std::string& error_message,
                                            ErrorType error_type) {
  return AuthResult(kParseErrorPrefix + error_message, error_type);
}

AuthResult AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                      const net::X509Certificate& peer_cert) {
  DeviceAuthMessage auth_message;
  AuthResult result = ParseAuthMessage(challenge_reply, &auth_message);
  if (!result.success()) {
    return result;
  }

  // Get the DER-encoded form of the certificate.
  std::string peer_cert_der;
  if (!net::X509Certificate::GetDEREncoded(peer_cert.os_cert_handle(),
                                           &peer_cert_der) ||
      peer_cert_der.empty()) {
    return AuthResult::CreateWithParseError(
        "Could not create DER-encoded peer cert.",
        AuthResult::ERROR_CERT_PARSING_FAILED);
  }

  // Ensure the peer cert is valid and doesn't have an excessive remaining
  // lifetime. Although it is not verified as an X.509 certificate, the entire
  // structure is signed by the AuthResponse, so the validity field from X.509
  // is repurposed as this signature's expiration.
  base::Time expiry = peer_cert.valid_expiry();
  base::Time lifetime_limit =
      base::Time::Now() +
      base::TimeDelta::FromDays(kMaxSelfSignedCertLifetimeInDays);
  if (peer_cert.valid_start().is_null() ||
      peer_cert.valid_start() > base::Time::Now()) {
    return AuthResult::CreateWithParseError(
        "Certificate's valid start date is in the future.",
        AuthResult::ERROR_TLS_CERT_VALID_START_DATE_IN_FUTURE);
  }
  if (expiry.is_null() || peer_cert.HasExpired()) {
    return AuthResult::CreateWithParseError("Certificate has expired.",
                                            AuthResult::ERROR_TLS_CERT_EXPIRED);
  }
  if (expiry > lifetime_limit) {
    return AuthResult::CreateWithParseError(
        "Peer cert lifetime is too long.",
        AuthResult::ERROR_TLS_CERT_VALIDITY_PERIOD_TOO_LONG);
  }

  const AuthResponse& response = auth_message.response();
  return VerifyCredentials(response, peer_cert_der);
}

// This function does the following
//
// * Verifies that the certificate chain |response.client_auth_certificate| +
//   |response.intermediate_certificate| is valid and chains to a trusted
//   Cast root. The list of trusted Cast roots can be overrided by providing a
//   non-nullptr |cast_trust_store|. The certificate is verified at
//   |verification_time|.
//
// * Verifies that none of the certificates in the chain are revoked based on
//   the CRL provided in the response |response.crl|. The CRL is verified to be
//   valid and its issuer certificate chains to a trusted Cast CRL root. The
//   list of trusted Cast CRL roots can be overrided by providing a non-nullptr
//   |crl_trust_store|. If |crl_policy| is CRL_OPTIONAL then the result of
//   revocation checking is ignored. The CRL is verified at
//   |verification_time|.
//
// * Verifies that |response.signature| matches the signature
//   of |signature_input| by |response.client_auth_certificate|'s public
//   key.
AuthResult VerifyCredentialsImpl(const AuthResponse& response,
                                 const std::string& signature_input,
                                 const cast_crypto::CRLPolicy& crl_policy,
                                 net::TrustStore* cast_trust_store,
                                 net::TrustStore* crl_trust_store,
                                 const base::Time& verification_time) {
  // Verify the certificate
  std::unique_ptr<cast_crypto::CertVerificationContext> verification_context;

  // Build a single vector containing the certificate chain.
  std::vector<std::string> cert_chain;
  cert_chain.push_back(response.client_auth_certificate());
  cert_chain.insert(cert_chain.end(),
                    response.intermediate_certificate().begin(),
                    response.intermediate_certificate().end());

  // Parse the CRL.
  std::unique_ptr<cast_crypto::CastCRL> crl =
      cast_crypto::ParseAndVerifyCRLUsingCustomTrustStore(
          response.crl(), verification_time, crl_trust_store);
  if (!crl) {
    // CRL is invalid.
    UMA_HISTOGRAM_ENUMERATION("Cast.Channel.Certificate",
                              CERT_STATUS_INVALID_CRL, CERT_STATUS_COUNT);
    if (crl_policy == cast_crypto::CRLPolicy::CRL_REQUIRED) {
      return AuthResult("Failed verifying Cast CRL.",
                        AuthResult::ERROR_CRL_INVALID);
    }
  }

  cast_crypto::CastDeviceCertPolicy device_policy;
  bool verification_success =
      cast_crypto::VerifyDeviceCertUsingCustomTrustStore(
          cert_chain, verification_time, &verification_context, &device_policy,
          crl.get(), crl_policy, cast_trust_store);
  if (!verification_success) {
    // TODO(ryanchung): Once this feature is completely rolled-out, remove the
    // reverification step and use error reporting to get verification errors
    // for metrics.
    bool verification_no_crl_success =
        cast_crypto::VerifyDeviceCertUsingCustomTrustStore(
            cert_chain, verification_time, &verification_context,
            &device_policy, nullptr, cast_crypto::CRLPolicy::CRL_OPTIONAL,
            cast_trust_store);
    if (!verification_no_crl_success) {
      // TODO(eroman): The error information was lost; this error is ambiguous.
      UMA_HISTOGRAM_ENUMERATION("Cast.Channel.Certificate",
                                CERT_STATUS_VERIFICATION_FAILED,
                                CERT_STATUS_COUNT);
      return AuthResult("Failed verifying cast device certificate",
                        AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA);
    }
    if (crl) {
      // If CRL was not present, it should've been recorded as such.
      UMA_HISTOGRAM_ENUMERATION("Cast.Channel.Certificate", CERT_STATUS_REVOKED,
                                CERT_STATUS_COUNT);
    }
    if (crl_policy == cast_crypto::CRLPolicy::CRL_REQUIRED) {
      // Device is revoked.
      return AuthResult("Failed certificate revocation check.",
                        AuthResult::ERROR_CERT_REVOKED);
    }
  }
  // The certificate is verified at this point.
  UMA_HISTOGRAM_ENUMERATION("Cast.Channel.Certificate", CERT_STATUS_OK,
                            CERT_STATUS_COUNT);
  if (!verification_context->VerifySignatureOverData(response.signature(),
                                                     signature_input)) {
    return AuthResult("Failed verifying signature over data",
                      AuthResult::ERROR_SIGNED_BLOBS_MISMATCH);
  }

  AuthResult success;

  // Set the policy into the result.
  switch (device_policy) {
    case cast_crypto::CastDeviceCertPolicy::AUDIO_ONLY:
      success.channel_policies = AuthResult::POLICY_AUDIO_ONLY;
      break;
    case cast_crypto::CastDeviceCertPolicy::NONE:
      success.channel_policies = AuthResult::POLICY_NONE;
      break;
  }

  return success;
}

AuthResult VerifyCredentials(const AuthResponse& response,
                             const std::string& signature_input) {
  base::Time now = base::Time::Now();
  cast_crypto::CRLPolicy policy = cast_crypto::CRLPolicy::CRL_REQUIRED;
  if (!base::FeatureList::IsEnabled(kEnforceRevocationChecking)) {
    policy = cast_crypto::CRLPolicy::CRL_OPTIONAL;
  }
  return VerifyCredentialsImpl(response, signature_input, policy, nullptr,
                               nullptr, now);
}

AuthResult VerifyCredentialsForTest(const AuthResponse& response,
                                    const std::string& signature_input,
                                    const cast_crypto::CRLPolicy& crl_policy,
                                    net::TrustStore* cast_trust_store,
                                    net::TrustStore* crl_trust_store,
                                    const base::Time& verification_time) {
  return VerifyCredentialsImpl(response, signature_input, crl_policy,
                               cast_trust_store, crl_trust_store,
                               verification_time);
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
