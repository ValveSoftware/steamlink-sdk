// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_

#include <memory>

#include "content/common/content_export.h"
#include "media/capture/video/video_capture_buffer_tracker_factory.h"

namespace content {

class CONTENT_EXPORT VideoCaptureBufferTrackerFactoryImpl
    : public media::VideoCaptureBufferTrackerFactory {
 public:
  std::unique_ptr<media::VideoCaptureBufferTracker> CreateTracker(
      media::VideoPixelStorage storage) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_BUFFER_TRACKER_FACTORY_IMPL_H_
