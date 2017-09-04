// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/HeapAllocator.h"

namespace blink {

void HeapAllocator::backingFree(void* address) {
  if (!address)
    return;

  ThreadState* state = ThreadState::current();
  if (state->sweepForbidden())
    return;
  ASSERT(!state->isInGC());

  // Don't promptly free large objects because their page is never reused.
  // Don't free backings allocated on other threads.
  BasePage* page = pageFromObject(address);
  if (page->isLargeObjectPage() || page->arena()->getThreadState() != state)
    return;

  HeapObjectHeader* header = HeapObjectHeader::fromPayload(address);
  ASSERT(header->checkHeader());
  NormalPageArena* arena = static_cast<NormalPage*>(page)->arenaForNormalPage();
  state->promptlyFreed(header->gcInfoIndex());
  arena->promptlyFreeObject(header);
}

void HeapAllocator::freeVectorBacking(void* address) {
  backingFree(address);
}

void HeapAllocator::freeInlineVectorBacking(void* address) {
  backingFree(address);
}

void HeapAllocator::freeHashTableBacking(void* address) {
  backingFree(address);
}

bool HeapAllocator::backingExpand(void* address, size_t newSize) {
  if (!address)
    return false;

  ThreadState* state = ThreadState::current();
  if (state->sweepForbidden())
    return false;
  ASSERT(!state->isInGC());
  ASSERT(state->isAllocationAllowed());
  DCHECK_EQ(&state->heap(), &ThreadState::fromObject(address)->heap());

  // FIXME: Support expand for large objects.
  // Don't expand backings allocated on other threads.
  BasePage* page = pageFromObject(address);
  if (page->isLargeObjectPage() || page->arena()->getThreadState() != state)
    return false;

  HeapObjectHeader* header = HeapObjectHeader::fromPayload(address);
  ASSERT(header->checkHeader());
  NormalPageArena* arena = static_cast<NormalPage*>(page)->arenaForNormalPage();
  bool succeed = arena->expandObject(header, newSize);
  if (succeed)
    state->allocationPointAdjusted(arena->arenaIndex());
  return succeed;
}

bool HeapAllocator::expandVectorBacking(void* address, size_t newSize) {
  return backingExpand(address, newSize);
}

bool HeapAllocator::expandInlineVectorBacking(void* address, size_t newSize) {
  return backingExpand(address, newSize);
}

bool HeapAllocator::expandHashTableBacking(void* address, size_t newSize) {
  return backingExpand(address, newSize);
}

bool HeapAllocator::backingShrink(void* address,
                                  size_t quantizedCurrentSize,
                                  size_t quantizedShrunkSize) {
  if (!address || quantizedShrunkSize == quantizedCurrentSize)
    return true;

  ASSERT(quantizedShrunkSize < quantizedCurrentSize);

  ThreadState* state = ThreadState::current();
  if (state->sweepForbidden())
    return false;
  ASSERT(!state->isInGC());
  ASSERT(state->isAllocationAllowed());
  DCHECK_EQ(&state->heap(), &ThreadState::fromObject(address)->heap());

  // FIXME: Support shrink for large objects.
  // Don't shrink backings allocated on other threads.
  BasePage* page = pageFromObject(address);
  if (page->isLargeObjectPage() || page->arena()->getThreadState() != state)
    return false;

  HeapObjectHeader* header = HeapObjectHeader::fromPayload(address);
  ASSERT(header->checkHeader());
  NormalPageArena* arena = static_cast<NormalPage*>(page)->arenaForNormalPage();
  // We shrink the object only if the shrinking will make a non-small
  // prompt-free block.
  // FIXME: Optimize the threshold size.
  if (quantizedCurrentSize <=
          quantizedShrunkSize + sizeof(HeapObjectHeader) + sizeof(void*) * 32 &&
      !arena->isObjectAllocatedAtAllocationPoint(header))
    return true;

  bool succeededAtAllocationPoint =
      arena->shrinkObject(header, quantizedShrunkSize);
  if (succeededAtAllocationPoint)
    state->allocationPointAdjusted(arena->arenaIndex());
  return true;
}

bool HeapAllocator::shrinkVectorBacking(void* address,
                                        size_t quantizedCurrentSize,
                                        size_t quantizedShrunkSize) {
  return backingShrink(address, quantizedCurrentSize, quantizedShrunkSize);
}

bool HeapAllocator::shrinkInlineVectorBacking(void* address,
                                              size_t quantizedCurrentSize,
                                              size_t quantizedShrunkSize) {
  return backingShrink(address, quantizedCurrentSize, quantizedShrunkSize);
}

}  // namespace blink
