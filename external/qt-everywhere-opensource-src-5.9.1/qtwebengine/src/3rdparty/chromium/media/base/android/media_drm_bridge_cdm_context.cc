// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge_cdm_context.h"

#include "media/base/android/media_drm_bridge.h"

namespace media {

MediaDrmBridgeCdmContext::MediaDrmBridgeCdmContext(
    MediaDrmBridge* media_drm_bridge)
    : media_drm_bridge_(media_drm_bridge) {
  DCHECK(media_drm_bridge_);
}

MediaDrmBridgeCdmContext::~MediaDrmBridgeCdmContext() {}

Decryptor* MediaDrmBridgeCdmContext::GetDecryptor() {
  return nullptr;
}

int MediaDrmBridgeCdmContext::GetCdmId() const {
  return kInvalidCdmId;
}

int MediaDrmBridgeCdmContext::RegisterPlayer(
    const base::Closure& new_key_cb,
    const base::Closure& cdm_unset_cb) {
  return media_drm_bridge_->RegisterPlayer(new_key_cb, cdm_unset_cb);
}

void MediaDrmBridgeCdmContext::UnregisterPlayer(int registration_id) {
  media_drm_bridge_->UnregisterPlayer(registration_id);
}

void MediaDrmBridgeCdmContext::SetMediaCryptoReadyCB(
    const MediaCryptoReadyCB& media_crypto_ready_cb) {
  media_drm_bridge_->SetMediaCryptoReadyCB(media_crypto_ready_cb);
}

}  // namespace media
