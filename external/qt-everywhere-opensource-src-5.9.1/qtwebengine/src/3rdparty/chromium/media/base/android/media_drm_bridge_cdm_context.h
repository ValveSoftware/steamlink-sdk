// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CDM_CONTEXT_H_
#define MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CDM_CONTEXT_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "media/base/cdm_context.h"
#include "media/base/media_export.h"
#include "media/base/player_tracker.h"

namespace media {

class MediaDrmBridge;

// The CdmContext implementation for MediaDrmBridge. MediaDrmBridge supports
// neither Decryptor nor CDM ID, but uses MediaCrypto to connect to MediaCodec.
// MediaCodec-based decoders should cast the given CdmContext to this class to
// access APIs defined in this class.
//
// Methods can be called on any thread. The registered callbacks will be fired
// on the thread |media_drm_bridge_| is running on. The caller should make sure
// that the callbacks are posted to the correct thread.
//
// TODO(xhwang): Remove PlayerTracker interface.
class MEDIA_EXPORT MediaDrmBridgeCdmContext : public CdmContext,
                                              public PlayerTracker {
 public:
  using JavaObjectPtr =
      std::unique_ptr<base::android::ScopedJavaGlobalRef<jobject>>;

  // Notification called when MediaCrypto object is ready.
  // Parameters:
  // |media_crypto| - reference to MediaCrypto object
  // |needs_protected_surface| - true if protected surface is required
  using MediaCryptoReadyCB = base::Callback<void(JavaObjectPtr media_crypto,
                                                 bool needs_protected_surface)>;

  // The |media_drm_bridge| owns |this| and is guaranteed to outlive |this|.
  explicit MediaDrmBridgeCdmContext(MediaDrmBridge* media_drm_bridge);

  ~MediaDrmBridgeCdmContext() final;

  // CdmContext implementation.
  Decryptor* GetDecryptor() final;
  int GetCdmId() const final;

  // PlayerTracker implementation.
  // Methods can be called on any thread. The registered callbacks will be fired
  // on |task_runner_|. The caller should make sure that the callbacks are
  // posted to the correct thread.
  //
  // Note: RegisterPlayer() must be called before SetMediaCryptoReadyCB() to
  // avoid missing any new key notifications.
  int RegisterPlayer(const base::Closure& new_key_cb,
                     const base::Closure& cdm_unset_cb) final;
  void UnregisterPlayer(int registration_id) final;

  void SetMediaCryptoReadyCB(const MediaCryptoReadyCB& media_crypto_ready_cb);

 private:
  MediaDrmBridge* const media_drm_bridge_;

  DISALLOW_COPY_AND_ASSIGN(MediaDrmBridgeCdmContext);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_DRM_BRIDGE_CDM_CONTEXT_H_
