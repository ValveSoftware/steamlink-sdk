// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MEDIA_METADATA_H_
#define CONTENT_PUBLIC_COMMON_MEDIA_METADATA_H_

#include <vector>

#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/common/manifest.h"

namespace content {

// The MediaMetadata is a structure carrying information associated to a
// content::MediaSession.
struct CONTENT_EXPORT MediaMetadata {
  // TODO(zqzhang): move |Manifest::Icon| to a common place. See
  // https://crbug.com/621859.
  using MediaImage = Manifest::Icon;

  MediaMetadata();
  ~MediaMetadata();

  MediaMetadata(const MediaMetadata& other);

  bool operator==(const MediaMetadata& other) const;
  bool operator!=(const MediaMetadata& other) const;

  // Title associated to the MediaSession.
  base::string16 title;

  // Artist associated to the MediaSession.
  base::string16 artist;

  // Album associated to the MediaSession.
  base::string16 album;

  // Artwork associated to the MediaSession.
  std::vector<MediaImage> artwork;
};

}  // namespace content

#endif // CONTENT_PUBLIC_COMMON_MEDIA_METADATA_H_
