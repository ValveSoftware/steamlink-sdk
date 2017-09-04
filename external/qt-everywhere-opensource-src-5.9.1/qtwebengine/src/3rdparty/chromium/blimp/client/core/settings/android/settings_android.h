// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SETTINGS_ANDROID_SETTINGS_ANDROID_H_
#define BLIMP_CLIENT_CORE_SETTINGS_ANDROID_SETTINGS_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "blimp/client/core/settings/settings.h"
#include "components/prefs/pref_service.h"

namespace blimp {
namespace client {

class SettingsAndroid : public Settings {
 public:
  static bool RegisterJni(JNIEnv* env);
  static SettingsAndroid* FromJavaObject(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj);
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  explicit SettingsAndroid(PrefService* local_state);
  ~SettingsAndroid() override;

  void Destroy(JNIEnv* env, jobject jobj);
  void SetEnableBlimpModeWrap(JNIEnv* env, jobject jobj, bool enable);
  void SetRecordWholeDocumentWrap(JNIEnv* env, jobject jobj, bool enable);
  void SetShowNetworkStatsWrap(JNIEnv* env, jobject jobj, bool enable);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(SettingsAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SETTINGS_ANDROID_SETTINGS_ANDROID_H_
