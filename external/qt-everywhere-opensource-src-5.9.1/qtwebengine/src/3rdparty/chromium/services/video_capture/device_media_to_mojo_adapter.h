// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_MEDIA_TO_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_MEDIA_TO_MOJO_ADAPTER_H_

#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video_capture_types.h"
#include "services/video_capture/public/interfaces/device.mojom.h"

namespace video_capture {

// Implementation of mojom::Device backed by a given instance of
// media::VideoCaptureDevice.
class DeviceMediaToMojoAdapter : public mojom::Device {
 public:
  DeviceMediaToMojoAdapter(std::unique_ptr<media::VideoCaptureDevice> device,
                           const media::VideoCaptureJpegDecoderFactoryCB&
                               jpeg_decoder_factory_callback);
  ~DeviceMediaToMojoAdapter() override;

  // mojom::Device:
  void Start(const CaptureSettings& requested_settings,
             mojom::ReceiverPtr receiver) override;

  void Stop();

  void OnClientConnectionErrorOrClose();

 private:
  const std::unique_ptr<media::VideoCaptureDevice> device_;
  media::VideoCaptureJpegDecoderFactoryCB jpeg_decoder_factory_callback_;
  bool device_running_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_MEDIA_TO_MOJO_ADAPTER_H_
