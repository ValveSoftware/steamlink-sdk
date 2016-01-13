// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/device_form_factor_android.h"

#include "base/android/jni_android.h"
#include "jni/DeviceFormFactor_jni.h"

namespace ui {

DeviceFormFactor GetDeviceFormFactor() {
  bool is_tablet = Java_DeviceFormFactor_isTablet(
      base::android::AttachCurrentThread(),
      base::android::GetApplicationContext());
  return is_tablet ? DEVICE_FORM_FACTOR_TABLET : DEVICE_FORM_FACTOR_PHONE;
}

bool RegisterDeviceFormFactorAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace ui
