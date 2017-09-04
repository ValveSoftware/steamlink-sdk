// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_FEEDBACK_ANDROID_BLIMP_FEEDBACK_DATA_ANDROID_H_
#define BLIMP_CLIENT_CORE_FEEDBACK_ANDROID_BLIMP_FEEDBACK_DATA_ANDROID_H_

#include <string>
#include <unordered_map>

#include "base/android/scoped_java_ref.h"

namespace blimp {
namespace client {

// Creates a new BlimpFeedbackData Java object from the data provided in
// |feedback_data| by copying the values.
base::android::ScopedJavaLocalRef<jobject> CreateBlimpFeedbackDataJavaObject(
    const std::unordered_map<std::string, std::string>& feedback_data);

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_FEEDBACK_ANDROID_BLIMP_FEEDBACK_DATA_ANDROID_H_
