// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/feedback/android/blimp_feedback_data_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "blimp/client/core/feedback/blimp_feedback_data.h"
#include "jni/BlimpFeedbackData_jni.h"

namespace blimp {
namespace client {

base::android::ScopedJavaLocalRef<jobject> CreateBlimpFeedbackDataJavaObject(
    const std::unordered_map<std::string, std::string>& feedback_data) {
  std::vector<std::string> keys;
  std::vector<std::string> values;
  for (const auto& item : feedback_data) {
    keys.push_back(item.first);
    values.push_back(item.second);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_BlimpFeedbackData_create(
      env, base::android::ToJavaArrayOfStrings(env, keys),
      base::android::ToJavaArrayOfStrings(env, values));
}

}  // namespace client
}  // namespace blimp
