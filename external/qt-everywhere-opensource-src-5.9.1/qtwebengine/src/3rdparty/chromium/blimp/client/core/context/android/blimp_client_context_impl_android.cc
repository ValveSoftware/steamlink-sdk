// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/context/android/blimp_client_context_impl_android.h"

#include <string>
#include <unordered_map>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "blimp/client/core/contents/blimp_contents_impl.h"
#include "blimp/client/core/feedback/android/blimp_feedback_data_android.h"
#include "blimp/client/core/settings/android/blimp_settings_android.h"
#include "blimp/client/core/settings/android/settings_android.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "jni/BlimpClientContextImpl_jni.h"
#include "ui/android/window_android.h"

namespace blimp {
namespace client {

// static
bool BlimpClientContextImplAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
BlimpClientContextImplAndroid* BlimpClientContextImplAndroid::FromJavaObject(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  return reinterpret_cast<BlimpClientContextImplAndroid*>(
      Java_BlimpClientContextImpl_getNativePtr(env, jobj));
}

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/android/dummy_client_context_android.cc should be linked
// in to any binary using BlimpClientContext::GetJavaObject.
// static
base::android::ScopedJavaLocalRef<jobject> BlimpClientContext::GetJavaObject(
    BlimpClientContext* blimp_client_context) {
  BlimpClientContextImplAndroid* blimp_client_context_impl_android =
      static_cast<BlimpClientContextImplAndroid*>(blimp_client_context);
  return blimp_client_context_impl_android->GetJavaObject();
}

BlimpClientContextImplAndroid::BlimpClientContextImplAndroid(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
    std::unique_ptr<CompositorDependencies> compositor_dependencies,
    std::unique_ptr<Settings> settings)
    : BlimpClientContextImpl(io_thread_task_runner,
                             file_thread_task_runner,
                             std::move(compositor_dependencies),
                             std::move(settings)) {
  JNIEnv* env = base::android::AttachCurrentThread();

  java_obj_.Reset(env, Java_BlimpClientContextImpl_create(
                           env, reinterpret_cast<intptr_t>(this))
                           .obj());
}

BlimpClientContextImplAndroid::~BlimpClientContextImplAndroid() {
  Java_BlimpClientContextImpl_clearNativePtr(
      base::android::AttachCurrentThread(), java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
BlimpClientContextImplAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_obj_);
}

base::android::ScopedJavaLocalRef<jobject>
BlimpClientContextImplAndroid::CreateBlimpContentsJava(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    jlong window_android_ptr) {
  ui::WindowAndroid* window_android =
      reinterpret_cast<ui::WindowAndroid*>(window_android_ptr);
  std::unique_ptr<BlimpContents> blimp_contents =
      CreateBlimpContents(window_android);

  // This intentionally releases the ownership and gives it to Java.
  BlimpContentsImpl* blimp_contents_impl =
      static_cast<BlimpContentsImpl*>(blimp_contents.release());
  return blimp_contents_impl->GetJavaObject();
}

base::android::ScopedJavaLocalRef<jobject>
BlimpClientContextImplAndroid::CreateBlimpFeedbackDataJava(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  std::unordered_map<std::string, std::string> feedback_data =
      CreateFeedbackData();
  return CreateBlimpFeedbackDataJavaObject(feedback_data);
}

GURL BlimpClientContextImplAndroid::GetAssignerURL() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> jurl =
      Java_BlimpClientContextImpl_getAssignerUrl(env, java_obj_);
  GURL assigner_url = GURL(ConvertJavaStringToUTF8(env, jurl));
  DCHECK(assigner_url.is_valid());
  return assigner_url;
}

void BlimpClientContextImplAndroid::ConnectFromJava(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  BlimpClientContextImpl::Connect();
}

void BlimpClientContextImplAndroid::InitSettingsPage(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj,
    const base::android::JavaRef<jobject>& blimp_settings) {
  BlimpSettingsAndroid* settings_android =
      BlimpSettingsAndroid::FromJavaObject(env, blimp_settings);
  DCHECK(settings_android);

  // Set the delegate for settings.
  settings_android->SetDelegate(this);
}

base::android::ScopedJavaLocalRef<jobject>
BlimpClientContextImplAndroid::GetSettings(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  return static_cast<SettingsAndroid*>(settings())->GetJavaObject();
}

}  // namespace client
}  // namespace blimp
