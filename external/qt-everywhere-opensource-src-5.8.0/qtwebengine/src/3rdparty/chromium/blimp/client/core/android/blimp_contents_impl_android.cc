// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/android/blimp_contents_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "blimp/client/core/android/blimp_navigation_controller_impl_android.h"
#include "jni/BlimpContentsImpl_jni.h"

namespace blimp {
namespace client {

// static
bool BlimpContentsImplAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
BlimpContentsImplAndroid* BlimpContentsImplAndroid::FromJavaObject(
    JNIEnv* env,
    jobject jobj) {
  return reinterpret_cast<BlimpContentsImplAndroid*>(
      Java_BlimpContentsImpl_getNativePtr(env, jobj));
}

base::android::ScopedJavaLocalRef<jobject>
BlimpContentsImplAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

BlimpContentsImplAndroid::BlimpContentsImplAndroid(
    BlimpContentsImpl* blimp_contents_impl)
    : blimp_contents_impl_(blimp_contents_impl),
      blimp_navigation_controller_impl_android_(
          static_cast<BlimpNavigationControllerImpl*>(
              &(blimp_contents_impl->GetNavigationController()))) {
  JNIEnv* env = base::android::AttachCurrentThread();

  java_obj_.Reset(
      env, Java_BlimpContentsImpl_create(
               env, reinterpret_cast<intptr_t>(this),
               blimp_navigation_controller_impl_android_.GetJavaObject().obj())
               .obj());
}

void BlimpContentsImplAndroid::Destroy(JNIEnv* env, jobject jobj) {
  delete blimp_contents_impl_;
}

BlimpContentsImplAndroid::~BlimpContentsImplAndroid() {
  Java_BlimpContentsImpl_clearNativePtr(base::android::AttachCurrentThread(),
                                        java_obj_.obj());
}

}  // namespace client
}  // namespace blimp
