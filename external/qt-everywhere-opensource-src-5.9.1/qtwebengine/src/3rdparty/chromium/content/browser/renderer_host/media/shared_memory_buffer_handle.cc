// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/shared_memory_buffer_handle.h"

#include "content/browser/renderer_host/media/shared_memory_buffer_tracker.h"

namespace content {

SharedMemoryBufferHandle::SharedMemoryBufferHandle(
    SharedMemoryBufferTracker* tracker)
    : tracker_(tracker) {}

SharedMemoryBufferHandle::~SharedMemoryBufferHandle() = default;

gfx::Size SharedMemoryBufferHandle::dimensions() const {
  return tracker_->dimensions();
}

size_t SharedMemoryBufferHandle::mapped_size() const {
  return tracker_->mapped_size_;
}

void* SharedMemoryBufferHandle::data(int plane) {
  DCHECK_EQ(plane, 0);
  return tracker_->shared_memory_.memory();
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
base::FileDescriptor SharedMemoryBufferHandle::AsPlatformFile() {
  return tracker_->shared_memory_.handle();
}
#endif

bool SharedMemoryBufferHandle::IsBackedByVideoFrame() const {
  return false;
}

scoped_refptr<media::VideoFrame> SharedMemoryBufferHandle::GetVideoFrame() {
  return scoped_refptr<media::VideoFrame>();
}

}  // namespace content
