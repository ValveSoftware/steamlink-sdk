// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_DRM_CREDENTIAL_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_DRM_CREDENTIAL_MANAGER_H_

#include <jni.h>
#include <string>

#include "base/callback.h"
#include "base/memory/singleton.h"
#include "media/base/android/media_drm_bridge.h"

namespace content {

// This class manages the media DRM credentials on Android.
class MediaDrmCredentialManager {
 public:
  static MediaDrmCredentialManager* GetInstance();

  typedef base::Callback<void(bool)> ResetCredentialsCB;

  // Called to reset the DRM credentials. (for Java)
  static void ResetCredentials(JNIEnv* env, jclass clazz, jobject callback);

  // Called to reset the DRM credentials. The result is returned in the
  // |reset_credentials_cb|.
  void ResetCredentials(const ResetCredentialsCB& reset_credentials_cb);

  static bool RegisterMediaDrmCredentialManager(JNIEnv* env);

 private:
  friend struct DefaultSingletonTraits<MediaDrmCredentialManager>;
  friend class Singleton<MediaDrmCredentialManager>;
  typedef media::MediaDrmBridge::SecurityLevel SecurityLevel;

  MediaDrmCredentialManager();
  ~MediaDrmCredentialManager();

  // Callback function passed to MediaDrmBridge. It is called when credentials
  // reset is completed.
  void OnResetCredentialsCompleted(SecurityLevel security_level, bool success);

  // Resets DRM credentials for a particular |security_level|. Returns false if
  // we fail to create the MediaDrmBridge at all, in which case we cannot reset
  // the credentials. Otherwise, the result is returned asynchronously in
  // OnResetCredentialsCompleted() function.
  bool ResetCredentialsInternal(SecurityLevel security_level);

  // The MediaDrmBridge object used to perform the credential reset.
  scoped_ptr<media::MediaDrmBridge> media_drm_bridge_;

  // The callback provided by the caller.
  ResetCredentialsCB reset_credentials_cb_;

  DISALLOW_COPY_AND_ASSIGN(MediaDrmCredentialManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_DRM_CREDENTIAL_MANAGER_H_
