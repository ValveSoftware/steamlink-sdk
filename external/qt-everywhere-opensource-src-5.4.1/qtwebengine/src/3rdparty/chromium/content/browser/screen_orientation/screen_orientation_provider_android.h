// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/compiler_specific.h"
#include "content/browser/screen_orientation/screen_orientation_provider.h"

namespace content {

class ScreenOrientationProviderAndroid : public ScreenOrientationProvider {
 public:
  ScreenOrientationProviderAndroid();

  static bool Register(JNIEnv* env);

  // ScreenOrientationProvider
  virtual void LockOrientation(blink::WebScreenOrientationLockType) OVERRIDE;
  virtual void UnlockOrientation() OVERRIDE;

 private:
  virtual ~ScreenOrientationProviderAndroid();

  base::android::ScopedJavaGlobalRef<jobject> j_screen_orientation_provider_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationProviderAndroid);
};

} // namespace content

#endif // CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_ANDROID_H_
