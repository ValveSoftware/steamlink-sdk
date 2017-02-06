// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_ANDROID_BLIMP_CLIENT_SESSION_ANDROID_H_
#define BLIMP_CLIENT_APP_ANDROID_BLIMP_CLIENT_SESSION_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/macros.h"
#include "blimp/client/session/blimp_client_session.h"

namespace blimp {
namespace client {

class AssignmentSource;

class BlimpClientSessionAndroid : public BlimpClientSession {
 public:
  static bool RegisterJni(JNIEnv* env);
  static BlimpClientSessionAndroid* FromJavaObject(JNIEnv* env, jobject jobj);

  BlimpClientSessionAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jstring>& jassigner_url);

  // Methods called from Java via JNI.
  // |jclient_auth_token| is an OAuth2 access token created by GoogleAuthUtil.
  // See BlimpClientSession::Connect() for more information.
  void Connect(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jobj,
               const base::android::JavaParamRef<jstring>& jclient_auth_token);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

  // Returns an integer array to Java representing blimp debug statistics which
  // contain bytes received, bytes sent, number of commits in order.
  base::android::ScopedJavaLocalRef<jintArray> GetDebugInfo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj);

 private:
  ~BlimpClientSessionAndroid() override;

  // BlimpClientSession implementation.
  void OnAssignmentConnectionAttempted(AssignmentSource::Result result,
                                       const Assignment& assignment) override;

  // NetworkEventObserver implementation.
  void OnConnected() override;
  void OnDisconnected(int error_code) override;

  // Reference to the Java object which owns this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientSessionAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_ANDROID_BLIMP_CLIENT_SESSION_ANDROID_H_
