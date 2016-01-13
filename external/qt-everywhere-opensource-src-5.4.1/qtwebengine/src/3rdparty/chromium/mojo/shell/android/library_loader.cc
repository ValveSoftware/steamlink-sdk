// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/logging.h"
#include "mojo/services/native_viewport/native_viewport_android.h"
#include "mojo/shell/android/mojo_main.h"
#include "net/android/net_jni_registrar.h"

namespace {

base::android::RegistrationMethod kMojoRegisteredMethods[] = {
  { "MojoMain", mojo::RegisterMojoMain },
  { "NativeViewportAndroid", mojo::services::NativeViewportAndroid::Register },
};

bool RegisterMojoJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kMojoRegisteredMethods,
                               arraysize(kMojoRegisteredMethods));
}

}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!base::android::RegisterLibraryLoaderEntryHook(env))
    return -1;

  if (!base::android::RegisterJni(env))
    return -1;

  if (!net::android::RegisterJni(env))
    return -1;

  if (!RegisterMojoJni(env))
    return -1;

  return JNI_VERSION_1_4;
}
