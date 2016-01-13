// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_AUDIO_SINK_H_
#define CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_AUDIO_SINK_H_

#include <vector>

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/public/renderer/media_stream_sink.h"

namespace blink {
class WebMediaStreamTrack;
}

namespace media {
class AudioParameters;
}

namespace content {

class CONTENT_EXPORT MediaStreamAudioSink : public MediaStreamSink {
 public:
  // Adds a MediaStreamAudioSink to the audio track to receive audio data from
  // the track.
  // Called on the main render thread.
  static void AddToAudioTrack(MediaStreamAudioSink* sink,
                              const blink::WebMediaStreamTrack& track);

  // Removes a MediaStreamAudioSink from the audio track to stop receiving
  // audio data from the track.
  // Called on the main render thread.
  static void RemoveFromAudioTrack(MediaStreamAudioSink* sink,
                                   const blink::WebMediaStreamTrack& track);

  // Callback on delivering the interleaved audio data.
  // |audio_data| is the pointer to the audio data.
  // |sample_rate| is the sample frequency of |audio_data|.
  // |number_of_channels| is the number of audio channels of |audio_data|.
  // |number_of_frames| is the number of audio frames in the |audio_data|.
  // Called on real-time audio thread.
  virtual void OnData(const int16* audio_data,
                      int sample_rate,
                      int number_of_channels,
                      int number_of_frames) = 0;

  // Callback called when the format of the audio stream has changed.
  // This is called on the same thread as OnData().
  virtual void OnSetFormat(const media::AudioParameters& params) = 0;

 protected:
  virtual ~MediaStreamAudioSink() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_MEDIA_STREAM_AUDIO_SINK_H_
