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
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

WindowAndroid::WindowAndroid(JNIEnv* env, jobject obj, int display_id)
    : display_id_(display_id), compositor_(NULL) {
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
  DCHECK(parent_ == nullptr) << "WindowAndroid must be a root view.";
  DCHECK(!compositor_);
  Java_WindowAndroid_clearNativePointer(AttachCurrentThread(), GetJavaObject());
}

WindowAndroid* WindowAndroid::CreateForTesting() {
  JNIEnv* env = AttachCurrentThread();
  const JavaRef<jobject>& context = base::android::GetApplicationContext();
  long native_pointer = Java_WindowAndroid_createForTesting(env, context);
  return reinterpret_cast<WindowAndroid*>(native_pointer);
}

void WindowAndroid::DestroyForTesting() {
  delete this;
}

void WindowAndroid::OnCompositingDidCommit() {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnCompositingDidCommit();
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
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnAttachCompositor();
}

void WindowAndroid::DetachCompositor() {
  compositor_ = NULL;
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnDetachCompositor();
  observer_list_.Clear();
}

void WindowAndroid::RequestVSyncUpdate() {
  JNIEnv* env = AttachCurrentThread();
  Java_WindowAndroid_requestVSyncUpdate(env, GetJavaObject());
}

void WindowAndroid::SetNeedsAnimate() {
  if (compositor_)
    compositor_->SetNeedsAnimate();
}

void WindowAndroid::Animate(base::TimeTicks begin_frame_time) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnAnimate(begin_frame_time);
}

void WindowAndroid::OnVSync(JNIEnv* env,
                            const JavaParamRef<jobject>& obj,
                            jlong time_micros,
                            jlong period_micros) {
  base::TimeTicks frame_time(base::TimeTicks::FromInternalValue(time_micros));
  base::TimeDelta vsync_period(
      base::TimeDelta::FromMicroseconds(period_micros));
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnVSync(frame_time, vsync_period);
  if (compositor_)
    compositor_->OnVSync(frame_time, vsync_period);
}

void WindowAndroid::OnVisibilityChanged(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj,
                                        bool visible) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnRootWindowVisibilityChanged(visible);
}

void WindowAndroid::OnActivityStopped(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnActivityStopped();
}

void WindowAndroid::OnActivityStarted(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  for (WindowAndroidObserver& observer : observer_list_)
    observer.OnActivityStarted();
}

bool WindowAndroid::HasPermission(const std::string& permission) {
  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_hasPermission(
      env, GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, permission));
}

bool WindowAndroid::CanRequestPermission(const std::string& permission) {
  JNIEnv* env = AttachCurrentThread();
  return Java_WindowAndroid_canRequestPermission(
      env, GetJavaObject(),
      base::android::ConvertUTF8ToJavaString(env, permission));
}

WindowAndroid* WindowAndroid::GetWindowAndroid() const {
  DCHECK(parent_ == nullptr);
  return const_cast<WindowAndroid*>(this);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj, int sdk_display_id) {
  WindowAndroid* window = new WindowAndroid(env, obj, sdk_display_id);
  return reinterpret_cast<intptr_t>(window);
}

}  // namespace ui
