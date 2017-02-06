// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MEDIA_METADATA_H_
#define CONTENT_PUBLIC_COMMON_MEDIA_METADATA_H_

#include "base/strings/string16.h"
#include "content/common/content_export.h"

namespace content {

// The MediaMetadata is a structure carrying information associated to a
// content::MediaSession.
struct CONTENT_EXPORT MediaMetadata {
  MediaMetadata();
  ~MediaMetadata();

  bool operator==(const MediaMetadata& other) const;
  bool operator!=(const MediaMetadata& other) const;

  // Title associated to the MediaSession.
  base::string16 title;

  // Artist associated to the MediaSession.
  base::string16 artist;

  // Album associated to the MediaSession.
  base::string16 album;

  // Maximum length for all the strings inside the MediaMetadata when it is sent
  // over IPC. The renderer process should truncate the strings before sending
  // the MediaMetadata and the browser process must do the same when receiving
  // it.
  static const size_t kMaxIPCStringLength;
};

}  // namespace content

#endif // CONTENT_PUBLIC_COMMON_MEDIA_METADATA_H_
