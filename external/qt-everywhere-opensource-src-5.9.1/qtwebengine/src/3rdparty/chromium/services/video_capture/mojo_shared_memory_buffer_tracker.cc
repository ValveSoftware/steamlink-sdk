// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/mojo_shared_memory_buffer_tracker.h"

#include "base/memory/ptr_util.h"
#include "services/video_capture/mojo_shared_memory_buffer_handle.h"

namespace video_capture {

MojoSharedMemoryBufferTracker::MojoSharedMemoryBufferTracker()
    : VideoCaptureBufferTracker() {}

MojoSharedMemoryBufferTracker::~MojoSharedMemoryBufferTracker() = default;

bool MojoSharedMemoryBufferTracker::Init(const gfx::Size& dimensions,
                                         media::VideoPixelFormat format,
                                         media::VideoPixelStorage storage_type,
                                         base::Lock* lock) {
  if ((format != media::PIXEL_FORMAT_I420) ||
      (storage_type != media::PIXEL_STORAGE_CPU)) {
    LOG(ERROR) << "Unsupported pixel format or storage type";
    return false;
  }

  set_dimensions(dimensions);
  set_max_pixel_count(dimensions.GetArea());
  set_pixel_format(format);
  set_storage_type(storage_type);
  mapped_size_ =
      media::VideoCaptureFormat(dimensions, 0.0f, format, storage_type)
          .ImageAllocationSize();

  frame_ = media::MojoSharedBufferVideoFrame::CreateDefaultI420(
      dimensions, base::TimeDelta());
  return true;
}

std::unique_ptr<media::VideoCaptureBufferHandle>
MojoSharedMemoryBufferTracker::GetBufferHandle() {
  return base::MakeUnique<MojoSharedMemoryBufferHandle>(this);
}

mojo::ScopedSharedBufferHandle
MojoSharedMemoryBufferTracker::GetHandleForTransit() {
  return mojo::ScopedSharedBufferHandle();
}

}  // namespace video_capture
