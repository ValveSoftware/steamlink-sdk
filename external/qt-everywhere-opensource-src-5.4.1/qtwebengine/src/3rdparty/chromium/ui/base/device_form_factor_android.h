// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DEVICE_FORM_FACTOR_ANDROID_H_
#define UI_BASE_DEVICE_FORM_FACTOR_ANDROID_H_

#include <jni.h>

#include "ui/base/device_form_factor.h"

namespace ui {

bool RegisterDeviceFormFactorAndroid(JNIEnv* env);

}  // namespace ui

#endif  // UI_BASE_DEVICE_FORM_FACTOR_ANDROID_H_
