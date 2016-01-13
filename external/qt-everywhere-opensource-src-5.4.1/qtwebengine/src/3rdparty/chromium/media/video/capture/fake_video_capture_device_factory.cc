// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/fake_video_capture_device_factory.h"

#include "base/strings/stringprintf.h"
#include "media/video/capture/fake_video_capture_device.h"

namespace media {

FakeVideoCaptureDeviceFactory::FakeVideoCaptureDeviceFactory()
    : number_of_devices_(1) {
}

scoped_ptr<VideoCaptureDevice> FakeVideoCaptureDeviceFactory::Create(
    const VideoCaptureDevice::Name& device_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (int n = 0; n < number_of_devices_; ++n) {
    std::string possible_id = base::StringPrintf("/dev/video%d", n);
    if (device_name.id().compare(possible_id) == 0)
      return scoped_ptr<VideoCaptureDevice>(new FakeVideoCaptureDevice());
  }
  return scoped_ptr<VideoCaptureDevice>();
}

void FakeVideoCaptureDeviceFactory::GetDeviceNames(
    VideoCaptureDevice::Names* const device_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(device_names->empty());
  for (int n = 0; n < number_of_devices_; ++n) {
    VideoCaptureDevice::Name name(base::StringPrintf("fake_device_%d", n),
                                  base::StringPrintf("/dev/video%d", n));
    device_names->push_back(name);
  }
}

void FakeVideoCaptureDeviceFactory::GetDeviceSupportedFormats(
    const VideoCaptureDevice::Name& device,
    VideoCaptureFormats* supported_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const int frame_rate = 1000 / FakeVideoCaptureDevice::kFakeCaptureTimeoutMs;
  const gfx::Size supported_sizes[] = {gfx::Size(320, 240),
                                       gfx::Size(640, 480),
                                       gfx::Size(1280, 720)};
  supported_formats->clear();
  for (size_t i = 0; i < arraysize(supported_sizes); ++i) {
    supported_formats->push_back(VideoCaptureFormat(supported_sizes[i],
                                                    frame_rate,
                                                    media::PIXEL_FORMAT_I420));
  }
}

}  // namespace media
