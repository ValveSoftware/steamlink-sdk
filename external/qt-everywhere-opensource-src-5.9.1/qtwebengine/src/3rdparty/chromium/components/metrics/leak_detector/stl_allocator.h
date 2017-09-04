// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_LEAK_DETECTOR_STL_ALLOCATOR_H_
#define COMPONENTS_METRICS_LEAK_DETECTOR_STL_ALLOCATOR_H_

#include <stddef.h>

#include <limits>
#include <memory>

#include "base/logging.h"

// Generic allocator class for STL objects.
// deallocate() to use the template class Alloc's allocation.
// that uses a given type-less allocator Alloc, which must provide:
//   static void* Alloc::Allocate(size_t size);
//   static void Alloc::Free(void* ptr, size_t size);
//
// Inherits from the default allocator, std::allocator. Overrides allocate() and
// deallocate() and some other functions.
//
// STLAllocator<T, MyAlloc> provides the same thread-safety guarantees as
// MyAlloc.
//
// Usage example:
//   set<T, less<T>, STLAllocator<T, MyAlloc> > my_set;

template <typename T, class Alloc>
class STLAllocator : public std::allocator<T> {
 public:
  typedef size_t size_type;
  typedef T* pointer;

  template <class T1>
  struct rebind {
    typedef STLAllocator<T1, Alloc> other;
  };

  STLAllocator() {}
  explicit STLAllocator(const STLAllocator&) {}
  template <class T1>
  STLAllocator(const STLAllocator<T1, Alloc>&) {}
  ~STLAllocator() {}

  pointer allocate(size_type n, const void* = 0) {
    // Make sure the computation of the total allocation size does not cause an
    // integer overflow.
    RAW_CHECK(n < max_size());
    return static_cast<T*>(Alloc::Allocate(n * sizeof(T)));
  }

  void deallocate(pointer p, size_type n) { Alloc::Free(p, n * sizeof(T)); }

  size_type max_size() const {
    return std::numeric_limits<size_t>::max() / sizeof(T);
  }
};

#endif  // COMPONENTS_METRICS_LEAK_DETECTOR_STL_ALLOCATOR_H_
