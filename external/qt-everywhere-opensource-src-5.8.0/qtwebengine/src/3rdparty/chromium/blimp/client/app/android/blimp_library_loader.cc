// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/android/blimp_library_loader.h"

#include <vector>

#include "base/android/base_jni_onload.h"
#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "blimp/client/app/android/blimp_jni_registrar.h"
#include "blimp/client/app/blimp_startup.h"
#include "jni/BlimpLibraryLoader_jni.h"
#include "net/android/net_jni_registrar.h"
#include "ui/gl/gl_surface.h"

namespace {

bool OnLibrariesLoaded(JNIEnv* env, jclass clazz) {
  blimp::client::InitializeLogging();
  return true;
}

bool OnJniInitializationComplete() {
  base::android::SetLibraryLoadedHook(&OnLibrariesLoaded);
  return true;
}

bool RegisterJni(JNIEnv* env) {
  if (!base::android::RegisterJni(env))
    return false;

  if (!net::android::RegisterJni(env))
    return false;

  if (!blimp::client::RegisterBlimpJni(env))
    return false;

  return true;
}

}  // namespace

namespace blimp {
namespace client {

static jboolean StartBlimp(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  if (!InitializeMainMessageLoop())
    return false;

  base::MessageLoopForUI::current()->Start();

  return true;
}

bool RegisterBlimpLibraryLoaderJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace client
}  // namespace blimp

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  std::vector<base::android::RegisterCallback> register_callbacks;
  register_callbacks.push_back(base::Bind(&RegisterJni));

  std::vector<base::android::InitCallback> init_callbacks;
  init_callbacks.push_back(base::Bind(&OnJniInitializationComplete));

  // Although we only need to register JNI for base/ and blimp/, this follows
  // the general Chrome for Android pattern, to be future-proof against future
  // changes to JNI.
  if (!base::android::OnJNIOnLoadRegisterJNI(vm, register_callbacks) ||
      !base::android::OnJNIOnLoadInit(init_callbacks)) {
    return -1;
  }

  return JNI_VERSION_1_4;
}
