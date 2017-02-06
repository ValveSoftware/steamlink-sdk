// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/service_tab_launcher/browser/android/service_tab_launcher.h"

#include "base/android/context_utils.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "jni/ServiceTabLauncher_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::GetApplicationContext;

// Called by Java when the WebContents instance for a request Id is available.
void OnWebContentsForRequestAvailable(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    jint request_id,
    const JavaParamRef<jobject>& android_web_contents) {
  service_tab_launcher::ServiceTabLauncher::GetInstance()->OnTabLaunched(
      request_id,
      content::WebContents::FromJavaWebContents(android_web_contents));
}

namespace service_tab_launcher {

// static
ServiceTabLauncher* ServiceTabLauncher::GetInstance() {
  return base::Singleton<ServiceTabLauncher>::get();
}

ServiceTabLauncher::ServiceTabLauncher() {
  java_object_.Reset(
      Java_ServiceTabLauncher_getInstance(AttachCurrentThread(),
                                          GetApplicationContext()));
}

ServiceTabLauncher::~ServiceTabLauncher() {}

void ServiceTabLauncher::LaunchTab(
    content::BrowserContext* browser_context,
    const content::OpenURLParams& params,
    const TabLaunchedCallback& callback) {
  if (!java_object_.obj()) {
    LOG(ERROR) << "No ServiceTabLauncher is available to launch a new tab.";
    callback.Run(nullptr);
    return;
  }

  WindowOpenDisposition disposition = params.disposition;
  if (disposition != NEW_WINDOW && disposition != NEW_POPUP &&
      disposition != NEW_FOREGROUND_TAB && disposition != NEW_BACKGROUND_TAB) {
    // ServiceTabLauncher can currently only launch new tabs.
    NOTIMPLEMENTED();
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> url = ConvertUTF8ToJavaString(
      env, params.url.spec());
  ScopedJavaLocalRef<jstring> referrer_url =
      ConvertUTF8ToJavaString(env, params.referrer.url.spec());
  ScopedJavaLocalRef<jstring> headers = ConvertUTF8ToJavaString(
      env, params.extra_headers);

  ScopedJavaLocalRef<jobject> post_data;

  int request_id = tab_launched_callbacks_.Add(
      new TabLaunchedCallback(callback));
  DCHECK_GE(request_id, 1);

  Java_ServiceTabLauncher_launchTab(env,
                                    java_object_.obj(),
                                    GetApplicationContext(),
                                    request_id,
                                    browser_context->IsOffTheRecord(),
                                    url.obj(),
                                    disposition,
                                    referrer_url.obj(),
                                    params.referrer.policy,
                                    headers.obj(),
                                    post_data.obj());
}

void ServiceTabLauncher::OnTabLaunched(int request_id,
                                       content::WebContents* web_contents) {
  TabLaunchedCallback* callback = tab_launched_callbacks_.Lookup(request_id);
  DCHECK(callback);

  if (callback)
    callback->Run(web_contents);

  tab_launched_callbacks_.Remove(request_id);
}

bool ServiceTabLauncher::RegisterServiceTabLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace service_tab_launcher
