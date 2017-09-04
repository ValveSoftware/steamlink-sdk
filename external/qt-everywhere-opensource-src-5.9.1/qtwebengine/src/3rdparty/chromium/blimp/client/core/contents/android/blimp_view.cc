// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/android/blimp_view.h"

#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "jni/BlimpView_jni.h"
#include "ui/android/window_android.h"
#include "ui/events/android/motion_event_android.h"

namespace blimp {
namespace client {

// static
bool BlimpView::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

BlimpView::BlimpView(ui::WindowAndroid* window,
                     BlimpContentsViewImpl* blimp_contents_view)
    : blimp_contents_view_(blimp_contents_view) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env,
                  Java_BlimpView_create(env, reinterpret_cast<intptr_t>(this),
                                        window->GetJavaObject())
                      .obj());
}

BlimpView::~BlimpView() {
  Java_BlimpView_clearNativePtr(base::android::AttachCurrentThread(),
                                java_obj_);
}

base::android::ScopedJavaLocalRef<jobject> BlimpView::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
BlimpView::CreateViewAndroidDelegate() {
  return Java_BlimpView_createViewAndroidDelegate(
      base::android::AttachCurrentThread(), java_obj_);
}

void BlimpView::OnSizeChanged(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& jobj,
                              jint width,
                              jint height,
                              jfloat device_scale_factor_dp_to_px) {
  blimp_contents_view_->SetSizeAndScale(gfx::Size(width, height),
                                        device_scale_factor_dp_to_px);
}

jboolean BlimpView::OnTouchEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& motion_event,
    jlong time_ms,
    jint android_action,
    jint pointer_count,
    jint history_size,
    jint action_index,
    jfloat pos_x_0,
    jfloat pos_y_0,
    jfloat pos_x_1,
    jfloat pos_y_1,
    jint pointer_id_0,
    jint pointer_id_1,
    jfloat touch_major_0,
    jfloat touch_major_1,
    jfloat touch_minor_0,
    jfloat touch_minor_1,
    jfloat orientation_0,
    jfloat orientation_1,
    jfloat tilt_0,
    jfloat tilt_1,
    jfloat raw_pos_x,
    jfloat raw_pos_y,
    jint android_tool_type_0,
    jint android_tool_type_1,
    jint android_button_state,
    jint android_meta_state,
    jfloat device_scale_factor_dp_to_px) {
  ui::MotionEventAndroid::Pointer pointer0(
      pointer_id_0, pos_x_0, pos_y_0, touch_major_0, touch_minor_0,
      orientation_0, tilt_0, android_tool_type_0);
  ui::MotionEventAndroid::Pointer pointer1(
      pointer_id_1, pos_x_1, pos_y_1, touch_major_1, touch_minor_1,
      orientation_1, tilt_1, android_tool_type_1);
  ui::MotionEventAndroid event(
      1.f / device_scale_factor_dp_to_px, env, motion_event, time_ms,
      android_action, pointer_count, history_size, action_index,
      android_button_state, android_meta_state, raw_pos_x - pos_x_0,
      raw_pos_y - pos_y_0, &pointer0, &pointer1);
  return blimp_contents_view_->OnTouchEvent(event);
}

}  // namespace client
}  // namespace blimp
