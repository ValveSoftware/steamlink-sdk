// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_SHARED_MEMORY_BUFFER_TRACKER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_SHARED_MEMORY_BUFFER_TRACKER_H_

#include "media/capture/video/video_capture_buffer_tracker.h"

namespace content {

// Tracker specifics for SharedMemory.
class SharedMemoryBufferTracker final
    : public media::VideoCaptureBufferTracker {
 public:
  SharedMemoryBufferTracker();

  bool Init(const gfx::Size& dimensions,
            media::VideoPixelFormat format,
            media::VideoPixelStorage storage_type,
            base::Lock* lock) override;

  std::unique_ptr<media::VideoCaptureBufferHandle> GetBufferHandle() override;
  mojo::ScopedSharedBufferHandle GetHandleForTransit() override;

 private:
  friend class SharedMemoryBufferHandle;

  // The memory created to be shared with renderer processes.
  base::SharedMemory shared_memory_;
  size_t mapped_size_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_SHARED_MEMORY_BUFFER_TRACKER_H_
