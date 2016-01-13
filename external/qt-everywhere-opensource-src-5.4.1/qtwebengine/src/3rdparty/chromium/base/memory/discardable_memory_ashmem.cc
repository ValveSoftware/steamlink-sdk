// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_ashmem.h"

#include "base/memory/discardable_memory_ashmem_allocator.h"

namespace base {
namespace internal {

DiscardableMemoryAshmem::DiscardableMemoryAshmem(
    size_t bytes,
    DiscardableMemoryAshmemAllocator* allocator,
    DiscardableMemoryManager* manager)
    : bytes_(bytes),
      allocator_(allocator),
      manager_(manager),
      is_locked_(false) {
  manager_->Register(this, bytes_);
}

DiscardableMemoryAshmem::~DiscardableMemoryAshmem() {
  if (is_locked_)
    Unlock();

  manager_->Unregister(this);
}

bool DiscardableMemoryAshmem::Initialize() {
  return Lock() != DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;
}

DiscardableMemoryLockStatus DiscardableMemoryAshmem::Lock() {
  DCHECK(!is_locked_);

  bool purged = false;
  if (!manager_->AcquireLock(this, &purged))
    return DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;

  is_locked_ = true;
  return purged ? DISCARDABLE_MEMORY_LOCK_STATUS_PURGED
                : DISCARDABLE_MEMORY_LOCK_STATUS_SUCCESS;
}

void DiscardableMemoryAshmem::Unlock() {
  DCHECK(is_locked_);
  manager_->ReleaseLock(this);
  is_locked_ = false;
}

void* DiscardableMemoryAshmem::Memory() const {
  DCHECK(is_locked_);
  DCHECK(ashmem_chunk_);
  return ashmem_chunk_->Memory();
}

bool DiscardableMemoryAshmem::AllocateAndAcquireLock() {
  if (ashmem_chunk_)
    return ashmem_chunk_->Lock();

  ashmem_chunk_ = allocator_->Allocate(bytes_);
  return false;
}

void DiscardableMemoryAshmem::ReleaseLock() {
  ashmem_chunk_->Unlock();
}

void DiscardableMemoryAshmem::Purge() {
  ashmem_chunk_.reset();
}

}  // namespace internal
}  // namespace base
