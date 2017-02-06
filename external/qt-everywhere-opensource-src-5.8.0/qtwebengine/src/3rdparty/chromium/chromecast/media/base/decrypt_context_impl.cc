// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/decrypt_context_impl.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {
namespace {
void BufferDecryptCB(bool* called, bool* ret, bool success) {
  DCHECK(called);
  DCHECK(ret);
  *called = true;
  *ret = success;
}
}

DecryptContextImpl::DecryptContextImpl(CastKeySystem key_system)
    : key_system_(key_system) {}

DecryptContextImpl::~DecryptContextImpl() {}

CastKeySystem DecryptContextImpl::GetKeySystem() {
  return key_system_;
}

bool DecryptContextImpl::Decrypt(CastDecoderBuffer* buffer,
                                 std::vector<uint8_t>* output) {
  output->resize(buffer->data_size());
  return Decrypt(buffer, output->data(), 0);
}

bool DecryptContextImpl::Decrypt(CastDecoderBuffer* buffer,
                                 uint8_t* output,
                                 size_t data_offset) {
  bool called = false;
  bool success = false;
  DecryptAsync(buffer, output, data_offset,
               base::Bind(&BufferDecryptCB, &called, &success));
  CHECK(called) << "Sync Decrypt isn't supported";

  return success;
}

void DecryptContextImpl::DecryptAsync(CastDecoderBuffer* buffer,
                                      uint8_t* output,
                                      size_t data_offset,
                                      const DecryptCB& decrypt_cb) {
  decrypt_cb.Run(false);
}

bool DecryptContextImpl::CanDecryptToBuffer() const {
  return false;
}

}  // namespace media
}  // namespace chromecast
