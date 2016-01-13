// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/android/view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "jni/ViewAndroid_jni.h"
#include "ui/base/android/window_android.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

ViewAndroid::ViewAndroid(JNIEnv* env, jobject obj, WindowAndroid* window)
  : weak_java_view_(env, obj),
    window_android_(window) {}

void ViewAndroid::Destroy(JNIEnv* env, jobject obj) {
  DCHECK(obj && env->IsSameObject(weak_java_view_.get(env).obj(), obj));
  delete this;
}

ScopedJavaLocalRef<jobject> ViewAndroid::GetJavaObject() {
  // It is mandatory to explicitly call destroy() before releasing the java
  // side object. This could be changed in future by adding CleanupReference
  // based destruct path;
  return weak_java_view_.get(AttachCurrentThread());
}

WindowAndroid* ViewAndroid::GetWindowAndroid() {
  return window_android_;
}

bool ViewAndroid::RegisterViewAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ViewAndroid::~ViewAndroid() {
}

jlong Init(JNIEnv* env, jobject obj, jlong window) {
  ViewAndroid* view = new ViewAndroid(
      env, obj, reinterpret_cast<ui::WindowAndroid*>(window));
  return reinterpret_cast<intptr_t>(view);
}

}  // namespace ui
