// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_audio_renderer_sink.h"

namespace media {

MockAudioRendererSink::MockAudioRendererSink() {}
MockAudioRendererSink::~MockAudioRendererSink() {}

void MockAudioRendererSink::Initialize(const AudioParameters& params,
                                       RenderCallback* renderer) {
  callback_ = renderer;
}

}  // namespace media
