// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_MALLOC_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_MALLOC_H_

#include "base/memory/discardable_memory.h"

namespace base {
namespace internal {

class DiscardableMemoryMalloc : public DiscardableMemory {
 public:
  explicit DiscardableMemoryMalloc(size_t size);
  virtual ~DiscardableMemoryMalloc();

  bool Initialize();

  // Overridden from DiscardableMemory:
  virtual DiscardableMemoryLockStatus Lock() OVERRIDE;
  virtual void Unlock() OVERRIDE;
  virtual void* Memory() const OVERRIDE;

 private:
  scoped_ptr<uint8, FreeDeleter> memory_;
  const size_t size_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryMalloc);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_MALLOC_H_
