// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CLIENT_ANDROID_H_
#define MEDIA_BASE_ANDROID_MEDIA_CLIENT_ANDROID_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "media/base/media_export.h"

namespace media {

class MediaClientAndroid;
class MediaDrmBridgeDelegate;

// Setter for MediaClientAndroid. This should be called in all processes where
// we want to run media Android code. Also it should be called before any media
// playback could occur.
MEDIA_EXPORT void SetMediaClientAndroid(MediaClientAndroid* media_client);

#if defined(MEDIA_IMPLEMENTATION)
// Getter for the client. Returns nullptr if no customized client is needed.
MediaClientAndroid* GetMediaClientAndroid();
#endif

using UUID = std::vector<uint8_t>;

// A client interface for embedders (e.g. content/browser or content/gpu) to
// provide customized additions to Android's media handling.
class MEDIA_EXPORT MediaClientAndroid {
 public:
  typedef base::hash_map<std::string, UUID> KeySystemUuidMap;

  MediaClientAndroid();
  virtual ~MediaClientAndroid();

  // Adds extra mappings from key-system name to Android UUID into |map|.
  virtual void AddKeySystemUUIDMappings(KeySystemUuidMap* map);

  // Returns a MediaDrmBridgeDelegate that corresponds to |scheme_uuid|.
  // MediaClientAndroid retains ownership.
  virtual media::MediaDrmBridgeDelegate* GetMediaDrmBridgeDelegate(
      const UUID& scheme_uuid);

 private:
  friend class KeySystemManager;

  base::hash_map<std::string, UUID> key_system_uuid_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaClientAndroid);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CLIENT_ANDROID_H_
