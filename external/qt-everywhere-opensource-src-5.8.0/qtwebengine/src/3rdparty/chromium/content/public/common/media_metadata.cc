// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/media_metadata.h"

namespace content {

const size_t MediaMetadata::kMaxIPCStringLength = 4 * 1024;

MediaMetadata::MediaMetadata() = default;

MediaMetadata::~MediaMetadata() = default;

bool MediaMetadata::operator==(const MediaMetadata& other) const {
  return title == other.title && artist == other.artist && album == other.album;
}

bool MediaMetadata::operator!=(const MediaMetadata& other) const {
  return !this->operator==(other);
}

}  // namespace content
