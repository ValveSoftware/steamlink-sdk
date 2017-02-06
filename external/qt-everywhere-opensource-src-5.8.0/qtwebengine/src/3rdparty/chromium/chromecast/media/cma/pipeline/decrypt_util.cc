// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/decrypt_util.h"

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chromecast/media/base/decrypt_context_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "media/base/decoder_buffer.h"

namespace chromecast {
namespace media {

namespace {

class DecoderBufferClear : public DecoderBufferBase {
 public:
  explicit DecoderBufferClear(scoped_refptr<DecoderBufferBase> buffer);

  // DecoderBufferBase implementation.
  StreamId stream_id() const override;
  int64_t timestamp() const override;
  void set_timestamp(base::TimeDelta timestamp) override;
  const uint8_t* data() const override;
  uint8_t* writable_data() const override;
  size_t data_size() const override;
  const CastDecryptConfig* decrypt_config() const override;
  bool end_of_stream() const override;
  scoped_refptr<::media::DecoderBuffer> ToMediaBuffer() const override;

 private:
  ~DecoderBufferClear() override;

  scoped_refptr<DecoderBufferBase> const buffer_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBufferClear);
};

DecoderBufferClear::DecoderBufferClear(scoped_refptr<DecoderBufferBase> buffer)
    : buffer_(buffer) {}

DecoderBufferClear::~DecoderBufferClear() {
}

StreamId DecoderBufferClear::stream_id() const {
  return buffer_->stream_id();
}

int64_t DecoderBufferClear::timestamp() const {
  return buffer_->timestamp();
}

void DecoderBufferClear::set_timestamp(base::TimeDelta timestamp) {
  buffer_->set_timestamp(timestamp);
}

const uint8_t* DecoderBufferClear::data() const {
  return buffer_->data();
}

uint8_t* DecoderBufferClear::writable_data() const {
  return buffer_->writable_data();
}

size_t DecoderBufferClear::data_size() const {
  return buffer_->data_size();
}

const CastDecryptConfig* DecoderBufferClear::decrypt_config() const {
  // Buffer is clear so no decryption info.
  return nullptr;
}

bool DecoderBufferClear::end_of_stream() const {
  return buffer_->end_of_stream();
}

scoped_refptr<::media::DecoderBuffer>
DecoderBufferClear::ToMediaBuffer() const {
  return buffer_->ToMediaBuffer();
}

void OnBufferDecrypted(scoped_refptr<DecoderBufferBase> buffer,
                       const BufferDecryptedCB& buffer_decrypted_cb,
                       bool success) {
  scoped_refptr<DecoderBufferBase> out_buffer =
      success ? new DecoderBufferClear(buffer) : buffer;
  buffer_decrypted_cb.Run(out_buffer, success);
}
}  // namespace

void DecryptDecoderBuffer(scoped_refptr<DecoderBufferBase> buffer,
                          DecryptContextImpl* decrypt_ctxt,
                          const BufferDecryptedCB& buffer_decrypted_cb) {
  decrypt_ctxt->DecryptAsync(
      buffer.get(), buffer->writable_data(), 0,
      base::Bind(&OnBufferDecrypted, buffer, buffer_decrypted_cb));
}

}  // namespace media
}  // namespace chromecast
