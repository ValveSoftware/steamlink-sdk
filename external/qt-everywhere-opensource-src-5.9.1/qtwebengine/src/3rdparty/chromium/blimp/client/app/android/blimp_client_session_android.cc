// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/blimp_client_session_android.h"

#include <string>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/contents/tab_control_feature.h"
#include "blimp/client/core/session/assignment_source.h"
#include "blimp/client/core/settings/settings_feature.h"
#include "blimp/net/blimp_stats.h"
#include "components/version_info/version_info.h"
#include "jni/BlimpClientSession_jni.h"
#include "net/base/net_errors.h"
#include "ui/android/window_android.h"

namespace blimp {
namespace client {
namespace {
const int kDummyTabId = 0;

GURL CreateAssignerGURL(const std::string& assigner_url) {
  GURL parsed_url(assigner_url);
  CHECK(parsed_url.is_valid());
  return parsed_url;
}

}  // namespace

static jlong Init(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& jobj,
                  const base::android::JavaParamRef<jstring>& jassigner_url,
                  jlong window_android_ptr) {
  return reinterpret_cast<intptr_t>(new BlimpClientSessionAndroid(
      env, jobj, jassigner_url, window_android_ptr));
}

// static
bool BlimpClientSessionAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
BlimpClientSessionAndroid* BlimpClientSessionAndroid::FromJavaObject(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jobj) {
  return reinterpret_cast<BlimpClientSessionAndroid*>(
      Java_BlimpClientSession_getNativePtr(env, jobj));
}

BlimpClientSessionAndroid::BlimpClientSessionAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jassigner_url,
    jlong window_android_ptr)
    : BlimpClientSession(CreateAssignerGURL(
          base::android::ConvertJavaStringToUTF8(jassigner_url))) {
  java_obj_.Reset(env, jobj);

  ui::WindowAndroid* window =
      reinterpret_cast<ui::WindowAndroid*>(window_android_ptr);
  ime_dialog_.reset(new ImeHelperDialog(window));
  GetImeFeature()->set_delegate(ime_dialog_.get());

  // Create a single tab's WebContents.
  // TODO(kmarshall): Remove this once we add tab-literacy to Blimp.
  GetTabControlFeature()->CreateTab(kDummyTabId);
}

void BlimpClientSessionAndroid::Connect(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj,
    const base::android::JavaParamRef<jstring>& jclient_auth_token) {
  std::string client_auth_token;
  if (jclient_auth_token.obj()) {
    client_auth_token =
        base::android::ConvertJavaStringToUTF8(env, jclient_auth_token);
  }

  BlimpClientSession::Connect(client_auth_token);
}

BlimpClientSessionAndroid::~BlimpClientSessionAndroid() {
  GetImeFeature()->set_delegate(nullptr);
}

void BlimpClientSessionAndroid::OnConnected() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BlimpClientSession_onConnected(env, java_obj_);
}

void BlimpClientSessionAndroid::OnDisconnected(int result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BlimpClientSession_onDisconnected(
      env, java_obj_, base::android::ConvertUTF8ToJavaString(
                          env, net::ErrorToShortString(result)));
}

void BlimpClientSessionAndroid::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  delete this;
}

void BlimpClientSessionAndroid::OnAssignmentConnectionAttempted(
    AssignmentRequestResult result,
    const Assignment& assignment) {
  // Notify the front end of the assignment result.
  std::string engine_ip = IPAddressToStringWithPort(
      assignment.engine_endpoint.address(), assignment.engine_endpoint.port());
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BlimpClientSession_onAssignmentReceived(
      env, java_obj_, static_cast<jint>(result),
      base::android::ConvertUTF8ToJavaString(env, engine_ip),
      base::android::ConvertUTF8ToJavaString(env,
                                             version_info::GetVersionNumber()));

  BlimpClientSession::OnAssignmentConnectionAttempted(result, assignment);
}

base::android::ScopedJavaLocalRef<jintArray>
BlimpClientSessionAndroid::GetDebugInfo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jobj) {
  BlimpStats* stats = BlimpStats::GetInstance();
  int metrics[] = {stats->Get(BlimpStats::BYTES_RECEIVED),
                   stats->Get(BlimpStats::BYTES_SENT),
                   stats->Get(BlimpStats::COMMIT)};
  return base::android::ToJavaIntArray(env, metrics, arraysize(metrics));
}

}  // namespace client
}  // namespace blimp
