// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_factory_media_to_mojo_adapter.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/video_capture/device_media_to_mojo_adapter.h"
#include "services/video_capture/device_mojo_mock_to_media_adapter.h"
#include "services/video_capture/public/cpp/capture_settings.h"

namespace video_capture {

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::ActiveDeviceEntry() =
    default;

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::~ActiveDeviceEntry() =
    default;

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::ActiveDeviceEntry(
    DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry&& other) = default;

DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry&
DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry::operator=(
    DeviceFactoryMediaToMojoAdapter::ActiveDeviceEntry&& other) = default;

DeviceFactoryMediaToMojoAdapter::DeviceFactoryMediaToMojoAdapter(
    std::unique_ptr<media::VideoCaptureDeviceFactory> device_factory,
    const media::VideoCaptureJpegDecoderFactoryCB&
        jpeg_decoder_factory_callback)
    : device_factory_(std::move(device_factory)),
      jpeg_decoder_factory_callback_(jpeg_decoder_factory_callback) {}

DeviceFactoryMediaToMojoAdapter::~DeviceFactoryMediaToMojoAdapter() = default;

void DeviceFactoryMediaToMojoAdapter::EnumerateDeviceDescriptors(
    const EnumerateDeviceDescriptorsCallback& callback) {
  media::VideoCaptureDeviceDescriptors descriptors;
  device_factory_->GetDeviceDescriptors(&descriptors);
  callback.Run(std::move(descriptors));
}

void DeviceFactoryMediaToMojoAdapter::GetSupportedFormats(
    const std::string& device_id,
    const GetSupportedFormatsCallback& callback) {
  media::VideoCaptureDeviceDescriptor descriptor;
  media::VideoCaptureFormats media_formats;
  if (LookupDescriptorFromId(device_id, &descriptor))
    device_factory_->GetSupportedFormats(descriptor, &media_formats);
  std::vector<I420CaptureFormat> result;
  for (const auto& media_format : media_formats) {
    // The Video Capture Service requires devices to deliver frames either in
    // I420 or MJPEG formats.
    // TODO(chfremer): Add support for Y16 format. See crbug.com/624436.
    if (media_format.pixel_format != media::PIXEL_FORMAT_I420 &&
        media_format.pixel_format != media::PIXEL_FORMAT_MJPEG) {
      continue;
    }
    I420CaptureFormat format;
    format.frame_size = media_format.frame_size;
    format.frame_rate = media_format.frame_rate;
    if (base::ContainsValue(result, format))
      continue;  // Result already contains this format
    result.push_back(format);
  }
  callback.Run(std::move(result));
}

void DeviceFactoryMediaToMojoAdapter::CreateDevice(
    const std::string& device_id,
    mojom::DeviceRequest device_request,
    const CreateDeviceCallback& callback) {
  auto active_device_iter = active_devices_by_id_.find(device_id);
  if (active_device_iter != active_devices_by_id_.end()) {
    // The requested device is already in use.
    // Revoke the access and close the device, then bind to the new request.
    ActiveDeviceEntry& device_entry = active_device_iter->second;
    device_entry.binding->Unbind();
    device_entry.device->Stop();
    device_entry.binding->Bind(std::move(device_request));
    device_entry.binding->set_connection_error_handler(base::Bind(
        &DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose,
        base::Unretained(this), device_id));
    callback.Run(mojom::DeviceAccessResultCode::SUCCESS);
    return;
  }

  // Create device
  media::VideoCaptureDeviceDescriptor descriptor;
  if (LookupDescriptorFromId(device_id, &descriptor) == false) {
    callback.Run(mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND);
    return;
  }
  std::unique_ptr<media::VideoCaptureDevice> media_device =
      device_factory_->CreateDevice(descriptor);
  if (media_device == nullptr) {
    callback.Run(mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND);
    return;
  }

  // Add entry to active_devices to keep track of it
  ActiveDeviceEntry device_entry;
  device_entry.device = base::MakeUnique<DeviceMediaToMojoAdapter>(
      std::move(media_device), jpeg_decoder_factory_callback_);
  device_entry.binding = base::MakeUnique<mojo::Binding<mojom::Device>>(
      device_entry.device.get(), std::move(device_request));
  device_entry.binding->set_connection_error_handler(base::Bind(
      &DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose,
      base::Unretained(this), device_id));
  active_devices_by_id_[device_id] = std::move(device_entry);

  callback.Run(mojom::DeviceAccessResultCode::SUCCESS);
}

void DeviceFactoryMediaToMojoAdapter::OnClientConnectionErrorOrClose(
    const std::string& device_id) {
  active_devices_by_id_[device_id].device->Stop();
  active_devices_by_id_.erase(device_id);
}

bool DeviceFactoryMediaToMojoAdapter::LookupDescriptorFromId(
    const std::string& device_id,
    media::VideoCaptureDeviceDescriptor* descriptor) {
  media::VideoCaptureDeviceDescriptors descriptors;
  device_factory_->GetDeviceDescriptors(&descriptors);
  auto descriptor_iter = std::find_if(
      descriptors.begin(), descriptors.end(),
      [&device_id](const media::VideoCaptureDeviceDescriptor& descriptor) {
        return descriptor.device_id == device_id;
      });
  if (descriptor_iter == descriptors.end())
    return false;
  *descriptor = *descriptor_iter;
  return true;
}

}  // namespace video_capture
