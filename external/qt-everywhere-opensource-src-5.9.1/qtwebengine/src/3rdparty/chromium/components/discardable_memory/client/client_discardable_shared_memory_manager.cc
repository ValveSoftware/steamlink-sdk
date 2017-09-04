// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"

#include <inttypes.h>

#include <algorithm>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/debug/crash_logging.h"
#include "base/macros.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/memory.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"

namespace discardable_memory {
namespace {

// Default allocation size.
const size_t kAllocationSize = 4 * 1024 * 1024;

// Global atomic to generate unique discardable shared memory IDs.
base::StaticAtomicSequenceNumber g_next_discardable_shared_memory_id;

class DiscardableMemoryImpl : public base::DiscardableMemory {
 public:
  DiscardableMemoryImpl(ClientDiscardableSharedMemoryManager* manager,
                        std::unique_ptr<DiscardableSharedMemoryHeap::Span> span)
      : manager_(manager), span_(std::move(span)), is_locked_(true) {}

  ~DiscardableMemoryImpl() override {
    if (is_locked_)
      manager_->UnlockSpan(span_.get());

    manager_->ReleaseSpan(std::move(span_));
  }

  // Overridden from base::DiscardableMemory:
  bool Lock() override {
    DCHECK(!is_locked_);

    if (!manager_->LockSpan(span_.get()))
      return false;

    is_locked_ = true;
    return true;
  }
  void Unlock() override {
    DCHECK(is_locked_);

    manager_->UnlockSpan(span_.get());
    is_locked_ = false;
  }
  void* data() const override {
    DCHECK(is_locked_);
    return reinterpret_cast<void*>(span_->start() * base::GetPageSize());
  }

  base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
      const char* name,
      base::trace_event::ProcessMemoryDump* pmd) const override {
    return manager_->CreateMemoryAllocatorDump(span_.get(), name, pmd);
  }

 private:
  ClientDiscardableSharedMemoryManager* const manager_;
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> span_;
  bool is_locked_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryImpl);
};

void SendDeletedDiscardableSharedMemoryMessage(
    ClientDiscardableSharedMemoryManager::Delegate* delegate,
    DiscardableSharedMemoryId id) {
  delegate->DeletedDiscardableSharedMemory(id);
}

}  // namespace

ClientDiscardableSharedMemoryManager::ClientDiscardableSharedMemoryManager(
    Delegate* delegate)
    : heap_(base::GetPageSize()), delegate_(delegate) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "ClientDiscardableSharedMemoryManager",
      base::ThreadTaskRunnerHandle::Get());
}

ClientDiscardableSharedMemoryManager::~ClientDiscardableSharedMemoryManager() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  // TODO(reveman): Determine if this DCHECK can be enabled. crbug.com/430533
  // DCHECK_EQ(heap_.GetSize(), heap_.GetSizeOfFreeLists());
  if (heap_.GetSize())
    MemoryUsageChanged(0, 0);
}

