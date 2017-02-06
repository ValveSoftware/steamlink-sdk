// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screen_orientation/screen_orientation_delegate_android.h"

#include "content/browser/android/content_view_core_impl.h"
#include "jni/ScreenOrientationProvider_jni.h"

namespace content {

ScreenOrientationDelegateAndroid::ScreenOrientationDelegateAndroid() {
}

ScreenOrientationDelegateAndroid::~ScreenOrientationDelegateAndroid() {
}

// static
bool ScreenOrientationDelegateAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
void ScreenOrientationDelegateAndroid::StartAccurateListening() {
  Java_ScreenOrientationProvider_startAccurateListening(
      base::android::AttachCurrentThread());
}

// static
void ScreenOrientationDelegateAndroid::StopAccurateListening() {
  Java_ScreenOrientationProvider_stopAccurateListening(
      base::android::AttachCurrentThread());
}

bool ScreenOrientationDelegateAndroid::FullScreenRequired(
    WebContents* web_contents) {
  ContentViewCoreImpl* cvc =
      ContentViewCoreImpl::FromWebContents(web_contents);
  bool fullscreen_required = cvc ? cvc->IsFullscreenRequiredForOrientationLock()
                                 : true;
  return fullscreen_required;
}

void ScreenOrientationDelegateAndroid::Lock(
    WebContents* web_contents,
    blink::WebScreenOrientationLockType lock_orientation) {
  Java_ScreenOrientationProvider_lockOrientation(
      base::android::AttachCurrentThread(), lock_orientation);
}

bool ScreenOrientationDelegateAndroid::ScreenOrientationProviderSupported() {
  // Always supported on Android
  return true;
}

void ScreenOrientationDelegateAndroid::Unlock(WebContents* web_contents) {
  Java_ScreenOrientationProvider_unlockOrientation(
      base::android::AttachCurrentThread());
}

} // namespace content
