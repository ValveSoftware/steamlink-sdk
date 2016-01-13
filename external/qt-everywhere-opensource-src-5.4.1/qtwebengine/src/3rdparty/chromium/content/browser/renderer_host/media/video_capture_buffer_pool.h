// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "ui/gfx/size.h"

namespace media {

class VideoFrame;

}  // namespace media

namespace content {

// A thread-safe class that does the bookkeeping and lifetime management for a
// pool of shared-memory pixel buffers cycled between an in-process producer
// (e.g. a VideoCaptureDevice) and a set of out-of-process consumers. The pool
// is intended to be orchestrated by a VideoCaptureController, but is designed
// to outlive the controller if necessary.
//
// Producers get a buffer by calling ReserveForProducer(), and may pass on their
// ownership to the consumer by calling HoldForConsumers(), or drop the buffer
// (without further processing) by calling RelinquishProducerReservation().
// Consumers signal that they are done with the buffer by calling
// RelinquishConsumerHold().
//
// Buffers are allocated on demand, but there will never be more than |count|
// buffers in existence at any time. Buffers are identified by an int value
// called |buffer_id|. -1 (kInvalidId) is never a valid ID, and is returned by
// some methods to indicate failure. The active set of buffer ids may change
// over the lifetime of the buffer pool, as existing buffers are freed and
// reallocated at larger size. When reallocation occurs, new buffer IDs will
// circulate.
class CONTENT_EXPORT VideoCaptureBufferPool
    : public base::RefCountedThreadSafe<VideoCaptureBufferPool> {
 public:
  static const int kInvalidId;
  explicit VideoCaptureBufferPool(int count);

  // One-time (per client/per-buffer) initialization to share a particular
  // buffer to a process. The size of the allocation is returned as
  // |memory_size|.
  base::SharedMemoryHandle ShareToProcess(int buffer_id,
                                          base::ProcessHandle process_handle,
                                          size_t* memory_size);

  // Query the memory parameters of |buffer_id|. Fills in parameters in the
  // pointer arguments, and returns true iff the buffer exists.
  bool GetBufferInfo(int buffer_id, void** memory, size_t* size);

  // Reserve or allocate a buffer of at least |size| bytes and return its id.
  // This will fail (returning kInvalidId) if the pool already is at its |count|
  // limit of the number of allocations, and all allocated buffers are in use by
  // the producer and/or consumers.
  //
  // If successful, the reserved buffer remains reserved (and writable by the
  // producer) until ownership is transferred either to the consumer via
  // HoldForConsumers(), or back to the pool with
  // RelinquishProducerReservation().
  //
  // On occasion, this call will decide to free an old buffer to make room for a
  // new allocation at a larger size. If so, the ID of the destroyed buffer is
  // returned via |buffer_id_to_drop|.
  int ReserveForProducer(size_t size, int* buffer_id_to_drop);

  // Indicate that a buffer held for the producer should be returned back to the
  // pool without passing on to the consumer. This effectively is the opposite
  // of ReserveForProducer().
  void RelinquishProducerReservation(int buffer_id);

  // Transfer a buffer from producer to consumer ownership.
  // |buffer_id| must be a buffer index previously returned by
  // ReserveForProducer(), and not already passed to HoldForConsumers().
  void HoldForConsumers(int buffer_id, int num_clients);

  // Indicate that one or more consumers are done with a particular buffer. This
  // effectively is the opposite of HoldForConsumers(). Once the consumers are
  // done, a buffer is returned to the pool for reuse.
  void RelinquishConsumerHold(int buffer_id, int num_clients);

  int count() const { return count_; }

 private:
  friend class base::RefCountedThreadSafe<VideoCaptureBufferPool>;

  // Per-buffer state.
  struct Buffer {
    Buffer();

    // The memory created to be shared with renderer processes.
    base::SharedMemory shared_memory;

    // Tracks whether this buffer is currently referenced by the producer.
    bool held_by_producer;

    // Number of consumer processes which hold this shared memory.
    int consumer_hold_count;
  };

  typedef std::map<int, Buffer*> BufferMap;

  virtual ~VideoCaptureBufferPool();

  int ReserveForProducerInternal(size_t size, int* buffer_id_to_drop);

  Buffer* GetBuffer(int buffer_id);

  // The max number of buffers that the pool is allowed to have at any moment.
  const int count_;

  // Protects everything below it.
  base::Lock lock_;

  // The ID of the next buffer.
  int next_buffer_id_;

  // The buffers, indexed by |buffer_id|.
  BufferMap buffers_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureBufferPool);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_POOL_H_
