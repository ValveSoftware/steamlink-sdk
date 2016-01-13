// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "allocator_shim/allocator_stub.h"

#if defined(OS_MACOSX) || defined(OS_ANDROID)
#error "The allocator stub isn't supported (or needed) on mac or android."
#endif

void* Allocate(std::size_t n) {
  return operator new(n);
}

void Dellocate(void* p) {
  return operator delete(p);
}
