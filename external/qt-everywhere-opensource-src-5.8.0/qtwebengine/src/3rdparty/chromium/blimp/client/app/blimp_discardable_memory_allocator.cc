// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/blimp_discardable_memory_allocator.h"

#include "base/macros.h"
#include "base/memory/discardable_memory.h"
#include "base/stl_util.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"

namespace blimp {
namespace client {

namespace {

size_t kDesiredMaxMemory = 20 * 1024 * 1024; /* 20 MB */

}  // namespace

// Interface to the rest of the program. These objects are owned outside of the
// allocator.
class BlimpDiscardableMemoryAllocator::DiscardableMemoryChunkImpl
    : public base::DiscardableMemory {
 public:
  DiscardableMemoryChunkImpl(size_t size,
                             BlimpDiscardableMemoryAllocator* allocator)
      : is_locked_(true),
        size_(size),
        data_(new uint8_t[size]),
        allocator_(allocator) {}

  ~DiscardableMemoryChunkImpl() override {
    base::AutoLock lock(allocator_->lock_);
    // Either the memory is discarded or the memory chunk is unlocked.
    DCHECK(data_ || !is_locked_);
    if (!is_locked_ && data_)
      allocator_->NotifyDestructed(unlocked_position_);
  }

  // Overridden from DiscardableMemoryChunk:
  bool Lock() override {
    base::AutoLock lock(allocator_->lock_);
    DCHECK(!is_locked_);
    if (!data_)
      return false;

    is_locked_ = true;
    allocator_->NotifyLocked(unlocked_position_);
    return true;
  }

  void Unlock() override {
    base::AutoLock lock(allocator_->lock_);
    DCHECK(is_locked_);
    DCHECK(data_);
    is_locked_ = false;
    unlocked_position_ = allocator_->NotifyUnlocked(this);

    // Allow the allocator to discard our memory if required.
    allocator_->FreeUnlockedChunksIfNeeded();
  }

  void* data() const override {
    if (data_) {
      DCHECK(is_locked_);
      return data_.get();
    }
    return nullptr;
  }

  base::trace_event::MemoryAllocatorDump* CreateMemoryAllocatorDump(
      const char* name,
      base::trace_event::ProcessMemoryDump* pmd) const override {
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes, size_);

    // Memory is allocated from system allocator (malloc).
    pmd->AddSuballocation(dump->guid(),
                          base::trace_event::MemoryDumpManager::GetInstance()
                              ->system_allocator_pool_name());
    return dump;
  }

  size_t size() const { return size_; }

  void Discard() {
    allocator_->lock_.AssertAcquired();
    DCHECK(!is_locked_);
    data_.reset();
  }

 private:
  bool is_locked_;
  size_t size_;
  std::unique_ptr<uint8_t[]> data_;
  BlimpDiscardableMemoryAllocator* allocator_;

  MemoryChunkList::iterator unlocked_position_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DiscardableMemoryChunkImpl);
};

BlimpDiscardableMemoryAllocator::BlimpDiscardableMemoryAllocator()
  : BlimpDiscardableMemoryAllocator(kDesiredMaxMemory) {}

BlimpDiscardableMemoryAllocator::BlimpDiscardableMemoryAllocator(
    size_t desired_max_memory)
    : desired_max_memory_(desired_max_memory),
      total_live_memory_(0u),
      locked_chunks_(0) {
}

BlimpDiscardableMemoryAllocator::~BlimpDiscardableMemoryAllocator() {
  DCHECK_EQ(0, locked_chunks_);
  STLDeleteElements(&live_unlocked_chunks_);
}

std::unique_ptr<base::DiscardableMemory>
BlimpDiscardableMemoryAllocator::AllocateLockedDiscardableMemory(size_t size) {
  base::AutoLock lock(lock_);
  std::unique_ptr<DiscardableMemoryChunkImpl> chunk(
      new DiscardableMemoryChunkImpl(size, this));
  total_live_memory_ += size;
  locked_chunks_++;

  FreeUnlockedChunksIfNeeded();

  return std::move(chunk);
}

BlimpDiscardableMemoryAllocator::MemoryChunkList::iterator
BlimpDiscardableMemoryAllocator::NotifyUnlocked(
    DiscardableMemoryChunkImpl* chunk) {
  lock_.AssertAcquired();
  locked_chunks_--;
  return live_unlocked_chunks_.insert(live_unlocked_chunks_.end(), chunk);
}

void BlimpDiscardableMemoryAllocator::NotifyLocked(
MemoryChunkList::iterator it) {
  lock_.AssertAcquired();
  locked_chunks_++;
  live_unlocked_chunks_.erase(it);
}

void BlimpDiscardableMemoryAllocator::NotifyDestructed(
    MemoryChunkList::iterator it) {
  lock_.AssertAcquired();
  live_unlocked_chunks_.erase(it);
}

void BlimpDiscardableMemoryAllocator::FreeUnlockedChunksIfNeeded() {
  lock_.AssertAcquired();

  // Go through the list of unlocked live chunks starting from the least
  // recently used, freeing as many as we can until we get our size under the
  // desired maximum.
  auto it = live_unlocked_chunks_.begin();
  while (total_live_memory_ > desired_max_memory_ &&
         it != live_unlocked_chunks_.end()) {
    total_live_memory_ -= (*it)->size();
    (*it)->Discard();
    it = live_unlocked_chunks_.erase(it);
  }
}

}  // namespace client
}  // namespace blimp
