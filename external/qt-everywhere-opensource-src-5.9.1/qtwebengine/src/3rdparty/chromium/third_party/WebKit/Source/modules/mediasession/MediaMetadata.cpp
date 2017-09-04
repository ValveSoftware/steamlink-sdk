// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaMetadata.h"

#include "core/dom/ExecutionContext.h"
#include "modules/mediasession/MediaImage.h"
#include "modules/mediasession/MediaMetadataInit.h"

namespace blink {

// static
MediaMetadata* MediaMetadata::create(ExecutionContext* context,
                                     const MediaMetadataInit& metadata) {
  return new MediaMetadata(context, metadata);
}

MediaMetadata::MediaMetadata(ExecutionContext* context,
                             const MediaMetadataInit& metadata) {
  m_title = metadata.title();
  m_artist = metadata.artist();
  m_album = metadata.album();
  for (const auto& image : metadata.artwork())
    m_artwork.append(MediaImage::create(context, image));
}

String MediaMetadata::title() const {
  return m_title;
}

String MediaMetadata::artist() const {
  return m_artist;
}

String MediaMetadata::album() const {
  return m_album;
}

const HeapVector<Member<MediaImage>>& MediaMetadata::artwork() const {
  return m_artwork;
}

DEFINE_TRACE(MediaMetadata) {
  visitor->trace(m_artwork);
}

}  // namespace blink
