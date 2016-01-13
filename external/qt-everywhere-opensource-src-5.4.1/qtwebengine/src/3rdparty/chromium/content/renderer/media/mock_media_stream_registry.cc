// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_registry.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

static const char kTestStreamLabel[] = "stream_label";

MockMediaStreamRegistry::MockMediaStreamRegistry() {
}

void MockMediaStreamRegistry::Init(const std::string& stream_url) {
  stream_url_ = stream_url;
  blink::WebVector<blink::WebMediaStreamTrack> webkit_audio_tracks;
  blink::WebVector<blink::WebMediaStreamTrack> webkit_video_tracks;
  blink::WebString label(kTestStreamLabel);
  test_stream_.initialize(label, webkit_audio_tracks, webkit_video_tracks);
  test_stream_.setExtraData(new MediaStream(test_stream_));
}

void MockMediaStreamRegistry::AddVideoTrack(const std::string& track_id) {
  blink::WebMediaStreamSource blink_source;
  blink_source.initialize("mock video source id",
                          blink::WebMediaStreamSource::TypeVideo,
                          "mock video source name");
  MockMediaStreamVideoSource* native_source =
      new MockMediaStreamVideoSource(false);
  blink_source.setExtraData(native_source);
  blink::WebMediaStreamTrack blink_track;
  blink_track.initialize(base::UTF8ToUTF16(track_id), blink_source);
  blink::WebMediaConstraints constraints;
  constraints.initialize();

  MediaStreamVideoTrack* native_track =
      new MediaStreamVideoTrack(native_source,
                                constraints,
                                MediaStreamVideoSource::ConstraintsCallback(),
                                true);
  blink_track.setExtraData(native_track);
  test_stream_.addTrack(blink_track);
}

blink::WebMediaStream MockMediaStreamRegistry::GetMediaStream(
    const std::string& url) {
  if (url != stream_url_) {
    return blink::WebMediaStream();
  }
  return test_stream_;
}

const blink::WebMediaStream MockMediaStreamRegistry::test_stream() const {
  return test_stream_;
}

}  // namespace content
