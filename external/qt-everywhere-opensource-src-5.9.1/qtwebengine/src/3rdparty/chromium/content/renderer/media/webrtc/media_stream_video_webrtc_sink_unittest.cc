// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_video_webrtc_sink.h"

#include "content/child/child_process.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class MediaStreamVideoWebRtcSinkTest : public ::testing::Test {
 public:
  MediaStreamVideoWebRtcSinkTest() {}

  void SetVideoTrack(blink::WebMediaConstraints constraints) {
    registry_.Init("stream URL");
    registry_.AddVideoTrack("test video track", constraints);
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    registry_.test_stream().videoTracks(video_tracks);
    track_ = video_tracks[0];
    // TODO(hta): Verify that track_ is valid. When constraints produce
    // no valid format, using the track will cause a crash.
  }

 protected:
  blink::WebMediaStreamTrack track_;
  MockPeerConnectionDependencyFactory dependency_factory_;

 private:
  MockMediaStreamRegistry registry_;
  // A ChildProcess and a MessageLoopForUI are both needed to fool the Tracks
  // and Sources in |registry_| into believing they are on the right threads.
  base::MessageLoopForUI message_loop_;
  const ChildProcess child_process_;
};

TEST_F(MediaStreamVideoWebRtcSinkTest, NoiseReductionDefaultsToNotSet) {
  blink::WebMediaConstraints constraints;
  constraints.initialize();
  SetVideoTrack(constraints);
  MediaStreamVideoWebRtcSink my_sink(track_, &dependency_factory_);
  EXPECT_TRUE(my_sink.webrtc_video_track());
  EXPECT_FALSE(my_sink.SourceNeedsDenoisingForTesting());
}

TEST_F(MediaStreamVideoWebRtcSinkTest, NoiseReductionConstraintPassThrough) {
  MockConstraintFactory factory;
  factory.basic().googNoiseReduction.setExact(true);
  SetVideoTrack(factory.CreateWebMediaConstraints());
  MediaStreamVideoWebRtcSink my_sink(track_, &dependency_factory_);
  EXPECT_TRUE(my_sink.SourceNeedsDenoisingForTesting());
  EXPECT_TRUE(*(my_sink.SourceNeedsDenoisingForTesting()));
}

}  // namespace
}  // namespace content
