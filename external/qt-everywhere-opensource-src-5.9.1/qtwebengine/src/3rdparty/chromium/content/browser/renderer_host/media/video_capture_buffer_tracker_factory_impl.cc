// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_buffer_tracker_factory_impl.h"

#include "base/memory/ptr_util.h"

#include "content/browser/renderer_host/media/shared_memory_buffer_tracker.h"

namespace content {

std::unique_ptr<media::VideoCaptureBufferTracker>
VideoCaptureBufferTrackerFactoryImpl::CreateTracker(
    media::VideoPixelStorage storage) {
  DCHECK_EQ(media::PIXEL_STORAGE_CPU, storage);
  return base::MakeUnique<SharedMemoryBufferTracker>();
}

}  // namespace content
