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

  MojoResult rv = AllocMessage(capacity, nullptr, 0,
                               MOJO_ALLOC_MESSAGE_FLAG_NONE, &message_);
  CHECK_EQ(rv, MOJO_RESULT_OK);

  void* buffer = nullptr;
  if (capacity != 0) {
    rv = GetMessageBuffer(message_.get(), &buffer);
    CHECK_EQ(rv, MOJO_RESULT_OK);

    if (zero_initialized)
      memset(buffer, 0, capacity);
  }
  Initialize(buffer, capacity);
}

MessageBuffer::MessageBuffer(ScopedMessageHandle message, uint32_t num_bytes) {
  message_ = std::move(message);

  void* buffer = nullptr;
  if (num_bytes != 0) {
    MojoResult rv = GetMessageBuffer(message_.get(), &buffer);
    CHECK_EQ(rv, MOJO_RESULT_OK);
  }
  Initialize(buffer, num_bytes);
}

MessageBuffer::~MessageBuffer() {}

void MessageBuffer::NotifyBadMessage(const std::string& error) {
  DCHECK(message_.is_valid());
  MojoResult result = mojo::NotifyBadMessage(message_.get(), error);
  DCHECK_EQ(result, MOJO_RESULT_OK);
}

}  // namespace internal
}  // namespace mojo
