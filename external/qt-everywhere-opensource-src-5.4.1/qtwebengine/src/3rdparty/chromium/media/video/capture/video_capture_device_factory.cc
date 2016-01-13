// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/video_capture_device_factory.h"

#include "base/command_line.h"
#include "media/base/media_switches.h"
#include "media/video/capture/fake_video_capture_device_factory.h"
#include "media/video/capture/file_video_capture_device_factory.h"

#if defined(OS_MACOSX)
#include "media/video/capture/mac/video_capture_device_factory_mac.h"
#elif defined(OS_LINUX)
#include "media/video/capture/linux/video_capture_device_factory_linux.h"
#elif defined(OS_ANDROID)
#include "media/video/capture/android/video_capture_device_factory_android.h"
#elif defined(OS_WIN)
#include "media/video/capture/win/video_capture_device_factory_win.h"
#endif

namespace media {

// static
scoped_ptr<VideoCaptureDeviceFactory> VideoCaptureDeviceFactory::CreateFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  // Use a Fake or File Video Device Factory if the command line flags are
  // present, otherwise use the normal, platform-dependent, device factory.
  if (command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream)) {
    if (command_line->HasSwitch(switches::kUseFileForFakeVideoCapture)) {
      return scoped_ptr<VideoCaptureDeviceFactory>(new
          media::FileVideoCaptureDeviceFactory());
    } else {
      return scoped_ptr<VideoCaptureDeviceFactory>(new
          media::FakeVideoCaptureDeviceFactory());
    }
  } else {
    // |ui_task_runner| is needed for the Linux ChromeOS factory to retrieve
    // screen rotations and for the Mac factory to run QTKit device enumeration.
#if defined(OS_MACOSX)
    return scoped_ptr<VideoCaptureDeviceFactory>(new
        VideoCaptureDeviceFactoryMac(ui_task_runner));
#elif defined(OS_LINUX)
    return scoped_ptr<VideoCaptureDeviceFactory>(new
        VideoCaptureDeviceFactoryLinux(ui_task_runner));
#elif defined(OS_ANDROID)
    return scoped_ptr<VideoCaptureDeviceFactory>(new
        VideoCaptureDeviceFactoryAndroid());
#elif defined(OS_WIN)
    return scoped_ptr<VideoCaptureDeviceFactory>(new
        VideoCaptureDeviceFactoryWin());
#else
    return scoped_ptr<VideoCaptureDeviceFactory>(new
        VideoCaptureDeviceFactory());
#endif
  }
}

VideoCaptureDeviceFactory::VideoCaptureDeviceFactory() {
  thread_checker_.DetachFromThread();
}

VideoCaptureDeviceFactory::~VideoCaptureDeviceFactory() {}

void VideoCaptureDeviceFactory::EnumerateDeviceNames(const base::Callback<
    void(scoped_ptr<media::VideoCaptureDevice::Names>)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!callback.is_null());
  scoped_ptr<VideoCaptureDevice::Names> device_names(
      new VideoCaptureDevice::Names());
  GetDeviceNames(device_names.get());
  callback.Run(device_names.Pass());
}

}  // namespace media
