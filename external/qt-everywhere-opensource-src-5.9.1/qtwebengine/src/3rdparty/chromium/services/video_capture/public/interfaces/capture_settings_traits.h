// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_CAPTURE_SETTINGS_TRAITS_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_CAPTURE_SETTINGS_TRAITS_H_

#include "media/capture/video_capture_types.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "services/video_capture/public/interfaces/device.mojom.h"

namespace mojo {

template <>
struct StructTraits<video_capture::mojom::I420CaptureFormatDataView,
                    video_capture::I420CaptureFormat> {
  static const gfx::Size& frame_size(
      const video_capture::I420CaptureFormat& input) {
    return input.frame_size;
  }

  static float frame_rate(const video_capture::I420CaptureFormat& input) {
    return input.frame_rate;
  }

  static bool Read(video_capture::mojom::I420CaptureFormatDataView data,
                   video_capture::I420CaptureFormat* out);
};

template <>
struct StructTraits<video_capture::mojom::CaptureSettingsDataView,
                    video_capture::CaptureSettings> {
  static const video_capture::I420CaptureFormat& format(
      const video_capture::CaptureSettings& input) {
    return input.format;
  }

  static media::ResolutionChangePolicy resolution_change_policy(
      const video_capture::CaptureSettings& input) {
    return input.resolution_change_policy;
  }

  static media::PowerLineFrequency power_line_frequency(
      const video_capture::CaptureSettings& input) {
    return input.power_line_frequency;
  }

  static bool Read(video_capture::mojom::CaptureSettingsDataView data,
                   video_capture::CaptureSettings* out);
};
}

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_CAPTURE_SETTINGS_TRAITS_H_
