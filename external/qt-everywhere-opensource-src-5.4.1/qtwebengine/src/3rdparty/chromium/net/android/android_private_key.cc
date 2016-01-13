// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/android_private_key.h"

#include <vector>
#include "jni/AndroidPrivateKey_jni.h"

namespace net {
namespace android {

base::android::ScopedJavaLocalRef<jobject> GetKeyStore(
    jobject private_key_ref) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_AndroidPrivateKey_getKeyStore(
              env, private_key_ref);
}

bool RegisterAndroidPrivateKey(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace net
