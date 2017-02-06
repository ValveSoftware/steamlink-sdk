// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_client_android.h"

#include "base/logging.h"
#include "base/stl_util.h"

namespace media {

static MediaClientAndroid* g_media_client = nullptr;

void SetMediaClientAndroid(MediaClientAndroid* media_client) {
  DCHECK(!g_media_client);
  g_media_client = media_client;
}

MediaClientAndroid* GetMediaClientAndroid() {
  return g_media_client;
}

MediaClientAndroid::MediaClientAndroid() {
}

MediaClientAndroid::~MediaClientAndroid() {
}

void MediaClientAndroid::AddKeySystemUUIDMappings(KeySystemUuidMap* map) {
}

media::MediaDrmBridgeDelegate* MediaClientAndroid::GetMediaDrmBridgeDelegate(
    const std::vector<uint8_t>& scheme_uuid) {
  return nullptr;
}

}  // namespace media
