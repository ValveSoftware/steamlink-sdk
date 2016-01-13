// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "base/logging.h"
#include "base/memory/discardable_memory_emulated.h"
#include "base/memory/discardable_memory_malloc.h"

namespace base {

// static
void DiscardableMemory::RegisterMemoryPressureListeners() {
  internal::DiscardableMemoryEmulated::RegisterMemoryPressureListeners();
}

// static
void DiscardableMemory::UnregisterMemoryPressureListeners() {
  internal::DiscardableMemoryEmulated::UnregisterMemoryPressureListeners();
}

// static
bool DiscardableMemory::ReduceMemoryUsage() {
  return internal::DiscardableMemoryEmulated::ReduceMemoryUsage();
}

// static
void DiscardableMemory::GetSupportedTypes(
    std::vector<DiscardableMemoryType>* types) {
  const DiscardableMemoryType supported_types[] = {
    DISCARDABLE_MEMORY_TYPE_EMULATED,
    DISCARDABLE_MEMORY_TYPE_MALLOC
  };
  types->assign(supported_types, supported_types + arraysize(supported_types));
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemoryWithType(
    DiscardableMemoryType type, size_t size) {
  switch (type) {
    case DISCARDABLE_MEMORY_TYPE_NONE:
    case DISCARDABLE_MEMORY_TYPE_ASHMEM:
    case DISCARDABLE_MEMORY_TYPE_MAC:
      return scoped_ptr<DiscardableMemory>();
    case DISCARDABLE_MEMORY_TYPE_EMULATED: {
      scoped_ptr<internal::DiscardableMemoryEmulated> memory(
          new internal::DiscardableMemoryEmulated(size));
      if (!memory->Initialize())
        return scoped_ptr<DiscardableMemory>();

      return memory.PassAs<DiscardableMemory>();
    }
    case DISCARDABLE_MEMORY_TYPE_MALLOC: {
      scoped_ptr<internal::DiscardableMemoryMalloc> memory(
          new internal::DiscardableMemoryMalloc(size));
      if (!memory->Initialize())
        return scoped_ptr<DiscardableMemory>();

      return memory.PassAs<DiscardableMemory>();
    }
  }

  NOTREACHED();
  return scoped_ptr<DiscardableMemory>();
}

// static
void DiscardableMemory::PurgeForTesting() {
  internal::DiscardableMemoryEmulated::PurgeForTesting();
}

}  // namespace base
