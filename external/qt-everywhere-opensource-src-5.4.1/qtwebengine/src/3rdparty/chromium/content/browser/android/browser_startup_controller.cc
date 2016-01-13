// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/browser_startup_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/browser/android/content_startup_flags.h"

#include "jni/BrowserStartupController_jni.h"

namespace content {

bool BrowserMayStartAsynchronously() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_BrowserStartupController_browserMayStartAsynchonously(env);
}

void BrowserStartupComplete(int result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BrowserStartupController_browserStartupComplete(env, result);
}

bool RegisterBrowserStartupController(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

static void SetCommandLineFlags(JNIEnv* env,
                                jclass clazz,
                                jint max_render_process_count,
                                jstring plugin_descriptor) {
  std::string plugin_str =
      (plugin_descriptor == NULL
           ? std::string()
           : base::android::ConvertJavaStringToUTF8(env, plugin_descriptor));
  SetContentCommandLineFlags(max_render_process_count, plugin_str);
}

static jboolean IsOfficialBuild(JNIEnv* env, jclass clazz) {
#if defined(OFFICIAL_BUILD)
  return true;
#else
  return false;
#endif
}

static jboolean IsPluginEnabled(JNIEnv* env, jclass clazz) {
#if defined(ENABLE_PLUGINS)
  return true;
#else
  return false;
#endif
}

}  // namespace content
