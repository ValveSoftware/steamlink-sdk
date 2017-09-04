// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DISCARDABLE_MEMORY_CLIENT_CLIENT_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
#define COMPONENTS_DISCARDABLE_MEMORY_CLIENT_CLIENT_DISCARDABLE_SHARED_MEMORY_MANAGER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_handle.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/discardable_memory/common/discardable_memory_export.h"
#include "components/discardable_memory/common/discardable_shared_memory_heap.h"
#include "components/discardable_memory/common/discardable_shared_memory_id.h"

namespace discardable_memory {

// Implementation of DiscardableMemoryAllocator that allocates
// discardable memory segments through the browser process.
class DISCARDABLE_MEMORY_EXPORT ClientDiscardableSharedMemoryManager
    : public base::DiscardableMemoryAllocator,
      public base::trace_event::MemoryDumpProvider {
 public:
  class Delegate {
   public:
    virtual void AllocateLockedDiscardableSharedMemory(
        size_t size,
        DiscardableSharedMemoryId id,
        base::SharedMemoryHandle* handle) = 0;
    virtual void DeletedDiscardableSharedMemory(
        DiscardableSharedMemoryId id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit ClientDiscardableSharedMemoryManager(Delegate* delegate);
  ~ClientDiscardableSharedMemoryManager() override;

  // Overridden from base::DiscardableMemoryAllocator:
  std::unique_ptr<base::DiscardableMemory> AllocateLockedDiscardableMemory(
      size_t size) override;

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Release memory and associated resources that have been purged.
  void ReleaseFreeMemory();

  bool LockSpan(DiscardableSharedMemoryHeap::Span* span);
  void UnlockSpan(DiscardableSharedMemoryHeap::Span* span);
  void ReleaseSpan(std::unique_ptr<DiscardableSharedMemoryHeap::Span> span);

  base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
      DiscardableSharedMemoryHeap::Span* span,
      const char* name,
      base::trace_event::ProcessMemoryDump* pmd) const;

  struct Statistics {
    size_t total_size;
    size_t freelist_size;
  };

  Statistics GetStatistics() const;

 private:
  std::unique_ptr<base::DiscardableSharedMemory>
  AllocateLockedDiscardableSharedMemory(size_t size,
                                        DiscardableSharedMemoryId id);
  void MemoryUsageChanged(size_t new_bytes_allocated,
                          size_t new_bytes_free) const;

  mutable base::Lock lock_;
  DiscardableSharedMemoryHeap heap_;
  Delegate* const delegate_;

  DISALLOW_COPY_AND_ASSIGN(ClientDiscardableSharedMemoryManager);
};

}  // namespace discardable_memory

#endif  // COMPONENTS_DISCARDABLE_MEMORY_CLIENT_CLIENT_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
