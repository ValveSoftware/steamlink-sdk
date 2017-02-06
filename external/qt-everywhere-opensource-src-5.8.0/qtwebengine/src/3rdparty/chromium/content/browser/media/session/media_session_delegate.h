// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_DELEGATE_H_
#define CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_DELEGATE_H_

#include "content/browser/media/session/media_session.h"

namespace content {

// MediaSessionDelegate is an interface abstracting audio focus handling for the
// MediaSession class.
class MediaSessionDelegate {
 public:
  // Factory method returning an implementation of MediaSessionDelegate.
  static std::unique_ptr<MediaSessionDelegate> Create(
      MediaSession* media_session);

  virtual bool RequestAudioFocus(MediaSession::Type type) = 0;
  virtual void AbandonAudioFocus() = 0;
};

}  // namespace content

#endif // CONTENT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_DELEGATE_H_
