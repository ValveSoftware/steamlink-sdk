// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/android/ime_helper_dialog.h"

#include "base/android/jni_string.h"
#include "base/callback_helpers.h"
#include "jni/ImeHelperDialog_jni.h"
#include "ui/android/window_android.h"

namespace blimp {
namespace client {

// static
bool ImeHelperDialog::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ImeHelperDialog::ImeHelperDialog(ui::WindowAndroid* window) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(Java_ImeHelperDialog_create(
      env, reinterpret_cast<intptr_t>(this), window->GetJavaObject()));
}

ImeHelperDialog::~ImeHelperDialog() {
  Java_ImeHelperDialog_clearNativePtr(base::android::AttachCurrentThread(),
                                      java_obj_);
}

void ImeHelperDialog::OnShowImeRequested(
    const ImeFeature::WebInputRequest& request) {
  current_request_ = request;

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK_NE(ui::TEXT_INPUT_TYPE_NONE, current_request_.input_type);
  Java_ImeHelperDialog_onShowImeRequested(
      env, java_obj_, current_request_.input_type,
      base::android::ConvertUTF8ToJavaString(env, current_request_.text));
}

void ImeHelperDialog::OnHideImeRequested() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ImeHelperDialog_onHideImeRequested(env, java_obj_);
}

void ImeHelperDialog::OnImeTextEntered(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& text,
    jboolean submit) {
  std::string text_input = base::android::ConvertJavaStringToUTF8(env, text);

  ImeFeature::WebInputResponse response;
  response.text = text_input;
  response.submit = submit;

  base::ResetAndReturn(&current_request_.show_ime_callback).Run(response);
}

}  // namespace client
}  // namespace blimp
