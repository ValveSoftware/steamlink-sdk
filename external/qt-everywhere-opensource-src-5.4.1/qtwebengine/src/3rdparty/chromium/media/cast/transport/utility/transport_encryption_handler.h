// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_TRANSPORT_UTILITY_ENCRYPTION_HANDLER_H_
#define MEDIA_CAST_TRANSPORT_TRANSPORT_UTILITY_ENCRYPTION_HANDLER_H_

// Helper class to handle encryption for the Cast Transport library.
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/threading/non_thread_safe.h"

namespace crypto {
class Encryptor;
class SymmetricKey;
}

namespace media {
namespace cast {
namespace transport {

class TransportEncryptionHandler : public base::NonThreadSafe {
 public:
  TransportEncryptionHandler();
  ~TransportEncryptionHandler();

  bool Initialize(std::string aes_key, std::string aes_iv_mask);

  bool Encrypt(uint32 frame_id,
               const base::StringPiece& data,
               std::string* encrypted_data);

  bool Decrypt(uint32 frame_id,
               const base::StringPiece& ciphertext,
               std::string* plaintext);

  // TODO(miu): This naming is very misleading.  It should be called
  // is_activated() since Initialize() without keys (i.e., cypto is disabled)
  // may have succeeded.
  bool initialized() const { return initialized_; }

 private:
  scoped_ptr<crypto::SymmetricKey> key_;
  scoped_ptr<crypto::Encryptor> encryptor_;
  std::string iv_mask_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(TransportEncryptionHandler);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_TRANSPORT_UTILITY_ENCRYPTION_HANDLER_H_
