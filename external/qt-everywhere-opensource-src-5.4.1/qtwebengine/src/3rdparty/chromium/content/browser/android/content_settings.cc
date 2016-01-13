// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_settings.h"

#include "base/android/jni_android.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/web_contents.h"
#include "jni/ContentSettings_jni.h"
#include "webkit/common/webpreferences.h"

namespace content {

ContentSettings::ContentSettings(JNIEnv* env,
                         jobject obj,
                         WebContents* contents)
    : WebContentsObserver(contents),
      content_settings_(env, obj) {
}

ContentSettings::~ContentSettings() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = content_settings_.get(env);
  if (obj.obj()) {
    Java_ContentSettings_onNativeContentSettingsDestroyed(env, obj.obj(),
        reinterpret_cast<intptr_t>(this));
  }
}

// static
bool ContentSettings::RegisterContentSettings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

bool ContentSettings::GetJavaScriptEnabled(JNIEnv* env, jobject obj) {
  RenderViewHost* render_view_host = web_contents()->GetRenderViewHost();
  if (!render_view_host)
    return false;
  return render_view_host->GetDelegate()->GetWebkitPrefs().javascript_enabled;
}

void ContentSettings::WebContentsDestroyed() {
  delete this;
}

static jlong Init(JNIEnv* env, jobject obj, jlong nativeContentViewCore) {
  WebContents* web_contents =
      reinterpret_cast<ContentViewCoreImpl*>(nativeContentViewCore)
          ->GetWebContents();
  ContentSettings* content_settings =
      new ContentSettings(env, obj, web_contents);
  return reinterpret_cast<intptr_t>(content_settings);
}

}  // namespace content
