// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a fake VideoCaptureDeviceFactory class.

#ifndef MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include "media/capture/video/fake_video_capture_device.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

// Extension of VideoCaptureDeviceFactory to create and manipulate fake devices,
// not including file-based ones.
class CAPTURE_EXPORT FakeVideoCaptureDeviceFactory
    : public VideoCaptureDeviceFactory {
 public:
  FakeVideoCaptureDeviceFactory();
  ~FakeVideoCaptureDeviceFactory() override {}

  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats) override;

  void set_number_of_devices(int number_of_devices) {
    DCHECK(thread_checker_.CalledOnValidThread());
    number_of_devices_ = number_of_devices;
  }
  int number_of_devices() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return number_of_devices_;
  }

 private:
  void ParseCommandLine();

  int number_of_devices_;
  FakeVideoCaptureDevice::BufferOwnership fake_vcd_ownership_;
  float frame_rate_;
  bool command_line_parsed_ = false;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FAKE_VIDEO_CAPTURE_DEVICE_FACTORY_H_
