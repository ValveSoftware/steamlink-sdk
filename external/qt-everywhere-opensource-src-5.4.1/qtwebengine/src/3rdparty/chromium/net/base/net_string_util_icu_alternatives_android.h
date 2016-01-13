// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_STRING_UTIL_ICU_ALTERNATIVES_ANDROID_H_
#define NET_BASE_NET_STRING_UTIL_ICU_ALTERNATIVES_ANDROID_H_

#include <jni.h>

namespace net {

// Explicitly register static JNI functions needed when not using ICU.
bool RegisterNetStringUtils(JNIEnv* env);

}  // namespace net

#endif  // NET_BASE_NET_STRING_UTIL_ICU_ALTERNATIVES_ANDROID_H_

