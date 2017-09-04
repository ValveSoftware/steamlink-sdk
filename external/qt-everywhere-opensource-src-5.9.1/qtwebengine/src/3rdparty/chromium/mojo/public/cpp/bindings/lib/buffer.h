// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BUFFER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BUFFER_H_

#include <stddef.h>

#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"

namespace mojo {
namespace internal {

// Buffer provides an interface to allocate memory blocks which are 8-byte
// aligned and zero-initialized. It doesn't own the underlying memory. Users
// must ensure that the memory stays valid while using the allocated blocks from
// Buffer.
class Buffer {
 public:
  Buffer() {}

  // The memory must have been zero-initialized. |data| must be 8-byte
  // aligned.
  void Initialize(void* data, size_t size) {
    DCHECK(IsAligned(data));

    data_ = data;
    size_ = size;
    cursor_ = reinterpret_cast<uintptr_t>(data);
    data_end_ = cursor_ + size;
  }

  size_t size() const { return size_; }

  void* data() const { return data_; }

  // Allocates |num_bytes| from the buffer and returns a pointer to the start of
  // the allocated block.
  // The resulting address is 8-byte aligned, and the content of the memory is
  // zero-filled.
  void* Allocate(size_t num_bytes) {
    num_bytes = Align(num_bytes);
    uintptr_t result = cursor_;
    cursor_ += num_bytes;
    if (cursor_ > data_end_ || cursor_ < result) {
      NOTREACHED();
      cursor_ -= num_bytes;
      return nullptr;
    }

    return reinterpret_cast<void*>(result);
  }

 private:
  void* data_ = nullptr;
  size_t size_ = 0;

  uintptr_t cursor_ = 0;
  uintptr_t data_end_ = 0;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BUFFER_H_
