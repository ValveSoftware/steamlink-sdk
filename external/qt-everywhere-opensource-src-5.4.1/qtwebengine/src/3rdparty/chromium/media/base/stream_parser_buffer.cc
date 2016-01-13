// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/stream_parser_buffer.h"

#include "base/logging.h"
#include "media/base/buffers.h"

namespace media {

static scoped_refptr<StreamParserBuffer> CopyBuffer(
    const StreamParserBuffer& buffer) {
  if (buffer.end_of_stream())
    return StreamParserBuffer::CreateEOSBuffer();

  scoped_refptr<StreamParserBuffer> copied_buffer =
      StreamParserBuffer::CopyFrom(buffer.data(),
                                   buffer.data_size(),
                                   buffer.side_data(),
                                   buffer.side_data_size(),
                                   buffer.IsKeyframe(),
                                   buffer.type(),
                                   buffer.track_id());
  copied_buffer->SetDecodeTimestamp(buffer.GetDecodeTimestamp());
  copied_buffer->SetConfigId(buffer.GetConfigId());
  copied_buffer->set_timestamp(buffer.timestamp());
  copied_buffer->set_duration(buffer.duration());
  copied_buffer->set_discard_padding(buffer.discard_padding());
  copied_buffer->set_splice_timestamp(buffer.splice_timestamp());
  const DecryptConfig* decrypt_config = buffer.decrypt_config();
  if (decrypt_config) {
    copied_buffer->set_decrypt_config(
        make_scoped_ptr(new DecryptConfig(decrypt_config->key_id(),
                                          decrypt_config->iv(),
                                          decrypt_config->subsamples())));
  }

  return copied_buffer;
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CreateEOSBuffer() {
  return make_scoped_refptr(new StreamParserBuffer(NULL, 0, NULL, 0, false,
                                                   DemuxerStream::UNKNOWN, 0));
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CopyFrom(
    const uint8* data, int data_size, bool is_keyframe, Type type,
    TrackId track_id) {
  return make_scoped_refptr(
      new StreamParserBuffer(data, data_size, NULL, 0, is_keyframe, type,
                             track_id));
}

scoped_refptr<StreamParserBuffer> StreamParserBuffer::CopyFrom(
    const uint8* data, int data_size,
    const uint8* side_data, int side_data_size,
    bool is_keyframe, Type type, TrackId track_id) {
  return make_scoped_refptr(
      new StreamParserBuffer(data, data_size, side_data, side_data_size,
                             is_keyframe, type, track_id));
}

base::TimeDelta StreamParserBuffer::GetDecodeTimestamp() const {
  if (decode_timestamp_ == kNoTimestamp())
    return timestamp();
  return decode_timestamp_;
}

void StreamParserBuffer::SetDecodeTimestamp(base::TimeDelta timestamp) {
  decode_timestamp_ = timestamp;
  if (preroll_buffer_)
    preroll_buffer_->SetDecodeTimestamp(timestamp);
}

StreamParserBuffer::StreamParserBuffer(const uint8* data, int data_size,
                                       const uint8* side_data,
                                       int side_data_size, bool is_keyframe,
                                       Type type, TrackId track_id)
    : DecoderBuffer(data, data_size, side_data, side_data_size),
      is_keyframe_(is_keyframe),
      decode_timestamp_(kNoTimestamp()),
      config_id_(kInvalidConfigId),
      type_(type),
      track_id_(track_id) {
  // TODO(scherkus): Should DataBuffer constructor accept a timestamp and
  // duration to force clients to set them? Today they end up being zero which
  // is both a common and valid value and could lead to bugs.
  if (data) {
    set_duration(kNoTimestamp());
  }
}

StreamParserBuffer::~StreamParserBuffer() {}

int StreamParserBuffer::GetConfigId() const {
  return config_id_;
}

void StreamParserBuffer::SetConfigId(int config_id) {
  config_id_ = config_id;
  if (preroll_buffer_)
    preroll_buffer_->SetConfigId(config_id);
}

void StreamParserBuffer::ConvertToSpliceBuffer(
    const BufferQueue& pre_splice_buffers) {
  DCHECK(splice_buffers_.empty());
  DCHECK(!end_of_stream());

  // Make a copy of this first, before making any changes.
  scoped_refptr<StreamParserBuffer> overlapping_buffer = CopyBuffer(*this);
  overlapping_buffer->set_splice_timestamp(kNoTimestamp());

  const scoped_refptr<StreamParserBuffer>& first_splice_buffer =
      pre_splice_buffers.front();

  // Ensure the given buffers are actually before the splice point.
  DCHECK(first_splice_buffer->timestamp() <= overlapping_buffer->timestamp());

  // TODO(dalecurtis): We should also clear |data| and |side_data|, but since
  // that implies EOS care must be taken to ensure there are no clients relying
  // on that behavior.

  // Move over any preroll from this buffer.
  if (preroll_buffer_) {
    DCHECK(!overlapping_buffer->preroll_buffer_);
    overlapping_buffer->preroll_buffer_.swap(preroll_buffer_);
  }

  // Rewrite |this| buffer as a splice buffer.
  SetDecodeTimestamp(first_splice_buffer->GetDecodeTimestamp());
  SetConfigId(first_splice_buffer->GetConfigId());
  set_timestamp(first_splice_buffer->timestamp());
  is_keyframe_ = first_splice_buffer->IsKeyframe();
  type_ = first_splice_buffer->type();
  track_id_ = first_splice_buffer->track_id();
  set_splice_timestamp(overlapping_buffer->timestamp());

  // The splice duration is the duration of all buffers before the splice plus
  // the highest ending timestamp after the splice point.
  set_duration(
      std::max(overlapping_buffer->timestamp() + overlapping_buffer->duration(),
               pre_splice_buffers.back()->timestamp() +
                   pre_splice_buffers.back()->duration()) -
      first_splice_buffer->timestamp());

  // Copy all pre splice buffers into our wrapper buffer.
  for (BufferQueue::const_iterator it = pre_splice_buffers.begin();
       it != pre_splice_buffers.end();
       ++it) {
    const scoped_refptr<StreamParserBuffer>& buffer = *it;
    DCHECK(!buffer->end_of_stream());
    DCHECK(!buffer->preroll_buffer());
    DCHECK(buffer->splice_buffers().empty());
    splice_buffers_.push_back(CopyBuffer(*buffer));
    splice_buffers_.back()->set_splice_timestamp(splice_timestamp());
  }

  splice_buffers_.push_back(overlapping_buffer);
}

void StreamParserBuffer::SetPrerollBuffer(
    const scoped_refptr<StreamParserBuffer>& preroll_buffer) {
  DCHECK(!preroll_buffer_);
  DCHECK(!end_of_stream());
  DCHECK(!preroll_buffer->end_of_stream());
  DCHECK(!preroll_buffer->preroll_buffer_);
  DCHECK(preroll_buffer->splice_timestamp() == kNoTimestamp());
  DCHECK(preroll_buffer->splice_buffers().empty());
  DCHECK(preroll_buffer->timestamp() <= timestamp());
  DCHECK(preroll_buffer->discard_padding() == DecoderBuffer::DiscardPadding());
  DCHECK_EQ(preroll_buffer->type(), type());
  DCHECK_EQ(preroll_buffer->track_id(), track_id());

  preroll_buffer_ = preroll_buffer;
  preroll_buffer_->set_timestamp(timestamp());
  preroll_buffer_->SetDecodeTimestamp(GetDecodeTimestamp());

  // Mark the entire buffer for discard.
  preroll_buffer_->set_discard_padding(
      std::make_pair(kInfiniteDuration(), base::TimeDelta()));
}

void StreamParserBuffer::set_timestamp(base::TimeDelta timestamp) {
  DecoderBuffer::set_timestamp(timestamp);
  if (preroll_buffer_)
    preroll_buffer_->set_timestamp(timestamp);
}

}  // namespace media
