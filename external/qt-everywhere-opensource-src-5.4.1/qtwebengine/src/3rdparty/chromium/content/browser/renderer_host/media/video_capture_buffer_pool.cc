// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"

namespace content {

const int VideoCaptureBufferPool::kInvalidId = -1;

VideoCaptureBufferPool::VideoCaptureBufferPool(int count)
    : count_(count),
      next_buffer_id_(0) {
}

VideoCaptureBufferPool::~VideoCaptureBufferPool() {
  STLDeleteValues(&buffers_);
}

base::SharedMemoryHandle VideoCaptureBufferPool::ShareToProcess(
    int buffer_id,
    base::ProcessHandle process_handle,
    size_t* memory_size) {
  base::AutoLock lock(lock_);

  Buffer* buffer = GetBuffer(buffer_id);
  if (!buffer) {
    NOTREACHED() << "Invalid buffer_id.";
    return base::SharedMemory::NULLHandle();
  }
  base::SharedMemoryHandle remote_handle;
  buffer->shared_memory.ShareToProcess(process_handle, &remote_handle);
  *memory_size = buffer->shared_memory.requested_size();
  return remote_handle;
}

bool VideoCaptureBufferPool::GetBufferInfo(int buffer_id,
                                           void** memory,
                                           size_t* size) {
  base::AutoLock lock(lock_);

  Buffer* buffer = GetBuffer(buffer_id);
  if (!buffer) {
    NOTREACHED() << "Invalid buffer_id.";
    return false;
  }

  DCHECK(buffer->held_by_producer);
  *memory = buffer->shared_memory.memory();
  *size = buffer->shared_memory.mapped_size();
  return true;
}

int VideoCaptureBufferPool::ReserveForProducer(size_t size,
                                               int* buffer_id_to_drop) {
  base::AutoLock lock(lock_);
  return ReserveForProducerInternal(size, buffer_id_to_drop);
}

void VideoCaptureBufferPool::RelinquishProducerReservation(int buffer_id) {
  base::AutoLock lock(lock_);
  Buffer* buffer = GetBuffer(buffer_id);
  if (!buffer) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK(buffer->held_by_producer);
  buffer->held_by_producer = false;
}

void VideoCaptureBufferPool::HoldForConsumers(
    int buffer_id,
    int num_clients) {
  base::AutoLock lock(lock_);
  Buffer* buffer = GetBuffer(buffer_id);
  if (!buffer) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK(buffer->held_by_producer);
  DCHECK(!buffer->consumer_hold_count);

  buffer->consumer_hold_count = num_clients;
  // Note: |held_by_producer| will stay true until
  // RelinquishProducerReservation() (usually called by destructor of the object
  // wrapping this buffer, e.g. a media::VideoFrame).
}

void VideoCaptureBufferPool::RelinquishConsumerHold(int buffer_id,
                                                    int num_clients) {
  base::AutoLock lock(lock_);
  Buffer* buffer = GetBuffer(buffer_id);
  if (!buffer) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK_GE(buffer->consumer_hold_count, num_clients);

  buffer->consumer_hold_count -= num_clients;
}

VideoCaptureBufferPool::Buffer::Buffer()
    : held_by_producer(false), consumer_hold_count(0) {}

int VideoCaptureBufferPool::ReserveForProducerInternal(size_t size,
                                                       int* buffer_id_to_drop) {
  lock_.AssertAcquired();

  // Look for a buffer that's allocated, big enough, and not in use. Track the
  // largest one that's not big enough, in case we have to reallocate a buffer.
  *buffer_id_to_drop = kInvalidId;
  size_t realloc_size = 0;
  BufferMap::iterator realloc = buffers_.end();
  for (BufferMap::iterator it = buffers_.begin(); it != buffers_.end(); ++it) {
    Buffer* buffer = it->second;
    if (!buffer->consumer_hold_count && !buffer->held_by_producer) {
      if (buffer->shared_memory.requested_size() >= size) {
        // Existing buffer is big enough. Reuse it.
        buffer->held_by_producer = true;
        return it->first;
      }
      if (buffer->shared_memory.requested_size() > realloc_size) {
        realloc_size = buffer->shared_memory.requested_size();
        realloc = it;
      }
    }
  }

  // Preferentially grow the pool by creating a new buffer. If we're at maximum
  // size, then reallocate by deleting an existing one instead.
  if (buffers_.size() == static_cast<size_t>(count_)) {
    if (realloc == buffers_.end()) {
      // We're out of space, and can't find an unused buffer to reallocate.
      return kInvalidId;
    }
    *buffer_id_to_drop = realloc->first;
    delete realloc->second;
    buffers_.erase(realloc);
  }

  // Create the new buffer.
  int buffer_id = next_buffer_id_++;
  scoped_ptr<Buffer> buffer(new Buffer());
  if (size) {
    // |size| can be 0 for buffers that do not require memory backing.
    if (!buffer->shared_memory.CreateAndMapAnonymous(size))
      return kInvalidId;
  }
  buffer->held_by_producer = true;
  buffers_[buffer_id] = buffer.release();
  return buffer_id;
}

VideoCaptureBufferPool::Buffer* VideoCaptureBufferPool::GetBuffer(
    int buffer_id) {
  BufferMap::iterator it = buffers_.find(buffer_id);
  if (it == buffers_.end())
    return NULL;
  return it->second;
}

}  // namespace content

