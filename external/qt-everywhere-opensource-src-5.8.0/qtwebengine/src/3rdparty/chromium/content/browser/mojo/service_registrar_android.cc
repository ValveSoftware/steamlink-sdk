// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/service_registrar_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "content/public/browser/android/service_registry_android.h"
#include "jni/ServiceRegistrar_jni.h"

namespace content {

// static
bool ServiceRegistrarAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void ServiceRegistrarAndroid::RegisterProcessHostServices(
    ServiceRegistryAndroid* registry) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ServiceRegistrar_registerProcessHostServices(
      env, registry->GetObj().obj(), base::android::GetApplicationContext());
}

// static
void ServiceRegistrarAndroid::RegisterFrameHostServices(
    ServiceRegistryAndroid* registry) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ServiceRegistrar_registerFrameHostServices(
      env, registry->GetObj().obj(), base::android::GetApplicationContext());
}
}  // namespace content
