// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_linux.h"

#include <stddef.h>

#include <list>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/capture/video/linux/v4l2_capture_delegate.h"

#if defined(OS_OPENBSD)
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#include <linux/version.h>
#endif

namespace media {

// Translates Video4Linux pixel formats to Chromium pixel formats.
// static
VideoPixelFormat VideoCaptureDeviceLinux::V4l2FourCcToChromiumPixelFormat(
    uint32_t v4l2_fourcc) {
  return V4L2CaptureDelegate::V4l2FourCcToChromiumPixelFormat(v4l2_fourcc);
}

// Gets a list of usable Four CC formats prioritized.
// static
std::list<uint32_t> VideoCaptureDeviceLinux::GetListOfUsableFourCCs(
    bool favour_mjpeg) {
  return V4L2CaptureDelegate::GetListOfUsableFourCcs(favour_mjpeg);
}

VideoCaptureDeviceLinux::VideoCaptureDeviceLinux(
    const VideoCaptureDeviceDescriptor& device_descriptor)
    : v4l2_thread_("V4L2CaptureThread"),
      device_descriptor_(device_descriptor) {}

VideoCaptureDeviceLinux::~VideoCaptureDeviceLinux() {
  // Check if the thread is running.
  // This means that the device has not been StopAndDeAllocate()d properly.
  DCHECK(!v4l2_thread_.IsRunning());
  v4l2_thread_.Stop();
}

void VideoCaptureDeviceLinux::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(!capture_impl_);
  if (v4l2_thread_.IsRunning())
    return;  // Wrong state.
  v4l2_thread_.Start();

  const int line_frequency =
      TranslatePowerLineFrequencyToV4L2(GetPowerLineFrequency(params));
  capture_impl_ = new V4L2CaptureDelegate(
      device_descriptor_, v4l2_thread_.task_runner(), line_frequency);
  if (!capture_impl_) {
    client->OnError(FROM_HERE, "Failed to create VideoCaptureDelegate");
    return;
  }
  v4l2_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2CaptureDelegate::AllocateAndStart, capture_impl_,
                 params.requested_format.frame_size.width(),
                 params.requested_format.frame_size.height(),
                 params.requested_format.frame_rate, base::Passed(&client)));
}

void VideoCaptureDeviceLinux::StopAndDeAllocate() {
  if (!v4l2_thread_.IsRunning())
    return;  // Wrong state.
  v4l2_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2CaptureDelegate::StopAndDeAllocate, capture_impl_));
  v4l2_thread_.Stop();

  capture_impl_ = nullptr;
}

void VideoCaptureDeviceLinux::TakePhoto(TakePhotoCallback callback) {
  DCHECK(capture_impl_);
  if (!v4l2_thread_.IsRunning())
    return;
  v4l2_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&V4L2CaptureDelegate::TakePhoto, capture_impl_,
                            base::Passed(&callback)));
}

void VideoCaptureDeviceLinux::GetPhotoCapabilities(
    GetPhotoCapabilitiesCallback callback) {
  if (!v4l2_thread_.IsRunning())
    return;  // Wrong state.
  v4l2_thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&V4L2CaptureDelegate::GetPhotoCapabilities,
                            capture_impl_, base::Passed(&callback)));
}

void VideoCaptureDeviceLinux::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  if (!v4l2_thread_.IsRunning())
    return;  // Wrong state.
  v4l2_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2CaptureDelegate::SetPhotoOptions, capture_impl_,
                 base::Passed(&settings), base::Passed(&callback)));
}

void VideoCaptureDeviceLinux::SetRotation(int rotation) {
  if (v4l2_thread_.IsRunning()) {
    v4l2_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&V4L2CaptureDelegate::SetRotation, capture_impl_, rotation));
  }
}

// static
int VideoCaptureDeviceLinux::TranslatePowerLineFrequencyToV4L2(
    PowerLineFrequency frequency) {
  switch (frequency) {
    case media::PowerLineFrequency::FREQUENCY_50HZ:
      return V4L2_CID_POWER_LINE_FREQUENCY_50HZ;
    case media::PowerLineFrequency::FREQUENCY_60HZ:
      return V4L2_CID_POWER_LINE_FREQUENCY_60HZ;
    default:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
      // If we have no idea of the frequency, at least try and set it to AUTO.
      return V4L2_CID_POWER_LINE_FREQUENCY_AUTO;
#else
      return V4L2_CID_POWER_LINE_FREQUENCY_60HZ;
#endif
  }
}

}  // namespace media