std::unique_ptr<base::DiscardableMemory>
ClientDiscardableSharedMemoryManager::AllocateLockedDiscardableMemory(
    size_t size) {
  base::AutoLock lock(lock_);

  DCHECK_NE(size, 0u);

  auto size_in_kb = static_cast<base::HistogramBase::Sample>(size / 1024);
  UMA_HISTOGRAM_CUSTOM_COUNTS("Memory.DiscardableAllocationSize",
                              size_in_kb,  // In KB
                              1,
                              4 * 1024 * 1024,  // 4 GB
                              50);

  // Round up to multiple of page size.
  size_t pages =
      std::max((size + base::GetPageSize() - 1) / base::GetPageSize(),
               static_cast<size_t>(1));

  // Default allocation size in pages.
  size_t allocation_pages = kAllocationSize / base::GetPageSize();

  size_t slack = 0;
  // When searching the free lists, allow a slack between required size and
  // free span size that is less or equal to kAllocationSize. This is to
  // avoid segments larger then kAllocationSize unless they are a perfect
  // fit. The result is that large allocations can be reused without reducing
  // the ability to discard memory.
  if (pages < allocation_pages)
    slack = allocation_pages - pages;

  size_t heap_size_prior_to_releasing_purged_memory = heap_.GetSize();
  for (;;) {
    // Search free lists for suitable span.
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> free_span =
        heap_.SearchFreeLists(pages, slack);
    if (!free_span.get())
      break;

    // Attempt to lock |free_span|. Delete span and search free lists again
    // if locking failed.
    if (free_span->shared_memory()->Lock(
            free_span->start() * base::GetPageSize() -
                reinterpret_cast<size_t>(free_span->shared_memory()->memory()),
            free_span->length() * base::GetPageSize()) ==
        base::DiscardableSharedMemory::FAILED) {
      DCHECK(!free_span->shared_memory()->IsMemoryResident());
      // We have to release purged memory before |free_span| can be destroyed.
      heap_.ReleasePurgedMemory();
      DCHECK(!free_span->shared_memory());
      continue;
    }

    free_span->set_is_locked(true);

    // Memory usage is guaranteed to have changed after having removed
    // at least one span from the free lists.
    MemoryUsageChanged(heap_.GetSize(), heap_.GetSizeOfFreeLists());

    return base::MakeUnique<DiscardableMemoryImpl>(this, std::move(free_span));
  }

  // Release purged memory to free up the address space before we attempt to
  // allocate more memory.
  heap_.ReleasePurgedMemory();

  // Make sure crash keys are up to date in case allocation fails.
  if (heap_.GetSize() != heap_size_prior_to_releasing_purged_memory)
    MemoryUsageChanged(heap_.GetSize(), heap_.GetSizeOfFreeLists());

  size_t pages_to_allocate =
      std::max(kAllocationSize / base::GetPageSize(), pages);
  size_t allocation_size_in_bytes = pages_to_allocate * base::GetPageSize();

  DiscardableSharedMemoryId new_id =
      g_next_discardable_shared_memory_id.GetNext();

  // Ask parent process to allocate a new discardable shared memory segment.
  std::unique_ptr<base::DiscardableSharedMemory> shared_memory(
      AllocateLockedDiscardableSharedMemory(allocation_size_in_bytes, new_id));

  // Create span for allocated memory.
  std::unique_ptr<DiscardableSharedMemoryHeap::Span> new_span(
      heap_.Grow(std::move(shared_memory), allocation_size_in_bytes, new_id,
                 base::Bind(&SendDeletedDiscardableSharedMemoryMessage,
                            delegate_, new_id)));
  new_span->set_is_locked(true);

  // Unlock and insert any left over memory into free lists.
  if (pages < pages_to_allocate) {
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> leftover =
        heap_.Split(new_span.get(), pages);
    leftover->shared_memory()->Unlock(
        leftover->start() * base::GetPageSize() -
            reinterpret_cast<size_t>(leftover->shared_memory()->memory()),
        leftover->length() * base::GetPageSize());
    leftover->set_is_locked(false);
    heap_.MergeIntoFreeLists(std::move(leftover));
  }

  MemoryUsageChanged(heap_.GetSize(), heap_.GetSizeOfFreeLists());

  return base::MakeUnique<DiscardableMemoryImpl>(this, std::move(new_span));
}

bool ClientDiscardableSharedMemoryManager::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);
  if (args.level_of_detail ==
      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND) {
    base::trace_event::MemoryAllocatorDump* total_dump =
        pmd->CreateAllocatorDump(
            base::StringPrintf("discardable/child_0x%" PRIXPTR,
                               reinterpret_cast<uintptr_t>(this)));
    const size_t total_size = heap_.GetSize();
    const size_t freelist_size = heap_.GetSizeOfFreeLists();
    total_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          total_size - freelist_size);
    total_dump->AddScalar("freelist_size",
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          freelist_size);
    return true;
  }

  return heap_.OnMemoryDump(pmd);
}

ClientDiscardableSharedMemoryManager::Statistics
ClientDiscardableSharedMemoryManager::GetStatistics() const {
  base::AutoLock lock(lock_);
  Statistics stats;
  stats.total_size = heap_.GetSize();
  stats.freelist_size = heap_.GetSizeOfFreeLists();
  return stats;
}

