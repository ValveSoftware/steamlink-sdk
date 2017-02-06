// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_crypto_delegate.h"

#include "base/macros.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/easy_unlock_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace extensions {

namespace easy_unlock_private = api::easy_unlock_private;

namespace {

// Converts encryption type to a string representation used by EasyUnlock dbus
// client.
std::string EncryptionTypeToString(easy_unlock_private::EncryptionType type) {
  switch (type) {
    case easy_unlock_private::ENCRYPTION_TYPE_AES_256_CBC:
      return easy_unlock::kEncryptionTypeAES256CBC;
    default:
      return easy_unlock::kEncryptionTypeNone;
  }
}

// Converts signature type to a string representation used by EasyUnlock dbus
// client.
std::string SignatureTypeToString(easy_unlock_private::SignatureType type) {
  switch (type) {
    case easy_unlock_private::SIGNATURE_TYPE_ECDSA_P256_SHA256:
      return easy_unlock::kSignatureTypeECDSAP256SHA256;
    case easy_unlock_private::SIGNATURE_TYPE_HMAC_SHA256:
      // Fall through to default.
    default:
      return easy_unlock::kSignatureTypeHMACSHA256;
  }
}

// ChromeOS specific EasyUnlockPrivateCryptoDelegate implementation.
class EasyUnlockPrivateCryptoDelegateChromeOS
    : public extensions::EasyUnlockPrivateCryptoDelegate {
 public:
  EasyUnlockPrivateCryptoDelegateChromeOS()
      : dbus_client_(
            chromeos::DBusThreadManager::Get()->GetEasyUnlockClient()) {
  }

  ~EasyUnlockPrivateCryptoDelegateChromeOS() override {}

  void GenerateEcP256KeyPair(const KeyPairCallback& callback) override {
    dbus_client_->GenerateEcP256KeyPair(callback);
  }

  void PerformECDHKeyAgreement(
      const easy_unlock_private::PerformECDHKeyAgreement::Params& params,
      const DataCallback& callback) override {
    dbus_client_->PerformECDHKeyAgreement(
        std::string(params.private_key.begin(), params.private_key.end()),
        std::string(params.public_key.begin(), params.public_key.end()),
        callback);
  }

  void CreateSecureMessage(
      const easy_unlock_private::CreateSecureMessage::Params& params,
      const DataCallback& callback) override {
    chromeos::EasyUnlockClient::CreateSecureMessageOptions options;
    options.key.assign(params.key.begin(), params.key.end());
    if (params.options.associated_data) {
      options.associated_data.assign(params.options.associated_data->begin(),
                                     params.options.associated_data->end());
    }
    if (params.options.public_metadata) {
      options.public_metadata.assign(params.options.public_metadata->begin(),
                                     params.options.public_metadata->end());
    }
    if (params.options.verification_key_id) {
      options.verification_key_id.assign(
          params.options.verification_key_id->begin(),
          params.options.verification_key_id->end());
    }
    if (params.options.decryption_key_id) {
      options.decryption_key_id.assign(
          params.options.decryption_key_id->begin(),
          params.options.decryption_key_id->end());
    }
    options.encryption_type =
        EncryptionTypeToString(params.options.encrypt_type);
    options.signature_type =
        SignatureTypeToString(params.options.sign_type);

    dbus_client_->CreateSecureMessage(
        std::string(params.payload.begin(), params.payload.end()), options,
        callback);
  }

  void UnwrapSecureMessage(
      const easy_unlock_private::UnwrapSecureMessage::Params& params,
      const DataCallback& callback) override {
    chromeos::EasyUnlockClient::UnwrapSecureMessageOptions options;
    options.key.assign(params.key.begin(), params.key.end());
    if (params.options.associated_data) {
      options.associated_data.assign(params.options.associated_data->begin(),
                                     params.options.associated_data->end());
    }
    options.encryption_type =
        EncryptionTypeToString(params.options.encrypt_type);
    options.signature_type =
        SignatureTypeToString(params.options.sign_type);

    dbus_client_->UnwrapSecureMessage(
        std::string(params.secure_message.begin(), params.secure_message.end()),
        options, callback);
  }

 private:
  chromeos::EasyUnlockClient* dbus_client_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockPrivateCryptoDelegateChromeOS);
};

}  // namespace

// static
std::unique_ptr<EasyUnlockPrivateCryptoDelegate>
EasyUnlockPrivateCryptoDelegate::Create() {
  return std::unique_ptr<EasyUnlockPrivateCryptoDelegate>(
      new EasyUnlockPrivateCryptoDelegateChromeOS());
}

}  // namespace extensions
