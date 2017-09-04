// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_MEDIA_TO_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_MEDIA_TO_MOJO_ADAPTER_H_

#include <map>

#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/interfaces/device_factory.mojom.h"

namespace video_capture {

class DeviceMediaToMojoAdapter;

// Wraps a media::VideoCaptureDeviceFactory and exposes its functionality
// through the mojom::DeviceFactory interface.
// Keeps track of device instances that have been created to ensure that
// it does not create more than one instance of the same
// media::VideoCaptureDevice at the same time.
class DeviceFactoryMediaToMojoAdapter : public mojom::DeviceFactory {
 public:
  DeviceFactoryMediaToMojoAdapter(
      std::unique_ptr<media::VideoCaptureDeviceFactory> device_factory,
      const media::VideoCaptureJpegDecoderFactoryCB&
          jpeg_decoder_factory_callback);
  ~DeviceFactoryMediaToMojoAdapter() override;

  // mojom::DeviceFactory:
  void EnumerateDeviceDescriptors(
      const EnumerateDeviceDescriptorsCallback& callback) override;
  void GetSupportedFormats(
      const std::string& device_id,
      const GetSupportedFormatsCallback& callback) override;
  void CreateDevice(const std::string& device_id,
                    mojom::DeviceRequest device_request,
                    const CreateDeviceCallback& callback) override;

 private:
  struct ActiveDeviceEntry {
    ActiveDeviceEntry();
    ~ActiveDeviceEntry();
    ActiveDeviceEntry(ActiveDeviceEntry&& other);
    ActiveDeviceEntry& operator=(ActiveDeviceEntry&& other);

    std::unique_ptr<DeviceMediaToMojoAdapter> device;
    // TODO(chfremer) Use mojo::Binding<> directly instead of unique_ptr<> when
    // mojo::Binding<> supports move operators.
    // https://crbug.com/644314
    std::unique_ptr<mojo::Binding<mojom::Device>> binding;
  };

  void OnClientConnectionErrorOrClose(const std::string& device_id);

  // Returns false if no descriptor found.
  bool LookupDescriptorFromId(const std::string& device_id,
                              media::VideoCaptureDeviceDescriptor* descriptor);

  const std::unique_ptr<media::VideoCaptureDeviceFactory> device_factory_;
  const media::VideoCaptureJpegDecoderFactoryCB jpeg_decoder_factory_callback_;
  std::map<std::string, ActiveDeviceEntry> active_devices_by_id_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_MEDIA_TO_MOJO_ADAPTER_H_
