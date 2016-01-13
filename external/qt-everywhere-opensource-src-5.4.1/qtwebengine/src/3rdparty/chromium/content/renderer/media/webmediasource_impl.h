// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIASOURCE_IMPL_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIASOURCE_IMPL_H_

#include <string>
#include <vector>

#include "media/base/media_log.h"
#include "third_party/WebKit/public/platform/WebMediaSource.h"

namespace media {
class ChunkDemuxer;
}

namespace content {

class WebMediaSourceImpl : public blink::WebMediaSource {
 public:
  WebMediaSourceImpl(media::ChunkDemuxer* demuxer, media::LogCB log_cb);
  virtual ~WebMediaSourceImpl();

  // blink::WebMediaSource implementation.
  virtual AddStatus addSourceBuffer(
      const blink::WebString& type,
      const blink::WebVector<blink::WebString>& codecs,
      blink::WebSourceBuffer** source_buffer);
  virtual double duration();
  virtual void setDuration(double duration);
  virtual void markEndOfStream(EndOfStreamStatus status);
  virtual void unmarkEndOfStream();

 private:
  media::ChunkDemuxer* demuxer_;  // Owned by WebMediaPlayerImpl.
  media::LogCB log_cb_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaSourceImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIASOURCE_IMPL_H_
