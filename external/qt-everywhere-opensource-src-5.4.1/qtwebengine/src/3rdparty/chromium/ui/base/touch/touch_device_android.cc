// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device.h"

#include "jni/TouchDevice_jni.h"

namespace ui {

bool IsTouchDevicePresent() {
  return true;
}

int MaxTouchPoints() {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject context = base::android::GetApplicationContext();
  jint max_touch_points = Java_TouchDevice_maxTouchPoints(env, context);
  return static_cast<int>(max_touch_points);
}

bool RegisterTouchDeviceAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace ui
