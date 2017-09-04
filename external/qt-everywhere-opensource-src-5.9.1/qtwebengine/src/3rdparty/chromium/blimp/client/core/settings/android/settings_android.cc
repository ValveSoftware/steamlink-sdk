// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/settings/android/settings_android.h"

#include "base/android/jni_android.h"
#include "jni/Settings_jni.h"

namespace blimp {
namespace client {

// static
bool SettingsAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
SettingsAndroid* SettingsAndroid::FromJavaObject(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  return reinterpret_cast<SettingsAndroid*>(
      Java_Settings_getNativePtr(env, jobj));
}

base::android::ScopedJavaLocalRef<jobject> SettingsAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

SettingsAndroid::SettingsAndroid(PrefService* local_state)
    : Settings(local_state) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env, Java_Settings_create(env, reinterpret_cast<intptr_t>(this)).obj());
}

SettingsAndroid::~SettingsAndroid() {
  Java_Settings_onNativeDestroyed(base::android::AttachCurrentThread(),
                                  java_obj_);
}

void SettingsAndroid::SetEnableBlimpModeWrap(JNIEnv* env,
                                             jobject jobj,
                                             bool enable) {
  SetEnableBlimpMode(enable);
}

void SettingsAndroid::SetRecordWholeDocumentWrap(JNIEnv* env,
                                                 jobject jobj,
                                                 bool enable) {
  SetRecordWholeDocument(enable);
}

void SettingsAndroid::SetShowNetworkStatsWrap(JNIEnv* env,
                                              jobject jobj,
                                              bool enable) {
  SetShowNetworkStats(enable);
}

}  // namespace client
}  // namespace blimp
