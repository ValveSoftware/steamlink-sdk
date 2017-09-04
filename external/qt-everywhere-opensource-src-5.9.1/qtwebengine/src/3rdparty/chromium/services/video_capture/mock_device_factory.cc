// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/mock_device_factory.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "services/video_capture/device_mojo_mock_to_media_adapter.h"

namespace {
// Report a single hard-coded supported format to clients.
media::VideoCaptureFormat kSupportedFormat(gfx::Size(),
                                           25.0f,
                                           media::PIXEL_FORMAT_I420,
                                           media::PIXEL_STORAGE_CPU);
}

namespace video_capture {

MockDeviceFactory::MockDeviceFactory() = default;

MockDeviceFactory::~MockDeviceFactory() = default;

void MockDeviceFactory::AddMockDevice(
    mojom::MockMediaDevicePtr device,
    const media::VideoCaptureDeviceDescriptor& descriptor) {
  devices_[descriptor] = std::move(device);
}

std::unique_ptr<media::VideoCaptureDevice> MockDeviceFactory::CreateDevice(
    const media::VideoCaptureDeviceDescriptor& device_descriptor) {
  mojom::MockMediaDevicePtr* device = &(devices_[device_descriptor]);
  if (device == nullptr) {
    return nullptr;
  }
  return base::MakeUnique<DeviceMojoMockToMediaAdapter>(device);
}

void MockDeviceFactory::GetDeviceDescriptors(
    media::VideoCaptureDeviceDescriptors* device_descriptors) {
  for (const auto& entry : devices_)
    device_descriptors->push_back(entry.first);
}

void MockDeviceFactory::GetSupportedFormats(
    const media::VideoCaptureDeviceDescriptor& device_descriptor,
    media::VideoCaptureFormats* supported_formats) {
  supported_formats->push_back(kSupportedFormat);
}

}  // namespace video_capture
