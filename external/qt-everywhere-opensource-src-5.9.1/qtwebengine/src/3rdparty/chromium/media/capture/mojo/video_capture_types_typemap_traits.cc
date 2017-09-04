// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/mojo/video_capture_types_typemap_traits.h"

#include "media/base/ipc/media_param_traits_macros.h"
#include "ui/gfx/geometry/mojo/geometry.mojom.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

// static
media::mojom::VideoPixelStorage
EnumTraits<media::mojom::VideoPixelStorage, media::VideoPixelStorage>::ToMojom(
    media::VideoPixelStorage video_pixel_storage) {
  DCHECK_EQ(media::PIXEL_STORAGE_CPU, video_pixel_storage);
  return media::mojom::VideoPixelStorage::CPU;
}

// static
bool EnumTraits<media::mojom::VideoPixelStorage, media::VideoPixelStorage>::
    FromMojom(media::mojom::VideoPixelStorage input,
              media::VideoPixelStorage* out) {
  DCHECK_EQ(media::mojom::VideoPixelStorage::CPU, input);
  *out = media::PIXEL_STORAGE_CPU;
  return true;
}

// static
media::mojom::ResolutionChangePolicy
EnumTraits<media::mojom::ResolutionChangePolicy,
           media::ResolutionChangePolicy>::ToMojom(media::ResolutionChangePolicy
                                                       input) {
  switch (input) {
    case media::RESOLUTION_POLICY_FIXED_RESOLUTION:
      return media::mojom::ResolutionChangePolicy::FIXED_RESOLUTION;
    case media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO:
      return media::mojom::ResolutionChangePolicy::FIXED_ASPECT_RATIO;
    case media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT:
      return media::mojom::ResolutionChangePolicy::ANY_WITHIN_LIMIT;
  }
  NOTREACHED();
  return media::mojom::ResolutionChangePolicy::FIXED_RESOLUTION;
}

// static
bool EnumTraits<media::mojom::ResolutionChangePolicy,
                media::ResolutionChangePolicy>::
    FromMojom(media::mojom::ResolutionChangePolicy input,
              media::ResolutionChangePolicy* output) {
  switch (input) {
    case media::mojom::ResolutionChangePolicy::FIXED_RESOLUTION:
      *output = media::RESOLUTION_POLICY_FIXED_RESOLUTION;
      return true;
    case media::mojom::ResolutionChangePolicy::FIXED_ASPECT_RATIO:
      *output = media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO;
      return true;
    case media::mojom::ResolutionChangePolicy::ANY_WITHIN_LIMIT:
      *output = media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
media::mojom::PowerLineFrequency EnumTraits<
    media::mojom::PowerLineFrequency,
    media::PowerLineFrequency>::ToMojom(media::PowerLineFrequency input) {
  switch (input) {
    case media::PowerLineFrequency::FREQUENCY_DEFAULT:
      return media::mojom::PowerLineFrequency::DEFAULT;
    case media::PowerLineFrequency::FREQUENCY_50HZ:
      return media::mojom::PowerLineFrequency::HZ_50;
    case media::PowerLineFrequency::FREQUENCY_60HZ:
      return media::mojom::PowerLineFrequency::HZ_60;
  }
  NOTREACHED();
  return media::mojom::PowerLineFrequency::DEFAULT;
}

// static
bool EnumTraits<media::mojom::PowerLineFrequency, media::PowerLineFrequency>::
    FromMojom(media::mojom::PowerLineFrequency input,
              media::PowerLineFrequency* output) {
  switch (input) {
    case media::mojom::PowerLineFrequency::DEFAULT:
      *output = media::PowerLineFrequency::FREQUENCY_DEFAULT;
      return true;
    case media::mojom::PowerLineFrequency::HZ_50:
      *output = media::PowerLineFrequency::FREQUENCY_50HZ;
      return true;
    case media::mojom::PowerLineFrequency::HZ_60:
      *output = media::PowerLineFrequency::FREQUENCY_60HZ;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<media::mojom::VideoCaptureFormatDataView,
                  media::VideoCaptureFormat>::
    Read(media::mojom::VideoCaptureFormatDataView data,
         media::VideoCaptureFormat* out) {
  if (!data.ReadFrameSize(&out->frame_size))
    return false;
  out->frame_rate = data.frame_rate();
  if (!data.ReadPixelFormat(&out->pixel_format))
    return false;
  if (!data.ReadPixelStorage(&out->pixel_storage))
    return false;
  return true;
}

// static
bool StructTraits<media::mojom::VideoCaptureParamsDataView,
                  media::VideoCaptureParams>::
    Read(media::mojom::VideoCaptureParamsDataView data,
         media::VideoCaptureParams* out) {
  if (!data.ReadRequestedFormat(&out->requested_format))
    return false;
  if (!data.ReadResolutionChangePolicy(&out->resolution_change_policy))
    return false;
  if (!data.ReadPowerLineFrequency(&out->power_line_frequency))
    return false;
  return true;
}

}  // namespace mojo
