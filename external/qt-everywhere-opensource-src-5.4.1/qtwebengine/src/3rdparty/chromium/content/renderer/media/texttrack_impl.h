// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_TEXTTRACK_IMPL_H_
#define CONTENT_RENDERER_MEDIA_TEXTTRACK_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/text_track.h"

namespace base {
class MessageLoopProxy;
}

namespace blink {
class WebInbandTextTrackClient;
class WebMediaPlayerClient;
}

namespace content {

class WebInbandTextTrackImpl;

class TextTrackImpl : public media::TextTrack {
 public:
  // Constructor assumes ownership of the |text_track| object.
  TextTrackImpl(const scoped_refptr<base::MessageLoopProxy>& message_loop,
                blink::WebMediaPlayerClient* client,
                scoped_ptr<WebInbandTextTrackImpl> text_track);

  virtual ~TextTrackImpl();

  virtual void addWebVTTCue(const base::TimeDelta& start,
                            const base::TimeDelta& end,
                            const std::string& id,
                            const std::string& content,
                            const std::string& settings) OVERRIDE;

 private:
  static void OnAddCue(WebInbandTextTrackImpl* text_track,
                       const base::TimeDelta& start,
                       const base::TimeDelta& end,
                       const std::string& id,
                       const std::string& content,
                       const std::string& settings);

  static void OnRemoveTrack(blink::WebMediaPlayerClient* client,
                            scoped_ptr<WebInbandTextTrackImpl> text_track);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  blink::WebMediaPlayerClient* client_;
  scoped_ptr<WebInbandTextTrackImpl> text_track_;
  DISALLOW_COPY_AND_ASSIGN(TextTrackImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_TEXTTRACK_IMPL_H_

