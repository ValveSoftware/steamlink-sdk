// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_params.h"

#include "media/base/audio_renderer_sink.h"

namespace content {

WebMediaPlayerParams::WebMediaPlayerParams(
    const base::Callback<void(const base::Closure&)>& defer_load_cb,
    const scoped_refptr<media::AudioRendererSink>& audio_renderer_sink)
    : defer_load_cb_(defer_load_cb),
      audio_renderer_sink_(audio_renderer_sink) {
}

WebMediaPlayerParams::~WebMediaPlayerParams() {}

}  // namespace content
