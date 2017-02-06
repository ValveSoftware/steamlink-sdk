// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/message_buffer.h"

#include <limits>

#include "mojo/public/cpp/bindings/lib/serialization_util.h"

namespace mojo {
namespace internal {

MessageBuffer::MessageBuffer(size_t capacity, bool zero_initialized) {
  DCHECK_LE(capacity, std::numeric_limits<uint32_t>::max());
  data_num_bytes_ = static_cast<uint32_t>(capacity);

  MojoResult rv = AllocMessage(capacity, nullptr, 0,
                               MOJO_ALLOC_MESSAGE_FLAG_NONE, &message_);
  CHECK_EQ(rv, MOJO_RESULT_OK);

  if (capacity == 0) {
    buffer_ = nullptr;
  } else {
    rv = GetMessageBuffer(message_.get(), &buffer_);
    CHECK_EQ(rv, MOJO_RESULT_OK);

    if (zero_initialized)
      memset(buffer_, 0, capacity);
  }
}

MessageBuffer::MessageBuffer(ScopedMessageHandle message, uint32_t num_bytes) {
  message_ = std::move(message);
  data_num_bytes_ = num_bytes;

  if (num_bytes == 0) {
    buffer_ = nullptr;
  } else {
    MojoResult rv = GetMessageBuffer(message_.get(), &buffer_);
    CHECK_EQ(rv, MOJO_RESULT_OK);
  }
}

MessageBuffer::~MessageBuffer() {}

void* MessageBuffer::Allocate(size_t delta) {
  delta = internal::Align(delta);

  DCHECK_LE(delta, static_cast<size_t>(data_num_bytes_));
  DCHECK_GT(bytes_claimed_ + static_cast<uint32_t>(delta), bytes_claimed_);

  uint32_t new_bytes_claimed = bytes_claimed_ + static_cast<uint32_t>(delta);
  if (new_bytes_claimed > data_num_bytes_) {
    NOTREACHED();
    return nullptr;
  }

  char* start = static_cast<char*>(buffer_) + bytes_claimed_;
  bytes_claimed_ = new_bytes_claimed;
  return static_cast<void*>(start);
}

void MessageBuffer::NotifyBadMessage(const std::string& error) {
  DCHECK(message_.is_valid());
  MojoResult result = mojo::NotifyBadMessage(message_.get(), error);
  DCHECK_EQ(result, MOJO_RESULT_OK);
}

}  // namespace internal
}  // namespace mojo
