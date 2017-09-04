// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_ANDROID_VARIATIONS_ASSOCIATED_DATA_ANDROID_H_
#define COMPONENTS_VARIATIONS_ANDROID_VARIATIONS_ASSOCIATED_DATA_ANDROID_H_

#include <jni.h>

#include <string>

namespace variations {

namespace android {

// Register JNI methods for VariationsAssociatedData.
bool RegisterVariationsAssociatedData(JNIEnv* env);

}  // namespace android

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_ANDROID_VARIATIONS_ASSOCIATED_DATA_ANDROID_H_
