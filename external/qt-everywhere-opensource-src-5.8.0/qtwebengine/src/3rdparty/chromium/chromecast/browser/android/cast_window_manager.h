// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ANDROID_CAST_WINDOW_MANAGER_H_
#define CHROMECAST_BROWSER_ANDROID_CAST_WINDOW_MANAGER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

class CastWindowAndroid;

namespace content {
class BrowserContext;
}

namespace chromecast {
namespace shell {

// Given a CastWindowAndroid instance, creates and returns a Java wrapper.
base::android::ScopedJavaLocalRef<jobject>
CreateCastWindowView(CastWindowAndroid* shell);

// Closes a previously created Java wrapper.
void CloseCastWindowView(jobject shell_wrapper);

// Registers the CastWindowManager native methods.
bool RegisterCastWindowManager(JNIEnv* env);

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ANDROID_CAST_WINDOW_MANAGER_H_
