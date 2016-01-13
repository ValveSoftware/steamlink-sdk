// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Linux specific implementation of VideoCaptureDevice.
// V4L2 is used for capturing. V4L2 does not provide its own thread for
// capturing so this implementation uses a Chromium thread for fetching frames
// from V4L2.

#ifndef MEDIA_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_
#define MEDIA_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_

#include <string>

#include "base/file_util.h"
#include "base/files/scoped_file.h"
#include "base/threading/thread.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"

namespace media {

class VideoCaptureDeviceLinux : public VideoCaptureDevice {
 public:
  static VideoPixelFormat V4l2ColorToVideoCaptureColorFormat(int32 v4l2_fourcc);
  static void GetListOfUsableFourCCs(bool favour_mjpeg,
                                     std::list<int>* fourccs);

  explicit VideoCaptureDeviceLinux(const Name& device_name);
  virtual ~VideoCaptureDeviceLinux();

  // VideoCaptureDevice implementation.
  virtual void AllocateAndStart(const VideoCaptureParams& params,
                                scoped_ptr<Client> client) OVERRIDE;

  virtual void StopAndDeAllocate() OVERRIDE;

 protected:
  void SetRotation(int rotation);

  // Once |v4l2_thread_| is started, only called on that thread.
  void SetRotationOnV4L2Thread(int rotation);

 private:
  enum InternalState {
    kIdle,  // The device driver is opened but camera is not in use.
    kCapturing,  // Video is being captured.
    kError  // Error accessing HW functions.
            // User needs to recover by destroying the object.
  };

  // Buffers used to receive video frames from with v4l2.
  struct Buffer {
    Buffer() : start(0), length(0) {}
    void* start;
    size_t length;
  };

  // Called on the v4l2_thread_.
  void OnAllocateAndStart(int width,
                          int height,
                          int frame_rate,
                          scoped_ptr<Client> client);
  void OnStopAndDeAllocate();
  void OnCaptureTask();

  bool AllocateVideoBuffers();
  void DeAllocateVideoBuffers();
  void SetErrorState(const std::string& reason);

  InternalState state_;
  scoped_ptr<VideoCaptureDevice::Client> client_;
  Name device_name_;
  base::ScopedFD device_fd_;  // File descriptor for the opened camera device.
  base::Thread v4l2_thread_;  // Thread used for reading data from the device.
  Buffer* buffer_pool_;
  int buffer_pool_size_;  // Number of allocated buffers.
  int timeout_count_;
  VideoCaptureFormat capture_format_;

  // Clockwise rotation in degrees.  This value should be 0, 90, 180, or 270.
  // This is only used on |v4l2_thread_| when it is running, or the constructor
  // thread otherwise.
  int rotation_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoCaptureDeviceLinux);
};

}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_LINUX_VIDEO_CAPTURE_DEVICE_LINUX_H_
