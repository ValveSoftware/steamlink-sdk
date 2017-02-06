// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_ANDROID_BLIMP_CONTENTS_FACTORY_H_
#define BLIMP_CLIENT_CORE_ANDROID_BLIMP_CONTENTS_FACTORY_H_

#include "base/android/jni_android.h"
#include "base/macros.h"

namespace blimp {
namespace client {

// Register the BlimpContentsFactory's native methods through JNI.
bool RegisterBlimpContentsFactoryJni(JNIEnv* env);

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_ANDROID_BLIMP_CONTENTS_FACTORY_H_
