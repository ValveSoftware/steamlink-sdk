// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/validation_context.h"

#include "base/logging.h"

namespace mojo {
namespace internal {

ValidationContext::ValidationContext(const void* data,
                                     size_t data_num_bytes,
                                     size_t num_handles,
                                     Message* message,
                                     const base::StringPiece& description,
                                     int stack_depth)
    : message_(message),
      description_(description),
      data_begin_(reinterpret_cast<uintptr_t>(data)),
      data_end_(data_begin_ + data_num_bytes),
      handle_begin_(0),
      handle_end_(static_cast<uint32_t>(num_handles)),
      stack_depth_(stack_depth) {
  if (data_end_ < data_begin_) {
    // The calculation of |data_end_| overflowed.
    // It shouldn't happen but if it does, set the range to empty so
    // IsValidRange() and ClaimMemory() always fail.
    NOTREACHED();
    data_end_ = data_begin_;
  }
  if (handle_end_ < num_handles) {
    // Assigning |num_handles| to |handle_end_| overflowed.
    // It shouldn't happen but if it does, set the handle index range to empty.
    NOTREACHED();
    handle_end_ = 0;
  }
}

ValidationContext::~ValidationContext() {
}

}  // namespace internal
}  // namespace mojo
