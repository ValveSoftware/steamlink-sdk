// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_DELEGATE_ANDROID_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_DELEGATE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "content/browser/media/session/media_session_delegate.h"

namespace content {

// MediaSessionDelegateAndroid handles the audio focus at a system level on
// Android. It is also proxying the JNI calls.
class MediaSessionDelegateAndroid : public MediaSessionDelegate {
 public:
  static bool Register(JNIEnv* env);

  explicit MediaSessionDelegateAndroid(MediaSession* media_session);
  ~MediaSessionDelegateAndroid();

  void Initialize();

  bool RequestAudioFocus(MediaSession::Type type) override;
  void AbandonAudioFocus() override;

  // Called when the Android system requests the MediaSession to be suspended.
  // Called by Java through JNI.
  void OnSuspend(JNIEnv* env,
                 const base::android::JavaParamRef<jobject>& obj,
                 jboolean temporary);

  // Called when the Android system requests the MediaSession to be resumed.
  // Called by Java through JNI.
  void OnResume(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Called when the Android system requests the MediaSession to duck.
  // Called by Java through JNI.
  void OnSetVolumeMultiplier(JNIEnv* env, jobject obj,
                             jdouble volume_multiplier);

  // Called when the Android system requests the MediaSession to duck.
  // Called by Java through JNI.
  void RecordSessionDuck(JNIEnv* env,
                         const base::android::JavaParamRef<jobject> &obj);

 private:
  // Weak pointer because |this| is owned by |media_session_|.
  MediaSession* media_session_;
  base::android::ScopedJavaGlobalRef<jobject> j_media_session_delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionDelegateAndroid);
};

}  // namespace content

#endif // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_DELEGATE_ANDROID_H_
