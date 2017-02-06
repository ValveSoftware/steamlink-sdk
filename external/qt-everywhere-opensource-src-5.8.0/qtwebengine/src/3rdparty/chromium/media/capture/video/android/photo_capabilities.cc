// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/android/photo_capabilities.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "jni/PhotoCapabilities_jni.h"

using base::android::AttachCurrentThread;

namespace media {

PhotoCapabilities::PhotoCapabilities(
    base::android::ScopedJavaLocalRef<jobject> object)
    : object_(object) {}

PhotoCapabilities::~PhotoCapabilities() {}

// static
bool PhotoCapabilities::RegisterPhotoCapabilities(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

int PhotoCapabilities::getMinZoom() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMinZoom(AttachCurrentThread(),
                                           object_.obj());
}

int PhotoCapabilities::getMaxZoom() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getMaxZoom(AttachCurrentThread(),
                                           object_.obj());
}

int PhotoCapabilities::getCurrentZoom() const {
  DCHECK(!object_.is_null());
  return Java_PhotoCapabilities_getCurrentZoom(AttachCurrentThread(),
                                               object_.obj());
}

}  // namespace media
