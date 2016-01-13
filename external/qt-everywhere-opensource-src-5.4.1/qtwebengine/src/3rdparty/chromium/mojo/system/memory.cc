// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/memory.h"

#include <limits>

#include "base/logging.h"
#include "build/build_config.h"

namespace mojo {
namespace system {

namespace internal {

template <size_t alignment>
bool IsAligned(const void* pointer) {
  return reinterpret_cast<uintptr_t>(pointer) % alignment == 0;
}

// MSVS (2010, 2013) sometimes (on the stack) aligns, e.g., |int64_t|s (for
// which |__alignof(int64_t)| is 8) to 4-byte boundaries. http://goo.gl/Y2n56T
#if defined(COMPILER_MSVC) && defined(ARCH_CPU_32_BITS)
template <>
bool IsAligned<8>(const void* pointer) {
  return reinterpret_cast<uintptr_t>(pointer) % 4 == 0;
}
#endif

template <size_t size, size_t alignment>
bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerHelper(const void* pointer) {
  // TODO(vtl): If running in kernel mode, do a full verification. For now, just
  // check that it's non-null and aligned. (A faster user mode implementation is
  // also possible if this check is skipped.)
  return !!pointer && IsAligned<alignment>(pointer);
}

// Explicitly instantiate the sizes we need. Add instantiations as needed.
template bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerHelper<1, 1>(
    const void*);
template bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerHelper<4, 4>(
    const void*);
template bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerHelper<8, 8>(
    const void*);
// Notwithstanding the comments above about MSVS, whenever we expect an
// alignment of 8 for something of size 4, it's due to an explicit (e.g.,
// #pragma align) alignment specification, which MSVS *does* respect. We want
// this in particular to check that various Options structs are aligned.
#if defined(COMPILER_MSVC) && defined(ARCH_CPU_32_BITS)
template <>
bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerHelper<4, 8>(
    const void* pointer) {
  return !!pointer && reinterpret_cast<uintptr_t>(pointer) % 8 == 0;
}
#else
template MOJO_SYSTEM_IMPL_EXPORT bool VerifyUserPointerHelper<4, 8>(
    const void*);
#endif

template <size_t size, size_t alignment>
bool VerifyUserPointerWithCountHelper(const void* pointer, size_t count) {
  if (count > std::numeric_limits<size_t>::max() / size)
    return false;

  // TODO(vtl): If running in kernel mode, do a full verification. For now, just
  // check that it's non-null and aligned if |count| is nonzero. (A faster user
  // mode implementation is also possible if this check is skipped.)
  return count == 0 || (!!pointer && IsAligned<alignment>(pointer));
}

// Explicitly instantiate the sizes we need. Add instantiations as needed.
template bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerWithCountHelper<1, 1>(
    const void*, size_t);
template bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerWithCountHelper<4, 4>(
    const void*, size_t);
template bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerWithCountHelper<8, 8>(
    const void*, size_t);

}  // nameespace internal

template <size_t alignment>
bool VerifyUserPointerWithSize(const void* pointer, size_t size) {
  // TODO(vtl): If running in kernel mode, do a full verification. For now, just
  // check that it's non-null and aligned. (A faster user mode implementation is
  // also possible if this check is skipped.)
  return size == 0 || (!!pointer && internal::IsAligned<alignment>(pointer));
}

// Explicitly instantiate the alignments we need. Add instantiations as needed.
template bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerWithSize<1>(const void*,
                                                                   size_t);
template bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerWithSize<4>(const void*,
                                                                   size_t);
template bool MOJO_SYSTEM_IMPL_EXPORT VerifyUserPointerWithSize<8>(const void*,
                                                                   size_t);

}  // namespace system
}  // namespace mojo
