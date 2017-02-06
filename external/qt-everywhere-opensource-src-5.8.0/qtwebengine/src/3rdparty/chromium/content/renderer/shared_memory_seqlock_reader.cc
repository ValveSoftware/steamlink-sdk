// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/shared_memory_seqlock_reader.h"

namespace content {
namespace internal {

SharedMemorySeqLockReaderBase::SharedMemorySeqLockReaderBase() { }

SharedMemorySeqLockReaderBase::~SharedMemorySeqLockReaderBase() { }

void*
SharedMemorySeqLockReaderBase::InitializeSharedMemory(
    base::SharedMemoryHandle shared_memory_handle, size_t buffer_size) {
  renderer_shared_memory_handle_ = shared_memory_handle;
  if (!base::SharedMemory::IsHandleValid(renderer_shared_memory_handle_))
    return 0;
  renderer_shared_memory_.reset(new base::SharedMemory(
      renderer_shared_memory_handle_, true));

  return (renderer_shared_memory_->Map(buffer_size))
      ? renderer_shared_memory_->memory()
      : 0;
}

bool SharedMemorySeqLockReaderBase::FetchFromBuffer(
    content::OneWriterSeqLock* seqlock, void* final, void* temp, void* from,
    size_t size) {

  if (!base::SharedMemory::IsHandleValid(renderer_shared_memory_handle_))
    return false;

  // Only try to read this many times before failing to avoid waiting here
  // very long in case of contention with the writer.
  int contention_count = -1;
  base::subtle::Atomic32 version;
  do {
    version = seqlock->ReadBegin();
    memcpy(temp, from, size);
    ++contention_count;
    if (contention_count == kMaximumContentionCount)
      break;
  } while (seqlock->ReadRetry(version));

  if (contention_count >= kMaximumContentionCount) {
    // We failed to successfully read, presumably because the hardware
    // thread was taking unusually long. Don't copy the data to the output
    // buffer, and simply leave what was there before.
    return false;
  }

  // New data was read successfully, copy it into the output buffer.
  memcpy(final, temp, size);
  return true;
}

}  // namespace internal
}  // namespace content
