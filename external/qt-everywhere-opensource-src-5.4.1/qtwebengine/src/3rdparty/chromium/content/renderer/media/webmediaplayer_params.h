// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_PARAMS_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_PARAMS_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"

namespace media {
class AudioRendererSink;
}

namespace content {

// Holds parameters for constructing WebMediaPlayerImpl without having
// to plumb arguments through various abstraction layers.
class WebMediaPlayerParams {
 public:
  // Parameters may be null.
  WebMediaPlayerParams(
      const base::Callback<void(const base::Closure&)>& defer_load_cb,
      const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink);
  ~WebMediaPlayerParams();

  base::Callback<void(const base::Closure&)> defer_load_cb() const {
    return defer_load_cb_;
  }

  const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink() const {
    return audio_renderer_sink_;
  }

 private:
  base::Callback<void(const base::Closure&)> defer_load_cb_;
  scoped_refptr<media::AudioRendererSink> audio_renderer_sink_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerParams);
};

}  // namespace media

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_PARAMS_H_
