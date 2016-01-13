// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include <mach/mach.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_vm.h"
#include "base/memory/discardable_memory_emulated.h"
#include "base/memory/discardable_memory_malloc.h"
#include "base/memory/discardable_memory_manager.h"
#include "base/memory/scoped_ptr.h"

namespace base {
namespace {

// For Mac, have the DiscardableMemoryManager trigger userspace eviction when
// address space usage gets too high (e.g. 512 MBytes).
const size_t kMacMemoryLimit = 512 * 1024 * 1024;

struct SharedState {
  SharedState()
      : manager(kMacMemoryLimit,
                kMacMemoryLimit,
                kMacMemoryLimit,
                TimeDelta::Max()) {}

  internal::DiscardableMemoryManager manager;
};
LazyInstance<SharedState>::Leaky g_shared_state = LAZY_INSTANCE_INITIALIZER;

// The VM subsystem allows tagging of memory and 240-255 is reserved for
// application use (see mach/vm_statistics.h). Pick 252 (after chromium's atomic
// weight of ~52).
const int kDiscardableMemoryTag = VM_MAKE_TAG(252);

class DiscardableMemoryMac
    : public DiscardableMemory,
      public internal::DiscardableMemoryManagerAllocation {
 public:
  explicit DiscardableMemoryMac(size_t bytes)
      : memory_(0, 0),
        bytes_(mach_vm_round_page(bytes)),
        is_locked_(false) {
    g_shared_state.Pointer()->manager.Register(this, bytes);
  }

  bool Initialize() { return Lock() != DISCARDABLE_MEMORY_LOCK_STATUS_FAILED; }

  virtual ~DiscardableMemoryMac() {
    if (is_locked_)
      Unlock();
    g_shared_state.Pointer()->manager.Unregister(this);
  }

  // Overridden from DiscardableMemory:
  virtual DiscardableMemoryLockStatus Lock() OVERRIDE {
    DCHECK(!is_locked_);

    bool purged = false;
    if (!g_shared_state.Pointer()->manager.AcquireLock(this, &purged))
      return DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;

    is_locked_ = true;
    return purged ? DISCARDABLE_MEMORY_LOCK_STATUS_PURGED
                  : DISCARDABLE_MEMORY_LOCK_STATUS_SUCCESS;
  }

  virtual void Unlock() OVERRIDE {
    DCHECK(is_locked_);
    g_shared_state.Pointer()->manager.ReleaseLock(this);
    is_locked_ = false;
  }

  virtual void* Memory() const OVERRIDE {
    DCHECK(is_locked_);
    return reinterpret_cast<void*>(memory_.address());
  }

  // Overridden from internal::DiscardableMemoryManagerAllocation:
  virtual bool AllocateAndAcquireLock() OVERRIDE {
    kern_return_t ret;
    bool persistent;
    if (!memory_.size()) {
      vm_address_t address = 0;
      ret = vm_allocate(
          mach_task_self(),
          &address,
          bytes_,
          VM_FLAGS_ANYWHERE | VM_FLAGS_PURGABLE | kDiscardableMemoryTag);
      MACH_CHECK(ret == KERN_SUCCESS, ret) << "vm_allocate";
      memory_.reset(address, bytes_);

      // When making a fresh allocation, it's impossible for |persistent| to
      // be true.
      persistent = false;
    } else {
      // |persistent| will be reset to false below if appropriate, but when
      // reusing an existing allocation, it's possible for it to be true.
      persistent = true;

#if !defined(NDEBUG)
      ret = vm_protect(mach_task_self(),
                       memory_.address(),
                       memory_.size(),
                       FALSE,
                       VM_PROT_DEFAULT);
      MACH_DCHECK(ret == KERN_SUCCESS, ret) << "vm_protect";
#endif
    }

    int state = VM_PURGABLE_NONVOLATILE;
    ret = vm_purgable_control(mach_task_self(),
                              memory_.address(),
                              VM_PURGABLE_SET_STATE,
                              &state);
    MACH_CHECK(ret == KERN_SUCCESS, ret) << "vm_purgable_control";
    if (state & VM_PURGABLE_EMPTY)
      persistent = false;

    return persistent;
  }

  virtual void ReleaseLock() OVERRIDE {
    int state = VM_PURGABLE_VOLATILE | VM_VOLATILE_GROUP_DEFAULT;
    kern_return_t ret = vm_purgable_control(mach_task_self(),
                                            memory_.address(),
                                            VM_PURGABLE_SET_STATE,
                                            &state);
    MACH_CHECK(ret == KERN_SUCCESS, ret) << "vm_purgable_control";

#if !defined(NDEBUG)
    ret = vm_protect(mach_task_self(),
                     memory_.address(),
                     memory_.size(),
                     FALSE,
                     VM_PROT_NONE);
    MACH_DCHECK(ret == KERN_SUCCESS, ret) << "vm_protect";
#endif
  }

  virtual void Purge() OVERRIDE {
    memory_.reset();
  }

 private:
  mac::ScopedMachVM memory_;
  const size_t bytes_;
  bool is_locked_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryMac);
};

}  // namespace

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
    DISCARDABLE_MEMORY_TYPE_MAC,
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
      return scoped_ptr<DiscardableMemory>();
    case DISCARDABLE_MEMORY_TYPE_MAC: {
      scoped_ptr<DiscardableMemoryMac> memory(new DiscardableMemoryMac(size));
      if (!memory->Initialize())
        return scoped_ptr<DiscardableMemory>();

      return memory.PassAs<DiscardableMemory>();
    }
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
  int state = 0;
  vm_purgable_control(mach_task_self(), 0, VM_PURGABLE_PURGE_ALL, &state);
  internal::DiscardableMemoryEmulated::PurgeForTesting();
}

}  // namespace base
