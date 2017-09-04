// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaMetadataSanitizer_h
#define MediaMetadataSanitizer_h

#include "public/platform/modules/mediasession/media_session.mojom-blink.h"

namespace blink {

class ExecutionContext;
class MediaMetadata;

class MediaMetadataSanitizer {
 public:
  // Produce the sanitized metadata, which will later be sent to the
  // MediaSession mojo service.
  static blink::mojom::blink::MediaMetadataPtr sanitizeAndConvertToMojo(
      const MediaMetadata*,
      ExecutionContext*);
};

}  // namespace blink

#endif  // MediaMetadataSanitizer_h
