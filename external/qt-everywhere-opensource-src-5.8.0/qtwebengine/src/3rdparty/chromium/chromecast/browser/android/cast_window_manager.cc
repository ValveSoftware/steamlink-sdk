// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/android/cast_window_manager.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "chromecast/base/pref_names.h"
#include "chromecast/browser/android/cast_window_android.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_main_parts.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_browser_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_channel.h"
#include "jni/CastWindowManager_jni.h"
#include "url/gurl.h"

namespace {

base::LazyInstance<base::android::ScopedJavaGlobalRef<jobject> >
    g_window_manager = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace chromecast {
namespace shell {

base::android::ScopedJavaLocalRef<jobject>
CreateCastWindowView(CastWindowAndroid* shell) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject j_window_manager = g_window_manager.Get().obj();
  return Java_CastWindowManager_createCastWindow(env, j_window_manager);
}

void CloseCastWindowView(jobject shell_wrapper) {
  JNIEnv* env = base::android::AttachCurrentThread();
  jobject j_window_manager = g_window_manager.Get().obj();
  Java_CastWindowManager_closeCastWindow(env, j_window_manager, shell_wrapper);
}

// Register native methods
bool RegisterCastWindowManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void Init(JNIEnv* env,
          const JavaParamRef<jclass>& clazz,
          const JavaParamRef<jobject>& obj) {
  g_window_manager.Get().Reset(obj);
}

jlong LaunchCastWindow(JNIEnv* env,
                       const JavaParamRef<jclass>& clazz,
                       const JavaParamRef<jstring>& jurl) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  return reinterpret_cast<jlong>(
      CastWindowAndroid::CreateNewWindow(
          CastBrowserProcess::GetInstance()->browser_context(),
          url));
}

void StopCastWindow(JNIEnv* env,
                    const JavaParamRef<jclass>& clazz,
                    jlong nativeCastWindow,
                    jboolean gracefully) {
  CastWindowAndroid* window =
      reinterpret_cast<CastWindowAndroid*>(nativeCastWindow);
  DCHECK(window);
  if (gracefully)
    window->Close();
  else
    window->Destroy();
}

void EnableDevTools(JNIEnv* env,
                    const JavaParamRef<jclass>& clazz,
                    jboolean enable) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CastBrowserProcess::GetInstance()->pref_service()->SetBoolean(
      prefs::kEnableRemoteDebugging, enable);
}

}  // namespace shell
}  // namespace chromecast
