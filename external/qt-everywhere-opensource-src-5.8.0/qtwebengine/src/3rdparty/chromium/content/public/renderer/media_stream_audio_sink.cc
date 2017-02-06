// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/media_stream_audio_sink.h"

#include "base/logging.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace content {

void MediaStreamAudioSink::AddToAudioTrack(
    MediaStreamAudioSink* sink,
    const blink::WebMediaStreamTrack& track) {
  DCHECK(track.source().getType() == blink::WebMediaStreamSource::TypeAudio);
  MediaStreamAudioTrack* native_track = MediaStreamAudioTrack::From(track);
  DCHECK(native_track);
  native_track->AddSink(sink);
}

void MediaStreamAudioSink::RemoveFromAudioTrack(
    MediaStreamAudioSink* sink,
    const blink::WebMediaStreamTrack& track) {
  MediaStreamAudioTrack* native_track = MediaStreamAudioTrack::From(track);
  DCHECK(native_track);
  native_track->RemoveSink(sink);
}

media::AudioParameters MediaStreamAudioSink::GetFormatFromAudioTrack(
    const blink::WebMediaStreamTrack& track) {
  MediaStreamAudioTrack* native_track = MediaStreamAudioTrack::From(track);
  DCHECK(native_track);
  return native_track->GetOutputFormat();
}

}  // namespace content
