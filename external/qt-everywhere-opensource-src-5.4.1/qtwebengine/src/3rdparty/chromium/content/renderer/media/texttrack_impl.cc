// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/texttrack_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/renderer/media/webinbandtexttrack_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "third_party/WebKit/public/platform/WebInbandTextTrackClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"

namespace content {

TextTrackImpl::TextTrackImpl(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    blink::WebMediaPlayerClient* client,
    scoped_ptr<WebInbandTextTrackImpl> text_track)
    : message_loop_(message_loop),
      client_(client),
      text_track_(text_track.Pass()) {
  client_->addTextTrack(text_track_.get());
}

TextTrackImpl::~TextTrackImpl() {
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&TextTrackImpl::OnRemoveTrack,
                 client_,
                 base::Passed(&text_track_)));
}

void TextTrackImpl::addWebVTTCue(const base::TimeDelta& start,
                                 const base::TimeDelta& end,
                                 const std::string& id,
                                 const std::string& content,
                                 const std::string& settings) {
  message_loop_->PostTask(
    FROM_HERE,
    base::Bind(&TextTrackImpl::OnAddCue,
                text_track_.get(),
                start, end,
                id, content, settings));
}

void TextTrackImpl::OnAddCue(WebInbandTextTrackImpl* text_track,
                             const base::TimeDelta& start,
                             const base::TimeDelta& end,
                             const std::string& id,
                             const std::string& content,
                             const std::string& settings) {
  if (blink::WebInbandTextTrackClient* client = text_track->client()) {
    client->addWebVTTCue(start.InSecondsF(),
                         end.InSecondsF(),
                         blink::WebString::fromUTF8(id),
                         blink::WebString::fromUTF8(content),
                         blink::WebString::fromUTF8(settings));
  }
}

void TextTrackImpl::OnRemoveTrack(
    blink::WebMediaPlayerClient* client,
    scoped_ptr<WebInbandTextTrackImpl> text_track) {
  if (text_track->client())
    client->removeTextTrack(text_track.get());
}

}  // namespace content
