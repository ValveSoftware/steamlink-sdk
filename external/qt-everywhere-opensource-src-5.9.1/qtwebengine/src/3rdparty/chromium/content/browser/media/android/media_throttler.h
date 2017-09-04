// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_THROTTLER_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_THROTTLER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/singleton.h"

namespace content {

class MediaThrottler {
 public:
  // Called to get the singleton MediaThrottler instance.
  static MediaThrottler* GetInstance();

  virtual ~MediaThrottler();

  // Called to request the permission to decode media data. Returns true if
  // permitted, or false otherwise.
  bool RequestDecoderResources();

  // Called when a decode request finishes.
  void OnDecodeRequestFinished();

  // Resets the throttler to a fresh state.
  void Reset();

 private:
  friend struct base::DefaultSingletonTraits<MediaThrottler>;
  MediaThrottler();

  base::android::ScopedJavaGlobalRef<jobject> j_media_throttler_;
  DISALLOW_COPY_AND_ASSIGN(MediaThrottler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_THROTTLER_H_
