// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/decrypt_context_impl_clearkey.h"

#include <openssl/aes.h>
#include <string.h>

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "crypto/symmetric_key.h"

namespace chromecast {
namespace media {

DecryptContextImplClearKey::DecryptContextImplClearKey(
    crypto::SymmetricKey* key)
    : DecryptContextImpl(KEY_SYSTEM_CLEAR_KEY), key_(key) {
  CHECK(key);
}

DecryptContextImplClearKey::~DecryptContextImplClearKey() {}

void DecryptContextImplClearKey::DecryptAsync(CastDecoderBuffer* buffer,
                                              uint8_t* output,
                                              size_t data_offset,
                                              const DecryptCB& decrypt_cb) {
  decrypt_cb.Run(DoDecrypt(buffer, output, data_offset));
}

bool DecryptContextImplClearKey::DoDecrypt(CastDecoderBuffer* buffer,
                                           uint8_t* output,
                                           size_t data_offset) {
  DCHECK(buffer);
  DCHECK(output);

  if (buffer->end_of_stream())
    return true;

  const CastDecryptConfig* decrypt_config = buffer->decrypt_config();
  if (!decrypt_config || decrypt_config->iv().size() == 0)
    return false;

  // Apply the |data_offset|, if requested.
  output += data_offset;

  // Get the key.
  std::string raw_key;
  if (!key_->GetRawKey(&raw_key)) {
    LOG(ERROR) << "Failed to get the underlying AES key";
    return false;
  }
  DCHECK_EQ(static_cast<int>(raw_key.length()), AES_BLOCK_SIZE);
  const uint8_t* key_u8 = reinterpret_cast<const uint8_t*>(raw_key.data());
  AES_KEY aes_key;
  if (AES_set_encrypt_key(key_u8, AES_BLOCK_SIZE * 8, &aes_key) != 0) {
    LOG(ERROR) << "Failed to set the AES key";
    return buffer;
  }

  // Get the IV.
  uint8_t aes_iv[AES_BLOCK_SIZE];
  DCHECK_EQ(static_cast<int>(decrypt_config->iv().length()), AES_BLOCK_SIZE);
  memcpy(aes_iv, decrypt_config->iv().data(), AES_BLOCK_SIZE);

  // Decryption state.
  unsigned int encrypted_byte_offset = 0;
  uint8_t ecount_buf[AES_BLOCK_SIZE];

  // Perform the decryption.
  const std::vector<SubsampleEntry>& subsamples = decrypt_config->subsamples();
  const uint8_t* data = buffer->data();
  uint32_t offset = 0;
  for (size_t k = 0; k < subsamples.size(); k++) {
    offset += subsamples[k].clear_bytes;
    uint32_t cypher_bytes = subsamples[k].cypher_bytes;
    CHECK_LE(static_cast<size_t>(offset + cypher_bytes), buffer->data_size());
    AES_ctr128_encrypt(data + offset, output + offset, cypher_bytes, &aes_key,
                       aes_iv, ecount_buf, &encrypted_byte_offset);
    offset += cypher_bytes;
  }
  return true;
}

bool DecryptContextImplClearKey::CanDecryptToBuffer() const {
  return true;
}

}  // namespace media
}  // namespace chromecast
