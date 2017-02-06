// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/android_cdm_factory.h"

#include "base/strings/string_number_conversions.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_config.h"
#include "media/base/key_systems.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#include "url/gurl.h"

namespace media {

AndroidCdmFactory::AndroidCdmFactory(const CreateFetcherCB& create_fetcher_cb)
    : create_fetcher_cb_(create_fetcher_cb) {}

AndroidCdmFactory::~AndroidCdmFactory() {}

void AndroidCdmFactory::Create(
    const std::string& key_system,
    const GURL& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const LegacySessionErrorCB& legacy_session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  // Bound |cdm_created_cb| so we always fire it asynchronously.
  CdmCreatedCB bound_cdm_created_cb = BindToCurrentLoop(cdm_created_cb);

  if (!security_origin.is_valid()) {
    bound_cdm_created_cb.Run(nullptr, "Invalid origin.");
    return;
  }

  std::string error_message;

  if (!MediaDrmBridge::IsKeySystemSupported(key_system)) {
    error_message = "Key system not supported unexpectedly: " + key_system;
    NOTREACHED() << error_message;
    bound_cdm_created_cb.Run(nullptr, error_message);
    return;
  }

  MediaDrmBridge::SecurityLevel security_level =
      MediaDrmBridge::SECURITY_LEVEL_DEFAULT;
  if (key_system == kWidevineKeySystem) {
    security_level = cdm_config.use_hw_secure_codecs
                         ? MediaDrmBridge::SECURITY_LEVEL_1
                         : MediaDrmBridge::SECURITY_LEVEL_3;
  } else if (!cdm_config.use_hw_secure_codecs) {
    // Assume other key systems require hardware-secure codecs and thus do not
    // support full compositing.
    error_message =
        key_system +
        " may require use_video_overlay_for_embedded_encrypted_video";
    NOTREACHED() << error_message;
    bound_cdm_created_cb.Run(nullptr, error_message);
    return;
  }

  scoped_refptr<MediaDrmBridge> cdm(MediaDrmBridge::Create(
      key_system, security_level, create_fetcher_cb_, session_message_cb,
      session_closed_cb, legacy_session_error_cb, session_keys_change_cb,
      session_expiration_update_cb));
  if (!cdm) {
    error_message = "MediaDrmBridge cannot be created for " + key_system +
                    " with security level " + base::IntToString(security_level);
    LOG(ERROR) << error_message;
    bound_cdm_created_cb.Run(nullptr, error_message);
    return;
  }

  // Success!
  bound_cdm_created_cb.Run(cdm, "");
}

}  // namespace media
