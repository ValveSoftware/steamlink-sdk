// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/crypto_verify_impl.h"

#include <stdint.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_credentials_getter.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_crypto.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/api/networking_private/networking_private_service_client.h"
#include "extensions/common/api/networking_private.h"

namespace extensions {

namespace {

const char kCryptoVerifySequenceTokenName[] = "CryptoVerify";

// Called from a blocking pool task runner. Returns true and sets |verified| if
// able to decode the credentials, otherwise sets |verified| to false and
// returns false.
bool DecodeAndVerifyCredentials(
    const CryptoVerifyImpl::Credentials& credentials,
    bool* verified) {
  std::string decoded_signed_data;
  if (!base::Base64Decode(credentials.signed_data, &decoded_signed_data)) {
    LOG(ERROR) << "Failed to decode signed data";
    *verified = false;
    return false;
  }
  *verified = networking_private_crypto::VerifyCredentials(
      credentials.certificate, credentials.intermediate_certificates,
      decoded_signed_data, credentials.unsigned_data, credentials.device_bssid);
  return true;
}

void VerifyDestinationCompleted(
    const CryptoVerifyImpl::BoolCallback& success_callback,
    const CryptoVerifyImpl::FailureCallback& failure_callback,
    bool* verified,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success)
    failure_callback.Run(networking_private::kErrorEncryptionError);
  else
    success_callback.Run(*verified);
}

// Called from a blocking pool task runner. Returns |data| encoded using
// |credentials| on success, or an empty string on failure.
std::string DoVerifyAndEncryptData(
    const CryptoVerifyImpl::Credentials& credentials,
    const std::string& data) {
  bool verified;
  if (!DecodeAndVerifyCredentials(credentials, &verified) || !verified)
    return std::string();

  std::string decoded_public_key;
  if (!base::Base64Decode(credentials.public_key, &decoded_public_key)) {
    LOG(ERROR) << "Failed to decode public key";
    return std::string();
  }

  std::vector<uint8_t> public_key_data(decoded_public_key.begin(),
                                       decoded_public_key.end());
  std::vector<uint8_t> ciphertext;
  if (!networking_private_crypto::EncryptByteString(public_key_data, data,
                                                    &ciphertext)) {
    LOG(ERROR) << "Failed to encrypt data";
    return std::string();
  }

  std::string base64_encoded_ciphertext;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(ciphertext.data()),
                        ciphertext.size()),
      &base64_encoded_ciphertext);
  return base64_encoded_ciphertext;
}

void VerifyAndEncryptDataCompleted(
    const CryptoVerifyImpl::StringCallback& success_callback,
    const CryptoVerifyImpl::FailureCallback& failure_callback,
    const std::string& encrypted_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (encrypted_data.empty())
    failure_callback.Run(networking_private::kErrorEncryptionError);
  else
    success_callback.Run(encrypted_data);
}

// Called when NetworkingPrivateCredentialsGetter completes (from an arbitrary
// thread). Posts the result to the UI thread.
void CredentialsGetterCompleted(
    const CryptoVerifyImpl::StringCallback& success_callback,
    const CryptoVerifyImpl::FailureCallback& failure_callback,
    const std::string& key_data,
    const std::string& error) {
  if (!error.empty()) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(failure_callback, error));
  } else {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     base::Bind(success_callback, key_data));
  }
}

