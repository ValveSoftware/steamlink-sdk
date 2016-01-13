// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/media_stream_video_sink.h"

#include "base/logging.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace content {

void MediaStreamVideoSink::AddToVideoTrack(
    MediaStreamVideoSink* sink,
    const VideoCaptureDeliverFrameCB& callback,
    const blink::WebMediaStreamTrack& track) {
  DCHECK_EQ(blink::WebMediaStreamSource::TypeVideo, track.source().type());
  MediaStreamVideoTrack* video_track =
      static_cast<MediaStreamVideoTrack*>(track.extraData());
  video_track->AddSink(sink, callback);
}

void MediaStreamVideoSink::RemoveFromVideoTrack(
    MediaStreamVideoSink* sink,
    const blink::WebMediaStreamTrack& track) {
  DCHECK_EQ(blink::WebMediaStreamSource::TypeVideo, track.source().type());
  MediaStreamVideoTrack* video_track =
      static_cast<MediaStreamVideoTrack*>(track.extraData());
  video_track->RemoveSink(sink);
}

}  // namespace content
