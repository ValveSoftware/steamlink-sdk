// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_mojo_mock_to_media_adapter.h"

#include "services/video_capture/device_client_media_to_mojo_mock_adapter.h"

namespace video_capture {

DeviceMojoMockToMediaAdapter::DeviceMojoMockToMediaAdapter(
    mojom::MockMediaDevicePtr* device)
    : device_(device) {}

DeviceMojoMockToMediaAdapter::~DeviceMojoMockToMediaAdapter() = default;

void DeviceMojoMockToMediaAdapter::AllocateAndStart(
    const media::VideoCaptureParams& params,
    std::unique_ptr<Client> client) {
  mojom::MockDeviceClientPtr client_proxy;
  auto client_request = mojo::GetProxy(&client_proxy);

  mojo::MakeStrongBinding(
      base::MakeUnique<DeviceClientMediaToMojoMockAdapter>(std::move(client)),
      std::move(client_request));

  (*device_)->AllocateAndStart(std::move(client_proxy));
}

void DeviceMojoMockToMediaAdapter::RequestRefreshFrame() {}

void DeviceMojoMockToMediaAdapter::StopAndDeAllocate() {
  (*device_)->StopAndDeAllocate();
}

void DeviceMojoMockToMediaAdapter::GetPhotoCapabilities(
    GetPhotoCapabilitiesCallback callback) {}

void DeviceMojoMockToMediaAdapter::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {}

void DeviceMojoMockToMediaAdapter::TakePhoto(TakePhotoCallback callback) {}

}  // namespace video_capture
