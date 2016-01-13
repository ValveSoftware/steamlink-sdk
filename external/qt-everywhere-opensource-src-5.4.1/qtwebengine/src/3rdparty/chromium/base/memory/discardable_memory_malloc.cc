// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_malloc.h"

#include "base/logging.h"

namespace base {
namespace internal {

DiscardableMemoryMalloc::DiscardableMemoryMalloc(size_t size) : size_(size) {
}

DiscardableMemoryMalloc::~DiscardableMemoryMalloc() {
}

bool DiscardableMemoryMalloc::Initialize() {
  return Lock() != DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;
}

DiscardableMemoryLockStatus DiscardableMemoryMalloc::Lock() {
  DCHECK(!memory_);

  memory_.reset(static_cast<uint8*>(malloc(size_)));
  if (!memory_)
    return DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;

  return DISCARDABLE_MEMORY_LOCK_STATUS_PURGED;
}

void DiscardableMemoryMalloc::Unlock() {
  DCHECK(memory_);
  memory_.reset();
}

void* DiscardableMemoryMalloc::Memory() const {
  DCHECK(memory_);
  return memory_.get();
}

}  // namespace internal
}  // namespace base
