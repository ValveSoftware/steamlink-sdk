// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/leak_detector/call_stack_manager.h"

#include <algorithm>  // For std::copy.
#include <new>

#include "base/hash.h"
#include "components/metrics/leak_detector/custom_allocator.h"

namespace metrics {
namespace leak_detector {

CallStackManager::CallStackManager() {}

CallStackManager::~CallStackManager() {
  // Free all call stack objects and clear |call_stacks_|. Make sure to save the
  // CallStack object pointer and remove it from the container before freeing
  // the CallStack memory.
  while (!call_stacks_.empty()) {
    CallStack* call_stack = *call_stacks_.begin();
    call_stacks_.erase(call_stacks_.begin());

    CustomAllocator::Free(call_stack->stack,
                          call_stack->depth * sizeof(*call_stack->stack));
    call_stack->stack = nullptr;
    call_stack->depth = 0;

    CustomAllocator::Free(call_stack, sizeof(CallStack));
  }
}

const CallStack* CallStackManager::GetCallStack(size_t depth,
                                                const void* const stack[]) {
  // Temporarily create a call stack object for lookup in |call_stacks_|.
  CallStack temp;
  temp.depth = depth;
  temp.stack = const_cast<const void**>(stack);
  // This is the only place where the call stack's hash is computed. This value
  // can be reused in the created object to avoid further hash computation.
  temp.hash =
      base::Hash(reinterpret_cast<const char*>(stack), sizeof(*stack) * depth);

  auto iter = call_stacks_.find(&temp);
  if (iter != call_stacks_.end())
    return *iter;

  // Since |call_stacks_| stores CallStack pointers rather than actual objects,
  // create new call objects manually here.
  CallStack* call_stack =
      new (CustomAllocator::Allocate(sizeof(CallStack))) CallStack;
  call_stack->depth = depth;
  call_stack->hash = temp.hash;  // Don't run the hash function again.
  call_stack->stack = reinterpret_cast<const void**>(
      CustomAllocator::Allocate(sizeof(*stack) * depth));
  std::copy(stack, stack + depth, call_stack->stack);

  call_stacks_.insert(call_stack);
  return call_stack;
}

bool CallStackManager::CallStackPointerEqual::operator()(
    const CallStack* c1,
    const CallStack* c2) const {
  return c1->depth == c2->depth && c1->hash == c2->hash &&
         std::equal(c1->stack, c1->stack + c1->depth, c2->stack);
}

}  // namespace leak_detector
}  // namespace metrics
