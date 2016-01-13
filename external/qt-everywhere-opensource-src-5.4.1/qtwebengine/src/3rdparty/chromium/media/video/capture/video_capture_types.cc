// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_types.h"

#include "media/base/limits.h"

namespace media {

VideoCaptureFormat::VideoCaptureFormat()
    : frame_rate(0.0f), pixel_format(PIXEL_FORMAT_UNKNOWN) {}

VideoCaptureFormat::VideoCaptureFormat(const gfx::Size& frame_size,
                                       float frame_rate,
                                       VideoPixelFormat pixel_format)
    : frame_size(frame_size),
      frame_rate(frame_rate),
      pixel_format(pixel_format) {}

bool VideoCaptureFormat::IsValid() const {
  return (frame_size.width() < media::limits::kMaxDimension) &&
         (frame_size.height() < media::limits::kMaxDimension) &&
         (frame_size.GetArea() >= 0) &&
         (frame_size.GetArea() < media::limits::kMaxCanvas) &&
         (frame_rate >= 0.0f) &&
         (frame_rate < media::limits::kMaxFramesPerSecond) &&
         (pixel_format >= PIXEL_FORMAT_UNKNOWN) &&
         (pixel_format < PIXEL_FORMAT_MAX);
}

VideoCaptureParams::VideoCaptureParams() : allow_resolution_change(false) {}

}  // namespace media
