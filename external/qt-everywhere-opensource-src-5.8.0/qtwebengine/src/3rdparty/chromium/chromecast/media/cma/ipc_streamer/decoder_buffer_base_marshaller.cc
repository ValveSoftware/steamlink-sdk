// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/ipc_streamer/decoder_buffer_base_marshaller.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "chromecast/media/cma/base/cast_decrypt_config_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/ipc/media_message.h"
#include "chromecast/media/cma/ipc/media_message_type.h"
#include "chromecast/media/cma/ipc_streamer/decrypt_config_marshaller.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"

namespace chromecast {
namespace media {

namespace {
const size_t kMaxFrameSize = 4 * 1024 * 1024;

class DecoderBufferFromMsg : public DecoderBufferBase {
 public:
  explicit DecoderBufferFromMsg(std::unique_ptr<MediaMessage> msg);

  void Initialize();

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
  ~DecoderBufferFromMsg() override;

  // Indicates whether this is an end of stream frame.
  bool is_eos_;

  // Stream Id this decoder buffer belongs to.
  StreamId stream_id_;

  // Frame timestamp.
  base::TimeDelta pts_;

  // CENC parameters.
  std::unique_ptr<CastDecryptConfig> decrypt_config_;

  // Size of the frame.
  size_t data_size_;

  // Keeps the message since frame data is not copied.
  std::unique_ptr<MediaMessage> msg_;
  uint8_t* data_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBufferFromMsg);
};

DecoderBufferFromMsg::DecoderBufferFromMsg(std::unique_ptr<MediaMessage> msg)
    : is_eos_(true), stream_id_(kPrimary), msg_(std::move(msg)), data_(NULL) {
  CHECK(msg_);
}

DecoderBufferFromMsg::~DecoderBufferFromMsg() {
}

void DecoderBufferFromMsg::Initialize() {
  CHECK_EQ(msg_->type(), FrameMediaMsg);

  CHECK(msg_->ReadPod(&is_eos_));
  if (is_eos_)
    return;

  CHECK(msg_->ReadPod(&stream_id_));

  int64_t pts_internal = 0;
  CHECK(msg_->ReadPod(&pts_internal));
  pts_ = base::TimeDelta::FromMicroseconds(pts_internal);

  bool has_decrypt_config = false;
  CHECK(msg_->ReadPod(&has_decrypt_config));
  if (has_decrypt_config)
    decrypt_config_.reset(DecryptConfigMarshaller::Read(msg_.get()).release());

  CHECK(msg_->ReadPod(&data_size_));
  CHECK_GT(data_size_, 0u);
  CHECK_LT(data_size_, kMaxFrameSize);

  // Get a pointer to the frame data inside the message.
  // Avoid copying the frame data here.
  data_ = static_cast<uint8_t*>(msg_->GetWritableBuffer(data_size_));
  CHECK(data_);

  if (decrypt_config_) {
    uint32_t subsample_total_size = 0;
    for (size_t k = 0; k < decrypt_config_->subsamples().size(); k++) {
      subsample_total_size += decrypt_config_->subsamples()[k].clear_bytes;
      subsample_total_size += decrypt_config_->subsamples()[k].cypher_bytes;
    }
    CHECK_EQ(subsample_total_size, data_size_);
  }
}

StreamId DecoderBufferFromMsg::stream_id() const {
  return stream_id_;
}

int64_t DecoderBufferFromMsg::timestamp() const {
  return pts_.InMicroseconds();
}

void DecoderBufferFromMsg::set_timestamp(base::TimeDelta timestamp) {
  pts_ = timestamp;
}

const uint8_t* DecoderBufferFromMsg::data() const {
  CHECK(msg_->IsSerializedMsgAvailable());
  return data_;
}

uint8_t* DecoderBufferFromMsg::writable_data() const {
  CHECK(msg_->IsSerializedMsgAvailable());
  return data_;
}

size_t DecoderBufferFromMsg::data_size() const {
  return data_size_;
}

const CastDecryptConfig* DecoderBufferFromMsg::decrypt_config() const {
  return decrypt_config_.get();
}

bool DecoderBufferFromMsg::end_of_stream() const {
  return is_eos_;
}

scoped_refptr<::media::DecoderBuffer>
DecoderBufferFromMsg::ToMediaBuffer() const {
  if (is_eos_)
    return ::media::DecoderBuffer::CreateEOSBuffer();

  scoped_refptr<::media::DecoderBuffer> copy =
      ::media::DecoderBuffer::CopyFrom(data(), data_size());
  copy->set_timestamp(base::TimeDelta::FromMicroseconds(timestamp()));
  return copy;
}

}  // namespace

// static
void DecoderBufferBaseMarshaller::Write(
    const scoped_refptr<DecoderBufferBase>& buffer,
    MediaMessage* msg) {
  CHECK(msg->WritePod(buffer->end_of_stream()));
  if (buffer->end_of_stream())
    return;

  CHECK(msg->WritePod(buffer->stream_id()));
  CHECK(msg->WritePod(buffer->timestamp()));

  bool has_decrypt_config = buffer->decrypt_config() != nullptr;
  CHECK(msg->WritePod(has_decrypt_config));

  if (has_decrypt_config)
    DecryptConfigMarshaller::Write(*buffer->decrypt_config(), msg);

  CHECK(msg->WritePod(buffer->data_size()));
  CHECK(msg->WriteBuffer(buffer->data(), buffer->data_size()));
}

// static
scoped_refptr<DecoderBufferBase> DecoderBufferBaseMarshaller::Read(
    std::unique_ptr<MediaMessage> msg) {
  scoped_refptr<DecoderBufferFromMsg> buffer(
      new DecoderBufferFromMsg(std::move(msg)));
  buffer->Initialize();
  return buffer;
}

}  // namespace media
}  // namespace chromecast
