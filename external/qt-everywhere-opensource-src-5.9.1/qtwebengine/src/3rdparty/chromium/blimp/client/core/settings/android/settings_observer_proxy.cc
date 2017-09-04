// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/settings/android/settings_observer_proxy.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "jni/SettingsObserverProxy_jni.h"

namespace blimp {
namespace client {

jlong Init(JNIEnv* env,
           const base::android::JavaParamRef<jobject>& jobj,
           const base::android::JavaParamRef<jobject>& jsettings) {
  SettingsAndroid* settings_android =
      SettingsAndroid::FromJavaObject(env, jsettings);
  CHECK(settings_android);

  return reinterpret_cast<intptr_t>(
      new SettingsObserverProxy(env, jobj, settings_android));
}

// static
bool SettingsObserverProxy::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

SettingsObserverProxy::SettingsObserverProxy(JNIEnv* env,
                                             jobject obj,
                                             SettingsAndroid* settings_android)
    : settings_android_(settings_android) {
  settings_android_->AddObserver(this);
  java_obj_.Reset(env, obj);
}

SettingsObserverProxy::~SettingsObserverProxy() {
  if (settings_android_)
    settings_android_->RemoveObserver(this);
}

void SettingsObserverProxy::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  settings_android_->RemoveObserver(this);
  delete this;
}

void SettingsObserverProxy::OnBlimpModeEnabled(bool enable) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SettingsObserverProxy_onBlimpModeEnabled(env, java_obj_, enable);
}

void SettingsObserverProxy::OnShowNetworkStatsChanged(bool enable) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SettingsObserverProxy_onShowNetworkStatsChanged(env, java_obj_, enable);
}

void SettingsObserverProxy::OnRecordWholeDocumentChanged(bool enable) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SettingsObserverProxy_onRecordWholeDocumentChanged(env, java_obj_,
                                                          enable);
}

void SettingsObserverProxy::OnRestartRequired() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SettingsObserverProxy_onRestartRequired(env, java_obj_);
}

}  // namespace client
}  // namespace blimp
