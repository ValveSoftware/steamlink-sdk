// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_util.h"

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/cast_certificate/cast_cert_validator.h"
#include "extensions/browser/api/cast_channel/cast_message_util.h"
#include "extensions/common/api/cast_channel/cast_channel.pb.h"
#include "net/cert/x509_certificate.h"
#include "net/der/parse_values.h"

namespace extensions {
namespace api {
namespace cast_channel {
namespace {

const char* const kParseErrorPrefix = "Failed to parse auth message: ";

// The maximum number of days a cert can live for.
const int kMaxSelfSignedCertLifetimeInDays = 4;

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
        AuthResult::ERROR_VALID_START_DATE_IN_FUTURE);
  }
  if (expiry.is_null() || peer_cert.HasExpired()) {
    return AuthResult::CreateWithParseError("Certificate has expired.",
                                            AuthResult::ERROR_CERT_EXPIRED);
  }
  if (expiry > lifetime_limit) {
    return AuthResult::CreateWithParseError(
        "Peer cert lifetime is too long.",
        AuthResult::ERROR_VALIDITY_PERIOD_TOO_LONG);
  }

  const AuthResponse& response = auth_message.response();
  return VerifyCredentials(response, peer_cert_der);
}

// This function does the following
//
// * Verifies that the certificate chain |response.client_auth_certificate| +
//   |response.intermediate_certificate| is valid and chains to a trusted
//   Cast root.
//
// * Verifies that |response.signature| matches the signature
//   of |signature_input| by |response.client_auth_certificate|'s public
//   key.
AuthResult VerifyCredentials(const AuthResponse& response,
                             const std::string& signature_input) {
  // Verify the certificate
  std::unique_ptr<cast_crypto::CertVerificationContext> verification_context;

  // Build a single vector containing the certificate chain.
  std::vector<std::string> cert_chain;
  cert_chain.push_back(response.client_auth_certificate());
  cert_chain.insert(cert_chain.end(),
                    response.intermediate_certificate().begin(),
                    response.intermediate_certificate().end());

  // Use the current time when checking certificate validity.
  base::Time::Exploded now;
  base::Time::Now().UTCExplode(&now);

  cast_crypto::CastDeviceCertPolicy device_policy;
  if (!cast_crypto::VerifyDeviceCert(cert_chain, now, &verification_context,
                                     &device_policy)) {
    // TODO(eroman): The error information was lost; this error is ambiguous.
    return AuthResult("Failed verifying cast device certificate",
                      AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA);
  }

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

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
