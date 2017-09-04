// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/settings/android/blimp_settings_android.h"

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "blimp/client/core/session/connection_status.h"
#include "blimp/client/core/settings/blimp_settings_delegate.h"
#include "blimp/client/public/blimp_client_context.h"
#include "jni/AboutBlimpPreferences_jni.h"

using base::android::AttachCurrentThread;

namespace blimp {
namespace client {

// static
bool BlimpSettingsAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
BlimpSettingsAndroid* BlimpSettingsAndroid::FromJavaObject(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  return reinterpret_cast<BlimpSettingsAndroid*>(
      Java_AboutBlimpPreferences_getNativePtr(env, jobj));
}

static jlong Init(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jobj) {
  return reinterpret_cast<intptr_t>(new BlimpSettingsAndroid(env, jobj));
}

BlimpSettingsAndroid::BlimpSettingsAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj)
    : delegate_(nullptr) {
  java_obj_.Reset(jobj);
}

BlimpSettingsAndroid::~BlimpSettingsAndroid() {
  Java_AboutBlimpPreferences_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_);
  if (delegate_) {
    delegate_->GetIdentitySource()->RemoveObserver(this);
    delegate_->GetConnectionStatus()->RemoveObserver(this);
  }
}

void BlimpSettingsAndroid::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  delete this;
}

void BlimpSettingsAndroid::SetDelegate(BlimpSettingsDelegate* delegate) {
  // Set the Delegate, it can only be called for once.
  DCHECK(!delegate_ && delegate);
  delegate_ = delegate;

  // Listen to sign in state change.
  delegate_->GetIdentitySource()->AddObserver(this);

  // Listen to connection state change.
  ConnectionStatus* conn_status = delegate_->GetConnectionStatus();
  DCHECK(conn_status);
  conn_status->AddObserver(this);

  // Propagate connection info if the client is connected to the engine.
  if (conn_status->is_connected()) {
    OnConnected();
  }
}

void BlimpSettingsAndroid::OnSignedOut() const {
  Java_AboutBlimpPreferences_onSignedOut(base::android::AttachCurrentThread(),
                                         java_obj_);
}

void BlimpSettingsAndroid::OnSignedIn() const {
  Java_AboutBlimpPreferences_onSignedIn(base::android::AttachCurrentThread(),
                                        java_obj_);
}

void BlimpSettingsAndroid::UpdateEngineInfo() const {
  DCHECK(delegate_);

  JNIEnv* env = base::android::AttachCurrentThread();
  std::string engine_id =
      delegate_->GetConnectionStatus()->engine_endpoint().address().ToString();

  base::android::ScopedJavaLocalRef<jstring> jengine_id(
      base::android::ConvertUTF8ToJavaString(env, engine_id));

  Java_AboutBlimpPreferences_setEngineInfo(env, java_obj_, jengine_id);
}

void BlimpSettingsAndroid::OnActiveAccountLogout() {
  OnSignedOut();
}

void BlimpSettingsAndroid::OnActiveAccountLogin() {
  OnSignedIn();
}

void BlimpSettingsAndroid::OnConnected() {
  UpdateEngineInfo();
}

void BlimpSettingsAndroid::OnDisconnected(int result) {
  UpdateEngineInfo();
}

}  // namespace client
}  // namespace blimp
