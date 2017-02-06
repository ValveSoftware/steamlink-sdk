// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_throttler.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "jni/MediaThrottler_jni.h"

namespace content {

// static
MediaThrottler* MediaThrottler::GetInstance() {
  return base::Singleton<MediaThrottler>::get();
}

// static
bool MediaThrottler::RegisterMediaThrottler(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

MediaThrottler::~MediaThrottler() {}

bool MediaThrottler::RequestDecoderResources() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_MediaThrottler_requestDecoderResources(
      env, j_media_throttler_.obj());
}

void MediaThrottler::OnDecodeRequestFinished() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaThrottler_onDecodeRequestFinished(env, j_media_throttler_.obj());
}

void MediaThrottler::Reset() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MediaThrottler_reset(env, j_media_throttler_.obj());
}

MediaThrottler::MediaThrottler() {
  JNIEnv* env = base::android::AttachCurrentThread();
  CHECK(env);

  j_media_throttler_.Reset(Java_MediaThrottler_create(
      env, base::android::GetApplicationContext()));
}

}  // namespace content