// Called from a blocking pool task runner. Returns true if
// NetworkingPrivateCredentialsGetter is successfully started (which will
// invoke the appropriate callback when completed), or false if unable
// to start the getter (credentials or public key decode failed).
bool DoVerifyAndEncryptCredentials(
    const std::string& guid,
    const CryptoVerifyImpl::Credentials& credentials,
    const CryptoVerifyImpl::StringCallback& success_callback,
    const CryptoVerifyImpl::FailureCallback& failure_callback) {
  bool verified;
  if (!DecodeAndVerifyCredentials(credentials, &verified) || !verified)
    return false;

  std::string decoded_public_key;
  if (!base::Base64Decode(credentials.public_key, &decoded_public_key)) {
    LOG(ERROR) << "Failed to decode public key";
    return false;
  }

  // Start getting credentials. CredentialsGetterCompleted will be called on
  // completion. On Windows it will be called from a different thread after
  // |credentials_getter| is deleted.
  std::unique_ptr<NetworkingPrivateCredentialsGetter> credentials_getter(
      NetworkingPrivateCredentialsGetter::Create());
  credentials_getter->Start(guid, decoded_public_key,
                            base::Bind(&CredentialsGetterCompleted,
                                       success_callback, failure_callback));
  return true;
}

void VerifyAndEncryptCredentialsCompleted(
    const CryptoVerifyImpl::FailureCallback& failure_callback,
    bool succeeded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // If VerifyAndEncryptCredentials succeeded, then the appropriate callback
  // will be triggered from CredentialsGetterCompleted.
  if (succeeded)
    return;
  failure_callback.Run(networking_private::kErrorEncryptionError);
}

}  // namespace

CryptoVerifyImpl::Credentials::Credentials(
    const VerificationProperties& properties) {
  certificate = properties.certificate;
  if (properties.intermediate_certificates.get())
    intermediate_certificates = *properties.intermediate_certificates;
  signed_data = properties.signed_data;

  std::vector<std::string> data_parts;
  data_parts.push_back(properties.device_ssid);
  data_parts.push_back(properties.device_serial);
  data_parts.push_back(properties.device_bssid);
  data_parts.push_back(properties.public_key);
  data_parts.push_back(properties.nonce);
  unsigned_data = base::JoinString(data_parts, ",");

  device_bssid = properties.device_bssid;
  public_key = properties.public_key;
}

CryptoVerifyImpl::Credentials::Credentials(const Credentials& other) = default;

CryptoVerifyImpl::Credentials::~Credentials() {
}

CryptoVerifyImpl::CryptoVerifyImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::SequencedWorkerPool::SequenceToken sequence_token =
      content::BrowserThread::GetBlockingPool()->GetNamedSequenceToken(
          kCryptoVerifySequenceTokenName);
  blocking_pool_task_runner_ =
      content::BrowserThread::GetBlockingPool()
          ->GetSequencedTaskRunnerWithShutdownBehavior(
              sequence_token, base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

CryptoVerifyImpl::~CryptoVerifyImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void CryptoVerifyImpl::VerifyDestination(
    const VerificationProperties& verification_properties,
    const BoolCallback& success_callback,
    const FailureCallback& failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Credentials credentials(verification_properties);
  bool* verified = new bool;
  base::PostTaskAndReplyWithResult(
      blocking_pool_task_runner_.get(), FROM_HERE,
      base::Bind(&DecodeAndVerifyCredentials, credentials, verified),
      base::Bind(&VerifyDestinationCompleted, success_callback,
                 failure_callback, base::Owned(verified)));
}

void CryptoVerifyImpl::VerifyAndEncryptCredentials(
    const std::string& guid,
    const VerificationProperties& verification_properties,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Credentials credentials(verification_properties);
  base::PostTaskAndReplyWithResult(
      blocking_pool_task_runner_.get(), FROM_HERE,
      base::Bind(&DoVerifyAndEncryptCredentials, guid, credentials,
                 success_callback, failure_callback),
      base::Bind(&VerifyAndEncryptCredentialsCompleted, failure_callback));
}

void CryptoVerifyImpl::VerifyAndEncryptData(
    const VerificationProperties& verification_properties,
    const std::string& data,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Credentials credentials(verification_properties);
  base::PostTaskAndReplyWithResult(
      blocking_pool_task_runner_.get(), FROM_HERE,
      base::Bind(&DoVerifyAndEncryptData, credentials, data),
      base::Bind(&VerifyAndEncryptDataCompleted, success_callback,
                 failure_callback));
}

}  // namespace extensions
