// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_ANDROID_BLIMP_VIEW_H_
#define BLIMP_CLIENT_CORE_CONTENTS_ANDROID_BLIMP_VIEW_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace blimp {
namespace client {

class BlimpContentsViewImpl;

// The JNI bridge for the Java BlimpView that will provide hooks to the Android
// framework to interact with the content. The Java object is created by
// constructed and owned by the native class.
class BlimpView {
 public:
  static bool RegisterJni(JNIEnv* env);

  // |blimp_contents_view| must be the BlimpContentsView that owns this
  // BlimpView.  |window| should also be the WindowAndroid that the
  // corresponding BlimpContents was created with.
  explicit BlimpView(ui::WindowAndroid* window,
                     BlimpContentsViewImpl* blimp_contents_view);
  ~BlimpView();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Creates a new ViewAndroidDelegate for this view.
  base::android::ScopedJavaLocalRef<jobject> CreateViewAndroidDelegate();

  void OnSizeChanged(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jobj,
                     jint width,
                     jint height,
                     jfloat device_scale_factor_dp_to_px);

  jboolean OnTouchEvent(
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
      jfloat device_scale_factor_dp_to_px);

 private:
  // The BlimpContentsViewImpl that this BlimpView is used for.
  // TODO(nyquist): Use a delegate instead of the BlimpContentsImpl.
  BlimpContentsViewImpl* blimp_contents_view_;

  // The Java object for this BlimpView.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(BlimpView);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_ANDROID_BLIMP_VIEW_H_
