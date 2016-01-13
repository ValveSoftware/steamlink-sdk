// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/sys_utils.h"

#include "base/android/build_info.h"
#include "base/sys_info.h"
#include "jni/SysUtils_jni.h"

namespace base {
namespace android {

bool SysUtils::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

bool SysUtils::IsLowEndDeviceFromJni() {
  JNIEnv* env = AttachCurrentThread();
  return Java_SysUtils_isLowEndDevice(env);
}

bool SysUtils::IsLowEndDevice() {
  static bool is_low_end = IsLowEndDeviceFromJni();
  return is_low_end;
}

size_t SysUtils::AmountOfPhysicalMemoryKBFromJni() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<size_t>(Java_SysUtils_amountOfPhysicalMemoryKB(env));
}

size_t SysUtils::AmountOfPhysicalMemoryKB() {
  static size_t amount_of_ram = AmountOfPhysicalMemoryKBFromJni();
  return amount_of_ram;
}

SysUtils::SysUtils() { }

}  // namespace android
}  // namespace base
