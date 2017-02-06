// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_device.h"

#include "base/android/context_utils.h"
#include "base/logging.h"
#include "jni/TouchDevice_jni.h"

namespace ui {

TouchScreensAvailability GetTouchScreensAvailability() {
  return TouchScreensAvailability::ENABLED;
}

int MaxTouchPoints() {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject context = base::android::GetApplicationContext();
  jint max_touch_points = Java_TouchDevice_maxTouchPoints(env, context);
  return static_cast<int>(max_touch_points);
}

int GetAvailablePointerTypes() {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject context = base::android::GetApplicationContext();
  jint available_pointer_types =
      Java_TouchDevice_availablePointerTypes(env, context);
  return static_cast<int>(available_pointer_types);
}

PointerType GetPrimaryPointerType() {
  int available_pointer_types = GetAvailablePointerTypes();
  if (available_pointer_types & POINTER_TYPE_COARSE)
    return POINTER_TYPE_COARSE;
  if (available_pointer_types & POINTER_TYPE_FINE)
    return POINTER_TYPE_FINE;
  DCHECK_EQ(available_pointer_types, POINTER_TYPE_NONE);
  return POINTER_TYPE_NONE;
}

int GetAvailableHoverTypes() {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject context = base::android::GetApplicationContext();
  jint available_hover_types =
      Java_TouchDevice_availableHoverTypes(env, context);
  return static_cast<int>(available_hover_types);
}

HoverType GetPrimaryHoverType() {
  int available_hover_types = GetAvailableHoverTypes();
  if (available_hover_types & HOVER_TYPE_ON_DEMAND)
    return HOVER_TYPE_ON_DEMAND;
  if (available_hover_types & HOVER_TYPE_HOVER)
    return HOVER_TYPE_HOVER;
  DCHECK_EQ(available_hover_types, HOVER_TYPE_NONE);
  return HOVER_TYPE_NONE;
}

bool RegisterTouchDeviceAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace ui
