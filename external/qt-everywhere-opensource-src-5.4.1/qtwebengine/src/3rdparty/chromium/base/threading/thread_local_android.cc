// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_local.h"

#include "base/logging.h"

namespace base {
namespace internal {

// static
void ThreadLocalPlatform::AllocateSlot(SlotType* slot) {
  bool succeed = slot->Initialize(NULL);
  CHECK(succeed);
}

// static
void ThreadLocalPlatform::FreeSlot(SlotType slot) {
  slot.Free();
}

// static
void* ThreadLocalPlatform::GetValueFromSlot(SlotType slot) {
  return slot.Get();
}

// static
void ThreadLocalPlatform::SetValueInSlot(SlotType slot, void* value) {
  slot.Set(value);
}

}  // namespace internal
}  // namespace base
