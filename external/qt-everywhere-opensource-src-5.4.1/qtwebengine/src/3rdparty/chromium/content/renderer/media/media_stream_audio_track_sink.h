// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_SINK_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_SINK_H_

#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "media/audio/audio_parameters.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"

namespace content {

class MediaStreamAudioSink;
class PeerConnectionAudioSink;

// Interface for reference counted holder of audio stream audio track sink.
class MediaStreamAudioTrackSink
    : public base::RefCountedThreadSafe<MediaStreamAudioTrackSink> {
 public:
   virtual int OnData(const int16* audio_data,
                      int sample_rate,
                      int number_of_channels,
                      int number_of_frames,
                      const std::vector<int>& channels,
                      int audio_delay_milliseconds,
                      int current_volume,
                      bool need_audio_processing,
                      bool key_pressed) = 0;

  virtual void OnSetFormat(const media::AudioParameters& params) = 0;

  virtual void OnReadyStateChanged(
      blink::WebMediaStreamSource::ReadyState state) = 0;

  virtual void Reset() = 0;

  virtual bool IsEqual(const MediaStreamAudioSink* other) const = 0;
  virtual bool IsEqual(const PeerConnectionAudioSink* other) const = 0;

  // Wrapper which allows to use std::find_if() when adding and removing
  // sinks to/from the list.
  struct WrapsMediaStreamSink {
    WrapsMediaStreamSink(MediaStreamAudioSink* sink) : sink_(sink) {}
    bool operator()(
        const scoped_refptr<MediaStreamAudioTrackSink>& owner) const {
      return owner->IsEqual(sink_);
    }
    MediaStreamAudioSink* sink_;
  };
  struct WrapsPeerConnectionSink {
    WrapsPeerConnectionSink(PeerConnectionAudioSink* sink) : sink_(sink) {}
    bool operator()(
        const scoped_refptr<MediaStreamAudioTrackSink>& owner) const {
      return owner->IsEqual(sink_);
    }
    PeerConnectionAudioSink* sink_;
  };

 protected:
  virtual ~MediaStreamAudioTrackSink() {}

 private:
  friend class base::RefCountedThreadSafe<MediaStreamAudioTrackSink>;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_TRACK_SINK_H_
