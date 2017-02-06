// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_H_
#define CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "chromecast/public/media/cast_key_system.h"
#include "chromecast/public/media/decrypt_context.h"

namespace chromecast {
namespace media {

// Base class of a decryption context: a decryption context gathers all the
// information needed to decrypt frames with a given key id.
// Each CDM should implement this and add fields needed to fully describe a
// decryption context.
class DecryptContextImpl : public DecryptContext {
 public:
  using DecryptCB = base::Callback<void(bool)>;

  explicit DecryptContextImpl(CastKeySystem key_system);
  ~DecryptContextImpl() override;

  // DecryptContext implementation:
  CastKeySystem GetKeySystem() override;
  bool Decrypt(CastDecoderBuffer* buffer,
               std::vector<uint8_t>* output) final;

  // TODO(smcgruer): Replace DecryptContext::Decrypt with this one in next
  // public api releasing.
  // Decrypts the given buffer. Returns true/false for success/failure.
  //
  // The decrypted data will be of size |buffer.data_size()| and there must be
  // enough space in |output| to store that data.
  //
  // If non-zero, |data_offset| specifies an offset to be applied to |output|
  // before the decrypted data is written.
  virtual bool Decrypt(CastDecoderBuffer* buffer,
                       uint8_t* output,
                       size_t data_offset);

  // Similar as the above one. Decryption success or not will be returned in
  // |decrypt_cb|. |decrypt_cb| will be called on caller's thread.
  virtual void DecryptAsync(CastDecoderBuffer* buffer,
                            uint8_t* output,
                            size_t data_offset,
                            const DecryptCB& decrypt_cb);

  // Returns whether the data can be decrypted into user memory.
  // If the key system doesn't support secure output or the app explicitly
  // requires non secure output, it should return true;
  // If the key system doesn't allow clear content to be decrypted into user
  // memory, it should return false.
  virtual bool CanDecryptToBuffer() const;

 private:
  CastKeySystem key_system_;

  // TODO(smcgruer): Restore macro usage next public API release.
  // DISALLOW_COPY_AND_ASSIGN(DecryptContextImpl);
  DecryptContextImpl(const DecryptContextImpl&) = delete;
  void operator=(const DecryptContextImpl&) = delete;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_DECRYPT_CONTEXT_IMPL_H_
