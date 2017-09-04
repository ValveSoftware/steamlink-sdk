// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_MOJO_VIDEO_CAPTURE_TYPES_TYPEMAP_TRAITS_H_
#define MEDIA_CAPTURE_MOJO_VIDEO_CAPTURE_TYPES_TYPEMAP_TRAITS_H_

#include "media/capture/video_capture_types.h"
#include "media/capture/mojo/video_capture_types.mojom.h"

namespace mojo {

template <>
struct EnumTraits<media::mojom::VideoPixelStorage, media::VideoPixelStorage> {
  static media::mojom::VideoPixelStorage ToMojom(
      media::VideoPixelStorage video_pixel_storage);

  static bool FromMojom(media::mojom::VideoPixelStorage input,
                        media::VideoPixelStorage* out);
};

template <>
struct EnumTraits<media::mojom::ResolutionChangePolicy,
                  media::ResolutionChangePolicy> {
  static media::mojom::ResolutionChangePolicy ToMojom(
      media::ResolutionChangePolicy policy);

  static bool FromMojom(media::mojom::ResolutionChangePolicy input,
                        media::ResolutionChangePolicy* out);
};

template <>
struct EnumTraits<media::mojom::PowerLineFrequency, media::PowerLineFrequency> {
  static media::mojom::PowerLineFrequency ToMojom(
      media::PowerLineFrequency frequency);

  static bool FromMojom(media::mojom::PowerLineFrequency input,
                        media::PowerLineFrequency* out);
};

template <>
struct StructTraits<media::mojom::VideoCaptureFormatDataView,
                    media::VideoCaptureFormat> {
  static const gfx::Size& frame_size(const media::VideoCaptureFormat& format) {
    return format.frame_size;
  }

  static float frame_rate(const media::VideoCaptureFormat& format) {
    return format.frame_rate;
  }

  static media::VideoPixelFormat pixel_format(
      const media::VideoCaptureFormat& format) {
    return format.pixel_format;
  }

  static media::VideoPixelStorage pixel_storage(
      const media::VideoCaptureFormat& format) {
    return format.pixel_storage;
  }

  static bool Read(media::mojom::VideoCaptureFormatDataView data,
                   media::VideoCaptureFormat* out);
};

template <>
struct StructTraits<media::mojom::VideoCaptureParamsDataView,
                    media::VideoCaptureParams> {
  static media::VideoCaptureFormat requested_format(
      const media::VideoCaptureParams& params) {
    return params.requested_format;
  }

  static media::ResolutionChangePolicy resolution_change_policy(
      const media::VideoCaptureParams& params) {
    return params.resolution_change_policy;
  }

  static media::PowerLineFrequency power_line_frequency(
      const media::VideoCaptureParams& params) {
    return params.power_line_frequency;
  }

  static bool Read(media::mojom::VideoCaptureParamsDataView data,
                   media::VideoCaptureParams* out);
};
}

#endif  // MEDIA_CAPTURE_MOJO_VIDEO_CAPTURE_TYPES_TYPEMAP_TRAITS_H_
