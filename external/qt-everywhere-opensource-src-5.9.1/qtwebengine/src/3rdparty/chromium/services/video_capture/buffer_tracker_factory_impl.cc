// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/buffer_tracker_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "services/video_capture/mojo_shared_memory_buffer_tracker.h"

namespace video_capture {

std::unique_ptr<media::VideoCaptureBufferTracker>
BufferTrackerFactoryImpl::CreateTracker(media::VideoPixelStorage storage) {
  switch (storage) {
    case media::PIXEL_STORAGE_CPU:
      return base::MakeUnique<MojoSharedMemoryBufferTracker>();
  }
  NOTREACHED();
  return std::unique_ptr<media::VideoCaptureBufferTracker>();
}

}  // namespace video_capture
