// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_MANAGER_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_MANAGER_H_

#include "base/base_export.h"
#include "base/containers/hash_tables.h"
#include "base/containers/mru_cache.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"

namespace base {
namespace internal {

// This interface is used by the DiscardableMemoryManager class to provide some
// level of userspace control over discardable memory allocations.
class DiscardableMemoryManagerAllocation {
 public:
  // Allocate and acquire a lock that prevents the allocation from being purged
  // by the system. Returns true if memory was previously allocated and is still
  // resident.
  virtual bool AllocateAndAcquireLock() = 0;

  // Release a previously acquired lock on the allocation so that it can be
  // purged by the system.
  virtual void ReleaseLock() = 0;

  // Explicitly purge this allocation. It is illegal to call this while a lock
  // is acquired on the allocation.
  virtual void Purge() = 0;

 protected:
  virtual ~DiscardableMemoryManagerAllocation() {}
};

}  // namespace internal
}  // namespace base

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template <>
struct hash<base::internal::DiscardableMemoryManagerAllocation*> {
  size_t operator()(
      base::internal::DiscardableMemoryManagerAllocation* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

namespace base {
namespace internal {

// The DiscardableMemoryManager manages a collection of
// DiscardableMemoryManagerAllocation instances. It is used on platforms that
// need some level of userspace control over discardable memory. It keeps track
// of all allocation instances (in case they need to be purged), and the total
// amount of allocated memory (in case this forces a purge). When memory usage
// reaches the limit, the manager purges the LRU memory.
//
// When notified of memory pressure, the manager either purges the LRU memory --
// if the pressure is moderate -- or all discardable memory if the pressure is
// critical.
class BASE_EXPORT_PRIVATE DiscardableMemoryManager {
 public:
  typedef DiscardableMemoryManagerAllocation Allocation;

  DiscardableMemoryManager(size_t memory_limit,
                           size_t soft_memory_limit,
                           size_t bytes_to_keep_under_moderate_pressure,
                           TimeDelta hard_memory_limit_expiration_time);
  virtual ~DiscardableMemoryManager();

  // Call this to register memory pressure listener. Must be called on a thread
  // with a MessageLoop current.
  void RegisterMemoryPressureListener();

  // Call this to unregister memory pressure listener.
  void UnregisterMemoryPressureListener();

  // The maximum number of bytes of memory that may be allocated before we force
  // a purge.
  void SetMemoryLimit(size_t bytes);

  // The number of bytes of memory that may be allocated but unused for the hard
  // limit expiration time without getting purged.
  void SetSoftMemoryLimit(size_t bytes);

  // Sets the amount of memory to keep when we're under moderate pressure.
  void SetBytesToKeepUnderModeratePressure(size_t bytes);

  // Sets the memory usage cutoff time for hard memory limit.
  void SetHardMemoryLimitExpirationTime(
      TimeDelta hard_memory_limit_expiration_time);

  // This will attempt to reduce memory footprint until within soft memory
  // limit. Returns true if there's no need to call this again until allocations
  // have been used.
  bool ReduceMemoryUsage();

  // Adds the given allocation to the manager's collection.
  void Register(Allocation* allocation, size_t bytes);

  // Removes the given allocation from the manager's collection.
  void Unregister(Allocation* allocation);

  // Returns false if an error occurred. Otherwise, returns true and sets
  // |purged| to indicate whether or not allocation has been purged since last
  // use.
  bool AcquireLock(Allocation* allocation, bool* purged);

  // Release a previously acquired lock on allocation. This allows the manager
  // to purge it if necessary.
  void ReleaseLock(Allocation* allocation);

  // Purges all discardable memory.
  void PurgeAll();

  // Returns true if allocation has been added to the manager's collection. This
  // should only be used by tests.
  bool IsRegisteredForTest(Allocation* allocation) const;

  // Returns true if allocation can be purged. This should only be used by
  // tests.
  bool CanBePurgedForTest(Allocation* allocation) const;

  // Returns total amount of allocated discardable memory. This should only be
  // used by tests.
  size_t GetBytesAllocatedForTest() const;

 private:
  struct AllocationInfo {
    explicit AllocationInfo(size_t bytes) : bytes(bytes), purgable(false) {}

    const size_t bytes;
    bool purgable;
    TimeTicks last_usage;
  };
  typedef HashingMRUCache<Allocation*, AllocationInfo> AllocationMap;

  // This can be called as a hint that the system is under memory pressure.
  void OnMemoryPressure(
      MemoryPressureListener::MemoryPressureLevel pressure_level);

  // Purges memory until usage is less or equal to
  // |bytes_to_keep_under_moderate_pressure_|.
  void PurgeUntilWithinBytesToKeepUnderModeratePressure();

  // Purges memory not used since |hard_memory_limit_expiration_time_| before
  // "right now" until usage is less or equal to |soft_memory_limit_|.
  // Returns true if total amount of memory is less or equal to soft memory
  // limit.
  bool PurgeIfNotUsedSinceHardLimitCutoffUntilWithinSoftMemoryLimit();

  // Purges memory that has not been used since |timestamp| until usage is less
  // or equal to |limit|.
  // Caller must acquire |lock_| prior to calling this function.
  void PurgeIfNotUsedSinceTimestampUntilUsageIsWithinLimitWithLockAcquired(
      TimeTicks timestamp,
      size_t limit);

  // Called when a change to |bytes_allocated_| has been made.
  void BytesAllocatedChanged(size_t new_bytes_allocated) const;

  // Virtual for tests.
  virtual TimeTicks Now() const;

  // Needs to be held when accessing members.
  mutable Lock lock_;

  // A MRU cache of all allocated bits of memory. Used for purging.
  AllocationMap allocations_;

  // The total amount of allocated memory.
  size_t bytes_allocated_;

  // The maximum number of bytes of memory that may be allocated.
  size_t memory_limit_;

  // The number of bytes of memory that may be allocated but not used for
  // |hard_memory_limit_expiration_time_| amount of time when receiving an idle
  // notification.
  size_t soft_memory_limit_;

  // Under moderate memory pressure, we will purge memory until usage is within
  // this limit.
  size_t bytes_to_keep_under_moderate_pressure_;

  // Allows us to be respond when the system reports that it is under memory
  // pressure.
  scoped_ptr<MemoryPressureListener> memory_pressure_listener_;

  // Amount of time it takes for an allocation to become affected by
  // |soft_memory_limit_|.
  TimeDelta hard_memory_limit_expiration_time_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryManager);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_MANAGER_H_
