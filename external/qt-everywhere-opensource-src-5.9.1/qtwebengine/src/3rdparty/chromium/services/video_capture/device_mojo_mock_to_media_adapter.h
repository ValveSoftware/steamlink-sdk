// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_MOJO_MOCK_TO_MEDIA_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_MOJO_MOCK_TO_MEDIA_ADAPTER_H_

#include "media/capture/video/video_capture_device.h"
#include "services/video_capture/public/interfaces/mock_device.mojom.h"

namespace video_capture {

// Adapter that allows a mojom::MockVideoCaptureDevice to be used in place of
// a media::VideoCaptureDevice. This is useful for plugging a mock devices into
// the service and testing that the service makes expected calls to the
// media::VideoCaptureDevice interface.
class DeviceMojoMockToMediaAdapter : public media::VideoCaptureDevice {
 public:
  DeviceMojoMockToMediaAdapter(mojom::MockMediaDevicePtr* device);
  ~DeviceMojoMockToMediaAdapter() override;

  // media::VideoCaptureDevice:
  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void RequestRefreshFrame() override;
  void StopAndDeAllocate() override;
  void GetPhotoCapabilities(GetPhotoCapabilitiesCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;

 private:
  mojom::MockMediaDevicePtr* const device_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_MOJO_MOCK_TO_MEDIA_ADAPTER_H_
