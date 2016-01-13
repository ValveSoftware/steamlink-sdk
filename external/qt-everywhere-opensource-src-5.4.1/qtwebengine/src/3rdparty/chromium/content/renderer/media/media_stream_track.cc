// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_track.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

namespace content {

MediaStreamTrack* MediaStreamTrack::GetTrack(
    const blink::WebMediaStreamTrack& track) {
  if (track.isNull())
    return NULL;
  return static_cast<MediaStreamTrack*>(track.extraData());
}

MediaStreamTrack::MediaStreamTrack(
    webrtc::MediaStreamTrackInterface* track, bool is_local_track)
    : track_(track),
      is_local_track_(is_local_track) {
}

MediaStreamTrack::~MediaStreamTrack() {
}

void MediaStreamTrack::SetEnabled(bool enabled) {
  if (track_)
    track_->set_enabled(enabled);
}

void MediaStreamTrack::Stop() {
  // Stop means that a track should be stopped permanently. But
  // since there is no proper way of doing that on a remote track, we can
  // at least disable the track. Blink will not call down to the content layer
  // after a track has been stopped.
  if (track_)
    track_->set_enabled(false);
}

webrtc::AudioTrackInterface* MediaStreamTrack::GetAudioAdapter() {
  return static_cast<webrtc::AudioTrackInterface*>(track_.get());
}

}  // namespace content
