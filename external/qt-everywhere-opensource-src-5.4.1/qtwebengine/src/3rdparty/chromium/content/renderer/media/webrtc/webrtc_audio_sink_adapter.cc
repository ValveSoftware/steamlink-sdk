// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/renderer/media/webrtc/webrtc_audio_sink_adapter.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

WebRtcAudioSinkAdapter::WebRtcAudioSinkAdapter(
    webrtc::AudioTrackSinkInterface* sink)
    : sink_(sink) {
  DCHECK(sink);
}

WebRtcAudioSinkAdapter::~WebRtcAudioSinkAdapter() {
}

bool WebRtcAudioSinkAdapter::IsEqual(
    const webrtc::AudioTrackSinkInterface* other) const {
  return (other == sink_);
}

void WebRtcAudioSinkAdapter::OnData(const int16* audio_data,
                                    int sample_rate,
                                    int number_of_channels,
                                    int number_of_frames) {
  sink_->OnData(audio_data, 16, sample_rate, number_of_channels,
                number_of_frames);
}

void WebRtcAudioSinkAdapter::OnSetFormat(
    const media::AudioParameters& params) {
  // No need to forward the OnSetFormat() callback to
  // webrtc::AudioTrackSinkInterface sink since the sink will handle the
  // format change in OnData().
}

}  // namespace content
