// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"

#include <stdlib.h>

namespace mojo {
namespace internal {

FixedBufferForTesting::FixedBufferForTesting(size_t size) {
  size = internal::Align(size);
  // Use calloc here to ensure all message memory is zero'd out.
  void* ptr = calloc(size, 1);
  Initialize(ptr, size);
}

FixedBufferForTesting::~FixedBufferForTesting() {
  free(data());
}

void* FixedBufferForTesting::Leak() {
  void* ptr = data();
  Initialize(nullptr, 0);
  return ptr;
}

}  // namespace internal
}  // namespace mojo
