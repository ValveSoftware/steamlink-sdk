// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEER_CONNECTION_AUDIO_SINK_OWNER_H_
#define CONTENT_RENDERER_MEDIA_PEER_CONNECTION_AUDIO_SINK_OWNER_H_

#include <vector>

#include "base/synchronization/lock.h"
#include "content/renderer/media/media_stream_audio_track_sink.h"

namespace content {

class PeerConnectionAudioSink;

// Reference counted holder of PeerConnectionAudioSink.
class PeerConnectionAudioSinkOwner : public MediaStreamAudioTrackSink {
 public:
  explicit PeerConnectionAudioSinkOwner(PeerConnectionAudioSink* sink);

  // MediaStreamAudioTrackSink implementation.
  virtual int OnData(const int16* audio_data,
                     int sample_rate,
                     int number_of_channels,
                     int number_of_frames,
                     const std::vector<int>& channels,
                     int audio_delay_milliseconds,
                     int current_volume,
                     bool need_audio_processing,
                     bool key_pressed) OVERRIDE;
  virtual void OnSetFormat(const media::AudioParameters& params) OVERRIDE;
  virtual void OnReadyStateChanged(
      blink::WebMediaStreamSource::ReadyState state) OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual bool IsEqual(const MediaStreamAudioSink* other) const OVERRIDE;
  virtual bool IsEqual(const PeerConnectionAudioSink* other) const OVERRIDE;

 protected:
  virtual ~PeerConnectionAudioSinkOwner() {}

 private:
  mutable base::Lock lock_;

  // Raw pointer to the delegate, the client need to call Reset() to set the
  // pointer to NULL before the delegate goes away.
  PeerConnectionAudioSink* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionAudioSinkOwner);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_PEER_CONNECTION_AUDIO_SINK_OWNER_H_
