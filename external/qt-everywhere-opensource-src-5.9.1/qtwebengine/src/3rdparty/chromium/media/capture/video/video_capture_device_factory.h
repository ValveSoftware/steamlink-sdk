// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/capture/video/video_capture_device.h"

namespace media {

// VideoCaptureDeviceFactory is the base class for creation of video capture
// devices in the different platforms. VCDFs are created by MediaStreamManager
// on IO thread and plugged into VideoCaptureManager, who owns and operates them
// in Device Thread (a.k.a. Audio Thread).
// Typical operation is to first call EnumerateDeviceDescriptors() to obtain
// information about available devices. The obtained descriptors can then be
// used to either obtain the supported formats of a device using
// GetSupportedFormats(), or to create an instance of VideoCaptureDevice for
// the device using CreateDevice().
// TODO(chfremer): Add a layer on top of the platform-specific implementations
// that uses strings instead of descriptors as keys for accessing devices.
// crbug.com/665065
class CAPTURE_EXPORT VideoCaptureDeviceFactory {
 public:
  static std::unique_ptr<VideoCaptureDeviceFactory> CreateFactory(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  VideoCaptureDeviceFactory();
  virtual ~VideoCaptureDeviceFactory();

  // Creates a VideoCaptureDevice object. Returns NULL if something goes wrong.
  virtual std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) = 0;

  // Asynchronous version of GetDeviceDescriptors calling back to |callback|.
  virtual void EnumerateDeviceDescriptors(
      const base::Callback<
          void(std::unique_ptr<VideoCaptureDeviceDescriptors>)>& callback);

  // Obtains the supported formats of a device.
  // This method should be called before allocating or starting a device. In
  // case format enumeration is not supported, or there was a problem
  // |supported_formats| will be empty.
  virtual void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats) = 0;

  // Gets descriptors of all video capture devices connected.
  // Used by the default implementation of EnumerateDevices().
  // Note: The same physical device may appear more than once if it is
  // accessible through different APIs.
  virtual void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) = 0;

 protected:
  base::ThreadChecker thread_checker_;

 private:
  static VideoCaptureDeviceFactory* CreateVideoCaptureDeviceFactory(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceFactory);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_DEVICE_FACTORY_H_
