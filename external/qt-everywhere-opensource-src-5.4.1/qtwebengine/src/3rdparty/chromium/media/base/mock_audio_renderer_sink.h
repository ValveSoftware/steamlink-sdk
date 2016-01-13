// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MOCK_AUDIO_RENDERER_SINK_H_
#define MEDIA_BASE_MOCK_AUDIO_RENDERER_SINK_H_

#include "media/audio/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockAudioRendererSink : public AudioRendererSink {
 public:
  MockAudioRendererSink();

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD0(Play, void());
  MOCK_METHOD1(SetVolume, bool(double volume));

  virtual void Initialize(const AudioParameters& params,
                          RenderCallback* renderer) OVERRIDE;
  AudioRendererSink::RenderCallback* callback() { return callback_; }

 protected:
  virtual ~MockAudioRendererSink();

 private:
  RenderCallback* callback_;

  DISALLOW_COPY_AND_ASSIGN(MockAudioRendererSink);
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_AUDIO_RENDERER_SINK_H_
