// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_DEVICE_FACTORY_ANDROID_H_
#define MEDIA_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_DEVICE_FACTORY_ANDROID_H_

#include "media/video/capture/video_capture_device_factory.h"

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "media/video/capture/video_capture_device.h"

namespace media {

// VideoCaptureDeviceFactory on Android. This class implements the static
// VideoCapture methods and the factory of VideoCaptureAndroid.
class MEDIA_EXPORT VideoCaptureDeviceFactoryAndroid :
  public VideoCaptureDeviceFactory {
 public:
  // Automatically generated enum to interface with Java world.
  enum AndroidImageFormat {
#define DEFINE_ANDROID_IMAGEFORMAT(name, value) name = value,
#include "media/video/capture/android/imageformat_list.h"
#undef DEFINE_ANDROID_IMAGEFORMAT
  };
  static bool RegisterVideoCaptureDeviceFactory(JNIEnv* env);
  static base::android::ScopedJavaLocalRef<jobject> createVideoCaptureAndroid(
      int id,
      jlong nativeVideoCaptureDeviceAndroid);

  VideoCaptureDeviceFactoryAndroid() {}
  virtual ~VideoCaptureDeviceFactoryAndroid() {}

  virtual scoped_ptr<VideoCaptureDevice> Create(
      const VideoCaptureDevice::Name& device_name) OVERRIDE;
  virtual void GetDeviceNames(VideoCaptureDevice::Names* device_names) OVERRIDE;
  virtual void GetDeviceSupportedFormats(
      const VideoCaptureDevice::Name& device,
      VideoCaptureFormats* supported_formats) OVERRIDE;

 private:

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceFactoryAndroid);};
}  // namespace media

#endif  // MEDIA_VIDEO_CAPTURE_ANDROID_VIDEO_CAPTURE_DEVICE_FACTORY_ANDROID_H_
