// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_registry.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

namespace {

const char kTestStreamLabel[] = "stream_label";

class MockCDQualityAudioSource : public MediaStreamAudioSource {
 public:
  MockCDQualityAudioSource() : MediaStreamAudioSource(true) {
    MediaStreamAudioSource::SetFormat(media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate,
        16,
        media::AudioParameters::kAudioCDSampleRate / 100));
    MediaStreamAudioSource::SetDeviceInfo(StreamDeviceInfo(
        MEDIA_DEVICE_AUDIO_CAPTURE, "Mock audio device", "mock_audio_device_id",
        media::AudioParameters::kAudioCDSampleRate,
        media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate / 100));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCDQualityAudioSource);
};

}  // namespace

MockMediaStreamRegistry::MockMediaStreamRegistry() {}

void MockMediaStreamRegistry::Init(const std::string& stream_url) {
  stream_url_ = stream_url;
  const blink::WebVector<blink::WebMediaStreamTrack> webkit_audio_tracks;
  const blink::WebVector<blink::WebMediaStreamTrack> webkit_video_tracks;
  const blink::WebString label(kTestStreamLabel);
  test_stream_.initialize(label, webkit_audio_tracks, webkit_video_tracks);
  test_stream_.setExtraData(new MediaStream());
}

void MockMediaStreamRegistry::AddVideoTrack(const std::string& track_id) {
  blink::WebMediaStreamSource blink_source;
  blink_source.initialize("mock video source id",
                          blink::WebMediaStreamSource::TypeVideo,
                          "mock video source name",
                          false /* remote */);
  MockMediaStreamVideoSource* native_source =
      new MockMediaStreamVideoSource(false /* manual get supported formats */);
  blink_source.setExtraData(native_source);
  blink::WebMediaStreamTrack blink_track;
  blink_track.initialize(base::UTF8ToUTF16(track_id), blink_source);
  blink::WebMediaConstraints constraints;
  constraints.initialize();

  MediaStreamVideoTrack* native_track = new MediaStreamVideoTrack(
      native_source, constraints, MediaStreamVideoSource::ConstraintsCallback(),
      true /* enabled */);
  blink_track.setTrackData(native_track);
  test_stream_.addTrack(blink_track);
}

void MockMediaStreamRegistry::AddAudioTrack(const std::string& track_id) {
  blink::WebMediaStreamSource blink_source;
  blink_source.initialize(
      "mock audio source id", blink::WebMediaStreamSource::TypeAudio,
      "mock audio source name", false /* remote */);
  MediaStreamAudioSource* const source = new MockCDQualityAudioSource();
  blink_source.setExtraData(source);  // Takes ownership.

  blink::WebMediaStreamTrack blink_track;
  blink_track.initialize(blink_source);
  CHECK(source->ConnectToTrack(blink_track));

  test_stream_.addTrack(blink_track);
}

blink::WebMediaStream MockMediaStreamRegistry::GetMediaStream(
    const std::string& url) {
  return (url != stream_url_) ? blink::WebMediaStream() : test_stream_;
}

}  // namespace content
