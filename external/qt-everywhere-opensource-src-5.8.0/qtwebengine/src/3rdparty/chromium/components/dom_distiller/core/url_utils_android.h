// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_ANDROID_H_
#define COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_ANDROID_H_

#include <jni.h>

#include <string>

namespace dom_distiller {

namespace url_utils {

namespace android {

// Register JNI methods
bool RegisterUrlUtils(JNIEnv* env);

}  // namespace android

}  // namespace url_utils

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_URL_UTILS_ANDROID_H_
