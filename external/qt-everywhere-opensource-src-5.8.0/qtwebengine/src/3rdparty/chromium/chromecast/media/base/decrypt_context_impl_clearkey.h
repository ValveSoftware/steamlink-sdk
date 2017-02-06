// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_CLEARKEY_H_
#define CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_CLEARKEY_H_

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "chromecast/media/base/decrypt_context_impl.h"

namespace crypto {
class SymmetricKey;
}

namespace chromecast {
namespace media {

class DecryptContextImplClearKey : public DecryptContextImpl {
 public:
  // Note: DecryptContextClearKey does not take ownership of |key|.
  explicit DecryptContextImplClearKey(crypto::SymmetricKey* key);
  ~DecryptContextImplClearKey() override;

  // DecryptContextImpl implementation.
  void DecryptAsync(CastDecoderBuffer* buffer,
                    uint8_t* output,
                    size_t data_offset,
                    const DecryptCB& decrypt_cb) override;

  bool CanDecryptToBuffer() const override;

 private:
  bool DoDecrypt(CastDecoderBuffer* buffer,
                 uint8_t* output,
                 size_t data_offset);
  crypto::SymmetricKey* const key_;

  DISALLOW_COPY_AND_ASSIGN(DecryptContextImplClearKey);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_CLEARKEY_H_
