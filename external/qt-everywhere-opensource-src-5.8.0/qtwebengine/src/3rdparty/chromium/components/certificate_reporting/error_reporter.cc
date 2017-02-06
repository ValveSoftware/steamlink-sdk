// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_reporting/error_reporter.h"

#include <stddef.h>

#include <set>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/certificate_reporting/encrypted_cert_logger.pb.h"
#include "crypto/aead.h"
#include "crypto/curve25519.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"
#include "net/url_request/report_sender.h"

namespace certificate_reporting {

namespace {

// Constants used for crypto. The corresponding private key is used by
// the SafeBrowsing client-side detection server to decrypt reports.
static const uint8_t kServerPublicKey[] = {
    0x51, 0xcc, 0x52, 0x67, 0x42, 0x47, 0x3b, 0x10, 0xe8, 0x63, 0x18,
    0x3c, 0x61, 0xa7, 0x96, 0x76, 0x86, 0x91, 0x40, 0x71, 0x39, 0x5f,
    0x31, 0x1a, 0x39, 0x5b, 0x76, 0xb1, 0x6b, 0x3d, 0x6a, 0x2b};
static const uint32_t kServerPublicKeyVersion = 1;

static const char kHkdfLabel[] = "certificate report";

bool GetHkdfSubkeySecret(size_t subkey_length,
                         const uint8_t* private_key,
                         const uint8_t* public_key,
                         std::string* secret) {
  uint8_t shared_secret[crypto::curve25519::kBytes];
  if (!crypto::curve25519::ScalarMult(private_key, public_key, shared_secret))
    return false;

  // By mistake, the HKDF label here ends up with an extra null byte on
  // the end, due to using sizeof(kHkdfLabel) in the StringPiece
  // constructor instead of strlen(kHkdfLabel). Ideally this code should
  // be just passing kHkdfLabel directly into the HKDF constructor.
  //
  // TODO(estark): fix this in coordination with the server-side code --
  // perhaps by rolling the public key version forward and using the
  // version to decide whether to use the extra-null-byte version of the
  // label. https://crbug.com/517746
  crypto::HKDF hkdf(base::StringPiece(reinterpret_cast<char*>(shared_secret),
                                      sizeof(shared_secret)),
                    "" /* salt */,
                    base::StringPiece(kHkdfLabel, sizeof(kHkdfLabel)),
                    0 /* key bytes */, 0 /* iv bytes */, subkey_length);

  *secret = hkdf.subkey_secret().as_string();
  return true;
}

bool EncryptSerializedReport(const uint8_t* server_public_key,
                             uint32_t server_public_key_version,
                             const std::string& report,
                             EncryptedCertLoggerRequest* encrypted_report) {
  // Generate an ephemeral key pair to generate a shared secret.
  uint8_t public_key[crypto::curve25519::kBytes];
  uint8_t private_key[crypto::curve25519::kScalarBytes];

  crypto::RandBytes(private_key, sizeof(private_key));
  crypto::curve25519::ScalarBaseMult(private_key, public_key);

  crypto::Aead aead(crypto::Aead::AES_128_CTR_HMAC_SHA256);
  std::string key;
  if (!GetHkdfSubkeySecret(aead.KeyLength(), private_key,
                           reinterpret_cast<const uint8_t*>(server_public_key),
                           &key)) {
    LOG(ERROR) << "Error getting subkey secret.";
    return false;
  }
  aead.Init(&key);

  // Use an all-zero nonce because the key is random per-message.
  std::string nonce(aead.NonceLength(), '\0');

  std::string ciphertext;
  if (!aead.Seal(report, nonce, std::string(), &ciphertext)) {
    LOG(ERROR) << "Error sealing certificate report.";
    return false;
  }

  encrypted_report->set_encrypted_report(ciphertext);
  encrypted_report->set_server_public_key_version(server_public_key_version);
  encrypted_report->set_client_public_key(reinterpret_cast<char*>(public_key),
                                          sizeof(public_key));
  encrypted_report->set_algorithm(
      EncryptedCertLoggerRequest::AEAD_ECDH_AES_128_CTR_HMAC_SHA256);
  return true;
}

}  // namespace

ErrorReporter::ErrorReporter(
    net::URLRequestContext* request_context,
    const GURL& upload_url,
    net::ReportSender::CookiesPreference cookies_preference)
    : ErrorReporter(
          upload_url,
          kServerPublicKey,
          kServerPublicKeyVersion,
          base::WrapUnique(
              new net::ReportSender(request_context, cookies_preference))) {}

ErrorReporter::ErrorReporter(
    const GURL& upload_url,
    const uint8_t server_public_key[/* 32 */],
    const uint32_t server_public_key_version,
    std::unique_ptr<net::ReportSender> certificate_report_sender)
    : certificate_report_sender_(std::move(certificate_report_sender)),
      upload_url_(upload_url),
      server_public_key_(server_public_key),
      server_public_key_version_(server_public_key_version) {
  DCHECK(certificate_report_sender_);
  DCHECK(!upload_url.is_empty());
}

ErrorReporter::~ErrorReporter() {}

void ErrorReporter::SendExtendedReportingReport(
    const std::string& serialized_report) {
  if (upload_url_.SchemeIsCryptographic()) {
    certificate_report_sender_->Send(upload_url_, serialized_report);
  } else {
    DCHECK(IsHttpUploadUrlSupported());
    EncryptedCertLoggerRequest encrypted_report;
    if (!EncryptSerializedReport(server_public_key_, server_public_key_version_,
                                 serialized_report, &encrypted_report)) {
      LOG(ERROR) << "Failed to encrypt serialized report.";
      return;
    }
    std::string serialized_encrypted_report;
    encrypted_report.SerializeToString(&serialized_encrypted_report);
    certificate_report_sender_->Send(upload_url_, serialized_encrypted_report);
  }
}

bool ErrorReporter::IsHttpUploadUrlSupported() {
  return true;
}

// Used only by tests.
bool ErrorReporter::DecryptErrorReport(
    const uint8_t server_private_key[32],
    const EncryptedCertLoggerRequest& encrypted_report,
    std::string* decrypted_serialized_report) {
  crypto::Aead aead(crypto::Aead::AES_128_CTR_HMAC_SHA256);
  std::string key;
  if (!GetHkdfSubkeySecret(aead.KeyLength(), server_private_key,
                           reinterpret_cast<const uint8_t*>(
                               encrypted_report.client_public_key().data()),
                           &key)) {
    LOG(ERROR) << "Error getting subkey secret.";
    return false;
  }
  aead.Init(&key);

  // Use an all-zero nonce because the key is random per-message.
  std::string nonce(aead.NonceLength(), 0);

  return aead.Open(encrypted_report.encrypted_report(), nonce, std::string(),
                   decrypted_serialized_report);
}

}  // namespace certificate_reporting
