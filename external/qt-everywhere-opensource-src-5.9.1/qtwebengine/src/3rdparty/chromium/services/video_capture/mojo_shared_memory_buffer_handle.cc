// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/mojo_shared_memory_buffer_handle.h"

#include "services/video_capture/mojo_shared_memory_buffer_tracker.h"

namespace video_capture {

MojoSharedMemoryBufferHandle::MojoSharedMemoryBufferHandle(
    MojoSharedMemoryBufferTracker* tracker)
    : tracker_(tracker) {}

MojoSharedMemoryBufferHandle::~MojoSharedMemoryBufferHandle() = default;

gfx::Size MojoSharedMemoryBufferHandle::dimensions() const {
  return tracker_->dimensions();
}

size_t MojoSharedMemoryBufferHandle::mapped_size() const {
  return tracker_->mapped_size_;
}

void* MojoSharedMemoryBufferHandle::data(int plane) {
  DCHECK_GE(plane, 0);
  return tracker_->frame_->data(static_cast<size_t>(plane));
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
base::FileDescriptor MojoSharedMemoryBufferHandle::AsPlatformFile() {
  NOTREACHED();
  return base::FileDescriptor();
}
#endif

bool MojoSharedMemoryBufferHandle::IsBackedByVideoFrame() const {
  return true;
}

scoped_refptr<media::VideoFrame> MojoSharedMemoryBufferHandle::GetVideoFrame() {
  return tracker_->frame_;
}

}  // namespace video_capture
