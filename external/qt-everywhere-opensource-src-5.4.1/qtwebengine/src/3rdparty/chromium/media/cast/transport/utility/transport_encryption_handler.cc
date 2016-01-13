// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/utility/transport_encryption_handler.h"

#include "base/logging.h"
#include "crypto/encryptor.h"
#include "crypto/symmetric_key.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {
namespace transport {

TransportEncryptionHandler::TransportEncryptionHandler()
    : key_(), encryptor_(), iv_mask_(), initialized_(false) {}

TransportEncryptionHandler::~TransportEncryptionHandler() {}

bool TransportEncryptionHandler::Initialize(std::string aes_key,
                                            std::string aes_iv_mask) {
  initialized_ = false;
  if (aes_iv_mask.size() == kAesKeySize && aes_key.size() == kAesKeySize) {
    iv_mask_ = aes_iv_mask;
    key_.reset(
        crypto::SymmetricKey::Import(crypto::SymmetricKey::AES, aes_key));
    encryptor_.reset(new crypto::Encryptor());
    encryptor_->Init(key_.get(), crypto::Encryptor::CTR, std::string());
    initialized_ = true;
  } else if (aes_iv_mask.size() != 0 || aes_key.size() != 0) {
    DCHECK_EQ(aes_iv_mask.size(), 0u)
        << "Invalid Crypto configuration: aes_iv_mask.size";
    DCHECK_EQ(aes_key.size(), 0u)
        << "Invalid Crypto configuration: aes_key.size";
    return false;
  }
  return true;
}

bool TransportEncryptionHandler::Encrypt(uint32 frame_id,
                                         const base::StringPiece& data,
                                         std::string* encrypted_data) {
  if (!initialized_)
    return false;
  if (!encryptor_->SetCounter(GetAesNonce(frame_id, iv_mask_))) {
    NOTREACHED() << "Failed to set counter";
    return false;
  }
  if (!encryptor_->Encrypt(data, encrypted_data)) {
    NOTREACHED() << "Encrypt error";
    return false;
  }
  return true;
}

bool TransportEncryptionHandler::Decrypt(uint32 frame_id,
                                         const base::StringPiece& ciphertext,
                                         std::string* plaintext) {
  if (!initialized_) {
    return false;
  }
  if (!encryptor_->SetCounter(transport::GetAesNonce(frame_id, iv_mask_))) {
    NOTREACHED() << "Failed to set counter";
    return false;
  }
  if (!encryptor_->Decrypt(ciphertext, plaintext)) {
    VLOG(1) << "Decryption error";
    return false;
  }
  return true;
}

}  // namespace transport
}  // namespace cast
}  // namespace media
