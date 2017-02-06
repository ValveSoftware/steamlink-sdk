// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_ANDROID_BLIMP_NAVIGATION_CONTROLLER_IMPL_ANDROID_H_
#define BLIMP_CLIENT_CORE_ANDROID_BLIMP_NAVIGATION_CONTROLLER_IMPL_ANDROID_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "blimp/client/core/blimp_contents_impl.h"

namespace blimp {
namespace client {

class BlimpNavigationControllerDelegate;

class BlimpNavigationControllerImplAndroid {
 public:
  static bool RegisterJni(JNIEnv* env);
  static BlimpNavigationControllerImplAndroid* FromJavaObject(JNIEnv* env,
                                                              jobject jobj);
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  BlimpNavigationControllerImplAndroid(
      BlimpNavigationControllerImpl* blimp_navigation_controller_impl);
  ~BlimpNavigationControllerImplAndroid();

  void LoadURL(JNIEnv* env,
               jobject jobj,
               const base::android::JavaParamRef<jstring>& jurl);
  base::android::ScopedJavaLocalRef<jstring> GetURL(JNIEnv* env, jobject jobj);

 private:
  BlimpNavigationControllerImpl* blimp_navigation_controller_impl_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(BlimpNavigationControllerImplAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_ANDROID_BLIMP_NAVIGATION_CONTROLLER_IMPL_ANDROID_H_
