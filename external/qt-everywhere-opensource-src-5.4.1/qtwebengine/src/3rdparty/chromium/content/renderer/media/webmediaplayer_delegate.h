// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_DELEGATE_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_DELEGATE_H_

namespace blink {
class WebMediaPlayer;
}
namespace content {

// An interface to allow a WebMediaPlayerImpl to communicate changes of state
// to objects that need to know.
class WebMediaPlayerDelegate {
 public:
  WebMediaPlayerDelegate() {}

  // The specified player started playing media.
  virtual void DidPlay(blink::WebMediaPlayer* player) = 0;

  // The specified player stopped playing media.
  virtual void DidPause(blink::WebMediaPlayer* player) = 0;

  // The specified player was destroyed. Do not call any methods on it.
  virtual void PlayerGone(blink::WebMediaPlayer* player) = 0;

 protected:
  virtual ~WebMediaPlayerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_DELEGATE_H_
