// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"
#include "media/gpu/shared_memory_region.h"

namespace media {

SharedMemoryRegion::SharedMemoryRegion(const base::SharedMemoryHandle& handle,
                                       off_t offset,
                                       size_t size,
                                       bool read_only)
    : shm_(handle, read_only),
      offset_(offset),
      size_(size),
      alignment_size_(offset % base::SysInfo::VMAllocationGranularity()) {
  DCHECK_GE(offset_, 0) << "Invalid offset: " << offset_;
}

SharedMemoryRegion::SharedMemoryRegion(const BitstreamBuffer& bitstream_buffer,
                                       bool read_only)
    : SharedMemoryRegion(bitstream_buffer.handle(),
                         bitstream_buffer.offset(),
                         bitstream_buffer.size(),
                         read_only) {}

bool SharedMemoryRegion::Map() {
  if (offset_ < 0) {
    DVLOG(1) << "Invalid offset: " << offset_;
    return false;
  }
  return shm_.MapAt(offset_ - alignment_size_, size_ + alignment_size_);
}

void* SharedMemoryRegion::memory() {
  int8_t* addr = reinterpret_cast<int8_t*>(shm_.memory());
  return addr ? addr + alignment_size_ : nullptr;
}

}  // namespace media
