// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillable_page_utils_android.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/dom_distiller/content/browser/distillable_page_utils.h"
#include "content/public/browser/web_contents.h"
#include "jni/DistillablePageUtils_jni.h"

using base::android::ScopedJavaGlobalRef;

namespace dom_distiller {
namespace android {
namespace {
void OnIsPageDistillableResult(
    std::unique_ptr<ScopedJavaGlobalRef<jobject>> callback_holder,
    bool isDistillable) {
  Java_DistillablePageUtils_callOnIsPageDistillableResult(
      base::android::AttachCurrentThread(), callback_holder->obj(),
      isDistillable);
}

void OnIsPageDistillableUpdate(
    ScopedJavaGlobalRef<jobject>* callback_holder,
    bool isDistillable, bool isLast) {
  Java_DistillablePageUtils_callOnIsPageDistillableUpdate(
      base::android::AttachCurrentThread(), callback_holder->obj(),
      isDistillable, isLast);
}
}  // namespace

static void IsPageDistillable(JNIEnv* env,
                              const JavaParamRef<jclass>& jcaller,
                              const JavaParamRef<jobject>& webContents,
                              jboolean is_mobile_optimized,
                              const JavaParamRef<jobject>& callback) {
  content::WebContents* web_contents(
      content::WebContents::FromJavaWebContents(webContents));
  std::unique_ptr<ScopedJavaGlobalRef<jobject>> callback_holder(
      new ScopedJavaGlobalRef<jobject>());
  callback_holder->Reset(env, callback);

  if (!web_contents) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(OnIsPageDistillableResult,
                              base::Passed(&callback_holder), false));
    return;
  }
  IsDistillablePage(
      web_contents, is_mobile_optimized,
      base::Bind(OnIsPageDistillableResult, base::Passed(&callback_holder)));
}

static void SetDelegate(JNIEnv* env,
                        const JavaParamRef<jclass>& jcaller,
                        const JavaParamRef<jobject>& webContents,
                        const JavaParamRef<jobject>& callback) {
  content::WebContents* web_contents(
      content::WebContents::FromJavaWebContents(webContents));
  if (!web_contents) {
    return;
  }

  // TODO(wychen): check memory management
  ScopedJavaGlobalRef<jobject>* callback_holder(
      new ScopedJavaGlobalRef<jobject>());
  callback_holder->Reset(env, callback);

  DistillabilityDelegate delegate =
      base::Bind(OnIsPageDistillableUpdate, base::Owned(callback_holder));
  setDelegate(web_contents, delegate);
}

bool RegisterDistillablePageUtils(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}
}
