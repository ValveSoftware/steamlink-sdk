// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/android/blimp_contents_observer_proxy.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "blimp/client/core/contents/android/blimp_contents_impl_android.h"
#include "jni/BlimpContentsObserverProxy_jni.h"

namespace blimp {
namespace client {

jlong Init(JNIEnv* env,
           const base::android::JavaParamRef<jobject>& jobj,
           const base::android::JavaParamRef<jobject>& jblimp_contents_impl) {
  BlimpContentsImplAndroid* blimp_contents_impl_android =
      BlimpContentsImplAndroid::FromJavaObject(env, jblimp_contents_impl);
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
    : BlimpContentsObserver(blimp_contents_impl_android->blimp_contents_impl()),
      blimp_contents_impl_android_(blimp_contents_impl_android) {
  java_obj_.Reset(env, obj);
}

BlimpContentsObserverProxy::~BlimpContentsObserverProxy() {}

void BlimpContentsObserverProxy::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  blimp_contents_impl_android_->blimp_contents_impl()->RemoveObserver(this);
  delete this;
}

void BlimpContentsObserverProxy::OnNavigationStateChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BlimpContentsObserverProxy_onNavigationStateChanged(env, java_obj_);
}

void BlimpContentsObserverProxy::OnLoadingStateChanged(bool loading) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BlimpContentsObserverProxy_onLoadingStateChanged(env, java_obj_,
                                                        loading);
}

void BlimpContentsObserverProxy::OnPageLoadingStateChanged(bool loading) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BlimpContentsObserverProxy_onPageLoadingStateChanged(env, java_obj_,
                                                            loading);
}

}  // namespace client
}  // namespace blimp
