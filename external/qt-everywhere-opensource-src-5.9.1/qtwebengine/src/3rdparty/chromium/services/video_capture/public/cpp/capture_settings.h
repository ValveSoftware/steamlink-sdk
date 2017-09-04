// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_CAPTURE_SETTINGS_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_CAPTURE_SETTINGS_H_

#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/size.h"

namespace video_capture {

// Cpp equivalent of Mojo struct video_capture::mojom::CaptureFormat.
struct I420CaptureFormat {
  gfx::Size frame_size;
  float frame_rate;

  bool operator==(const I420CaptureFormat& other) const {
    return frame_size == other.frame_size && frame_rate == other.frame_rate;
  }

  void ConvertToMediaVideoCaptureFormat(
      media::VideoCaptureFormat* target) const {
    target->frame_size = frame_size;
    target->frame_rate = frame_rate;
    target->pixel_format = media::PIXEL_FORMAT_I420;
    target->pixel_storage = media::PIXEL_STORAGE_CPU;
  }
};

// Cpp equivalent of Mojo struct video_capture::mojom::CaptureSettings.
struct CaptureSettings {
  I420CaptureFormat format;
  media::ResolutionChangePolicy resolution_change_policy;
  media::PowerLineFrequency power_line_frequency;

  void ConvertToMediaVideoCaptureParams(
      media::VideoCaptureParams* target) const {
    format.ConvertToMediaVideoCaptureFormat(&(target->requested_format));
    target->resolution_change_policy = resolution_change_policy;
    target->power_line_frequency = power_line_frequency;
  }
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_CAPTURE_SETTINGS_H_
