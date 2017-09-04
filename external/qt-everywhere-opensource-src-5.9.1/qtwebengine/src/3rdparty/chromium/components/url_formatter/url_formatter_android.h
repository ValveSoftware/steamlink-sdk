// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_URL_FORMATTER_ANDROID_H_
#define COMPONENTS_URL_FORMATTER_URL_FORMATTER_ANDROID_H_

#include <jni.h>

namespace url_formatter {

namespace android {

bool RegisterUrlFormatterNatives(JNIEnv* env);

}  // namespace android

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_URL_FORMATTER_ANDROID_H_
