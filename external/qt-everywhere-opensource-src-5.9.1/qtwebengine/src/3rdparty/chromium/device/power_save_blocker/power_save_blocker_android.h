// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_POWER_SAVE_BLOCKER_POWER_SAVE_BLOCKER_ANDROID_H_
#define DEVICE_POWER_SAVE_BLOCKER_POWER_SAVE_BLOCKER_ANDROID_H_

#include <jni.h>

namespace device {

bool RegisterPowerSaveBlocker(JNIEnv* env);

}  // namespace device

#endif  // DEVICE_POWER_SAVE_BLOCKER_POWER_SAVE_BLOCKER_ANDROID_H_
