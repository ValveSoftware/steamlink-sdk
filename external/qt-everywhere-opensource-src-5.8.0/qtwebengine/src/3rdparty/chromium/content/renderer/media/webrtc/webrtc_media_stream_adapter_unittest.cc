// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_adapter.h"

#include <stddef.h>

#include <memory>

#include "base/message_loop/message_loop.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_audio_device_factory.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/processed_local_audio_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using ::testing::_;

namespace content {

class WebRtcMediaStreamAdapterTest : public ::testing::Test {
 public:
  void SetUp() override {
    child_process_.reset(new ChildProcess());
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
  }

  void TearDown() override {
    adapter_.reset();
    blink::WebHeap::collectAllGarbageForTesting();
  }

  blink::WebMediaStream CreateBlinkMediaStream(bool audio, bool video) {
    blink::WebVector<blink::WebMediaStreamTrack> audio_track_vector(
        audio ? static_cast<size_t>(1) : 0);
    if (audio) {
      blink::WebMediaStreamSource audio_source;
      audio_source.initialize("audio",
                              blink::WebMediaStreamSource::TypeAudio,
                              "audio",
                              false /* remote */);
      ProcessedLocalAudioSource* const source =
          new ProcessedLocalAudioSource(
              -1 /* consumer_render_frame_id is N/A for non-browser tests */,
              StreamDeviceInfo(MEDIA_DEVICE_AUDIO_CAPTURE, "Mock audio device",
                               "mock_audio_device_id",
                               media::AudioParameters::kAudioCDSampleRate,
                               media::CHANNEL_LAYOUT_STEREO,
                               media::AudioParameters::kAudioCDSampleRate / 50),
              dependency_factory_.get());
      source->SetAllowInvalidRenderFrameIdForTesting(true);
      source->SetSourceConstraints(
          MockConstraintFactory().CreateWebMediaConstraints());
      audio_source.setExtraData(source);  // Takes ownership.
      audio_track_vector[0].initialize(audio_source);
      EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(),
                  Initialize(_, _, -1));
      EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(),
                  SetAutomaticGainControl(true));
      EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(), Start());
      EXPECT_CALL(*mock_audio_device_factory_.mock_capturer_source(), Stop());
      CHECK(source->ConnectToTrack(audio_track_vector[0]));
    }

    blink::WebVector<blink::WebMediaStreamTrack> video_track_vector(
        video ? static_cast<size_t>(1) : 0);
    MediaStreamSource::SourceStoppedCallback dummy_callback;
    if (video) {
      blink::WebMediaStreamSource video_source;
      video_source.initialize("video",
                              blink::WebMediaStreamSource::TypeVideo,
                              "video",
                              false /* remote */);
      MediaStreamVideoSource* native_source =
          new MockMediaStreamVideoSource(false);
      video_source.setExtraData(native_source);
      blink::WebMediaConstraints constraints;
      constraints.initialize();
      video_track_vector[0] = MediaStreamVideoTrack::CreateVideoTrack(
          native_source, constraints,
          MediaStreamVideoSource::ConstraintsCallback(), true);
    }

    blink::WebMediaStream stream_desc;
    stream_desc.initialize("media stream",
                           audio_track_vector,
                           video_track_vector);
    stream_desc.setExtraData(new MediaStream());
    return stream_desc;
  }

  void CreateWebRtcMediaStream(const blink::WebMediaStream& blink_stream,
                               size_t expected_number_of_audio_tracks,
                               size_t expected_number_of_video_tracks) {
    adapter_.reset(new WebRtcMediaStreamAdapter(
        blink_stream, dependency_factory_.get()));

    EXPECT_EQ(expected_number_of_audio_tracks,
              adapter_->webrtc_media_stream()->GetAudioTracks().size());
    EXPECT_EQ(expected_number_of_video_tracks,
              adapter_->webrtc_media_stream()->GetVideoTracks().size());
    EXPECT_EQ(blink_stream.id().utf8(),
              adapter_->webrtc_media_stream()->label());
  }

  webrtc::MediaStreamInterface* webrtc_stream() {
    return adapter_->webrtc_media_stream();
  }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<ChildProcess> child_process_;
  std::unique_ptr<MockPeerConnectionDependencyFactory> dependency_factory_;
  std::unique_ptr<WebRtcMediaStreamAdapter> adapter_;
  MockAudioDeviceFactory mock_audio_device_factory_;
};

TEST_F(WebRtcMediaStreamAdapterTest, CreateWebRtcMediaStream) {
  blink::WebMediaStream blink_stream = CreateBlinkMediaStream(true, true);
  CreateWebRtcMediaStream(blink_stream, 1, 1);
}

// Test that we don't crash if a MediaStream is created in Blink with an unknown
// audio sources. This can happen if a MediaStream is created with
// remote audio track.
TEST_F(WebRtcMediaStreamAdapterTest,
       CreateWebRtcMediaStreamWithoutAudioSource) {
  // Create a blink MediaStream description.
  blink::WebMediaStreamSource audio_source;
  audio_source.initialize("audio source",
                          blink::WebMediaStreamSource::TypeAudio,
                          "something",
                          false /* remote */);

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks(
      static_cast<size_t>(1));
  audio_tracks[0].initialize(audio_source.id(), audio_source);
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks(
      static_cast<size_t>(0));

  blink::WebMediaStream blink_stream;
  blink_stream.initialize("new stream", audio_tracks, video_tracks);
  blink_stream.setExtraData(new content::MediaStream());
  CreateWebRtcMediaStream(blink_stream, 0, 0);
}

TEST_F(WebRtcMediaStreamAdapterTest, RemoveAndAddTrack) {
  blink::WebMediaStream blink_stream = CreateBlinkMediaStream(true, true);
  CreateWebRtcMediaStream(blink_stream, 1, 1);

  MediaStream* native_stream = MediaStream::GetMediaStream(blink_stream);

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  blink_stream.audioTracks(audio_tracks);

  native_stream->RemoveTrack(audio_tracks[0]);
  EXPECT_TRUE(webrtc_stream()->GetAudioTracks().empty());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  blink_stream.videoTracks(video_tracks);

  native_stream->RemoveTrack(video_tracks[0]);
  EXPECT_TRUE(webrtc_stream()->GetVideoTracks().empty());

  native_stream->AddTrack(audio_tracks[0]);
  EXPECT_EQ(1u, webrtc_stream()->GetAudioTracks().size());

  native_stream->AddTrack(video_tracks[0]);
  EXPECT_EQ(1u, webrtc_stream()->GetVideoTracks().size());
}

}  // namespace content
