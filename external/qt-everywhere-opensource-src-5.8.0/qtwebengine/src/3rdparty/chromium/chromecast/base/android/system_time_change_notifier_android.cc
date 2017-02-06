// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/android/system_time_change_notifier_android.h"

#include "base/android/context_utils.h"
#include "jni/SystemTimeChangeNotifierAndroid_jni.h"

namespace chromecast {

// static
bool SystemTimeChangeNotifierAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

SystemTimeChangeNotifierAndroid::SystemTimeChangeNotifierAndroid() {
}

SystemTimeChangeNotifierAndroid::~SystemTimeChangeNotifierAndroid() {
}

void SystemTimeChangeNotifierAndroid::Initialize() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_notifier_.Reset(Java_SystemTimeChangeNotifierAndroid_create(
      env, base::android::GetApplicationContext()));
  Java_SystemTimeChangeNotifierAndroid_initializeFromNative(
      env, java_notifier_.obj(), reinterpret_cast<jlong>(this));
}

void SystemTimeChangeNotifierAndroid::Finalize() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SystemTimeChangeNotifierAndroid_finalizeFromNative(
      env, java_notifier_.obj());
}

void SystemTimeChangeNotifierAndroid::OnTimeChanged(JNIEnv* env, jobject jobj) {
  NotifySystemTimeChanged();
}

}  // namespace chromecast
