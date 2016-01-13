// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "mojo/android/javatests/mojo_test_case.h"
#include "mojo/android/system/core_impl.h"

namespace {

base::android::RegistrationMethod kMojoRegisteredMethods[] = {
  { "CoreImpl", mojo::android::RegisterCoreImpl },
  { "MojoTestCase", mojo::android::RegisterMojoTestCase },
};

bool RegisterMojoJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kMojoRegisteredMethods,
                               arraysize(kMojoRegisteredMethods));
}

}  // namespace

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!base::android::RegisterLibraryLoaderEntryHook(env))
    return -1;

  if (!base::android::RegisterJni(env))
    return -1;

  if (!RegisterMojoJni(env))
    return -1;

  return JNI_VERSION_1_4;
}
