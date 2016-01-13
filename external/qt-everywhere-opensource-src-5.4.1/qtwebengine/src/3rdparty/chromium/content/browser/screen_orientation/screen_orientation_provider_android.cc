// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_provider_android.h"

#include "content/browser/screen_orientation/screen_orientation_dispatcher_host.h"
#include "jni/ScreenOrientationProvider_jni.h"

namespace content {

ScreenOrientationProviderAndroid::ScreenOrientationProviderAndroid() {
}

ScreenOrientationProviderAndroid::~ScreenOrientationProviderAndroid() {
}

// static
bool ScreenOrientationProviderAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void ScreenOrientationProviderAndroid::LockOrientation(
    blink::WebScreenOrientationLockType orientation) {
  if (j_screen_orientation_provider_.is_null()) {
    j_screen_orientation_provider_.Reset(Java_ScreenOrientationProvider_create(
        base::android::AttachCurrentThread()));
  }

  Java_ScreenOrientationProvider_lockOrientation(
      base::android::AttachCurrentThread(),
      j_screen_orientation_provider_.obj(), orientation);
}

void ScreenOrientationProviderAndroid::UnlockOrientation() {
  // j_screen_orientation_provider_ is set when locking. If the screen
  // orientation was not locked, unlocking should be a no-op.
  if (j_screen_orientation_provider_.is_null())
    return;

  Java_ScreenOrientationProvider_unlockOrientation(
      base::android::AttachCurrentThread(),
      j_screen_orientation_provider_.obj());
}

// static
ScreenOrientationProvider* ScreenOrientationDispatcherHost::CreateProvider() {
  return new ScreenOrientationProviderAndroid();
}

} // namespace content
