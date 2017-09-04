// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/context/android/dummy_blimp_client_context_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "blimp/client/public/blimp_client_context.h"
#include "jni/DummyBlimpClientContext_jni.h"

namespace blimp {
namespace client {

// static
DummyBlimpClientContextAndroid* DummyBlimpClientContextAndroid::FromJavaObject(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  return reinterpret_cast<DummyBlimpClientContextAndroid*>(
      Java_DummyBlimpClientContext_getNativePtr(env, jobj));
}

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/android/blimp_blimp_client_context_impl_android.cc should
// be linked in to any binary using BlimpClientContext::GetJavaObject.
// static
base::android::ScopedJavaLocalRef<jobject> BlimpClientContext::GetJavaObject(
    BlimpClientContext* blimp_client_context) {
  DummyBlimpClientContextAndroid* dummy_client_context_android =
      static_cast<DummyBlimpClientContextAndroid*>(blimp_client_context);
  return dummy_client_context_android->GetJavaObject();
}

DummyBlimpClientContextAndroid::DummyBlimpClientContextAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();

  java_obj_.Reset(env, Java_DummyBlimpClientContext_create(
                           env, reinterpret_cast<intptr_t>(this))
                           .obj());
}

DummyBlimpClientContextAndroid::~DummyBlimpClientContextAndroid() {
  Java_DummyBlimpClientContext_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
DummyBlimpClientContextAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

}  // namespace client
}  // namespace blimp
