// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_SINK_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_SINK_ADAPTER_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/media_stream_audio_sink.h"

namespace webrtc {
class AudioTrackSinkInterface;
}  // namespace webrtc

namespace content {

// Adapter to the webrtc::AudioTrackSinkInterface of the audio track.
// This class is used in between the MediaStreamAudioSink and
// webrtc::AudioTrackSinkInterface. It gets data callback via the
// MediaStreamAudioSink::OnData() interface and pass the data to
// webrtc::AudioTrackSinkInterface.
class WebRtcAudioSinkAdapter : public MediaStreamAudioSink {
 public:
  explicit WebRtcAudioSinkAdapter(
      webrtc::AudioTrackSinkInterface* sink);
  virtual ~WebRtcAudioSinkAdapter();

  bool IsEqual(const webrtc::AudioTrackSinkInterface* other) const;

 private:
  // MediaStreamAudioSink implementation.
  virtual void OnData(const int16* audio_data,
                      int sample_rate,
                      int number_of_channels,
                      int number_of_frames) OVERRIDE;
  virtual void OnSetFormat(const media::AudioParameters& params) OVERRIDE;

  webrtc::AudioTrackSinkInterface* const sink_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcAudioSinkAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_AUDIO_SINK_ADAPTER_H_
