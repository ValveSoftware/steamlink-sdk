// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/android/blimp_navigation_controller_impl_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "jni/BlimpNavigationControllerImpl_jni.h"

namespace blimp {
namespace client {

// static
bool BlimpNavigationControllerImplAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
BlimpNavigationControllerImplAndroid*
BlimpNavigationControllerImplAndroid::FromJavaObject(JNIEnv* env,
                                                     jobject jobj) {
  return reinterpret_cast<BlimpNavigationControllerImplAndroid*>(
      Java_BlimpNavigationControllerImpl_getNativePtr(env, jobj));
}

base::android::ScopedJavaLocalRef<jobject>
BlimpNavigationControllerImplAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

BlimpNavigationControllerImplAndroid::BlimpNavigationControllerImplAndroid(
    BlimpNavigationControllerImpl* blimp_navigation_controller_impl)
    : blimp_navigation_controller_impl_(blimp_navigation_controller_impl) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_BlimpNavigationControllerImpl_create(
                           env, reinterpret_cast<intptr_t>(this))
                           .obj());
}

BlimpNavigationControllerImplAndroid::~BlimpNavigationControllerImplAndroid() {
  Java_BlimpNavigationControllerImpl_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_.obj());
}

void BlimpNavigationControllerImplAndroid::LoadURL(
    JNIEnv* env,
    jobject jobj,
    const base::android::JavaParamRef<jstring>& jurl) {
  GURL url = GURL(base::android::ConvertJavaStringToUTF8(env, jurl));
  blimp_navigation_controller_impl_->LoadURL(url);
}

base::android::ScopedJavaLocalRef<jstring>
BlimpNavigationControllerImplAndroid::GetURL(JNIEnv* env, jobject jobj) {
  GURL url = blimp_navigation_controller_impl_->GetURL();
  return base::android::ConvertUTF8ToJavaString(env, url.spec());
}

}  // namespace client
}  // namespace blimp
