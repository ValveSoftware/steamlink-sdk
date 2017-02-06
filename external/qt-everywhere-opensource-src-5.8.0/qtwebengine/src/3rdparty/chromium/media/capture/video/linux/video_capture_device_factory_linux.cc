// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/video_capture_device_factory_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include "base/files/file_enumerator.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"

#if defined(OS_OPENBSD)
#include <sys/videoio.h>
#else
#include <linux/videodev2.h>
#endif

#if defined(OS_CHROMEOS)
#include "media/capture/video/linux/video_capture_device_chromeos.h"
#endif
#include "media/capture/video/linux/video_capture_device_linux.h"

namespace media {

static bool HasUsableFormats(int fd, uint32_t capabilities) {
  if (!(capabilities & V4L2_CAP_VIDEO_CAPTURE))
    return false;

  const std::list<uint32_t>& usable_fourccs =
      VideoCaptureDeviceLinux::GetListOfUsableFourCCs(false);
  v4l2_fmtdesc fmtdesc = {};
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  for (; HANDLE_EINTR(ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0;
       ++fmtdesc.index) {
    if (std::find(usable_fourccs.begin(), usable_fourccs.end(),
                  fmtdesc.pixelformat) != usable_fourccs.end()) {
      return true;
    }
  }

  DLOG(ERROR) << "No usable formats found";
  return false;
}

static std::list<float> GetFrameRateList(int fd,
                                         uint32_t fourcc,
                                         uint32_t width,
                                         uint32_t height) {
  std::list<float> frame_rates;

  v4l2_frmivalenum frame_interval = {};
  frame_interval.pixel_format = fourcc;
  frame_interval.width = width;
  frame_interval.height = height;
  for (; HANDLE_EINTR(ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval)) ==
             0;
       ++frame_interval.index) {
    if (frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
      if (frame_interval.discrete.numerator != 0) {
        frame_rates.push_back(
            frame_interval.discrete.denominator /
            static_cast<float>(frame_interval.discrete.numerator));
      }
    } else if (frame_interval.type == V4L2_FRMIVAL_TYPE_CONTINUOUS ||
               frame_interval.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
      // TODO(mcasas): see http://crbug.com/249953, support these devices.
      NOTIMPLEMENTED();
      break;
    }
  }
  // Some devices, e.g. Kinect, do not enumerate any frame rates, see
  // http://crbug.com/412284. Set their frame_rate to zero.
  if (frame_rates.empty())
    frame_rates.push_back(0);
  return frame_rates;
}

static void GetSupportedFormatsForV4L2BufferType(
    int fd,
    media::VideoCaptureFormats* supported_formats) {
  v4l2_fmtdesc v4l2_format = {};
  v4l2_format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  for (; HANDLE_EINTR(ioctl(fd, VIDIOC_ENUM_FMT, &v4l2_format)) == 0;
       ++v4l2_format.index) {
    VideoCaptureFormat supported_format;
    supported_format.pixel_format =
        VideoCaptureDeviceLinux::V4l2FourCcToChromiumPixelFormat(
            v4l2_format.pixelformat);

    if (supported_format.pixel_format == PIXEL_FORMAT_UNKNOWN)
      continue;

    v4l2_frmsizeenum frame_size = {};
    frame_size.pixel_format = v4l2_format.pixelformat;
    for (; HANDLE_EINTR(ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frame_size)) == 0;
         ++frame_size.index) {
      if (frame_size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
        supported_format.frame_size.SetSize(frame_size.discrete.width,
                                            frame_size.discrete.height);
      } else if (frame_size.type == V4L2_FRMSIZE_TYPE_STEPWISE ||
                 frame_size.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
        // TODO(mcasas): see http://crbug.com/249953, support these devices.
        NOTIMPLEMENTED();
      }

      const std::list<float> frame_rates = GetFrameRateList(
          fd, v4l2_format.pixelformat, frame_size.discrete.width,
          frame_size.discrete.height);
      for (const auto& frame_rate : frame_rates) {
        supported_format.frame_rate = frame_rate;
        supported_formats->push_back(supported_format);
        DVLOG(1) << VideoCaptureFormat::ToString(supported_format);
      }
    }
  }
}

VideoCaptureDeviceFactoryLinux::VideoCaptureDeviceFactoryLinux(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner) {
}

VideoCaptureDeviceFactoryLinux::~VideoCaptureDeviceFactoryLinux() {
}

std::unique_ptr<VideoCaptureDevice> VideoCaptureDeviceFactoryLinux::Create(
    const VideoCaptureDevice::Name& device_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(OS_CHROMEOS)
  VideoCaptureDeviceChromeOS* self =
      new VideoCaptureDeviceChromeOS(ui_task_runner_, device_name);
#else
  VideoCaptureDeviceLinux* self = new VideoCaptureDeviceLinux(device_name);
#endif
  if (!self)
    return std::unique_ptr<VideoCaptureDevice>();
  // Test opening the device driver. This is to make sure it is available.
  // We will reopen it again in our worker thread when someone
  // allocates the camera.
  base::ScopedFD fd(HANDLE_EINTR(open(device_name.id().c_str(), O_RDONLY)));
  if (!fd.is_valid()) {
    DLOG(ERROR) << "Cannot open device";
    delete self;
    return std::unique_ptr<VideoCaptureDevice>();
  }

  return std::unique_ptr<VideoCaptureDevice>(self);
}

void VideoCaptureDeviceFactoryLinux::GetDeviceNames(
    VideoCaptureDevice::Names* const device_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(device_names->empty());
  const base::FilePath path("/dev/");
  base::FileEnumerator enumerator(path, false, base::FileEnumerator::FILES,
                                  "video*");

  while (!enumerator.Next().empty()) {
    const base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    const std::string unique_id = path.value() + info.GetName().value();
    const base::ScopedFD fd(HANDLE_EINTR(open(unique_id.c_str(), O_RDONLY)));
    if (!fd.is_valid()) {
      DLOG(ERROR) << "Couldn't open " << info.GetName().value();
      continue;
    }
    // Test if this is a V4L2 capture device and if it has at least one
    // supported capture format. Devices that have capture and output
    // capabilities at the same time are memory-to-memory and are skipped, see
    // http://crbug.com/139356.
    v4l2_capability cap;
    if ((HANDLE_EINTR(ioctl(fd.get(), VIDIOC_QUERYCAP, &cap)) == 0) &&
        (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE &&
         !(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) &&
        HasUsableFormats(fd.get(), cap.capabilities)) {
      device_names->push_back(VideoCaptureDevice::Name(
          reinterpret_cast<char*>(cap.card), unique_id,
          VideoCaptureDevice::Name::V4L2_SINGLE_PLANE));
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
  if (!fd.is_valid())  // Failed to open this device.
    return;
  supported_formats->clear();

  DCHECK_NE(device.capture_api_type(),
            VideoCaptureDevice::Name::API_TYPE_UNKNOWN);
  GetSupportedFormatsForV4L2BufferType(fd.get(), supported_formats);
}

// static
VideoCaptureDeviceFactory*
VideoCaptureDeviceFactory::CreateVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return new VideoCaptureDeviceFactoryLinux(ui_task_runner);
}

}  // namespace media
