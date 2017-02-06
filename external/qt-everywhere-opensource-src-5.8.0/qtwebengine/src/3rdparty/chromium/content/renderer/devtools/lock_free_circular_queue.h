// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVTOOLS_LOCK_FREE_CIRCULAR_QUEUE_H_
#define CONTENT_RENDERER_DEVTOOLS_LOCK_FREE_CIRCULAR_QUEUE_H_

#include <stddef.h>

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/memory/aligned_memory.h"

#define CACHELINE_ALIGNED ALIGNAS(64)

namespace content {

MSVC_PUSH_DISABLE_WARNING(4324)  // structure was padded due to align

// Lock-free cache-friendly sampling circular queue for large
// records. Intended for fast transfer of large records between a
// single producer and a single consumer. If the queue is full,
// StartEnqueue will return nullptr. The queue is designed with
// a goal in mind to evade cache lines thrashing by preventing
// simultaneous reads and writes to adjanced memory locations.
template <typename T, unsigned Length>
class LockFreeCircularQueue {
 public:
  // Executed on the application thread.
  LockFreeCircularQueue();
  ~LockFreeCircularQueue();

  // StartEnqueue returns a pointer to a memory location for storing the next
  // record or nullptr if all entries are full at the moment.
  T* StartEnqueue();
  // Notifies the queue that the producer has complete writing data into the
  // memory returned by StartEnqueue and it can be passed to the consumer.
  void FinishEnqueue();

  // Executed on the consumer (analyzer) thread.
  // Retrieves, but does not remove, the head of this queue, returning nullptr
  // if this queue is empty. After the record had been read by a consumer,
  // Remove must be called.
  T* Peek();
  void Remove();

  // The class fields have stricter alignment requirements than a normal new
  // can fulfil, so we need to provide our own new/delete here.
  void* operator new(size_t size);
  void operator delete(void* ptr);

 private:
  // Reserved values for the entry marker.
  enum {
    kEmpty,  // Marks clean (processed) entries.
    kFull    // Marks entries already filled by the producer but not yet
             // completely processed by the consumer.
  };

  struct CACHELINE_ALIGNED Entry {
    Entry() : marker(kEmpty) {}
    T record;
    base::subtle::Atomic32 marker;
  };

  Entry* Next(Entry* entry);

  Entry buffer_[Length];
  CACHELINE_ALIGNED Entry* enqueue_pos_;
  CACHELINE_ALIGNED Entry* dequeue_pos_;

  DISALLOW_COPY_AND_ASSIGN(LockFreeCircularQueue);
};

MSVC_POP_WARNING()

template <typename T, unsigned L>
LockFreeCircularQueue<T, L>::LockFreeCircularQueue()
    : enqueue_pos_(buffer_), dequeue_pos_(buffer_) {
}

template <typename T, unsigned L>
LockFreeCircularQueue<T, L>::~LockFreeCircularQueue() {
}

template <typename T, unsigned L>
T* LockFreeCircularQueue<T, L>::Peek() {
  base::subtle::MemoryBarrier();
  if (base::subtle::Acquire_Load(&dequeue_pos_->marker) == kFull) {
    return &dequeue_pos_->record;
  }
  return nullptr;
}

template <typename T, unsigned L>
void LockFreeCircularQueue<T, L>::Remove() {
  base::subtle::Release_Store(&dequeue_pos_->marker, kEmpty);
  dequeue_pos_ = Next(dequeue_pos_);
}

template <typename T, unsigned L>
T* LockFreeCircularQueue<T, L>::StartEnqueue() {
  base::subtle::MemoryBarrier();
  if (base::subtle::Acquire_Load(&enqueue_pos_->marker) == kEmpty) {
    return &enqueue_pos_->record;
  }
  return nullptr;
}

template <typename T, unsigned L>
void LockFreeCircularQueue<T, L>::FinishEnqueue() {
  base::subtle::Release_Store(&enqueue_pos_->marker, kFull);
  enqueue_pos_ = Next(enqueue_pos_);
}

template <typename T, unsigned L>
typename LockFreeCircularQueue<T, L>::Entry* LockFreeCircularQueue<T, L>::Next(
    Entry* entry) {
  Entry* next = entry + 1;
  if (next == &buffer_[L])
    return buffer_;
  return next;
}

template <typename T, unsigned L>
void* LockFreeCircularQueue<T, L>::operator new(size_t size) {
  typedef LockFreeCircularQueue<T, L> QueueTypeAlias;
  return base::AlignedAlloc(size, ALIGNOF(QueueTypeAlias));
}

template <typename T, unsigned L>
void LockFreeCircularQueue<T, L>::operator delete(void* ptr) {
  base::AlignedFree(ptr);
}

}  // namespace content

#endif  // CONTENT_RENDERER_DEVTOOLS_LOCK_FREE_CIRCULAR_QUEUE_H_
