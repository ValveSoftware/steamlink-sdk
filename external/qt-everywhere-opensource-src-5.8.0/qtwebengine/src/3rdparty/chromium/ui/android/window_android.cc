// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/window_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "jni/WindowAndroid_jni.h"
#include "ui/android/window_android_compositor.h"
#include "ui/android/window_android_observer.h"

namespace ui {

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

WindowAndroid::WindowAndroid(JNIEnv* env, jobject obj) : compositor_(NULL) {
  java_window_.Reset(env, obj);
}

void WindowAndroid::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

ScopedJavaLocalRef<jobject> WindowAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_window_);
}

bool WindowAndroid::RegisterWindowAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WindowAndroid::~WindowAndroid() {
  DCHECK(!compositor_);
}

WindowAndroid* WindowAndroid::createForTesting() {
  JNIEnv* env = AttachCurrentThread();
  jobject context = base::android::GetApplicationContext();
  return new WindowAndroid(
      env, Java_WindowAndroid_createForTesting(env, context).obj());
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

void WindowAndroid::OnVSync(JNIEnv* env,
                            const JavaParamRef<jobject>& obj,
                            jlong time_micros,
                            jlong period_micros) {
  base::TimeTicks frame_time(base::TimeTicks::FromInternalValue(time_micros));
  base::TimeDelta vsync_period(
      base::TimeDelta::FromMicroseconds(period_micros));
  FOR_EACH_OBSERVER(
      WindowAndroidObserver,
      observer_list_,
      OnVSync(frame_time, vsync_period));
  if (compositor_)
    compositor_->OnVSync(frame_time, vsync_period);
}

void WindowAndroid::OnVisibilityChanged(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        bool visible) {
  FOR_EACH_OBSERVER(WindowAndroidObserver, observer_list_,
                    OnRootWindowVisibilityChanged(visible));
}

void WindowAndroid::OnActivityStopped(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  FOR_EACH_OBSERVER(WindowAndroidObserver, observer_list_, OnActivityStopped());
}

void WindowAndroid::OnActivityStarted(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  FOR_EACH_OBSERVER(WindowAndroidObserver, observer_list_, OnActivityStarted());
}

bool WindowAndroid::HasPermission(const std::string& permission) {
  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_hasPermission(
      env,
      GetJavaObject().obj(),
      base::android::ConvertUTF8ToJavaString(env, permission).obj());
}

bool WindowAndroid::CanRequestPermission(const std::string& permission) {
  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_canRequestPermission(
      env,
      GetJavaObject().obj(),
      base::android::ConvertUTF8ToJavaString(env, permission).obj());
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  WindowAndroid* window = new WindowAndroid(env, obj);
  return reinterpret_cast<intptr_t>(window);
}

}  // namespace ui
