// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_MEMORY_H_
#define MOJO_SYSTEM_MEMORY_H_

#include <stddef.h>

#include "mojo/public/c/system/macros.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

namespace internal {

template <size_t size, size_t alignment>
bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerHelper(const void* pointer);

// Note: This is also used by options_validation.h.
template <size_t size, size_t alignment>
bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerWithCountHelper(
    const void* pointer,
    size_t count);

}  // namespace internal

// Verify (insofar as possible/necessary) that a |T| can be read from the user
// |pointer|.
template <typename T>
bool VerifyUserPointer(const T* pointer) {
  return internal::VerifyUserPointerHelper<sizeof(T), MOJO_ALIGNOF(T)>(pointer);
}

// Verify (insofar as possible/necessary) that |count| |T|s can be read from the
// user |pointer|; |count| may be zero. (This is done carefully since |count *
// sizeof(T)| may overflow a |size_t|.)
template <typename T>
bool VerifyUserPointerWithCount(const T* pointer, size_t count) {
  return internal::VerifyUserPointerWithCountHelper<sizeof(T),
                                                    MOJO_ALIGNOF(T)>(pointer,
                                                                     count);
}

// Verify that |size| bytes (which may be zero) can be read from the user
// |pointer|, and that |pointer| has the specified |alignment| (if |size| is
// nonzero).
template <size_t alignment>
bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerWithSize(const void* pointer,
                                                       size_t size);

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_MEMORY_H_
