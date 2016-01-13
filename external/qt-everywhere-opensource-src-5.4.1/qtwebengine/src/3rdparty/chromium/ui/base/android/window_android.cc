// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/android/window_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "jni/WindowAndroid_jni.h"
#include "ui/base/android/window_android_compositor.h"
#include "ui/base/android/window_android_observer.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

WindowAndroid::WindowAndroid(JNIEnv* env, jobject obj, jlong vsync_period)
  : weak_java_window_(env, obj),
    compositor_(NULL),
    vsync_period_(base::TimeDelta::FromInternalValue(vsync_period)) {
}

void WindowAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

ScopedJavaLocalRef<jobject> WindowAndroid::GetJavaObject() {
  return weak_java_window_.get(AttachCurrentThread());
}

bool WindowAndroid::RegisterWindowAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WindowAndroid::~WindowAndroid() {
  DCHECK(!compositor_);
}

void WindowAndroid::OnCompositingDidCommit() {
  FOR_EACH_OBSERVER(WindowAndroidObserver,
                    observer_list_,
                    OnCompositingDidCommit());
}

void WindowAndroid::AddObserver(WindowAndroidObserver* observer) {
  if (!observer_list_.HasObserver(observer))
    observer_list_.AddObserver(observer);
}

void WindowAndroid::RemoveObserver(WindowAndroidObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void WindowAndroid::AttachCompositor(WindowAndroidCompositor* compositor) {
  if (compositor_ && compositor != compositor_)
    DetachCompositor();

  compositor_ = compositor;
  FOR_EACH_OBSERVER(WindowAndroidObserver,
                    observer_list_,
                    OnAttachCompositor());
}

void WindowAndroid::DetachCompositor() {
  compositor_ = NULL;
  FOR_EACH_OBSERVER(WindowAndroidObserver,
                    observer_list_,
                    OnDetachCompositor());
  observer_list_.Clear();
}

void WindowAndroid::RequestVSyncUpdate() {
  JNIEnv* env = AttachCurrentThread();
  Java_WindowAndroid_requestVSyncUpdate(env, GetJavaObject().obj());
}

void WindowAndroid::SetNeedsAnimate() {
  if (compositor_)
    compositor_->SetNeedsAnimate();
}

void WindowAndroid::Animate(base::TimeTicks begin_frame_time) {
  FOR_EACH_OBSERVER(
      WindowAndroidObserver, observer_list_, OnAnimate(begin_frame_time));
}

void WindowAndroid::OnVSync(JNIEnv* env, jobject obj, jlong time_micros) {
  base::TimeTicks frame_time(base::TimeTicks::FromInternalValue(time_micros));
  FOR_EACH_OBSERVER(
      WindowAndroidObserver,
      observer_list_,
      OnVSync(frame_time, vsync_period_));
  if (compositor_)
    compositor_->OnVSync(frame_time, vsync_period_);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env, jobject obj, jlong vsync_period) {
  WindowAndroid* window = new WindowAndroid(env, obj, vsync_period);
  return reinterpret_cast<intptr_t>(window);
}

}  // namespace ui
