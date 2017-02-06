// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/android/cast_metrics_helper_android.h"

#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "jni/CastMetricsHelper_jni.h"

namespace chromecast {

// static
bool CastMetricsHelperAndroid::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void LogMediaPlay(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPlay();
}

void LogMediaPause(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPause();
}

}  // namespace chromecast
