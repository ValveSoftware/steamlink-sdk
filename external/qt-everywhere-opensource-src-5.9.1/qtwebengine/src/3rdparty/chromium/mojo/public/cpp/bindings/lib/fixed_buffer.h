// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_FIXED_BUFFER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_FIXED_BUFFER_H_

#include <stddef.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"

namespace mojo {
namespace internal {

// FixedBufferForTesting owns its buffer. The Leak method may be used to steal
// the underlying memory.
class MOJO_CPP_BINDINGS_EXPORT FixedBufferForTesting
    : NON_EXPORTED_BASE(public Buffer) {
 public:
  explicit FixedBufferForTesting(size_t size);
  ~FixedBufferForTesting();

  // Returns the internal memory owned by the Buffer to the caller. The Buffer
  // relinquishes its pointer, effectively resetting the state of the Buffer
  // and leaving the caller responsible for freeing the returned memory address
  // when no longer needed.
  void* Leak();

 private:
  DISALLOW_COPY_AND_ASSIGN(FixedBufferForTesting);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_FIXED_BUFFER_H_
