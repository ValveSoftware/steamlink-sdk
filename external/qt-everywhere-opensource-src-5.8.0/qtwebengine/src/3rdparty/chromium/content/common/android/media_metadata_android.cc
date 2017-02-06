// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/android/media_metadata_android.h"

#include "base/android/jni_string.h"
#include "content/public/common/media_metadata.h"
#include "jni/MediaMetadata_jni.h"

namespace content {

// static
base::android::ScopedJavaLocalRef<jobject>
MediaMetadataAndroid::CreateJavaObject(
    JNIEnv* env, const MediaMetadata& metadata) {
  ScopedJavaLocalRef<jstring> j_title(
      base::android::ConvertUTF16ToJavaString(env, metadata.title));
  ScopedJavaLocalRef<jstring> j_artist(
      base::android::ConvertUTF16ToJavaString(env, metadata.artist));
  ScopedJavaLocalRef<jstring> j_album(
      base::android::ConvertUTF16ToJavaString(env, metadata.album));

  return Java_MediaMetadata_create(
      env, j_title.obj(), j_artist.obj(), j_album.obj());
}

// static
bool MediaMetadataAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace content
