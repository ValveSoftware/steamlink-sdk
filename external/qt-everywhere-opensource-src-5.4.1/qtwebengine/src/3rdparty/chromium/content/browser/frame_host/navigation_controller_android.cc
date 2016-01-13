// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_controller_android.h"

#include "base/android/jni_android.h"
#include "content/public/browser/navigation_controller.h"
#include "jni/NavigationControllerImpl_jni.h"

using base::android::AttachCurrentThread;

namespace content {

// static
bool NavigationControllerAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

NavigationControllerAndroid::NavigationControllerAndroid(
    NavigationController* navigation_controller)
    : navigation_controller_(navigation_controller) {
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(env,
             Java_NavigationControllerImpl_create(
                 env, reinterpret_cast<intptr_t>(this)).obj());
}

NavigationControllerAndroid::~NavigationControllerAndroid() {
  Java_NavigationControllerImpl_destroy(AttachCurrentThread(), obj_.obj());
}

base::android::ScopedJavaLocalRef<jobject>
NavigationControllerAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

jboolean NavigationControllerAndroid::CanGoBack(JNIEnv* env, jobject obj) {
  return navigation_controller_->CanGoBack();
}

jboolean NavigationControllerAndroid::CanGoForward(JNIEnv* env,
                                                   jobject obj) {
  return navigation_controller_->CanGoForward();
}

jboolean NavigationControllerAndroid::CanGoToOffset(JNIEnv* env,
                                                    jobject obj,
                                                    jint offset) {
  return navigation_controller_->CanGoToOffset(offset);
}

void NavigationControllerAndroid::GoBack(JNIEnv* env, jobject obj) {
  navigation_controller_->GoBack();
}

void NavigationControllerAndroid::GoForward(JNIEnv* env, jobject obj) {
  navigation_controller_->GoForward();
}

void NavigationControllerAndroid::GoToOffset(JNIEnv* env,
                                             jobject obj,
                                             jint offset) {
  navigation_controller_->GoToOffset(offset);
}

void NavigationControllerAndroid::GoToNavigationIndex(JNIEnv* env,
                                                      jobject obj,
                                                      jint index) {
  navigation_controller_->GoToIndex(index);
}

}  // namespace content
