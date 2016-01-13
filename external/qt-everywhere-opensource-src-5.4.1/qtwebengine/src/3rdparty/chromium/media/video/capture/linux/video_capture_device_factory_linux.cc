// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/linux/video_capture_device_factory_linux.h"

#include <errno.h>
#include <fcntl.h>
#if defined(OS_OPENBSD)
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif
#include <sys/ioctl.h>

#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#if defined(OS_CHROMEOS)
#include "media/video/capture/linux/video_capture_device_chromeos.h"
#endif
#include "media/video/capture/linux/video_capture_device_linux.h"

namespace media {

static bool HasUsableFormats(int fd) {
  v4l2_fmtdesc fmtdesc;
  std::list<int> usable_fourccs;

  media::VideoCaptureDeviceLinux::GetListOfUsableFourCCs(false,
                                                         &usable_fourccs);

  memset(&fmtdesc, 0, sizeof(v4l2_fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  while (HANDLE_EINTR(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0) {
    if (std::find(usable_fourccs.begin(), usable_fourccs.end(),
                  fmtdesc.pixelformat) != usable_fourccs.end())
      return true;

    fmtdesc.index++;
  }
  return false;
}

VideoCaptureDeviceFactoryLinux::VideoCaptureDeviceFactoryLinux(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner) {
}

VideoCaptureDeviceFactoryLinux::~VideoCaptureDeviceFactoryLinux() {
}

scoped_ptr<VideoCaptureDevice> VideoCaptureDeviceFactoryLinux::Create(
    const VideoCaptureDevice::Name& device_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(OS_CHROMEOS)
  VideoCaptureDeviceChromeOS* self =
      new VideoCaptureDeviceChromeOS(ui_task_runner_, device_name);
#else
  VideoCaptureDeviceLinux* self = new VideoCaptureDeviceLinux(device_name);
#endif
  if (!self)
    return scoped_ptr<VideoCaptureDevice>();
  // Test opening the device driver. This is to make sure it is available.
  // We will reopen it again in our worker thread when someone
  // allocates the camera.
  base::ScopedFD fd(HANDLE_EINTR(open(device_name.id().c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    DVLOG(1) << "Cannot open device";
    delete self;
    return scoped_ptr<VideoCaptureDevice>();
  }

  return scoped_ptr<VideoCaptureDevice>(self);
}

void VideoCaptureDeviceFactoryLinux::GetDeviceNames(
    VideoCaptureDevice::Names* const device_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(device_names->empty());
  base::FilePath path("/dev/");
  base::FileEnumerator enumerator(
      path, false, base::FileEnumerator::FILES, "video*");

  while (!enumerator.Next().empty()) {
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();

    std::string unique_id = path.value() + info.GetName().value();
    base::ScopedFD fd(HANDLE_EINTR(open(unique_id.c_str(), O_RDONLY)));
    if (!fd.is_valid()) {
      // Failed to open this device.
      continue;
    }
    // Test if this is a V4L2 capture device.
    v4l2_capability cap;
    if ((HANDLE_EINTR(ioctl(fd.get(), VIDIOC_QUERYCAP, &cap)) == 0) &&
        (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) &&
        !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
      // This is a V4L2 video capture device
      if (HasUsableFormats(fd.get())) {
        VideoCaptureDevice::Name device_name(base::StringPrintf("%s", cap.card),
                                             unique_id);
        device_names->push_back(device_name);
      } else {
        DVLOG(1) << "No usable formats reported by " << info.GetName().value();
      }
    }
  }
}

void VideoCaptureDeviceFactoryLinux::GetDeviceSupportedFormats(
    const VideoCaptureDevice::Name& device,
    VideoCaptureFormats* supported_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device.id().empty())
    return;
  base::ScopedFD fd(HANDLE_EINTR(open(device.id().c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    // Failed to open this device.
    return;
  }
  supported_formats->clear();

  // Retrieve the caps one by one, first get pixel format, then sizes, then
  // frame rates. See http://linuxtv.org/downloads/v4l-dvb-apis for reference.
  v4l2_fmtdesc pixel_format = {};
  pixel_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  while (HANDLE_EINTR(ioctl(fd.get(), VIDIOC_ENUM_FMT, &pixel_format)) == 0) {
    VideoCaptureFormat supported_format;
    supported_format.pixel_format =
        VideoCaptureDeviceLinux::V4l2ColorToVideoCaptureColorFormat(
            (int32)pixel_format.pixelformat);
    if (supported_format.pixel_format == PIXEL_FORMAT_UNKNOWN) {
      ++pixel_format.index;
      continue;
    }

    v4l2_frmsizeenum frame_size = {};
    frame_size.pixel_format = pixel_format.pixelformat;
    while (HANDLE_EINTR(ioctl(fd.get(), VIDIOC_ENUM_FRAMESIZES, &frame_size)) ==
           0) {
      if (frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        supported_format.frame_size.SetSize(
            frame_size.discrete.width, frame_size.discrete.height);
      } else if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
        // TODO(mcasas): see http://crbug.com/249953, support these devices.
        NOTIMPLEMENTED();
      } else if (frame_size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
        // TODO(mcasas): see http://crbug.com/249953, support these devices.
        NOTIMPLEMENTED();
      }
      v4l2_frmivalenum frame_interval = {};
      frame_interval.pixel_format = pixel_format.pixelformat;
      frame_interval.width = frame_size.discrete.width;
      frame_interval.height = frame_size.discrete.height;
      while (HANDLE_EINTR(ioctl(
                 fd.get(), VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval)) == 0) {
        if (frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
          if (frame_interval.discrete.numerator != 0) {
            supported_format.frame_rate =
                static_cast<float>(frame_interval.discrete.denominator) /
                static_cast<float>(frame_interval.discrete.numerator);
          } else {
            supported_format.frame_rate = 0;
          }
        } else if (frame_interval.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
          // TODO(mcasas): see http://crbug.com/249953, support these devices.
          NOTIMPLEMENTED();
          break;
        } else if (frame_interval.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
          // TODO(mcasas): see http://crbug.com/249953, support these devices.
          NOTIMPLEMENTED();
          break;
        }
        supported_formats->push_back(supported_format);
        ++frame_interval.index;
      }
      ++frame_size.index;
    }
    ++pixel_format.index;
  }
  return;
}

}  // namespace media
