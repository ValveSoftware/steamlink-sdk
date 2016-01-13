// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_EMULATED_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_EMULATED_H_

#include "base/memory/discardable_memory.h"

#include "base/memory/discardable_memory_manager.h"

namespace base {
namespace internal {

class DiscardableMemoryEmulated
    : public DiscardableMemory,
      public internal::DiscardableMemoryManagerAllocation {
 public:
  explicit DiscardableMemoryEmulated(size_t bytes);
  virtual ~DiscardableMemoryEmulated();

  static void RegisterMemoryPressureListeners();
  static void UnregisterMemoryPressureListeners();
  static bool ReduceMemoryUsage();

  static void PurgeForTesting();

  bool Initialize();

  // Overridden from DiscardableMemory:
  virtual DiscardableMemoryLockStatus Lock() OVERRIDE;
  virtual void Unlock() OVERRIDE;
  virtual void* Memory() const OVERRIDE;

  // Overridden from internal::DiscardableMemoryManagerAllocation:
  virtual bool AllocateAndAcquireLock() OVERRIDE;
  virtual void ReleaseLock() OVERRIDE {}
  virtual void Purge() OVERRIDE;

 private:
  const size_t bytes_;
  scoped_ptr<uint8[]> memory_;
  bool is_locked_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryEmulated);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_EMULATED_H_
