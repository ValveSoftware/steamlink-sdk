// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/android/blimp_contents_impl_android.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/ptr_util.h"
#include "blimp/client/core/contents/android/blimp_navigation_controller_impl_android.h"
#include "blimp/client/core/contents/android/blimp_view.h"
#include "blimp/client/core/contents/blimp_contents_view_impl_android.h"
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
    const base::android::JavaRef<jobject>& jobj) {
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
  BlimpView* blimp_view = static_cast<BlimpContentsViewImplAndroid*>(
                              blimp_contents_impl_->GetView())
                              ->GetBlimpView();

  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env,
                  Java_BlimpContentsImpl_create(
                      env, reinterpret_cast<intptr_t>(this),
                      blimp_navigation_controller_impl_android_.GetJavaObject(),
                      blimp_view->GetJavaObject())
                      .obj());
}

void BlimpContentsImplAndroid::Destroy(JNIEnv* env, jobject jobj) {
  delete blimp_contents_impl_;
}

void BlimpContentsImplAndroid::Show(JNIEnv* env, jobject jobj) {
  blimp_contents_impl_->Show();
}

void BlimpContentsImplAndroid::Hide(JNIEnv* env, jobject jobj) {
  blimp_contents_impl_->Hide();
}

BlimpContentsImplAndroid::~BlimpContentsImplAndroid() {
  Java_BlimpContentsImpl_clearNativePtr(base::android::AttachCurrentThread(),
                                        java_obj_);
}

}  // namespace client
}  // namespace blimp
