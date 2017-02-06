// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_linux.h"

#include <stddef.h>

#include <list>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "media/capture/video/linux/v4l2_capture_delegate.h"

#if defined(OS_OPENBSD)
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif

namespace media {

// USB VID and PID are both 4 bytes long.
static const size_t kVidPidSize = 4;

// /sys/class/video4linux/video{N}/device is a symlink to the corresponding
// USB device info directory.
static const char kVidPathTemplate[] =
    "/sys/class/video4linux/%s/device/../idVendor";
static const char kPidPathTemplate[] =
    "/sys/class/video4linux/%s/device/../idProduct";

static bool ReadIdFile(const std::string& path, std::string* id) {
  char id_buf[kVidPidSize];
  FILE* file = fopen(path.c_str(), "rb");
  if (!file)
    return false;
  const bool success = fread(id_buf, kVidPidSize, 1, file) == 1;
  fclose(file);
  if (!success)
    return false;
  id->append(id_buf, kVidPidSize);
  return true;
}

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

const std::string VideoCaptureDevice::Name::GetModel() const {
  // |unique_id| is of the form "/dev/video2".  |file_name| is "video2".
  const std::string dev_dir = "/dev/";
  DCHECK_EQ(0, unique_id_.compare(0, dev_dir.length(), dev_dir));
  const std::string file_name =
      unique_id_.substr(dev_dir.length(), unique_id_.length());

  const std::string vidPath =
      base::StringPrintf(kVidPathTemplate, file_name.c_str());
  const std::string pidPath =
      base::StringPrintf(kPidPathTemplate, file_name.c_str());

  std::string usb_id;
  if (!ReadIdFile(vidPath, &usb_id))
    return "";
  usb_id.append(":");
  if (!ReadIdFile(pidPath, &usb_id))
    return "";

  return usb_id;
}

VideoCaptureDeviceLinux::VideoCaptureDeviceLinux(const Name& device_name)
    : v4l2_thread_("V4L2CaptureThread"), device_name_(device_name) {}

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
      device_name_, v4l2_thread_.task_runner(), line_frequency);
  if (!capture_impl_) {
    client->OnError(FROM_HERE, "Failed to create VideoCaptureDelegate");
    return;
  }
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2CaptureDelegate::AllocateAndStart, capture_impl_,
                 params.requested_format.frame_size.width(),
                 params.requested_format.frame_size.height(),
                 params.requested_format.frame_rate, base::Passed(&client)));
}

void VideoCaptureDeviceLinux::StopAndDeAllocate() {
  if (!v4l2_thread_.IsRunning())
    return;  // Wrong state.
  v4l2_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&V4L2CaptureDelegate::StopAndDeAllocate, capture_impl_));
  v4l2_thread_.Stop();

  capture_impl_ = NULL;
}

void VideoCaptureDeviceLinux::SetRotation(int rotation) {
  if (v4l2_thread_.IsRunning()) {
    v4l2_thread_.message_loop()->PostTask(
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
      // If we have no idea of the frequency, at least try and set it to AUTO.
      return V4L2_CID_POWER_LINE_FREQUENCY_AUTO;
  }
}

}  // namespace media
