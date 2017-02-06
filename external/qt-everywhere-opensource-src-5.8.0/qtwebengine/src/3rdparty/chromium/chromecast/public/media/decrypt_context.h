// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_DECRYPT_CONTEXT_H_
#define CHROMECAST_PUBLIC_MEDIA_DECRYPT_CONTEXT_H_

#include <stdint.h>
#include <vector>

#include "cast_key_system.h"

namespace chromecast {
namespace media {
class CastDecoderBuffer;

// Provides the information needed to decrypt frames.
class DecryptContext {
 public:
  virtual ~DecryptContext() {}

  // Get the key system to use for decryption.
  virtual CastKeySystem GetKeySystem() = 0;

  // Decrypts the given buffer.  Returns true/false for success/failure,
  // and places the decrypted data in output if successful.
  virtual bool Decrypt(CastDecoderBuffer* buffer,
                       std::vector<uint8_t>* output) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_DECRYPT_CONTEXT_H_
