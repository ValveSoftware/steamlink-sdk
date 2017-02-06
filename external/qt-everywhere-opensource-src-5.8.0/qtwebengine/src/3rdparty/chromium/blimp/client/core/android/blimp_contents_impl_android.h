// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_ANDROID_BLIMP_CONTENTS_IMPL_ANDROID_H_
#define BLIMP_CLIENT_CORE_ANDROID_BLIMP_CONTENTS_IMPL_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "blimp/client/core/android/blimp_navigation_controller_impl_android.h"
#include "blimp/client/core/blimp_contents_impl.h"

namespace blimp {
namespace client {

class BlimpContentsImplAndroid : public base::SupportsUserData::Data {
 public:
  static bool RegisterJni(JNIEnv* env);
  static BlimpContentsImplAndroid* FromJavaObject(JNIEnv* env, jobject jobj);
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  explicit BlimpContentsImplAndroid(BlimpContentsImpl* blimp_contents_impl);
  ~BlimpContentsImplAndroid() override;

  BlimpContentsImpl* blimp_contents_impl() { return blimp_contents_impl_; }

  void Destroy(JNIEnv* env, jobject jobj);

 private:
  BlimpContentsImpl* blimp_contents_impl_;
  BlimpNavigationControllerImplAndroid
      blimp_navigation_controller_impl_android_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsImplAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_ANDROID_BLIMP_CONTENTS_IMPL_ANDROID_H_
