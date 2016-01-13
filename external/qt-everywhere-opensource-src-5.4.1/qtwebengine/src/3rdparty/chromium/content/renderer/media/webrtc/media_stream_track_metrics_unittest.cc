// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/media_stream_track_metrics.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

using webrtc::AudioSourceInterface;
using webrtc::AudioTrackInterface;
using webrtc::AudioTrackSinkInterface;
using webrtc::MediaStreamInterface;
using webrtc::ObserverInterface;
using webrtc::PeerConnectionInterface;
using webrtc::VideoRendererInterface;
using webrtc::VideoSourceInterface;
using webrtc::VideoTrackInterface;

namespace content {

// A very simple mock that implements only the id() method.
class MockAudioTrackInterface : public AudioTrackInterface {
 public:
  explicit MockAudioTrackInterface(const std::string& id) : id_(id) {}
  virtual ~MockAudioTrackInterface() {}

  virtual std::string id() const OVERRIDE { return id_; }

  MOCK_METHOD1(RegisterObserver, void(ObserverInterface*));
  MOCK_METHOD1(UnregisterObserver, void(ObserverInterface*));
  MOCK_CONST_METHOD0(kind, std::string());
  MOCK_CONST_METHOD0(enabled, bool());
  MOCK_CONST_METHOD0(state, TrackState());
  MOCK_METHOD1(set_enabled, bool(bool));
  MOCK_METHOD1(set_state, bool(TrackState));
  MOCK_CONST_METHOD0(GetSource, AudioSourceInterface*());
  MOCK_METHOD1(AddSink, void(AudioTrackSinkInterface*));
  MOCK_METHOD1(RemoveSink, void(AudioTrackSinkInterface*));

 private:
  std::string id_;
};

// A very simple mock that implements only the id() method.
class MockVideoTrackInterface : public VideoTrackInterface {
 public:
  explicit MockVideoTrackInterface(const std::string& id) : id_(id) {}
  virtual ~MockVideoTrackInterface() {}

  virtual std::string id() const OVERRIDE { return id_; }

  MOCK_METHOD1(RegisterObserver, void(ObserverInterface*));
  MOCK_METHOD1(UnregisterObserver, void(ObserverInterface*));
  MOCK_CONST_METHOD0(kind, std::string());
  MOCK_CONST_METHOD0(enabled, bool());
  MOCK_CONST_METHOD0(state, TrackState());
  MOCK_METHOD1(set_enabled, bool(bool));
  MOCK_METHOD1(set_state, bool(TrackState));
  MOCK_METHOD1(AddRenderer, void(VideoRendererInterface*));
  MOCK_METHOD1(RemoveRenderer, void(VideoRendererInterface*));
  MOCK_CONST_METHOD0(GetSource, VideoSourceInterface*());

 private:
  std::string id_;
};

class MockMediaStreamTrackMetrics : public MediaStreamTrackMetrics {
 public:
  virtual ~MockMediaStreamTrackMetrics() {}

  MOCK_METHOD4(SendLifetimeMessage,
               void(const std::string&, TrackType, LifetimeEvent, StreamType));

  using MediaStreamTrackMetrics::MakeUniqueIdImpl;
};

class MediaStreamTrackMetricsTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    metrics_.reset(new MockMediaStreamTrackMetrics());
    stream_ = new talk_base::RefCountedObject<MockMediaStream>("stream");
  }

  virtual void TearDown() OVERRIDE {
    metrics_.reset();
    stream_ = NULL;
  }

  scoped_refptr<MockAudioTrackInterface> MakeAudioTrack(std::string id) {
    return new talk_base::RefCountedObject<MockAudioTrackInterface>(id);
  }

  scoped_refptr<MockVideoTrackInterface> MakeVideoTrack(std::string id) {
    return new talk_base::RefCountedObject<MockVideoTrackInterface>(id);
  }

  scoped_ptr<MockMediaStreamTrackMetrics> metrics_;
  scoped_refptr<MediaStreamInterface> stream_;
};

