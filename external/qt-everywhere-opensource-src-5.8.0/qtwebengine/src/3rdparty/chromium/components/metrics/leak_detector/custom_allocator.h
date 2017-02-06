// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_CUSTOM_ALLOCATOR_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_CUSTOM_ALLOCATOR_H_

#include <stddef.h>

#include <type_traits>

namespace metrics {
namespace leak_detector {

// Custom allocator class to be passed to STLAllocator as a template argument.
//
// By default, CustomAllocator uses the default allocator (new/delete), but the
// caller of Initialize ()can provide a pair of alternative alloc/ free
// functions to use as an external allocator.
//
// This is a stateless class, but there is static data within the module that
// needs to be created and deleted.
//
// Not thread-safe.
class CustomAllocator {
 public:
  using AllocFunc = std::add_pointer<void*(size_t)>::type;
  using FreeFunc = std::add_pointer<void(void*, size_t)>::type;

  // Initialize CustomAllocator to use the default allocator.
  static void Initialize();

  // Initialize CustomAllocator to use the given alloc/free functions.
  static void Initialize(AllocFunc alloc_func, FreeFunc free_func);

  // Performs any cleanup required, e.g. unset custom functions. Returns true
  // on success or false if something failed.
  static bool Shutdown();

  static bool IsInitialized();

  // These functions must match the specifications in STLAllocator.
  static void* Allocate(size_t size);
  static void Free(void* ptr, size_t size);
};

}  // namespace leak_detector
}  // namespace metrics

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_CUSTOM_ALLOCATOR_H_
