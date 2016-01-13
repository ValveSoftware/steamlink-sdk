// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PRELOAD_H_
#define CONTENT_RENDERER_MEDIA_PRELOAD_H_

namespace content {

// Used to specify video preload states. They are "hints" to the browser about
// how aggressively the browser should load and buffer data.
// Please see the HTML5 spec for the descriptions of these values:
// http://www.w3.org/TR/html5/video.html#attr-media-preload
//
// Enum values must match the values in WebCore::MediaPlayer::Preload and
// there will be assertions at compile time if they do not match.
enum Preload {
  NONE,
  METADATA,
  AUTO,
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_PRELOAD_H_
