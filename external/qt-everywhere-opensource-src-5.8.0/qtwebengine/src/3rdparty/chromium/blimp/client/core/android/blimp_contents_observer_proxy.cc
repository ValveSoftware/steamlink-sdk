// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/android/blimp_contents_observer_proxy.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "blimp/client/core/android/blimp_contents_impl_android.h"
#include "jni/BlimpContentsObserverProxy_jni.h"

namespace blimp {
namespace client {

jlong Init(JNIEnv* env,
           const base::android::JavaParamRef<jobject>& jobj,
           const base::android::JavaParamRef<jobject>& jblimp_contents_impl) {
  BlimpContentsImplAndroid* blimp_contents_impl_android =
      BlimpContentsImplAndroid::FromJavaObject(env, jblimp_contents_impl.obj());
  CHECK(blimp_contents_impl_android);

  return reinterpret_cast<intptr_t>(
      new BlimpContentsObserverProxy(env, jobj, blimp_contents_impl_android));
}

// static
bool BlimpContentsObserverProxy::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

BlimpContentsObserverProxy::BlimpContentsObserverProxy(
    JNIEnv* env,
    jobject obj,
    BlimpContentsImplAndroid* blimp_contents_impl_android)
    : blimp_contents_impl_android_(blimp_contents_impl_android) {
  java_obj_.Reset(env, obj);
  blimp_contents_impl_android_->blimp_contents_impl()->AddObserver(this);
}

BlimpContentsObserverProxy::~BlimpContentsObserverProxy() {}

void BlimpContentsObserverProxy::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  blimp_contents_impl_android_->blimp_contents_impl()->RemoveObserver(this);
  delete this;
}

void BlimpContentsObserverProxy::OnURLUpdated(const GURL& url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj(java_obj_);
  base::android::ScopedJavaLocalRef<jstring> jstring_url(
      base::android::ConvertUTF8ToJavaString(env, url.spec()));

  Java_BlimpContentsObserverProxy_onUrlUpdated(env, obj.obj(),
                                               jstring_url.obj());
}

}  // namespace client
}  // namespace blimp
