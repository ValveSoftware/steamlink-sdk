// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_ASHMEM_ALLOCATOR_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_ASHMEM_ALLOCATOR_H_

#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/synchronization/lock.h"

namespace base {
namespace internal {

class AshmemRegion;

// Internal class, whose instances are returned to the client of the allocator
// (e.g. DiscardableMemoryAshmem), that mimicks the DiscardableMemory interface.
class BASE_EXPORT_PRIVATE DiscardableAshmemChunk {
 public:
  ~DiscardableAshmemChunk();

  // Returns whether the memory is still resident.
  bool Lock();

  void Unlock();

  void* Memory() const;

 private:
  friend class AshmemRegion;

  DiscardableAshmemChunk(AshmemRegion* ashmem_region,
                         int fd,
                         void* address,
                         size_t offset,
                         size_t size);

  AshmemRegion* const ashmem_region_;
  const int fd_;
  void* const address_;
  const size_t offset_;
  const size_t size_;
  bool locked_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableAshmemChunk);
};

// Ashmem regions are backed by a file (descriptor) therefore they are a limited
// resource. This allocator minimizes the problem by allocating large ashmem
// regions internally and returning smaller chunks to the client.
// Allocated chunks are systematically aligned on a page boundary therefore this
// allocator should not be used for small allocations.
class BASE_EXPORT_PRIVATE DiscardableMemoryAshmemAllocator {
 public:
  // Note that |name| is only used for debugging/measurement purposes.
  // |ashmem_region_size| is the size that will be used to create the underlying
  // ashmem regions and is expected to be greater or equal than 32 MBytes.
  DiscardableMemoryAshmemAllocator(const std::string& name,
                                   size_t ashmem_region_size);

  ~DiscardableMemoryAshmemAllocator();

  // Note that the allocator must outlive the returned DiscardableAshmemChunk
  // instance.
  scoped_ptr<DiscardableAshmemChunk> Allocate(size_t size);

  // Returns the size of the last ashmem region which was created. This is used
  // for testing only.
  size_t last_ashmem_region_size() const;

 private:
  friend class AshmemRegion;

  void DeleteAshmemRegion_Locked(AshmemRegion* region);

  const std::string name_;
  const size_t ashmem_region_size_;
  mutable Lock lock_;
  size_t last_ashmem_region_size_;
  ScopedVector<AshmemRegion> ashmem_regions_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryAshmemAllocator);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_ASHMEM_ALLOCATOR_H_
