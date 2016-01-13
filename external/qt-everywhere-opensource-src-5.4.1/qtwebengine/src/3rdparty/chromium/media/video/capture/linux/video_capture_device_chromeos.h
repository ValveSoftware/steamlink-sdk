// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_DEVICE_CHROMEOS_H_
#define MEDIA_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_DEVICE_CHROMEOS_H_

#include "media/video/capture/linux/video_capture_device_linux.h"

namespace gfx {
class Display;
}  // namespace gfx

namespace media {

// This class is functionally the same as VideoCaptureDeviceLinux, with the
// exception that it is aware of the orientation of the internal Display.  When
// the internal Display is rotated, the frames captured are rotated to match.
class VideoCaptureDeviceChromeOS : public VideoCaptureDeviceLinux {
 public:
  explicit VideoCaptureDeviceChromeOS(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      const Name& device_name);
  virtual ~VideoCaptureDeviceChromeOS();

 private:
  class ScreenObserverDelegate;

  void SetDisplayRotation(const gfx::Display& display);
  scoped_refptr<ScreenObserverDelegate> screen_observer_delegate_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceChromeOS);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_DEVICE_CHROMEOS_H_