TEST_F(MediaStreamTrackMetricsTest, MakeUniqueId) {
  // The important testable properties of the unique ID are that it
  // should differ when any of the three constituents differ
  // (PeerConnection pointer, track ID, remote or not. Also, testing
  // that the implementation does not discard the upper 32 bits of the
  // PeerConnection pointer is important.
  //
  // The important hard-to-test property is that the ID be generated
  // using a hash function with virtually zero chance of
  // collisions. We don't test this, we rely on MD5 having this
  // property.

  // Lower 32 bits the same, upper 32 differ.
  EXPECT_NE(
      metrics_->MakeUniqueIdImpl(
          0x1000000000000001, "x", MediaStreamTrackMetrics::RECEIVED_STREAM),
      metrics_->MakeUniqueIdImpl(
          0x2000000000000001, "x", MediaStreamTrackMetrics::RECEIVED_STREAM));

  // Track ID differs.
  EXPECT_NE(metrics_->MakeUniqueIdImpl(
                42, "x", MediaStreamTrackMetrics::RECEIVED_STREAM),
            metrics_->MakeUniqueIdImpl(
                42, "y", MediaStreamTrackMetrics::RECEIVED_STREAM));

  // Remove vs. local track differs.
  EXPECT_NE(metrics_->MakeUniqueIdImpl(
                42, "x", MediaStreamTrackMetrics::RECEIVED_STREAM),
            metrics_->MakeUniqueIdImpl(
                42, "x", MediaStreamTrackMetrics::SENT_STREAM));
}

TEST_F(MediaStreamTrackMetricsTest, BasicRemoteStreams) {
  scoped_refptr<MockAudioTrackInterface> audio(MakeAudioTrack("audio"));
  scoped_refptr<MockVideoTrackInterface> video(MakeVideoTrack("video"));
  stream_->AddTrack(audio);
  stream_->AddTrack(video);
  metrics_->AddStream(MediaStreamTrackMetrics::RECEIVED_STREAM, stream_);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionDisconnected);
}

TEST_F(MediaStreamTrackMetricsTest, BasicLocalStreams) {
  scoped_refptr<MockAudioTrackInterface> audio(MakeAudioTrack("audio"));
  scoped_refptr<MockVideoTrackInterface> video(MakeVideoTrack("video"));
  stream_->AddTrack(audio);
  stream_->AddTrack(video);
  metrics_->AddStream(MediaStreamTrackMetrics::SENT_STREAM, stream_);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  metrics_->IceConnectionChange(PeerConnectionInterface::kIceConnectionFailed);
}

TEST_F(MediaStreamTrackMetricsTest, LocalStreamAddedAferIceConnect) {
  metrics_->IceConnectionChange(
        PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));

  scoped_refptr<MockAudioTrackInterface> audio(MakeAudioTrack("audio"));
  scoped_refptr<MockVideoTrackInterface> video(MakeVideoTrack("video"));
  stream_->AddTrack(audio);
  stream_->AddTrack(video);
  metrics_->AddStream(MediaStreamTrackMetrics::SENT_STREAM, stream_);
}

TEST_F(MediaStreamTrackMetricsTest, RemoteStreamAddedAferIceConnect) {
  metrics_->IceConnectionChange(
        PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));

  scoped_refptr<MockAudioTrackInterface> audio(MakeAudioTrack("audio"));
  scoped_refptr<MockVideoTrackInterface> video(MakeVideoTrack("video"));
  stream_->AddTrack(audio);
  stream_->AddTrack(video);
  metrics_->AddStream(MediaStreamTrackMetrics::RECEIVED_STREAM, stream_);
}

TEST_F(MediaStreamTrackMetricsTest, RemoteStreamTrackAdded) {
  scoped_refptr<MockAudioTrackInterface> initial(MakeAudioTrack("initial"));
  scoped_refptr<MockAudioTrackInterface> added(MakeAudioTrack("added"));
  stream_->AddTrack(initial);
  metrics_->AddStream(MediaStreamTrackMetrics::RECEIVED_STREAM, stream_);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("initial",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("added",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));
  stream_->AddTrack(added);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("initial",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("added",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));
  metrics_->IceConnectionChange(PeerConnectionInterface::kIceConnectionFailed);
}

