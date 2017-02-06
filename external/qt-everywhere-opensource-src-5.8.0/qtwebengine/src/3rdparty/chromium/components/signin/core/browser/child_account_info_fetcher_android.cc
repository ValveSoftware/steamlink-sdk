// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/child_account_info_fetcher_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "jni/ChildAccountInfoFetcher_jni.h"

// static
void ChildAccountInfoFetcherAndroid::StartFetchingChildAccountInfo(
    AccountFetcherService* service,
    const std::string& account_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string account_name =
      service->account_tracker_service()->GetAccountInfo(account_id).email;
  // The AccountTrackerService may not be populated correctly in tests.
  if (account_name.empty())
    return;
  Java_ChildAccountInfoFetcher_fetch(
      env, reinterpret_cast<jlong>(service),
      base::android::ConvertUTF8ToJavaString(env, account_id).obj(),
      base::android::ConvertUTF8ToJavaString(env, account_name).obj());
}

// static
bool ChildAccountInfoFetcherAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void SetIsChildAccount(JNIEnv* env,
                       const JavaParamRef<jclass>& caller,
                       jlong native_service,
                       const JavaParamRef<jstring>& j_account_id,
                       jboolean is_child_account) {
  AccountFetcherService* service =
      reinterpret_cast<AccountFetcherService*>(native_service);
  service->SetIsChildAccount(
      base::android::ConvertJavaStringToUTF8(env, j_account_id),
      is_child_account);
}
