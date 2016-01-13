// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/android/video_capture_device_factory_android.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "jni/VideoCaptureFactory_jni.h"
#include "media/video/capture/android/video_capture_device_android.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace media {

// static
bool VideoCaptureDeviceFactoryAndroid::RegisterVideoCaptureDeviceFactory(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

//static
ScopedJavaLocalRef<jobject>
VideoCaptureDeviceFactoryAndroid::createVideoCaptureAndroid(
    int id,
    jlong nativeVideoCaptureDeviceAndroid) {
  return (Java_VideoCaptureFactory_createVideoCapture(
      AttachCurrentThread(),
      base::android::GetApplicationContext(),
      id,
      nativeVideoCaptureDeviceAndroid));
}

scoped_ptr<VideoCaptureDevice> VideoCaptureDeviceFactoryAndroid::Create(
    const VideoCaptureDevice::Name& device_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int id;
  if (!base::StringToInt(device_name.id(), &id))
    return scoped_ptr<VideoCaptureDevice>();

  VideoCaptureDeviceAndroid* video_capture_device(
      new VideoCaptureDeviceAndroid(device_name));

  if (video_capture_device->Init())
    return scoped_ptr<VideoCaptureDevice>(video_capture_device);

  DLOG(ERROR) << "Error creating Video Capture Device.";
  return scoped_ptr<VideoCaptureDevice>();
}

void VideoCaptureDeviceFactoryAndroid::GetDeviceNames(
    VideoCaptureDevice::Names* device_names) {
  DCHECK(thread_checker_.CalledOnValidThread());
  device_names->clear();

  JNIEnv* env = AttachCurrentThread();

  int num_cameras = Java_ChromiumCameraInfo_getNumberOfCameras(
      env, base::android::GetApplicationContext());
  DVLOG(1) << "VideoCaptureDevice::GetDeviceNames: num_cameras=" << num_cameras;
  if (num_cameras <= 0)
    return;

  for (int camera_id = num_cameras - 1; camera_id >= 0; --camera_id) {
    ScopedJavaLocalRef<jobject> ci =
        Java_ChromiumCameraInfo_getAt(env, camera_id);

    VideoCaptureDevice::Name name(
        base::android::ConvertJavaStringToUTF8(
            Java_ChromiumCameraInfo_getDeviceName(env, ci.obj())),
        base::StringPrintf("%d", Java_ChromiumCameraInfo_getId(env, ci.obj())));
    device_names->push_back(name);

    DVLOG(1) << "VideoCaptureDeviceFactoryAndroid::GetDeviceNames: camera"
             << "device_name=" << name.name() << ", unique_id=" << name.id()
             << ", orientation "
             << Java_ChromiumCameraInfo_getOrientation(env, ci.obj());
  }
}

void VideoCaptureDeviceFactoryAndroid::GetDeviceSupportedFormats(
    const VideoCaptureDevice::Name& device,
    VideoCaptureFormats* capture_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int id;
  if (!base::StringToInt(device.id(), &id))
    return;
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> collected_formats =
      Java_VideoCaptureFactory_getDeviceSupportedFormats(env, id);
  if (collected_formats.is_null())
    return;

  jsize num_formats = env->GetArrayLength(collected_formats.obj());
  for (int i = 0; i < num_formats; ++i) {
    base::android::ScopedJavaLocalRef<jobject> format(
        env, env->GetObjectArrayElement(collected_formats.obj(), i));

    VideoPixelFormat pixel_format = media::PIXEL_FORMAT_UNKNOWN;
    switch (media::Java_VideoCaptureFactory_getCaptureFormatPixelFormat(
        env, format.obj())) {
      case ANDROID_IMAGEFORMAT_YV12:
        pixel_format = media::PIXEL_FORMAT_YV12;
        break;
      case ANDROID_IMAGEFORMAT_NV21:
        pixel_format = media::PIXEL_FORMAT_NV21;
        break;
      default:
        break;
    }
    VideoCaptureFormat capture_format(
        gfx::Size(media::Java_VideoCaptureFactory_getCaptureFormatWidth(env,
                      format.obj()),
                  media::Java_VideoCaptureFactory_getCaptureFormatHeight(env,
                      format.obj())),
        media::Java_VideoCaptureFactory_getCaptureFormatFramerate(env,
                                                                  format.obj()),
        pixel_format);
    capture_formats->push_back(capture_format);
    DVLOG(1) << device.name() << " resolution: "
        << capture_format.frame_size.ToString() << ", fps: "
        << capture_format.frame_rate << ", pixel format: "
        << capture_format.pixel_format;
  }
}

}  // namespace media
