// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/display_android_manager.h"

#include <jni.h>
#include <map>

#include "base/android/jni_android.h"
#include "base/stl_util.h"
#include "jni/DisplayAndroidManager_jni.h"
#include "ui/android/screen_android.h"
#include "ui/android/window_android.h"
#include "ui/display/display.h"

namespace ui {

using base::android::AttachCurrentThread;
using display::Display;
using display::DisplayList;

void SetScreenAndroid() {
  // Do not override existing Screen.
  DCHECK_EQ(display::Screen::GetScreen(), nullptr);

  DisplayAndroidManager* manager = new DisplayAndroidManager();
  display::Screen::SetScreenInstance(manager);

  JNIEnv* env = AttachCurrentThread();
  Java_DisplayAndroidManager_onNativeSideCreated(env, (jlong)manager);
}

bool RegisterScreenAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

DisplayAndroidManager::DisplayAndroidManager() {}

DisplayAndroidManager::~DisplayAndroidManager() {}

// Screen interface.

Display DisplayAndroidManager::GetDisplayNearestWindow(
    gfx::NativeView view) const {
  ui::WindowAndroid* window = view ? view->GetWindowAndroid() : nullptr;
  if (window) {
    DisplayList::Displays::const_iterator it =
        display_list().FindDisplayById(window->display_id());
    if (it != display_list().displays().end()) {
      return *it;
    }
  }
  return GetPrimaryDisplay();
}

// There is no notion of relative display positions on Android.
Display DisplayAndroidManager::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

// There is no notion of relative display positions on Android.
Display DisplayAndroidManager::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  NOTIMPLEMENTED();
  return GetPrimaryDisplay();
}

// Methods called from Java

void DisplayAndroidManager::UpdateDisplay(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobject,
    jint sdkDisplayId,
    jint physicalWidth,
    jint physicalHeight,
    jint width,
    jint height,
    jfloat dipScale,
    jint rotationDegrees,
    jint bitsPerPixel,
    jint bitsPerComponent) {
  gfx::Rect bounds_in_pixels = gfx::Rect(physicalWidth, physicalHeight);

  // Physical width and height might be not supported.
  if (bounds_in_pixels.IsEmpty())
    bounds_in_pixels = gfx::Rect(width, height);

  const gfx::Rect bounds_in_dip = gfx::Rect(
      gfx::ScaleToCeiledSize(bounds_in_pixels.size(), 1.0f / dipScale));

  display::Display display(sdkDisplayId, bounds_in_dip);

  display.set_device_scale_factor(dipScale);
  display.SetRotationAsDegree(rotationDegrees);
  display.set_color_depth(bitsPerPixel);
  display.set_depth_per_component(bitsPerComponent);
  display.set_is_monochrome(bitsPerComponent == 0);

  ProcessDisplayChanged(display, sdkDisplayId == primary_display_id_);
}

void DisplayAndroidManager::RemoveDisplay(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobject,
    jint sdkDisplayId) {
  display_list().RemoveDisplay(sdkDisplayId);
}

void DisplayAndroidManager::SetPrimaryDisplayId(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobject,
    jint sdkDisplayId) {
  primary_display_id_ = sdkDisplayId;
}

}  // namespace ui
