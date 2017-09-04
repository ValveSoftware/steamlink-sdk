// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/interfaces/capture_settings_traits.h"

#include "media/capture/mojo/video_capture_types_typemap_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

// static
bool StructTraits<video_capture::mojom::I420CaptureFormatDataView,
                  video_capture::I420CaptureFormat>::
    Read(video_capture::mojom::I420CaptureFormatDataView data,
         video_capture::I420CaptureFormat* out) {
  if (!data.ReadFrameSize(&out->frame_size))
    return false;
  out->frame_rate = data.frame_rate();
  return true;
}

// static
bool StructTraits<video_capture::mojom::CaptureSettingsDataView,
                  video_capture::CaptureSettings>::
    Read(video_capture::mojom::CaptureSettingsDataView data,
         video_capture::CaptureSettings* out) {
  if (!data.ReadFormat(&out->format))
    return false;
  if (!data.ReadResolutionChangePolicy(&out->resolution_change_policy))
    return false;
  if (!data.ReadPowerLineFrequency(&out->power_line_frequency))
    return false;
  return true;
}

}  // namespace mojo
