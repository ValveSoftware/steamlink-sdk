// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/chromecast_config_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/lazy_instance.h"
#include "jni/ChromecastConfigAndroid_jni.h"

namespace chromecast {
namespace android {

namespace {
base::LazyInstance<ChromecastConfigAndroid> g_instance =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
ChromecastConfigAndroid* ChromecastConfigAndroid::GetInstance() {
  return g_instance.Pointer();
}

// static
bool ChromecastConfigAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ChromecastConfigAndroid::ChromecastConfigAndroid() {
}

ChromecastConfigAndroid::~ChromecastConfigAndroid() {
}

bool ChromecastConfigAndroid::CanSendUsageStats() {
  // TODO(gunsch): make opt-in.stats pref the source of truth for this data,
  // instead of Android prefs, then delete ChromecastConfigAndroid.
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ChromecastConfigAndroid_canSendUsageStats(
      env, base::android::GetApplicationContext());
}

// Registers a handler to be notified when SendUsageStats is changed.
void ChromecastConfigAndroid::SetSendUsageStatsChangedCallback(
    const base::Callback<void(bool)>& callback) {
  send_usage_stats_changed_callback_ = callback;
}

// Called from Java.
void SetSendUsageStatsEnabled(JNIEnv* env,
                              const JavaParamRef<jclass>& caller,
                              jboolean enabled) {
  ChromecastConfigAndroid::GetInstance()->
      send_usage_stats_changed_callback().Run(enabled);
}

}  // namespace android
}  // namespace chromecast
