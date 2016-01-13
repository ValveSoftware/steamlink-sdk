// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_manager.h"

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/containers/mru_cache.h"
#include "base/debug/crash_logging.h"
#include "base/debug/trace_event.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"

namespace base {
namespace internal {

DiscardableMemoryManager::DiscardableMemoryManager(
    size_t memory_limit,
    size_t soft_memory_limit,
    size_t bytes_to_keep_under_moderate_pressure,
    TimeDelta hard_memory_limit_expiration_time)
    : allocations_(AllocationMap::NO_AUTO_EVICT),
      bytes_allocated_(0u),
      memory_limit_(memory_limit),
      soft_memory_limit_(soft_memory_limit),
      bytes_to_keep_under_moderate_pressure_(
          bytes_to_keep_under_moderate_pressure),
      hard_memory_limit_expiration_time_(hard_memory_limit_expiration_time) {
  BytesAllocatedChanged(bytes_allocated_);
}

DiscardableMemoryManager::~DiscardableMemoryManager() {
  DCHECK(allocations_.empty());
  DCHECK_EQ(0u, bytes_allocated_);
}

void DiscardableMemoryManager::RegisterMemoryPressureListener() {
  AutoLock lock(lock_);
  DCHECK(base::MessageLoop::current());
  DCHECK(!memory_pressure_listener_);
  memory_pressure_listener_.reset(new MemoryPressureListener(base::Bind(
      &DiscardableMemoryManager::OnMemoryPressure, Unretained(this))));
}

void DiscardableMemoryManager::UnregisterMemoryPressureListener() {
  AutoLock lock(lock_);
  DCHECK(memory_pressure_listener_);
  memory_pressure_listener_.reset();
}

void DiscardableMemoryManager::SetMemoryLimit(size_t bytes) {
  AutoLock lock(lock_);
  memory_limit_ = bytes;
  PurgeIfNotUsedSinceTimestampUntilUsageIsWithinLimitWithLockAcquired(
      Now(), memory_limit_);
}

void DiscardableMemoryManager::SetSoftMemoryLimit(size_t bytes) {
  AutoLock lock(lock_);
  soft_memory_limit_ = bytes;
}

void DiscardableMemoryManager::SetBytesToKeepUnderModeratePressure(
    size_t bytes) {
  AutoLock lock(lock_);
  bytes_to_keep_under_moderate_pressure_ = bytes;
}

void DiscardableMemoryManager::SetHardMemoryLimitExpirationTime(
    TimeDelta hard_memory_limit_expiration_time) {
  AutoLock lock(lock_);
  hard_memory_limit_expiration_time_ = hard_memory_limit_expiration_time;
}

bool DiscardableMemoryManager::ReduceMemoryUsage() {
  return PurgeIfNotUsedSinceHardLimitCutoffUntilWithinSoftMemoryLimit();
}

void DiscardableMemoryManager::Register(Allocation* allocation, size_t bytes) {
  AutoLock lock(lock_);
  // A registered memory listener is currently required. This DCHECK can be
  // moved or removed if we decide that it's useful to relax this condition.
  // TODO(reveman): Enable this DCHECK when skia and blink are able to
  // register memory pressure listeners. crbug.com/333907
  // DCHECK(memory_pressure_listener_);
  DCHECK(allocations_.Peek(allocation) == allocations_.end());
  allocations_.Put(allocation, AllocationInfo(bytes));
}

void DiscardableMemoryManager::Unregister(Allocation* allocation) {
  AutoLock lock(lock_);
  AllocationMap::iterator it = allocations_.Peek(allocation);
  DCHECK(it != allocations_.end());
  const AllocationInfo& info = it->second;

  if (info.purgable) {
    size_t bytes_purgable = info.bytes;
    DCHECK_LE(bytes_purgable, bytes_allocated_);
    bytes_allocated_ -= bytes_purgable;
    BytesAllocatedChanged(bytes_allocated_);
  }
  allocations_.Erase(it);
}

bool DiscardableMemoryManager::AcquireLock(Allocation* allocation,
                                           bool* purged) {
  AutoLock lock(lock_);
  // Note: |allocations_| is an MRU cache, and use of |Get| here updates that
  // cache.
  AllocationMap::iterator it = allocations_.Get(allocation);
  DCHECK(it != allocations_.end());
  AllocationInfo* info = &it->second;

  if (!info->bytes)
    return false;

  TimeTicks now = Now();
  size_t bytes_required = info->purgable ? 0u : info->bytes;

  if (memory_limit_) {
    size_t limit = 0;
    if (bytes_required < memory_limit_)
      limit = memory_limit_ - bytes_required;

    PurgeIfNotUsedSinceTimestampUntilUsageIsWithinLimitWithLockAcquired(now,
                                                                        limit);
  }

  // Check for overflow.
  if (std::numeric_limits<size_t>::max() - bytes_required < bytes_allocated_)
    return false;

  *purged = !allocation->AllocateAndAcquireLock();
  info->purgable = false;
  info->last_usage = now;
  if (bytes_required) {
    bytes_allocated_ += bytes_required;
    BytesAllocatedChanged(bytes_allocated_);
  }
  return true;
}

void DiscardableMemoryManager::ReleaseLock(Allocation* allocation) {
  AutoLock lock(lock_);
  // Note: |allocations_| is an MRU cache, and use of |Get| here updates that
  // cache.
  AllocationMap::iterator it = allocations_.Get(allocation);
  DCHECK(it != allocations_.end());
  AllocationInfo* info = &it->second;

  TimeTicks now = Now();
  allocation->ReleaseLock();
  info->purgable = true;
  info->last_usage = now;

  PurgeIfNotUsedSinceTimestampUntilUsageIsWithinLimitWithLockAcquired(
      now, memory_limit_);
}

void DiscardableMemoryManager::PurgeAll() {
  AutoLock lock(lock_);
  PurgeIfNotUsedSinceTimestampUntilUsageIsWithinLimitWithLockAcquired(Now(), 0);
}

bool DiscardableMemoryManager::IsRegisteredForTest(
    Allocation* allocation) const {
  AutoLock lock(lock_);
  AllocationMap::const_iterator it = allocations_.Peek(allocation);
  return it != allocations_.end();
}

bool DiscardableMemoryManager::CanBePurgedForTest(
    Allocation* allocation) const {
  AutoLock lock(lock_);
  AllocationMap::const_iterator it = allocations_.Peek(allocation);
  return it != allocations_.end() && it->second.purgable;
}

size_t DiscardableMemoryManager::GetBytesAllocatedForTest() const {
  AutoLock lock(lock_);
  return bytes_allocated_;
}

void DiscardableMemoryManager::OnMemoryPressure(
    MemoryPressureListener::MemoryPressureLevel pressure_level) {
  switch (pressure_level) {
    case MemoryPressureListener::MEMORY_PRESSURE_MODERATE:
      PurgeUntilWithinBytesToKeepUnderModeratePressure();
      return;
    case MemoryPressureListener::MEMORY_PRESSURE_CRITICAL:
      PurgeAll();
      return;
  }

  NOTREACHED();
}

void
DiscardableMemoryManager::PurgeUntilWithinBytesToKeepUnderModeratePressure() {
  AutoLock lock(lock_);

  PurgeIfNotUsedSinceTimestampUntilUsageIsWithinLimitWithLockAcquired(
      Now(), bytes_to_keep_under_moderate_pressure_);
}

bool DiscardableMemoryManager::
    PurgeIfNotUsedSinceHardLimitCutoffUntilWithinSoftMemoryLimit() {
  AutoLock lock(lock_);

  PurgeIfNotUsedSinceTimestampUntilUsageIsWithinLimitWithLockAcquired(
      Now() - hard_memory_limit_expiration_time_, soft_memory_limit_);

  return bytes_allocated_ <= soft_memory_limit_;
}

void DiscardableMemoryManager::
    PurgeIfNotUsedSinceTimestampUntilUsageIsWithinLimitWithLockAcquired(
        TimeTicks timestamp,
        size_t limit) {
  lock_.AssertAcquired();

  size_t bytes_allocated_before_purging = bytes_allocated_;
  for (AllocationMap::reverse_iterator it = allocations_.rbegin();
       it != allocations_.rend();
       ++it) {
    Allocation* allocation = it->first;
    AllocationInfo* info = &it->second;

    if (bytes_allocated_ <= limit)
      break;

    bool purgable = info->purgable && info->last_usage <= timestamp;
    if (!purgable)
      continue;

    size_t bytes_purgable = info->bytes;
    DCHECK_LE(bytes_purgable, bytes_allocated_);
    bytes_allocated_ -= bytes_purgable;
    info->purgable = false;
    allocation->Purge();
  }

  if (bytes_allocated_ != bytes_allocated_before_purging)
    BytesAllocatedChanged(bytes_allocated_);
}

void DiscardableMemoryManager::BytesAllocatedChanged(
    size_t new_bytes_allocated) const {
  TRACE_COUNTER_ID1(
      "base", "DiscardableMemoryUsage", this, new_bytes_allocated);

  static const char kDiscardableMemoryUsageKey[] = "dm-usage";
  base::debug::SetCrashKeyValue(kDiscardableMemoryUsageKey,
                                Uint64ToString(new_bytes_allocated));
}

TimeTicks DiscardableMemoryManager::Now() const {
  return TimeTicks::Now();
}

}  // namespace internal
}  // namespace base
