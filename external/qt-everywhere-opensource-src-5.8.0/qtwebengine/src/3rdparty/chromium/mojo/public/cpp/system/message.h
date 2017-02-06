// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_SYSTEM_MESSAGE_H_
#define MOJO_PUBLIC_CPP_SYSTEM_MESSAGE_H_

#include <limits>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/cpp/system/handle.h"

namespace mojo {

const MojoMessageHandle kInvalidMessageHandleValue =
    MOJO_MESSAGE_HANDLE_INVALID;

// Handle wrapper base class for a |MojoMessageHandle|.
class MessageHandle {
 public:
  MessageHandle() : value_(kInvalidMessageHandleValue) {}
  explicit MessageHandle(MojoMessageHandle value) : value_(value) {}
  ~MessageHandle() {}

  void swap(MessageHandle& other) {
    MojoMessageHandle temp = value_;
    value_ = other.value_;
    other.value_ = temp;
  }

  bool is_valid() const { return value_ != kInvalidMessageHandleValue; }

  const MojoMessageHandle& value() const { return value_; }
  MojoMessageHandle* mutable_value() { return &value_; }
  void set_value(MojoMessageHandle value) { value_ = value; }

  void Close() {
    DCHECK(is_valid());
    MojoResult result = MojoFreeMessage(value_);
    ALLOW_UNUSED_LOCAL(result);
    DCHECK_EQ(MOJO_RESULT_OK, result);
  }

 private:
  MojoMessageHandle value_;
};

using ScopedMessageHandle = ScopedHandleBase<MessageHandle>;

inline MojoResult AllocMessage(size_t num_bytes,
                               const MojoHandle* handles,
                               size_t num_handles,
                               MojoAllocMessageFlags flags,
                               ScopedMessageHandle* handle) {
  DCHECK_LE(num_bytes, std::numeric_limits<uint32_t>::max());
  DCHECK_LE(num_handles, std::numeric_limits<uint32_t>::max());
  MojoMessageHandle raw_handle;
  MojoResult rv = MojoAllocMessage(static_cast<uint32_t>(num_bytes), handles,
                                   static_cast<uint32_t>(num_handles), flags,
                                   &raw_handle);
  if (rv != MOJO_RESULT_OK)
    return rv;

  handle->reset(MessageHandle(raw_handle));
  return MOJO_RESULT_OK;
}

inline MojoResult GetMessageBuffer(MessageHandle message, void** buffer) {
  DCHECK(message.is_valid());
  return MojoGetMessageBuffer(message.value(), buffer);
}

inline MojoResult NotifyBadMessage(MessageHandle message,
                                   const base::StringPiece& error) {
  DCHECK(message.is_valid());
  return MojoNotifyBadMessage(message.value(), error.data(), error.size());
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_SYSTEM_MESSAGE_H_
