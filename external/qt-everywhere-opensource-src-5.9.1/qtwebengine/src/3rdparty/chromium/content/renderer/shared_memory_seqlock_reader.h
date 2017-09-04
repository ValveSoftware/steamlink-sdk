// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_MEMORY_SEQLOCK_READER_H_
#define CONTENT_RENDERER_SHARED_MEMORY_SEQLOCK_READER_H_

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "device/base/synchronization/shared_memory_seqlock_buffer.h"

namespace content {
namespace internal {

class SharedMemorySeqLockReaderBase  {
 protected:
  SharedMemorySeqLockReaderBase();
  virtual ~SharedMemorySeqLockReaderBase();

  void* InitializeSharedMemory(
      base::SharedMemoryHandle shared_memory_handle,
      size_t buffer_size);

  bool FetchFromBuffer(device::OneWriterSeqLock* seqlock,
                       void* final,
                       void* temp,
                       void* from,
                       size_t size);

  static const int kMaximumContentionCount = 10;
  base::SharedMemoryHandle renderer_shared_memory_handle_;
  std::unique_ptr<base::SharedMemory> renderer_shared_memory_;
};

}  // namespace internal

// Template argument Data should be a pod-like structure only containing
// data fields, such that it is copyable by memcpy method.
template<typename Data>
class SharedMemorySeqLockReader
    : private internal::SharedMemorySeqLockReaderBase {
 public:
  SharedMemorySeqLockReader() : buffer_(0) { }
  virtual ~SharedMemorySeqLockReader() { }

  bool GetLatestData(Data* data) {
    DCHECK(buffer_);
    DCHECK(sizeof(*data) == sizeof(*temp_buffer_));
    return FetchFromBuffer(&buffer_->seqlock, data, temp_buffer_.get(),
        &buffer_->data, sizeof(*temp_buffer_));
  }

  bool Initialize(base::SharedMemoryHandle shared_memory_handle) {
    if (void* memory = InitializeSharedMemory(
            shared_memory_handle,
            sizeof(device::SharedMemorySeqLockBuffer<Data>))) {
      buffer_ = static_cast<device::SharedMemorySeqLockBuffer<Data>*>(memory);
      temp_buffer_.reset(new Data);
      return true;
    }
    return false;
  }

 private:
  device::SharedMemorySeqLockBuffer<Data>* buffer_;
  std::unique_ptr<Data> temp_buffer_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemorySeqLockReader);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_MEMORY_SEQLOCK_READER_H_
