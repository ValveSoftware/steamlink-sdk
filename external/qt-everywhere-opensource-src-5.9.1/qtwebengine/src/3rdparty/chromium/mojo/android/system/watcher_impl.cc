// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/android/system/watcher_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "jni/WatcherImpl_jni.h"
#include "mojo/public/cpp/system/handle.h"
#include "mojo/public/cpp/system/watcher.h"

namespace mojo {
namespace android {

using base::android::JavaParamRef;

namespace {

class JavaWatcherCallback {
 public:
  JavaWatcherCallback(JNIEnv* env, const JavaParamRef<jobject>& java_watcher) {
    java_watcher_.Reset(env, java_watcher);
  }

  void OnHandleReady(MojoResult result) {
    Java_WatcherImpl_onHandleReady(base::android::AttachCurrentThread(),
                                   java_watcher_, result);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_watcher_;
};

}  // namespace

static jlong CreateWatcher(JNIEnv* env, const JavaParamRef<jobject>& jcaller) {
  return reinterpret_cast<jlong>(new Watcher);
}

static jint Start(JNIEnv* env,
                  const JavaParamRef<jobject>& jcaller,
                  jlong watcher_ptr,
                  jint mojo_handle,
                  jint signals) {
  Watcher* watcher = reinterpret_cast<Watcher*>(watcher_ptr);
  return watcher->Start(
      mojo::Handle(static_cast<MojoHandle>(mojo_handle)),
      static_cast<MojoHandleSignals>(signals),
      base::Bind(&JavaWatcherCallback::OnHandleReady,
                 base::Owned(new JavaWatcherCallback(env, jcaller))));
}

static void Cancel(JNIEnv* env,
                   const JavaParamRef<jobject>& jcaller,
                   jlong watcher_ptr) {
  reinterpret_cast<Watcher*>(watcher_ptr)->Cancel();
}

static void Delete(JNIEnv* env,
                   const JavaParamRef<jobject>& jcaller,
                   jlong watcher_ptr) {
  delete reinterpret_cast<Watcher*>(watcher_ptr);
}

bool RegisterWatcherImpl(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android
}  // namespace mojo