TEST_F(MediaStreamTrackMetricsTest, LocalStreamTrackRemoved) {
  scoped_refptr<MockAudioTrackInterface> first(MakeAudioTrack("first"));
  scoped_refptr<MockAudioTrackInterface> second(MakeAudioTrack("second"));
  stream_->AddTrack(first);
  stream_->AddTrack(second);
  metrics_->AddStream(MediaStreamTrackMetrics::SENT_STREAM, stream_);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("first",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("second",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("first",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  stream_->RemoveTrack(first);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("second",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  metrics_->IceConnectionChange(PeerConnectionInterface::kIceConnectionFailed);
}

TEST_F(MediaStreamTrackMetricsTest, LocalStreamModificationsBeforeAndAfter) {
  scoped_refptr<MockAudioTrackInterface> first(MakeAudioTrack("first"));
  scoped_refptr<MockAudioTrackInterface> second(MakeAudioTrack("second"));
  stream_->AddTrack(first);
  metrics_->AddStream(MediaStreamTrackMetrics::SENT_STREAM, stream_);

  // This gets added after we start observing, but no lifetime message
  // should be sent at this point since the call is not connected. It
  // should get sent only once it gets connected.
  stream_->AddTrack(second);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("first",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("second",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("first",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("second",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  metrics_->IceConnectionChange(PeerConnectionInterface::kIceConnectionFailed);

  // This happens after the call is disconnected so no lifetime
  // message should be sent.
  stream_->RemoveTrack(first);
}

TEST_F(MediaStreamTrackMetricsTest, RemoteStreamMultipleDisconnects) {
  scoped_refptr<MockAudioTrackInterface> audio(MakeAudioTrack("audio"));
  stream_->AddTrack(audio);
  metrics_->AddStream(MediaStreamTrackMetrics::RECEIVED_STREAM, stream_);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::RECEIVED_STREAM));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionDisconnected);
  metrics_->IceConnectionChange(PeerConnectionInterface::kIceConnectionFailed);
  stream_->RemoveTrack(audio);
}

TEST_F(MediaStreamTrackMetricsTest, RemoteStreamConnectDisconnectTwice) {
  scoped_refptr<MockAudioTrackInterface> audio(MakeAudioTrack("audio"));
  stream_->AddTrack(audio);
  metrics_->AddStream(MediaStreamTrackMetrics::RECEIVED_STREAM, stream_);

  for (size_t i = 0; i < 2; ++i) {
    EXPECT_CALL(*metrics_,
                SendLifetimeMessage("audio",
                                    MediaStreamTrackMetrics::AUDIO_TRACK,
                                    MediaStreamTrackMetrics::CONNECTED,
                                    MediaStreamTrackMetrics::RECEIVED_STREAM));
    metrics_->IceConnectionChange(
        PeerConnectionInterface::kIceConnectionConnected);

    EXPECT_CALL(*metrics_,
                SendLifetimeMessage("audio",
                                    MediaStreamTrackMetrics::AUDIO_TRACK,
                                    MediaStreamTrackMetrics::DISCONNECTED,
                                    MediaStreamTrackMetrics::RECEIVED_STREAM));
    metrics_->IceConnectionChange(
        PeerConnectionInterface::kIceConnectionDisconnected);
  }

  stream_->RemoveTrack(audio);
}

TEST_F(MediaStreamTrackMetricsTest, LocalStreamRemovedNoDisconnect) {
  scoped_refptr<MockAudioTrackInterface> audio(MakeAudioTrack("audio"));
  scoped_refptr<MockVideoTrackInterface> video(MakeVideoTrack("video"));
  stream_->AddTrack(audio);
  stream_->AddTrack(video);
  metrics_->AddStream(MediaStreamTrackMetrics::SENT_STREAM, stream_);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  metrics_->RemoveStream(MediaStreamTrackMetrics::SENT_STREAM, stream_);
}

TEST_F(MediaStreamTrackMetricsTest, LocalStreamLargerTest) {
  scoped_refptr<MockAudioTrackInterface> audio1(MakeAudioTrack("audio1"));
  scoped_refptr<MockAudioTrackInterface> audio2(MakeAudioTrack("audio2"));
  scoped_refptr<MockAudioTrackInterface> audio3(MakeAudioTrack("audio3"));
  scoped_refptr<MockVideoTrackInterface> video1(MakeVideoTrack("video1"));
  scoped_refptr<MockVideoTrackInterface> video2(MakeVideoTrack("video2"));
  scoped_refptr<MockVideoTrackInterface> video3(MakeVideoTrack("video3"));
  stream_->AddTrack(audio1);
  stream_->AddTrack(video1);
  metrics_->AddStream(MediaStreamTrackMetrics::SENT_STREAM, stream_);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio1",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video1",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  metrics_->IceConnectionChange(
      PeerConnectionInterface::kIceConnectionConnected);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio2",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  stream_->AddTrack(audio2);
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video2",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  stream_->AddTrack(video2);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio1",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  stream_->RemoveTrack(audio1);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio3",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  stream_->AddTrack(audio3);
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video3",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  stream_->AddTrack(video3);

  // Add back audio1
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio1",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::CONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  stream_->AddTrack(audio1);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio2",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  stream_->RemoveTrack(audio2);
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video2",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  stream_->RemoveTrack(video2);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio1",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  stream_->RemoveTrack(audio1);
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video1",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  stream_->RemoveTrack(video1);

  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("audio3",
                                  MediaStreamTrackMetrics::AUDIO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  EXPECT_CALL(*metrics_,
              SendLifetimeMessage("video3",
                                  MediaStreamTrackMetrics::VIDEO_TRACK,
                                  MediaStreamTrackMetrics::DISCONNECTED,
                                  MediaStreamTrackMetrics::SENT_STREAM));
  metrics_->RemoveStream(MediaStreamTrackMetrics::SENT_STREAM, stream_);
}

}  // namespace content
