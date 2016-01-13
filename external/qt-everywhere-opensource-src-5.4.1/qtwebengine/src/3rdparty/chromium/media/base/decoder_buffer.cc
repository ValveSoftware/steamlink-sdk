// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"

#include "base/logging.h"
#include "media/base/buffers.h"
#include "media/base/decrypt_config.h"

namespace media {

DecoderBuffer::DecoderBuffer(int size)
    : size_(size),
      side_data_size_(0) {
  Initialize();
}

DecoderBuffer::DecoderBuffer(const uint8* data, int size,
                             const uint8* side_data, int side_data_size)
    : size_(size),
      side_data_size_(side_data_size) {
  if (!data) {
    CHECK_EQ(size_, 0);
    CHECK(!side_data);
    return;
  }

  Initialize();
  memcpy(data_.get(), data, size_);
  if (side_data)
    memcpy(side_data_.get(), side_data, side_data_size_);
}

DecoderBuffer::~DecoderBuffer() {}

void DecoderBuffer::Initialize() {
  CHECK_GE(size_, 0);
  data_.reset(reinterpret_cast<uint8*>(
      base::AlignedAlloc(size_ + kPaddingSize, kAlignmentSize)));
  memset(data_.get() + size_, 0, kPaddingSize);
  if (side_data_size_ > 0) {
    side_data_.reset(reinterpret_cast<uint8*>(
        base::AlignedAlloc(side_data_size_ + kPaddingSize, kAlignmentSize)));
    memset(side_data_.get() + side_data_size_, 0, kPaddingSize);
  }
  splice_timestamp_ = kNoTimestamp();
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(const uint8* data,
                                                     int data_size) {
  // If you hit this CHECK you likely have a bug in a demuxer. Go fix it.
  CHECK(data);
  return make_scoped_refptr(new DecoderBuffer(data, data_size, NULL, 0));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(const uint8* data,
                                                     int data_size,
                                                     const uint8* side_data,
                                                     int side_data_size) {
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
    << " encrypted: " << (decrypt_config_ != NULL)
    << " discard_padding (ms): (" << discard_padding_.first.InMilliseconds()
    << ", " << discard_padding_.second.InMilliseconds() << ")";
  return s.str();
}

void DecoderBuffer::set_timestamp(base::TimeDelta timestamp) {
  DCHECK(!end_of_stream());
  timestamp_ = timestamp;
}

}  // namespace media
