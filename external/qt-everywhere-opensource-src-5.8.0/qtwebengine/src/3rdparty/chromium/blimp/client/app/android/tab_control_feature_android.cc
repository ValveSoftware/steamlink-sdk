// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/tab_control_feature_android.h"

#include "blimp/client/app/android/blimp_client_session_android.h"
#include "blimp/client/feature/tab_control_feature.h"
#include "jni/TabControlFeature_jni.h"
#include "ui/gfx/geometry/size.h"

namespace blimp {
namespace client {

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& jobj,
                  const JavaParamRef<jobject>& blimp_client_session) {
  BlimpClientSession* client_session =
      BlimpClientSessionAndroid::FromJavaObject(env,
                                                blimp_client_session.obj());

  return reinterpret_cast<intptr_t>(
      new TabControlFeatureAndroid(env,
                                   jobj,
                                   client_session->GetTabControlFeature()));
}

// static
bool TabControlFeatureAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

TabControlFeatureAndroid::TabControlFeatureAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    TabControlFeature* tab_control_feature)
    : tab_control_feature_(tab_control_feature) {
  java_obj_.Reset(env, jobj);
}

TabControlFeatureAndroid::~TabControlFeatureAndroid() {}

void TabControlFeatureAndroid::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& jobj) {
  delete this;
}

void TabControlFeatureAndroid::OnContentAreaSizeChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint width,
    jint height,
    jfloat dp_to_px) {
  tab_control_feature_->SetSizeAndScale(gfx::Size(width, height), dp_to_px);
}

}  // namespace client
}  // namespace blimp
