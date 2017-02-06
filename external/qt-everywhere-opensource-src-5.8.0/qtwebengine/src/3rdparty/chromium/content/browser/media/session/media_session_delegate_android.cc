// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_delegate_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "jni/MediaSessionDelegate_jni.h"

namespace content {

// static
bool MediaSessionDelegateAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

MediaSessionDelegateAndroid::MediaSessionDelegateAndroid(
    MediaSession* media_session)
    : media_session_(media_session) {
}

MediaSessionDelegateAndroid::~MediaSessionDelegateAndroid() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  Java_MediaSessionDelegate_tearDown(env, j_media_session_delegate_.obj());
}

void MediaSessionDelegateAndroid::Initialize() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  j_media_session_delegate_.Reset(Java_MediaSessionDelegate_create(
      env,
      base::android::GetApplicationContext(),
      reinterpret_cast<intptr_t>(this)));
}

bool MediaSessionDelegateAndroid::RequestAudioFocus(MediaSession::Type type) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  return Java_MediaSessionDelegate_requestAudioFocus(
      env, j_media_session_delegate_.obj(),
      type == MediaSession::Type::Transient);
}

void MediaSessionDelegateAndroid::AbandonAudioFocus() {
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);
  Java_MediaSessionDelegate_abandonAudioFocus(
      env, j_media_session_delegate_.obj());
}

void MediaSessionDelegateAndroid::OnSuspend(
    JNIEnv*, const JavaParamRef<jobject>&, jboolean temporary) {
  // TODO(mlamouri): this check makes it so that if a MediaSession is paused and
  // then loses audio focus, it will still stay in the Suspended state.
  // See https://crbug.com/539998
  if (!media_session_->IsActive())
    return;

  if (temporary) {
    media_session_->Suspend(MediaSession::SuspendType::SYSTEM);
  } else {
    media_session_->Stop(MediaSession::SuspendType::SYSTEM);
  }
}

void MediaSessionDelegateAndroid::OnResume(
    JNIEnv*, const JavaParamRef<jobject>&) {
  if (!media_session_->IsReallySuspended())
    return;

  media_session_->Resume(MediaSession::SuspendType::SYSTEM);
}

void MediaSessionDelegateAndroid::OnSetVolumeMultiplier(
    JNIEnv*, jobject, jdouble volume_multiplier) {
  media_session_->SetVolumeMultiplier(volume_multiplier);
}

void MediaSessionDelegateAndroid::RecordSessionDuck(
    JNIEnv*, const JavaParamRef<jobject>&) {
  media_session_->RecordSessionDuck();
}

// static
std::unique_ptr<MediaSessionDelegate> MediaSessionDelegate::Create(
    MediaSession* media_session) {
  MediaSessionDelegateAndroid* delegate =
      new MediaSessionDelegateAndroid(media_session);
  delegate->Initialize();
  return std::unique_ptr<MediaSessionDelegate>(delegate);
}

}  // namespace content
