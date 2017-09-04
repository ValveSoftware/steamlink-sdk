// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/CallbackStack.h"
#include "wtf/PtrUtil.h"
#include "wtf/allocator/PageAllocator.h"
#include "wtf/allocator/Partitions.h"

namespace blink {

CallbackStackMemoryPool& CallbackStackMemoryPool::instance() {
  DEFINE_STATIC_LOCAL(CallbackStackMemoryPool, memoryPool, ());
  return memoryPool;
}

void CallbackStackMemoryPool::initialize() {
  m_freeListFirst = 0;
  for (size_t index = 0; index < kPooledBlockCount - 1; ++index) {
    m_freeListNext[index] = index + 1;
  }
  m_freeListNext[kPooledBlockCount - 1] = -1;
  m_pooledMemory = static_cast<CallbackStack::Item*>(
      WTF::allocPages(nullptr, kBlockBytes * kPooledBlockCount,
                      WTF::kPageAllocationGranularity, WTF::PageAccessible));
  CHECK(m_pooledMemory);
}

void CallbackStackMemoryPool::shutdown() {
  WTF::freePages(m_pooledMemory, kBlockBytes * kPooledBlockCount);
  m_pooledMemory = nullptr;
  m_freeListFirst = 0;
}

CallbackStack::Item* CallbackStackMemoryPool::allocate() {
  MutexLocker locker(m_mutex);
  // Allocate from a free list if available.
  if (m_freeListFirst != -1) {
    size_t index = m_freeListFirst;
    DCHECK(0 <= index && index < CallbackStackMemoryPool::kPooledBlockCount);
    m_freeListFirst = m_freeListNext[index];
    m_freeListNext[index] = -1;
    return m_pooledMemory + kBlockSize * index;
  }
  // Otherwise, allocate a new memory region.
  CallbackStack::Item* memory =
      static_cast<CallbackStack::Item*>(WTF::Partitions::fastZeroedMalloc(
          kBlockBytes, "CallbackStackMemoryPool"));
  CHECK(memory);
  return memory;
}

void CallbackStackMemoryPool::free(CallbackStack::Item* memory) {
  MutexLocker locker(m_mutex);
  int index = (reinterpret_cast<uintptr_t>(memory) -
               reinterpret_cast<uintptr_t>(m_pooledMemory)) /
              (kBlockSize * sizeof(CallbackStack::Item));
  // If the memory is a newly allocated region, free the memory.
  if (index < 0 || static_cast<int>(kPooledBlockCount) <= index) {
    WTF::Partitions::fastFree(memory);
    return;
  }
  // Otherwise, return the memory back to the free list.
  DCHECK_EQ(m_freeListNext[index], -1);
  m_freeListNext[index] = m_freeListFirst;
  m_freeListFirst = index;
}

CallbackStack::Block::Block(Block* next) {
  m_buffer = CallbackStackMemoryPool::instance().allocate();
#if ENABLE(ASSERT)
  clear();
#endif

  m_limit = &(m_buffer[CallbackStackMemoryPool::kBlockSize]);
  m_current = &(m_buffer[0]);
  m_next = next;
}

CallbackStack::Block::~Block() {
  CallbackStackMemoryPool::instance().free(m_buffer);
  m_buffer = nullptr;
  m_limit = nullptr;
  m_current = nullptr;
  m_next = nullptr;
}

#if ENABLE(ASSERT)
void CallbackStack::Block::clear() {
  for (size_t i = 0; i < CallbackStackMemoryPool::kBlockSize; i++)
    m_buffer[i] = Item(0, 0);
}
#endif

void CallbackStack::Block::invokeEphemeronCallbacks(Visitor* visitor) {
  // This loop can tolerate entries being added by the callbacks after
  // iteration starts.
  for (unsigned i = 0; m_buffer + i < m_current; i++) {
    Item& item = m_buffer[i];
    item.call(visitor);
  }
}

#if ENABLE(ASSERT)
bool CallbackStack::Block::hasCallbackForObject(const void* object) {
  for (unsigned i = 0; m_buffer + i < m_current; i++) {
    Item* item = &m_buffer[i];
    if (item->object() == object)
      return true;
  }
  return false;
}
#endif

std::unique_ptr<CallbackStack> CallbackStack::create() {
  return wrapUnique(new CallbackStack());
}

CallbackStack::CallbackStack() : m_first(nullptr), m_last(nullptr) {}

CallbackStack::~CallbackStack() {
  CHECK(isEmpty());
  m_first = nullptr;
  m_last = nullptr;
}

void CallbackStack::commit() {
  DCHECK(!m_first);
  m_first = new Block(m_first);
  m_last = m_first;
}

void CallbackStack::decommit() {
  if (!m_first)
    return;
  Block* next;
  for (Block* current = m_first->next(); current; current = next) {
    next = current->next();
    delete current;
  }
  delete m_first;
  m_last = m_first = nullptr;
}

bool CallbackStack::isEmpty() const {
  return !m_first || (hasJustOneBlock() && m_first->isEmptyBlock());
}

CallbackStack::Item* CallbackStack::allocateEntrySlow() {
  DCHECK(m_first);
  DCHECK(!m_first->allocateEntry());
  m_first = new Block(m_first);
  return m_first->allocateEntry();
}

CallbackStack::Item* CallbackStack::popSlow() {
  DCHECK(m_first);
  DCHECK(m_first->isEmptyBlock());

  for (;;) {
    Block* next = m_first->next();
    if (!next) {
#if ENABLE(ASSERT)
      m_first->clear();
#endif
      return nullptr;
    }
    delete m_first;
    m_first = next;
    if (Item* item = m_first->pop())
      return item;
  }
}

void CallbackStack::invokeEphemeronCallbacks(Visitor* visitor) {
  // The first block is the only one where new ephemerons are added, so we
  // call the callbacks on that last, to catch any new ephemerons discovered
  // in the callbacks.
  // However, if enough ephemerons were added, we may have a new block that
  // has been prepended to the chain. This will be very rare, but we can
  // handle the situation by starting again and calling all the callbacks
  // on the prepended blocks.
  Block* from = nullptr;
  Block* upto = nullptr;
  while (from != m_first) {
    upto = from;
    from = m_first;
    invokeOldestCallbacks(from, upto, visitor);
  }
}

void CallbackStack::invokeOldestCallbacks(Block* from,
                                          Block* upto,
                                          Visitor* visitor) {
  if (from == upto)
    return;
  DCHECK(from);
  // Recurse first so we get to the newly added entries last.
  invokeOldestCallbacks(from->next(), upto, visitor);
  from->invokeEphemeronCallbacks(visitor);
}

bool CallbackStack::hasJustOneBlock() const {
  DCHECK(m_first);
  return !m_first->next();
}

#if ENABLE(ASSERT)
bool CallbackStack::hasCallbackForObject(const void* object) {
  for (Block* current = m_first; current; current = current->next()) {
    if (current->hasCallbackForObject(object))
      return true;
  }
  return false;
}
#endif

}  // namespace blink
