// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Linux specific implementation of VideoCaptureDevice.
// V4L2 is used for capturing. V4L2 does not provide its own thread for
// capturing so this implementation uses a Chromium thread for fetching frames
// from V4L2.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_

#include <stdint.h>

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/threading/thread.h"
#include "media/base/video_capture_types.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

class V4L2CaptureDelegate;

// Linux V4L2 implementation of VideoCaptureDevice.
class VideoCaptureDeviceLinux : public VideoCaptureDevice {
 public:
  static VideoPixelFormat V4l2FourCcToChromiumPixelFormat(uint32_t v4l2_fourcc);
  static std::list<uint32_t> GetListOfUsableFourCCs(bool favour_mjpeg);

  explicit VideoCaptureDeviceLinux(const Name& device_name);
  ~VideoCaptureDeviceLinux() override;

  // VideoCaptureDevice implementation.
  void AllocateAndStart(const VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;

 protected:
  void SetRotation(int rotation);

 private:
  static int TranslatePowerLineFrequencyToV4L2(PowerLineFrequency frequency);

  // Internal delegate doing the actual capture setting, buffer allocation and
  // circulation with the V4L2 API. Created and deleted in the thread where
  // VideoCaptureDeviceLinux lives but otherwise operating on |v4l2_thread_|.
  scoped_refptr<V4L2CaptureDelegate> capture_impl_;

  base::Thread v4l2_thread_;  // Thread used for reading data from the device.

  const Name device_name_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceLinux);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_
