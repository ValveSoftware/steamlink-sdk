// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_VIBRATION_VIBRATION_PROVIDER_ANDROID_H_
#define CONTENT_BROWSER_VIBRATION_VIBRATION_PROVIDER_ANDROID_H_

#include "base/android/jni_android.h"
#include "content/public/browser/vibration_provider.h"

namespace content {

class VibrationProviderAndroid : public VibrationProvider {
 public:
  VibrationProviderAndroid();

  static bool Register(JNIEnv* env);

 private:
  virtual ~VibrationProviderAndroid();

  virtual void Vibrate(int64 milliseconds) OVERRIDE;
  virtual void CancelVibration() OVERRIDE;

  base::android::ScopedJavaGlobalRef<jobject> j_vibration_provider_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_VIBRATION_VIBRATION_PROVIDER_ANDROID_H_
