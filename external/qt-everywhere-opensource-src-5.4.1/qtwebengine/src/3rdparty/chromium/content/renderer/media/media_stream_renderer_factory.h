// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_RENDERER_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_RENDERER_FACTORY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/renderer/media/media_stream_audio_renderer.h"
#include "content/renderer/media/video_frame_provider.h"
#include "url/gurl.h"

namespace content {

// MediaStreamRendererFactory is used by WebMediaPlayerMS to create audio and
// video feeds from a MediaStream provided an URL.
// The factory methods are virtual in order for blink layouttests to be able to
// override them.
class CONTENT_EXPORT MediaStreamRendererFactory {
 public:
  MediaStreamRendererFactory();
  virtual ~MediaStreamRendererFactory();

  virtual scoped_refptr<VideoFrameProvider> GetVideoFrameProvider(
      const GURL& url,
      const base::Closure& error_cb,
      const VideoFrameProvider::RepaintCB& repaint_cb);

  virtual scoped_refptr<MediaStreamAudioRenderer> GetAudioRenderer(
      const GURL& url,
      int render_view_id,
      int render_frame_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamRendererFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_RENDERER_FACTORY_H_
