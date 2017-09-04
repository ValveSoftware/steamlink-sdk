// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_DEVICE_DESCRIPTOR_STRUCT_TRAITS_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_DEVICE_DESCRIPTOR_STRUCT_TRAITS_H_

#include "media/capture/video/video_capture_device_descriptor.h"
#include "services/video_capture/public/interfaces/device_descriptor.mojom.h"

namespace mojo {

template <>
struct EnumTraits<video_capture::mojom::VideoCaptureApi,
                  media::VideoCaptureApi> {
  static video_capture::mojom::VideoCaptureApi ToMojom(
      media::VideoCaptureApi input);
  static bool FromMojom(video_capture::mojom::VideoCaptureApi input,
                        media::VideoCaptureApi* output);
};

template <>
struct EnumTraits<video_capture::mojom::VideoCaptureTransportType,
                  media::VideoCaptureTransportType> {
  static video_capture::mojom::VideoCaptureTransportType ToMojom(
      media::VideoCaptureTransportType input);
  static bool FromMojom(video_capture::mojom::VideoCaptureTransportType input,
                        media::VideoCaptureTransportType* output);
};

template <>
struct StructTraits<video_capture::mojom::DeviceDescriptorDataView,
                    media::VideoCaptureDeviceDescriptor> {
  static const std::string& display_name(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.display_name;
  }

  static const std::string& device_id(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.device_id;
  }

  static const std::string& model_id(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.model_id;
  }

  static media::VideoCaptureApi capture_api(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.capture_api;
  }

  static media::VideoCaptureTransportType transport_type(
      const media::VideoCaptureDeviceDescriptor& input) {
    return input.transport_type;
  }

  static bool Read(video_capture::mojom::DeviceDescriptorDataView data,
                   media::VideoCaptureDeviceDescriptor* output);
};

}  // namespace mojo

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_DEVICE_DESCRIPTOR_STRUCT_TRAITS_H_
