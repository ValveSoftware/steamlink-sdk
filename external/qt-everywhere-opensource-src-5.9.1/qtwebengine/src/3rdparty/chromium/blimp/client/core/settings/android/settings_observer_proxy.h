// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SETTINGS_ANDROID_SETTINGS_OBSERVER_PROXY_H_
#define BLIMP_CLIENT_CORE_SETTINGS_ANDROID_SETTINGS_OBSERVER_PROXY_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "blimp/client/core/settings/android/settings_android.h"
#include "blimp/client/core/settings/settings_observer.h"

namespace blimp {
namespace client {

class SettingsObserverProxy : public SettingsObserver {
 public:
  static bool RegisterJni(JNIEnv* env);

  SettingsObserverProxy(JNIEnv* env,
                        jobject obj,
                        SettingsAndroid* settings_android);
  ~SettingsObserverProxy() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // SettingsObserver implementation.
  void OnBlimpModeEnabled(bool enable) override;
  void OnShowNetworkStatsChanged(bool enable) override;
  void OnRecordWholeDocumentChanged(bool enable) override;
  void OnRestartRequired() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  SettingsAndroid* settings_android_;

  DISALLOW_COPY_AND_ASSIGN(SettingsObserverProxy);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_SETTINGS_ANDROID_SETTINGS_OBSERVER_PROXY_H_
