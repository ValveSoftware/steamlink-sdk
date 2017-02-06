// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_vr_device.h"

#include <math.h>
#include <algorithm>

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "jni/CardboardVRDevice_jni.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

using base::android::AttachCurrentThread;

namespace device {

bool CardboardVRDevice::RegisterCardboardVRDevice(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

CardboardVRDevice::CardboardVRDevice(VRDeviceProvider* provider)
    : VRDevice(provider) {
  JNIEnv* env = AttachCurrentThread();
  j_cardboard_device_.Reset(Java_CardboardVRDevice_create(
      env, base::android::GetApplicationContext()));
  j_head_matrix_.Reset(env, env->NewFloatArray(16));
}

CardboardVRDevice::~CardboardVRDevice() {
  Java_CardboardVRDevice_stopTracking(AttachCurrentThread(),
                                      j_cardboard_device_.obj());
}

VRDisplayPtr CardboardVRDevice::GetVRDevice() {
  TRACE_EVENT0("input", "CardboardVRDevice::GetVRDevice");
  VRDisplayPtr device = VRDisplay::New();

  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_device_name =
      Java_CardboardVRDevice_getDeviceName(env, j_cardboard_device_.obj());
  device->displayName =
      base::android::ConvertJavaStringToUTF8(env, j_device_name.obj());

  ScopedJavaLocalRef<jfloatArray> j_fov(env, env->NewFloatArray(4));
  Java_CardboardVRDevice_getFieldOfView(env, j_cardboard_device_.obj(),
                                        j_fov.obj());

  std::vector<float> fov;
  base::android::JavaFloatArrayToFloatVector(env, j_fov.obj(), &fov);

  device->capabilities = VRDisplayCapabilities::New();
  device->capabilities->hasOrientation = true;
  device->capabilities->hasPosition = false;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = false;

  device->leftEye = VREyeParameters::New();
  device->rightEye = VREyeParameters::New();
  VREyeParametersPtr& left_eye = device->leftEye;
  VREyeParametersPtr& right_eye = device->rightEye;

  left_eye->fieldOfView = VRFieldOfView::New();
  left_eye->fieldOfView->upDegrees = fov[0];
  left_eye->fieldOfView->downDegrees = fov[1];
  left_eye->fieldOfView->leftDegrees = fov[2];
  left_eye->fieldOfView->rightDegrees = fov[3];

  // Cardboard devices always assume a mirrored FOV, so this is just the left
  // eye FOV with the left and right degrees swapped.
  right_eye->fieldOfView = VRFieldOfView::New();
  right_eye->fieldOfView->upDegrees = fov[0];
  right_eye->fieldOfView->downDegrees = fov[1];
  right_eye->fieldOfView->leftDegrees = fov[3];
  right_eye->fieldOfView->rightDegrees = fov[2];

  float ipd = Java_CardboardVRDevice_getIpd(env, j_cardboard_device_.obj());

  left_eye->offset = mojo::Array<float>::New(3);
  left_eye->offset[0] = ipd * -0.5f;
  left_eye->offset[1] = 0.0f;
  left_eye->offset[2] = 0.0f;

  right_eye->offset = mojo::Array<float>::New(3);
  right_eye->offset[0] = ipd * 0.5f;
  right_eye->offset[1] = 0.0f;
  right_eye->offset[2] = 0.0f;

  ScopedJavaLocalRef<jintArray> j_screen_size(env, env->NewIntArray(2));
  Java_CardboardVRDevice_getScreenSize(env, j_cardboard_device_.obj(),
                                       j_screen_size.obj());

  std::vector<int> screen_size;
  base::android::JavaIntArrayToIntVector(env, j_screen_size.obj(),
                                         &screen_size);

  left_eye->renderWidth = screen_size[0] / 2.0;
  left_eye->renderHeight = screen_size[1];

  right_eye->renderWidth = screen_size[0] / 2.0;
  right_eye->renderHeight = screen_size[1];

  return device;
}

VRPosePtr CardboardVRDevice::GetPose() {
  TRACE_EVENT0("input", "CardboardVRDevice::GetSensorState");
  VRPosePtr pose = VRPose::New();

  pose->timestamp = base::Time::Now().ToJsTime();

  JNIEnv* env = AttachCurrentThread();
  Java_CardboardVRDevice_getSensorState(env, j_cardboard_device_.obj(),
                                        j_head_matrix_.obj());

  std::vector<float> head_matrix;
  base::android::JavaFloatArrayToFloatVector(env, j_head_matrix_.obj(),
                                             &head_matrix);

  gfx::Transform transform(
      head_matrix[0], head_matrix[1], head_matrix[2], head_matrix[3],
      head_matrix[4], head_matrix[5], head_matrix[6], head_matrix[7],
      head_matrix[8], head_matrix[9], head_matrix[10], head_matrix[11],
      head_matrix[12], head_matrix[13], head_matrix[14], head_matrix[15]);

  gfx::DecomposedTransform decomposed_transform;
  gfx::DecomposeTransform(&decomposed_transform, transform);

  pose->orientation = mojo::Array<float>::New(4);
  pose->orientation[0] = decomposed_transform.quaternion[0];
  pose->orientation[1] = decomposed_transform.quaternion[1];
  pose->orientation[2] = decomposed_transform.quaternion[2];
  pose->orientation[3] = decomposed_transform.quaternion[3];

  pose->position = mojo::Array<float>::New(3);
  pose->position[0] = decomposed_transform.translate[0];
  pose->position[1] = decomposed_transform.translate[1];
  pose->position[2] = decomposed_transform.translate[2];

  return pose;
}

void CardboardVRDevice::ResetPose() {
  Java_CardboardVRDevice_resetSensor(AttachCurrentThread(),
                                     j_cardboard_device_.obj());
}

}  // namespace device
