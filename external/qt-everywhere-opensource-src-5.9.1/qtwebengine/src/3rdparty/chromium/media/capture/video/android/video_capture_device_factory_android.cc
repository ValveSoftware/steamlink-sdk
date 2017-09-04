// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/video_capture_device_factory_android.h"

#include <utility>

#include "base/android/context_utils.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "jni/VideoCaptureFactory_jni.h"
#include "media/capture/video/android/video_capture_device_android.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace media {

// static
ScopedJavaLocalRef<jobject>
VideoCaptureDeviceFactoryAndroid::createVideoCaptureAndroid(
    int id,
    jlong nativeVideoCaptureDeviceAndroid) {
  return (Java_VideoCaptureFactory_createVideoCapture(
      AttachCurrentThread(), base::android::GetApplicationContext(), id,
      nativeVideoCaptureDeviceAndroid));
}

std::unique_ptr<VideoCaptureDevice>
VideoCaptureDeviceFactoryAndroid::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int id;
  if (!base::StringToInt(device_descriptor.device_id, &id))
    return std::unique_ptr<VideoCaptureDevice>();

  std::unique_ptr<VideoCaptureDeviceAndroid> video_capture_device(
      new VideoCaptureDeviceAndroid(device_descriptor));

  if (video_capture_device->Init()) {
    if (test_mode_)
      video_capture_device->ConfigureForTesting();
    return std::move(video_capture_device);
  }

  DLOG(ERROR) << "Error creating Video Capture Device.";
  return nullptr;
}

void VideoCaptureDeviceFactoryAndroid::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK(thread_checker_.CalledOnValidThread());
  device_descriptors->clear();

  JNIEnv* env = AttachCurrentThread();

  const JavaRef<jobject>& context = base::android::GetApplicationContext();
  const int num_cameras =
      Java_VideoCaptureFactory_getNumberOfCameras(env, context);
  DVLOG(1) << __FUNCTION__ << ": num_cameras=" << num_cameras;
  if (num_cameras <= 0)
    return;

  for (int camera_id = num_cameras - 1; camera_id >= 0; --camera_id) {
    base::android::ScopedJavaLocalRef<jstring> device_name =
        Java_VideoCaptureFactory_getDeviceName(env, camera_id, context);
    if (device_name.obj() == NULL)
      continue;

    const int capture_api_type =
        Java_VideoCaptureFactory_getCaptureApiType(env, camera_id, context);
    const std::string display_name =
        base::android::ConvertJavaStringToUTF8(device_name);
    const std::string device_id = base::IntToString(camera_id);

    // Android cameras are not typically USB devices, and the model_id is
    // currently only used for USB model identifiers, so this implementation
    // just indicates an unknown device model (by not providing one).
    device_descriptors->emplace_back(
        display_name, device_id,
        static_cast<VideoCaptureApi>(capture_api_type));

    DVLOG(1) << __FUNCTION__ << ": camera "
             << "device_name=" << display_name << ", unique_id=" << device_id;
  }
}

void VideoCaptureDeviceFactoryAndroid::GetSupportedFormats(
    const VideoCaptureDeviceDescriptor& device,
    VideoCaptureFormats* capture_formats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int id;
  if (!base::StringToInt(device.device_id, &id))
    return;
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobjectArray> collected_formats =
      Java_VideoCaptureFactory_getDeviceSupportedFormats(
          env, base::android::GetApplicationContext(), id);
  if (collected_formats.is_null())
    return;

  jsize num_formats = env->GetArrayLength(collected_formats.obj());
  for (int i = 0; i < num_formats; ++i) {
    base::android::ScopedJavaLocalRef<jobject> format(
        env, env->GetObjectArrayElement(collected_formats.obj(), i));

    VideoPixelFormat pixel_format = media::PIXEL_FORMAT_UNKNOWN;
    switch (media::Java_VideoCaptureFactory_getCaptureFormatPixelFormat(
        env, format)) {
      case VideoCaptureDeviceAndroid::ANDROID_IMAGE_FORMAT_YV12:
        pixel_format = media::PIXEL_FORMAT_YV12;
        break;
      case VideoCaptureDeviceAndroid::ANDROID_IMAGE_FORMAT_NV21:
        pixel_format = media::PIXEL_FORMAT_NV21;
        break;
      default:
        // TODO(mcasas): break here and let the enumeration continue with
        // UNKNOWN pixel format because the platform doesn't know until capture,
        // but some unrelated tests timeout https://crbug.com/644910.
        continue;
    }
    VideoCaptureFormat capture_format(
        gfx::Size(
            media::Java_VideoCaptureFactory_getCaptureFormatWidth(env, format),
            media::Java_VideoCaptureFactory_getCaptureFormatHeight(env,
                                                                   format)),
        media::Java_VideoCaptureFactory_getCaptureFormatFramerate(env, format),
        pixel_format);
    capture_formats->push_back(capture_format);
    DVLOG(1) << device.display_name << " "
             << VideoCaptureFormat::ToString(capture_format);
  }
}

bool VideoCaptureDeviceFactoryAndroid::IsLegacyOrDeprecatedDevice(
    const std::string& device_id) {
  int id;
  if (!base::StringToInt(device_id, &id))
    return true;
  return (Java_VideoCaptureFactory_isLegacyOrDeprecatedDevice(
      AttachCurrentThread(), base::android::GetApplicationContext(), id));
}

// static
VideoCaptureDeviceFactory*
VideoCaptureDeviceFactory::CreateVideoCaptureDeviceFactory(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return new VideoCaptureDeviceFactoryAndroid();
}

}  // namespace media
