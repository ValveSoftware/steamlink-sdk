// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_ANDROID_CAST_METRICS_HELPER_ANDROID_H_
#define CHROMECAST_ANDROID_CAST_METRICS_HELPER_ANDROID_H_

#include <jni.h>
#include <vector>

#include "base/macros.h"

namespace chromecast {

class CastMetricsHelperAndroid {
 public:
  // Registers the JNI methods for CastMetricsHelperAndroid.
  static bool RegisterJni(JNIEnv* env);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CastMetricsHelperAndroid);
};

}  // namespace chromecast

#endif  // CHROMECAST_ANDROID_CAST_METRICS_HELPER_ANDROID_H_
