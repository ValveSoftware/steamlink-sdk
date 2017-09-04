// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_SETTINGS_ANDROID_BLIMP_SETTINGS_ANDROID_H_
#define BLIMP_CLIENT_CORE_SETTINGS_ANDROID_BLIMP_SETTINGS_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "blimp/client/core/session/identity_source.h"
#include "blimp/client/core/session/network_event_observer.h"

namespace blimp {
namespace client {

class BlimpClientContextImplAndroid;
class BlimpSettingsDelegate;

// JNI bridge between AboutBlimpPreferences.java and native code.
class BlimpSettingsAndroid : public IdentityProvider::Observer,
                             public NetworkEventObserver {
 public:
  static bool RegisterJni(JNIEnv* env);
  static BlimpSettingsAndroid* FromJavaObject(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jobj);
  BlimpSettingsAndroid(JNIEnv* env,
                       const base::android::JavaRef<jobject>& jobj);
  ~BlimpSettingsAndroid() override;

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

  void SetDelegate(BlimpSettingsDelegate* delegate);

 private:
  // Notify Java layer for user sign in state change.
  void OnSignedOut() const;
  void OnSignedIn() const;

  // Update engine info in Java, this can be either connection end point or
  // engine disconnection reason.
  void UpdateEngineInfo() const;

  // IdentityProvider::Observer implementation.
  void OnActiveAccountLogout() override;
  void OnActiveAccountLogin() override;

  // NetworkEventObserver implementation.
  void OnConnected() override;
  void OnDisconnected(int result) override;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // Delegate that provides functions needed by settings.
  BlimpSettingsDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(BlimpSettingsAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  //  BLIMP_CLIENT_CORE_SETTINGS_ANDROID_BLIMP_SETTINGS_ANDROID_H_
