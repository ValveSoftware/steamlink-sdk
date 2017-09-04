// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_

#include <memory>

#include "media/capture/video/video_capture_buffer_tracker_factory.h"

namespace video_capture {

// Implementation of media::VideoCaptureBufferTrackerFactory specific to the
// Mojo Video Capture service. It produces buffers that are backed by
// mojo shared memory.
class BufferTrackerFactoryImpl
    : public media::VideoCaptureBufferTrackerFactory {
 public:
  std::unique_ptr<media::VideoCaptureBufferTracker> CreateTracker(
      media::VideoPixelStorage storage) override;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_