void ClientDiscardableSharedMemoryManager::ReleaseFreeMemory() {
  base::AutoLock lock(lock_);

  size_t heap_size_prior_to_releasing_memory = heap_.GetSize();

  // Release both purged and free memory.
  heap_.ReleasePurgedMemory();
  heap_.ReleaseFreeMemory();

  if (heap_.GetSize() != heap_size_prior_to_releasing_memory)
    MemoryUsageChanged(heap_.GetSize(), heap_.GetSizeOfFreeLists());
}

bool ClientDiscardableSharedMemoryManager::LockSpan(
    DiscardableSharedMemoryHeap::Span* span) {
  base::AutoLock lock(lock_);

  if (!span->shared_memory())
    return false;

  size_t offset = span->start() * base::GetPageSize() -
                  reinterpret_cast<size_t>(span->shared_memory()->memory());
  size_t length = span->length() * base::GetPageSize();

  switch (span->shared_memory()->Lock(offset, length)) {
    case base::DiscardableSharedMemory::SUCCESS:
      span->set_is_locked(true);
      return true;
    case base::DiscardableSharedMemory::PURGED:
      span->shared_memory()->Unlock(offset, length);
      span->set_is_locked(false);
      return false;
    case base::DiscardableSharedMemory::FAILED:
      return false;
  }

  NOTREACHED();
  return false;
}

void ClientDiscardableSharedMemoryManager::UnlockSpan(
    DiscardableSharedMemoryHeap::Span* span) {
  base::AutoLock lock(lock_);

  DCHECK(span->shared_memory());
  size_t offset = span->start() * base::GetPageSize() -
                  reinterpret_cast<size_t>(span->shared_memory()->memory());
  size_t length = span->length() * base::GetPageSize();

  span->set_is_locked(false);
  return span->shared_memory()->Unlock(offset, length);
}

void ClientDiscardableSharedMemoryManager::ReleaseSpan(
    std::unique_ptr<DiscardableSharedMemoryHeap::Span> span) {
  base::AutoLock lock(lock_);

  // Delete span instead of merging it into free lists if memory is gone.
  if (!span->shared_memory())
    return;

  heap_.MergeIntoFreeLists(std::move(span));

  // Bytes of free memory changed.
  MemoryUsageChanged(heap_.GetSize(), heap_.GetSizeOfFreeLists());
}

base::trace_event::MemoryAllocatorDump*
ClientDiscardableSharedMemoryManager::CreateMemoryAllocatorDump(
    DiscardableSharedMemoryHeap::Span* span,
    const char* name,
    base::trace_event::ProcessMemoryDump* pmd) const {
  base::AutoLock lock(lock_);
  return heap_.CreateMemoryAllocatorDump(span, name, pmd);
}

std::unique_ptr<base::DiscardableSharedMemory>
ClientDiscardableSharedMemoryManager::AllocateLockedDiscardableSharedMemory(
    size_t size,
    DiscardableSharedMemoryId id) {
  TRACE_EVENT2("renderer",
               "ClientDiscardableSharedMemoryManager::"
               "AllocateLockedDiscardableSharedMemory",
               "size", size, "id", id);

  base::SharedMemoryHandle handle = base::SharedMemory::NULLHandle();
  delegate_->AllocateLockedDiscardableSharedMemory(size, id, &handle);
  std::unique_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory(handle));
  if (!memory->Map(size))
    base::TerminateBecauseOutOfMemory(size);
  return memory;
}

void ClientDiscardableSharedMemoryManager::MemoryUsageChanged(
    size_t new_bytes_total,
    size_t new_bytes_free) const {
  static const char kDiscardableMemoryAllocatedKey[] =
      "discardable-memory-allocated";
  base::debug::SetCrashKeyValue(kDiscardableMemoryAllocatedKey,
                                base::Uint64ToString(new_bytes_total));

  static const char kDiscardableMemoryFreeKey[] = "discardable-memory-free";
  base::debug::SetCrashKeyValue(kDiscardableMemoryFreeKey,
                                base::Uint64ToString(new_bytes_free));
}

}  // namespace discardable_memory
