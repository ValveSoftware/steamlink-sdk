// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/interfaces/device_descriptor_struct_traits.h"

namespace mojo {

// static
video_capture::mojom::VideoCaptureApi
EnumTraits<video_capture::mojom::VideoCaptureApi,
           media::VideoCaptureApi>::ToMojom(media::VideoCaptureApi input) {
  switch (input) {
    case media::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE:
      return video_capture::mojom::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;
    case media::VideoCaptureApi::WIN_MEDIA_FOUNDATION:
      return video_capture::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION;
    case media::VideoCaptureApi::WIN_DIRECT_SHOW:
      return video_capture::mojom::VideoCaptureApi::WIN_DIRECT_SHOW;
    case media::VideoCaptureApi::MACOSX_AVFOUNDATION:
      return video_capture::mojom::VideoCaptureApi::MACOSX_AVFOUNDATION;
    case media::VideoCaptureApi::MACOSX_DECKLINK:
      return video_capture::mojom::VideoCaptureApi::MACOSX_DECKLINK;
    case media::VideoCaptureApi::ANDROID_API1:
      return video_capture::mojom::VideoCaptureApi::ANDROID_API1;
    case media::VideoCaptureApi::ANDROID_API2_LEGACY:
      return video_capture::mojom::VideoCaptureApi::ANDROID_API2_LEGACY;
    case media::VideoCaptureApi::ANDROID_API2_FULL:
      return video_capture::mojom::VideoCaptureApi::ANDROID_API2_FULL;
    case media::VideoCaptureApi::ANDROID_API2_LIMITED:
      return video_capture::mojom::VideoCaptureApi::ANDROID_API2_LIMITED;
    case media::VideoCaptureApi::ANDROID_TANGO:
      return video_capture::mojom::VideoCaptureApi::ANDROID_TANGO;
    case media::VideoCaptureApi::UNKNOWN:
      return video_capture::mojom::VideoCaptureApi::UNKNOWN;
  }
  NOTREACHED();
  return video_capture::mojom::VideoCaptureApi::UNKNOWN;
}

// static
bool EnumTraits<video_capture::mojom::VideoCaptureApi, media::VideoCaptureApi>::
    FromMojom(video_capture::mojom::VideoCaptureApi input,
              media::VideoCaptureApi* output) {
  switch (input) {
    case video_capture::mojom::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE:
      *output = media::VideoCaptureApi::LINUX_V4L2_SINGLE_PLANE;
      return true;
    case video_capture::mojom::VideoCaptureApi::WIN_MEDIA_FOUNDATION:
      *output = media::VideoCaptureApi::WIN_MEDIA_FOUNDATION;
      return true;
    case video_capture::mojom::VideoCaptureApi::WIN_DIRECT_SHOW:
      *output = media::VideoCaptureApi::WIN_DIRECT_SHOW;
      return true;
    case video_capture::mojom::VideoCaptureApi::MACOSX_AVFOUNDATION:
      *output = media::VideoCaptureApi::MACOSX_AVFOUNDATION;
      return true;
    case video_capture::mojom::VideoCaptureApi::MACOSX_DECKLINK:
      *output = media::VideoCaptureApi::MACOSX_DECKLINK;
      return true;
    case video_capture::mojom::VideoCaptureApi::ANDROID_API1:
      *output = media::VideoCaptureApi::ANDROID_API1;
      return true;
    case video_capture::mojom::VideoCaptureApi::ANDROID_API2_LEGACY:
      *output = media::VideoCaptureApi::ANDROID_API2_LEGACY;
      return true;
    case video_capture::mojom::VideoCaptureApi::ANDROID_API2_FULL:
      *output = media::VideoCaptureApi::ANDROID_API2_FULL;
      return true;
    case video_capture::mojom::VideoCaptureApi::ANDROID_API2_LIMITED:
      *output = media::VideoCaptureApi::ANDROID_API2_LIMITED;
      return true;
    case video_capture::mojom::VideoCaptureApi::ANDROID_TANGO:
      *output = media::VideoCaptureApi::ANDROID_TANGO;
      return true;
    case video_capture::mojom::VideoCaptureApi::UNKNOWN:
      *output = media::VideoCaptureApi::UNKNOWN;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
video_capture::mojom::VideoCaptureTransportType EnumTraits<
    video_capture::mojom::VideoCaptureTransportType,
    media::VideoCaptureTransportType>::ToMojom(media::VideoCaptureTransportType
                                                   input) {
  switch (input) {
    case media::VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN:
      return video_capture::mojom::VideoCaptureTransportType::
          MACOSX_USB_OR_BUILT_IN;
    case media::VideoCaptureTransportType::OTHER_TRANSPORT:
      return video_capture::mojom::VideoCaptureTransportType::OTHER_TRANSPORT;
  }
  NOTREACHED();
  return video_capture::mojom::VideoCaptureTransportType::OTHER_TRANSPORT;
}

// static
bool EnumTraits<video_capture::mojom::VideoCaptureTransportType,
                media::VideoCaptureTransportType>::
    FromMojom(video_capture::mojom::VideoCaptureTransportType input,
              media::VideoCaptureTransportType* output) {
  switch (input) {
    case video_capture::mojom::VideoCaptureTransportType::
        MACOSX_USB_OR_BUILT_IN:
      *output = media::VideoCaptureTransportType::MACOSX_USB_OR_BUILT_IN;
      return true;
    case video_capture::mojom::VideoCaptureTransportType::OTHER_TRANSPORT:
      *output = media::VideoCaptureTransportType::OTHER_TRANSPORT;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<video_capture::mojom::DeviceDescriptorDataView,
                  media::VideoCaptureDeviceDescriptor>::
    Read(video_capture::mojom::DeviceDescriptorDataView data,
         media::VideoCaptureDeviceDescriptor* output) {
  if (!data.ReadDisplayName(&(output->display_name)))
    return false;

  if (!data.ReadDeviceId(&(output->device_id)))
    return false;

  if (!data.ReadModelId(&(output->model_id)))
    return false;

  if (!data.ReadCaptureApi(&(output->capture_api)))
    return false;

  if (!data.ReadTransportType(&(output->transport_type)))
    return false;

  return true;
}

}  // namespace mojo
