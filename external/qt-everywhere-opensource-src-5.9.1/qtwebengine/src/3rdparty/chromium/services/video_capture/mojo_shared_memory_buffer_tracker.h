// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_MOJO_SHARED_MEMORY_BUFFER_TRACKER_H_
#define SERVICES_VIDEO_CAPTURE_MOJO_SHARED_MEMORY_BUFFER_TRACKER_H_

#include "media/capture/video/video_capture_buffer_tracker.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"

namespace video_capture {

// Tracker specifics for MojoSharedMemory.
class MojoSharedMemoryBufferTracker final
    : public media::VideoCaptureBufferTracker {
 public:
  MojoSharedMemoryBufferTracker();
  ~MojoSharedMemoryBufferTracker() override;

  // Implementation of media::VideoCaptureBufferTracker
  bool Init(const gfx::Size& dimensions,
            media::VideoPixelFormat format,
            media::VideoPixelStorage storage_type,
            base::Lock* lock) override;
  std::unique_ptr<media::VideoCaptureBufferHandle> GetBufferHandle() override;
  mojo::ScopedSharedBufferHandle GetHandleForTransit() override;

 private:
  friend class MojoSharedMemoryBufferHandle;

  scoped_refptr<media::MojoSharedBufferVideoFrame> frame_;
  size_t mapped_size_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_MOJO_SHARED_MEMORY_BUFFER_TRACKER_H_
