// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"

namespace media {

// Allocates a block of memory which is padded for use with the SIMD
// optimizations used by FFmpeg.
static uint8_t* AllocateFFmpegSafeBlock(size_t size) {
  uint8_t* const block = reinterpret_cast<uint8_t*>(base::AlignedAlloc(
      size + DecoderBuffer::kPaddingSize, DecoderBuffer::kAlignmentSize));
  memset(block + size, 0, DecoderBuffer::kPaddingSize);
  return block;
}

DecoderBuffer::DecoderBuffer(size_t size)
    : size_(size), side_data_size_(0), is_key_frame_(false) {
  Initialize();
}

DecoderBuffer::DecoderBuffer(const uint8_t* data,
                             size_t size,
                             const uint8_t* side_data,
                             size_t side_data_size)
    : size_(size), side_data_size_(side_data_size), is_key_frame_(false) {
  if (!data) {
    CHECK_EQ(size_, 0u);
    CHECK(!side_data);
    return;
  }

  Initialize();

  memcpy(data_.get(), data, size_);

  if (!side_data) {
    CHECK_EQ(side_data_size, 0u);
    return;
  }

  DCHECK_GT(side_data_size_, 0u);
  memcpy(side_data_.get(), side_data, side_data_size_);
}

DecoderBuffer::~DecoderBuffer() {}

void DecoderBuffer::Initialize() {
  data_.reset(AllocateFFmpegSafeBlock(size_));
  if (side_data_size_ > 0)
    side_data_.reset(AllocateFFmpegSafeBlock(side_data_size_));
  splice_timestamp_ = kNoTimestamp();
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(const uint8_t* data,
                                                     size_t data_size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return make_scoped_refptr(new DecoderBuffer(data, data_size, NULL, 0));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(const uint8_t* data,
                                                     size_t data_size,
                                                     const uint8_t* side_data,
                                                     size_t side_data_size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  CHECK(side_data);
  return make_scoped_refptr(new DecoderBuffer(data, data_size,
                                              side_data, side_data_size));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CreateEOSBuffer() {
  return make_scoped_refptr(new DecoderBuffer(NULL, 0, NULL, 0));
}

std::string DecoderBuffer::AsHumanReadableString() {
  if (end_of_stream()) {
    return "end of stream";
  }

  std::ostringstream s;
  s << "timestamp: " << timestamp_.InMicroseconds()
    << " duration: " << duration_.InMicroseconds()
    << " size: " << size_
    << " side_data_size: " << side_data_size_
    << " is_key_frame: " << is_key_frame_
    << " encrypted: " << (decrypt_config_ != NULL)
    << " discard_padding (ms): (" << discard_padding_.first.InMilliseconds()
    << ", " << discard_padding_.second.InMilliseconds() << ")";

  if (decrypt_config_)
    s << " decrypt:" << (*decrypt_config_);

  return s.str();
}

void DecoderBuffer::set_timestamp(base::TimeDelta timestamp) {
  DCHECK(!end_of_stream());
  timestamp_ = timestamp;
}

void DecoderBuffer::CopySideDataFrom(const uint8_t* side_data,
                                     size_t side_data_size) {
  if (side_data_size > 0) {
    side_data_size_ = side_data_size;
    side_data_.reset(AllocateFFmpegSafeBlock(side_data_size_));
    memcpy(side_data_.get(), side_data, side_data_size_);
  } else {
    side_data_.reset();
    side_data_size_ = 0;
  }
}

}  // namespace media
