// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/vibration/vibration_provider_android.h"

#include <algorithm>

#include "content/browser/vibration/vibration_message_filter.h"
#include "content/common/view_messages.h"
#include "jni/VibrationProvider_jni.h"
#include "third_party/WebKit/public/platform/WebVibration.h"

using base::android::AttachCurrentThread;

namespace content {

VibrationProviderAndroid::VibrationProviderAndroid() {
}

VibrationProviderAndroid::~VibrationProviderAndroid() {
}

// static
bool VibrationProviderAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void VibrationProviderAndroid::Vibrate(int64 milliseconds) {
  if (j_vibration_provider_.is_null()) {
    j_vibration_provider_.Reset(
        Java_VibrationProvider_create(
            AttachCurrentThread(),
            base::android::GetApplicationContext()));
  }
  Java_VibrationProvider_vibrate(AttachCurrentThread(),
                                j_vibration_provider_.obj(),
                                milliseconds);
}

void VibrationProviderAndroid::CancelVibration() {
  // If somehow a cancel message is received before this object was
  // instantiated, it means there is no current vibration anyway. Just return.
  if (j_vibration_provider_.is_null())
    return;

  Java_VibrationProvider_cancelVibration(AttachCurrentThread(),
                                        j_vibration_provider_.obj());
}

// static
VibrationProvider* VibrationMessageFilter::CreateProvider() {
  return new VibrationProviderAndroid();
}

}  // namespace content
