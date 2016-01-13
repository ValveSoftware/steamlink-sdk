// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "allocator_shim/allocator_stub.h"

#if !defined(LIBPEERCONNECTION_IMPLEMENTATION) || defined(LIBPEERCONNECTION_LIB)
#error "Only compile the allocator proxy with the shared_library implementation"
#endif

#if defined(OS_MACOSX) || defined(OS_ANDROID)
#error "The allocator proxy isn't supported (or needed) on mac or android."
#endif

extern AllocateFunction g_alloc;
extern DellocateFunction g_dealloc;

// Override the global new/delete routines and proxy them over to the allocator
// routines handed to us via InitializeModule.

void* operator new(std::size_t n) throw() {
  return g_alloc(n);
}

void operator delete(void* p) throw() {
  g_dealloc(p);
}
