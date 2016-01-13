// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_source.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCConfiguration.h"
#include "third_party/WebKit/public/platform/WebRTCDTMFSenderHandler.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelHandler.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelInit.h"
#include "third_party/WebKit/public/platform/WebRTCICECandidate.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescription.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescriptionRequest.h"
#include "third_party/WebKit/public/platform/WebRTCStatsRequest.h"
#include "third_party/WebKit/public/platform/WebRTCVoidRequest.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"

static const char kDummySdp[] = "dummy sdp";
static const char kDummySdpType[] = "dummy type";

using blink::WebRTCPeerConnectionHandlerClient;
using testing::NiceMock;
using testing::_;
using testing::Ref;

namespace content {

class MockRTCStatsResponse : public LocalRTCStatsResponse {
 public:
  MockRTCStatsResponse()
      : report_count_(0),
        statistic_count_(0) {
  }

  virtual size_t addReport(blink::WebString type,
                           blink::WebString id,
                           double timestamp) OVERRIDE {
    ++report_count_;
    return report_count_;
  }

  virtual void addStatistic(size_t report,
                            blink::WebString name, blink::WebString value)
      OVERRIDE {
    ++statistic_count_;
  }
  int report_count() const { return report_count_; }

 private:
  int report_count_;
  int statistic_count_;
};

// Mocked wrapper for blink::WebRTCStatsRequest
class MockRTCStatsRequest : public LocalRTCStatsRequest {
 public:
  MockRTCStatsRequest()
      : has_selector_(false),
        request_succeeded_called_(false) {}

  virtual bool hasSelector() const OVERRIDE {
    return has_selector_;
  }
  virtual blink::WebMediaStreamTrack component() const OVERRIDE {
    return component_;
  }
  virtual scoped_refptr<LocalRTCStatsResponse> createResponse() OVERRIDE {
    DCHECK(!response_.get());
    response_ = new talk_base::RefCountedObject<MockRTCStatsResponse>();
    return response_;
  }

  virtual void requestSucceeded(const LocalRTCStatsResponse* response)
      OVERRIDE {
    EXPECT_EQ(response, response_.get());
    request_succeeded_called_ = true;
  }

  // Function for setting whether or not a selector is available.
  void setSelector(const blink::WebMediaStreamTrack& component) {
    has_selector_ = true;
    component_ = component;
  }

  // Function for inspecting the result of a stats request.
  MockRTCStatsResponse* result() {
    if (request_succeeded_called_) {
      return response_.get();
    } else {
      return NULL;
    }
  }

 private:
  bool has_selector_;
  blink::WebMediaStreamTrack component_;
  scoped_refptr<MockRTCStatsResponse> response_;
  bool request_succeeded_called_;
};

class MockPeerConnectionTracker : public PeerConnectionTracker {
 public:
  MOCK_METHOD1(UnregisterPeerConnection,
               void(RTCPeerConnectionHandler* pc_handler));
  // TODO(jiayl): add coverage for the following methods
  MOCK_METHOD2(TrackCreateOffer,
               void(RTCPeerConnectionHandler* pc_handler,
                    const RTCMediaConstraints& constraints));
  MOCK_METHOD2(TrackCreateAnswer,
               void(RTCPeerConnectionHandler* pc_handler,
                    const RTCMediaConstraints& constraints));
  MOCK_METHOD3(TrackSetSessionDescription,
               void(RTCPeerConnectionHandler* pc_handler,
                    const blink::WebRTCSessionDescription& desc,
                    Source source));
  MOCK_METHOD3(
      TrackUpdateIce,
      void(RTCPeerConnectionHandler* pc_handler,
           const std::vector<
               webrtc::PeerConnectionInterface::IceServer>& servers,
           const RTCMediaConstraints& options));
  MOCK_METHOD3(TrackAddIceCandidate,
               void(RTCPeerConnectionHandler* pc_handler,
                    const blink::WebRTCICECandidate& candidate,
                    Source source));
  MOCK_METHOD3(TrackAddStream,
               void(RTCPeerConnectionHandler* pc_handler,
                    const blink::WebMediaStream& stream,
                    Source source));
  MOCK_METHOD3(TrackRemoveStream,
               void(RTCPeerConnectionHandler* pc_handler,
                    const blink::WebMediaStream& stream,
                    Source source));
  MOCK_METHOD1(TrackOnIceComplete,
               void(RTCPeerConnectionHandler* pc_handler));
  MOCK_METHOD3(TrackCreateDataChannel,
               void(RTCPeerConnectionHandler* pc_handler,
                    const webrtc::DataChannelInterface* data_channel,
                    Source source));
  MOCK_METHOD1(TrackStop, void(RTCPeerConnectionHandler* pc_handler));
  MOCK_METHOD2(TrackSignalingStateChange,
               void(RTCPeerConnectionHandler* pc_handler,
                    WebRTCPeerConnectionHandlerClient::SignalingState state));
  MOCK_METHOD2(
      TrackIceConnectionStateChange,
      void(RTCPeerConnectionHandler* pc_handler,
           WebRTCPeerConnectionHandlerClient::ICEConnectionState state));
  MOCK_METHOD2(
      TrackIceGatheringStateChange,
      void(RTCPeerConnectionHandler* pc_handler,
           WebRTCPeerConnectionHandlerClient::ICEGatheringState state));
  MOCK_METHOD1(TrackOnRenegotiationNeeded,
               void(RTCPeerConnectionHandler* pc_handler));
  MOCK_METHOD2(TrackCreateDTMFSender,
               void(RTCPeerConnectionHandler* pc_handler,
                     const blink::WebMediaStreamTrack& track));
};

class RTCPeerConnectionHandlerUnderTest : public RTCPeerConnectionHandler {
 public:
  RTCPeerConnectionHandlerUnderTest(
      WebRTCPeerConnectionHandlerClient* client,
      PeerConnectionDependencyFactory* dependency_factory)
      : RTCPeerConnectionHandler(client, dependency_factory) {
  }

  MockPeerConnectionImpl* native_peer_connection() {
    return static_cast<MockPeerConnectionImpl*>(
        RTCPeerConnectionHandler::native_peer_connection());
  }
};

class RTCPeerConnectionHandlerTest : public ::testing::Test {
 public:
  RTCPeerConnectionHandlerTest() : mock_peer_connection_(NULL) {
    child_process_.reset(new ChildProcess());
  }

  virtual void SetUp() {
    mock_client_.reset(new NiceMock<MockWebRTCPeerConnectionHandlerClient>());
    mock_dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
    pc_handler_.reset(
        new RTCPeerConnectionHandlerUnderTest(mock_client_.get(),
                                              mock_dependency_factory_.get()));
    mock_tracker_.reset(new NiceMock<MockPeerConnectionTracker>());
    blink::WebRTCConfiguration config;
    blink::WebMediaConstraints constraints;
    EXPECT_TRUE(pc_handler_->InitializeForTest(config, constraints,
                                               mock_tracker_.get()));

    mock_peer_connection_ = pc_handler_->native_peer_connection();
    ASSERT_TRUE(mock_peer_connection_);
  }

  // Creates a WebKit local MediaStream.
  blink::WebMediaStream CreateLocalMediaStream(
      const std::string& stream_label) {
    std::string video_track_label("video-label");
    std::string audio_track_label("audio-label");

    blink::WebMediaStreamSource audio_source;
    audio_source.initialize(blink::WebString::fromUTF8(audio_track_label),
                            blink::WebMediaStreamSource::TypeAudio,
                            blink::WebString::fromUTF8("audio_track"));
    audio_source.setExtraData(new MediaStreamAudioSource());
    blink::WebMediaStreamSource video_source;
    video_source.initialize(blink::WebString::fromUTF8(video_track_label),
                            blink::WebMediaStreamSource::TypeVideo,
                            blink::WebString::fromUTF8("video_track"));
    MockMediaStreamVideoSource* native_video_source =
        new MockMediaStreamVideoSource(false);
    video_source.setExtraData(native_video_source);

    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks(
        static_cast<size_t>(1));
    audio_tracks[0].initialize(audio_source.id(), audio_source);
    audio_tracks[0].setExtraData(
        new MediaStreamTrack(
            WebRtcLocalAudioTrackAdapter::Create(audio_track_label,
                                                 NULL),
                                                 true));
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks(
        static_cast<size_t>(1));
    blink::WebMediaConstraints constraints;
    constraints.initialize();
    video_tracks[0] = MediaStreamVideoTrack::CreateVideoTrack(
        native_video_source, constraints,
        MediaStreamVideoSource::ConstraintsCallback(), true);

    blink::WebMediaStream local_stream;
    local_stream.initialize(base::UTF8ToUTF16(stream_label), audio_tracks,
                            video_tracks);
    local_stream.setExtraData(
        new MediaStream(local_stream));
    return local_stream;
  }

  // Creates a remote MediaStream and adds it to the mocked native
  // peer connection.
  scoped_refptr<webrtc::MediaStreamInterface>
  AddRemoteMockMediaStream(const std::string& stream_label,
                           const std::string& video_track_label,
                           const std::string& audio_track_label) {
    scoped_refptr<webrtc::MediaStreamInterface> stream(
        mock_dependency_factory_->CreateLocalMediaStream(stream_label));
    if (!video_track_label.empty()) {
      webrtc::VideoSourceInterface* source = NULL;
      scoped_refptr<webrtc::VideoTrackInterface> video_track(
          mock_dependency_factory_->CreateLocalVideoTrack(
              video_track_label, source));
      stream->AddTrack(video_track.get());
    }
    if (!audio_track_label.empty()) {
      scoped_refptr<WebRtcAudioCapturer> capturer;
      scoped_refptr<webrtc::AudioTrackInterface> audio_track(
          WebRtcLocalAudioTrackAdapter::Create(audio_track_label, NULL));
      stream->AddTrack(audio_track.get());
    }
    mock_peer_connection_->AddRemoteStream(stream.get());
    return stream;
  }

  base::MessageLoop message_loop_;
  scoped_ptr<ChildProcess> child_process_;
  scoped_ptr<MockWebRTCPeerConnectionHandlerClient> mock_client_;
  scoped_ptr<MockPeerConnectionDependencyFactory> mock_dependency_factory_;
  scoped_ptr<NiceMock<MockPeerConnectionTracker> > mock_tracker_;
  scoped_ptr<RTCPeerConnectionHandlerUnderTest> pc_handler_;

  // Weak reference to the mocked native peer connection implementation.
  MockPeerConnectionImpl* mock_peer_connection_;
};

TEST_F(RTCPeerConnectionHandlerTest, Destruct) {
  EXPECT_CALL(*mock_tracker_.get(), UnregisterPeerConnection(pc_handler_.get()))
      .Times(1);
  pc_handler_.reset(NULL);
}

TEST_F(RTCPeerConnectionHandlerTest, CreateOffer) {
  blink::WebRTCSessionDescriptionRequest request;
  blink::WebMediaConstraints options;
  EXPECT_CALL(*mock_tracker_.get(), TrackCreateOffer(pc_handler_.get(), _));

  // TODO(perkj): Can blink::WebRTCSessionDescriptionRequest be changed so
  // the |reqest| requestSucceeded can be tested? Currently the |request| object
  // can not be initialized from a unit test.
  EXPECT_FALSE(mock_peer_connection_->created_session_description() != NULL);
  pc_handler_->createOffer(request, options);
  EXPECT_TRUE(mock_peer_connection_->created_session_description() != NULL);
}

TEST_F(RTCPeerConnectionHandlerTest, CreateAnswer) {
  blink::WebRTCSessionDescriptionRequest request;
  blink::WebMediaConstraints options;
  EXPECT_CALL(*mock_tracker_.get(), TrackCreateAnswer(pc_handler_.get(), _));
  // TODO(perkj): Can blink::WebRTCSessionDescriptionRequest be changed so
  // the |reqest| requestSucceeded can be tested? Currently the |request| object
  // can not be initialized from a unit test.
  EXPECT_FALSE(mock_peer_connection_->created_session_description() != NULL);
  pc_handler_->createAnswer(request, options);
  EXPECT_TRUE(mock_peer_connection_->created_session_description() != NULL);
}

TEST_F(RTCPeerConnectionHandlerTest, setLocalDescription) {
  blink::WebRTCVoidRequest request;
  blink::WebRTCSessionDescription description;
  description.initialize(kDummySdpType, kDummySdp);
  // PeerConnectionTracker::TrackSetSessionDescription is expected to be called
  // before |mock_peer_connection| is called.
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetSessionDescription(pc_handler_.get(), Ref(description),
                                         PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_CALL(*mock_peer_connection_, SetLocalDescription(_, _));

  pc_handler_->setLocalDescription(request, description);
  EXPECT_EQ(description.type(), pc_handler_->localDescription().type());
  EXPECT_EQ(description.sdp(), pc_handler_->localDescription().sdp());

  std::string sdp_string;
  ASSERT_TRUE(mock_peer_connection_->local_description() != NULL);
  EXPECT_EQ(kDummySdpType, mock_peer_connection_->local_description()->type());
  mock_peer_connection_->local_description()->ToString(&sdp_string);
  EXPECT_EQ(kDummySdp, sdp_string);
}

TEST_F(RTCPeerConnectionHandlerTest, setRemoteDescription) {
  blink::WebRTCVoidRequest request;
  blink::WebRTCSessionDescription description;
  description.initialize(kDummySdpType, kDummySdp);

  // PeerConnectionTracker::TrackSetSessionDescription is expected to be called
  // before |mock_peer_connection| is called.
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackSetSessionDescription(pc_handler_.get(), Ref(description),
                                         PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_peer_connection_, SetRemoteDescription(_, _));

  pc_handler_->setRemoteDescription(request, description);
  EXPECT_EQ(description.type(), pc_handler_->remoteDescription().type());
  EXPECT_EQ(description.sdp(), pc_handler_->remoteDescription().sdp());

  std::string sdp_string;
  ASSERT_TRUE(mock_peer_connection_->remote_description() != NULL);
  EXPECT_EQ(kDummySdpType, mock_peer_connection_->remote_description()->type());
  mock_peer_connection_->remote_description()->ToString(&sdp_string);
  EXPECT_EQ(kDummySdp, sdp_string);
}

TEST_F(RTCPeerConnectionHandlerTest, updateICE) {
  blink::WebRTCConfiguration config;
  blink::WebMediaConstraints constraints;

  EXPECT_CALL(*mock_tracker_.get(), TrackUpdateIce(pc_handler_.get(), _, _));
  // TODO(perkj): Test that the parameters in |config| can be translated when a
  // WebRTCConfiguration can be constructed. It's WebKit class and can't be
  // initialized from a test.
  EXPECT_TRUE(pc_handler_->updateICE(config, constraints));
}

TEST_F(RTCPeerConnectionHandlerTest, addICECandidate) {
  blink::WebRTCICECandidate candidate;
  candidate.initialize(kDummySdp, "mid", 1);

  EXPECT_CALL(*mock_tracker_.get(),
              TrackAddIceCandidate(pc_handler_.get(),
                                   testing::Ref(candidate),
                                   PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_TRUE(pc_handler_->addICECandidate(candidate));
  EXPECT_EQ(kDummySdp, mock_peer_connection_->ice_sdp());
  EXPECT_EQ(1, mock_peer_connection_->sdp_mline_index());
  EXPECT_EQ("mid", mock_peer_connection_->sdp_mid());
}

TEST_F(RTCPeerConnectionHandlerTest, addAndRemoveStream) {
  std::string stream_label = "local_stream";
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream(stream_label));
  blink::WebMediaConstraints constraints;

  EXPECT_CALL(*mock_tracker_.get(),
              TrackAddStream(pc_handler_.get(),
                             testing::Ref(local_stream),
                             PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_CALL(*mock_tracker_.get(),
              TrackRemoveStream(pc_handler_.get(),
                                testing::Ref(local_stream),
                                PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_TRUE(pc_handler_->addStream(local_stream, constraints));
  EXPECT_EQ(stream_label, mock_peer_connection_->stream_label());
  EXPECT_EQ(1u,
      mock_peer_connection_->local_streams()->at(0)->GetAudioTracks().size());
  EXPECT_EQ(1u,
      mock_peer_connection_->local_streams()->at(0)->GetVideoTracks().size());

  EXPECT_FALSE(pc_handler_->addStream(local_stream, constraints));

  pc_handler_->removeStream(local_stream);
  EXPECT_EQ(0u, mock_peer_connection_->local_streams()->count());
}

TEST_F(RTCPeerConnectionHandlerTest, addStreamWithStoppedAudioAndVideoTrack) {
  std::string stream_label = "local_stream";
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream(stream_label));
  blink::WebMediaConstraints constraints;

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  local_stream.audioTracks(audio_tracks);
  MediaStreamAudioSource* native_audio_source =
      static_cast<MediaStreamAudioSource*>(
          audio_tracks[0].source().extraData());
  native_audio_source->StopSource();

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  local_stream.videoTracks(video_tracks);
  MediaStreamVideoSource* native_video_source =
      static_cast<MediaStreamVideoSource*>(
          video_tracks[0].source().extraData());
  native_video_source->StopSource();

  EXPECT_TRUE(pc_handler_->addStream(local_stream, constraints));
  EXPECT_EQ(stream_label, mock_peer_connection_->stream_label());
  EXPECT_EQ(
      1u,
      mock_peer_connection_->local_streams()->at(0)->GetAudioTracks().size());
  EXPECT_EQ(
      1u,
      mock_peer_connection_->local_streams()->at(0)->GetVideoTracks().size());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsNoSelector) {
  scoped_refptr<MockRTCStatsRequest> request(
      new talk_base::RefCountedObject<MockRTCStatsRequest>());
  pc_handler_->getStats(request.get());
  // Note that callback gets executed synchronously by mock.
  ASSERT_TRUE(request->result());
  EXPECT_LT(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsAfterClose) {
  scoped_refptr<MockRTCStatsRequest> request(
      new talk_base::RefCountedObject<MockRTCStatsRequest>());
  pc_handler_->stop();
  pc_handler_->getStats(request.get());
  // Note that callback gets executed synchronously by mock.
  ASSERT_TRUE(request->result());
  EXPECT_LT(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsWithLocalSelector) {
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream("local_stream"));
  blink::WebMediaConstraints constraints;
  pc_handler_->addStream(local_stream, constraints);
  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  local_stream.audioTracks(tracks);
  ASSERT_LE(1ul, tracks.size());

  scoped_refptr<MockRTCStatsRequest> request(
      new talk_base::RefCountedObject<MockRTCStatsRequest>());
  request->setSelector(tracks[0]);
  pc_handler_->getStats(request.get());
  EXPECT_EQ(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsWithRemoteSelector) {
  scoped_refptr<webrtc::MediaStreamInterface> stream(
      AddRemoteMockMediaStream("remote_stream", "video", "audio"));
  pc_handler_->OnAddStream(stream.get());
  const blink::WebMediaStream& remote_stream = mock_client_->remote_stream();

  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  remote_stream.audioTracks(tracks);
  ASSERT_LE(1ul, tracks.size());

  scoped_refptr<MockRTCStatsRequest> request(
      new talk_base::RefCountedObject<MockRTCStatsRequest>());
  request->setSelector(tracks[0]);
  pc_handler_->getStats(request.get());
  EXPECT_EQ(1, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, GetStatsWithBadSelector) {
  // The setup is the same as GetStatsWithLocalSelector, but the stream is not
  // added to the PeerConnection.
  blink::WebMediaStream local_stream(
      CreateLocalMediaStream("local_stream_2"));
  blink::WebMediaConstraints constraints;
  blink::WebVector<blink::WebMediaStreamTrack> tracks;

  local_stream.audioTracks(tracks);
  blink::WebMediaStreamTrack component = tracks[0];
  mock_peer_connection_->SetGetStatsResult(false);

  scoped_refptr<MockRTCStatsRequest> request(
      new talk_base::RefCountedObject<MockRTCStatsRequest>());
  request->setSelector(component);
  pc_handler_->getStats(request.get());
  EXPECT_EQ(0, request->result()->report_count());
}

TEST_F(RTCPeerConnectionHandlerTest, OnSignalingChange) {
  testing::InSequence sequence;

  webrtc::PeerConnectionInterface::SignalingState new_state =
      webrtc::PeerConnectionInterface::kHaveRemoteOffer;
  EXPECT_CALL(*mock_tracker_.get(), TrackSignalingStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveRemoteOffer));
  EXPECT_CALL(*mock_client_.get(), didChangeSignalingState(
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveRemoteOffer));
  pc_handler_->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kHaveLocalPrAnswer;
  EXPECT_CALL(*mock_tracker_.get(), TrackSignalingStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveLocalPrAnswer));
  EXPECT_CALL(*mock_client_.get(), didChangeSignalingState(
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveLocalPrAnswer));
  pc_handler_->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kHaveLocalOffer;
  EXPECT_CALL(*mock_tracker_.get(), TrackSignalingStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveLocalOffer));
  EXPECT_CALL(*mock_client_.get(), didChangeSignalingState(
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveLocalOffer));
  pc_handler_->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kHaveRemotePrAnswer;
  EXPECT_CALL(*mock_tracker_.get(), TrackSignalingStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveRemotePrAnswer));
  EXPECT_CALL(*mock_client_.get(), didChangeSignalingState(
      WebRTCPeerConnectionHandlerClient::SignalingStateHaveRemotePrAnswer));
  pc_handler_->OnSignalingChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kClosed;
  EXPECT_CALL(*mock_tracker_.get(), TrackSignalingStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::SignalingStateClosed));
  EXPECT_CALL(*mock_client_.get(), didChangeSignalingState(
      WebRTCPeerConnectionHandlerClient::SignalingStateClosed));
  pc_handler_->OnSignalingChange(new_state);
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceConnectionChange) {
  testing::InSequence sequence;

  webrtc::PeerConnectionInterface::IceConnectionState new_state =
      webrtc::PeerConnectionInterface::kIceConnectionNew;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateStarting));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateStarting));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionChecking;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateChecking));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateChecking));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionConnected;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateConnected));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateConnected));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionCompleted;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateCompleted));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateCompleted));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionFailed;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateFailed));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateFailed));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionDisconnected;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateDisconnected));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateDisconnected));
  pc_handler_->OnIceConnectionChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceConnectionClosed;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceConnectionStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateClosed));
  EXPECT_CALL(*mock_client_.get(), didChangeICEConnectionState(
      WebRTCPeerConnectionHandlerClient::ICEConnectionStateClosed));
  pc_handler_->OnIceConnectionChange(new_state);
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceGatheringChange) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(), TrackIceGatheringStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateNew));
  EXPECT_CALL(*mock_client_.get(), didChangeICEGatheringState(
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateNew));
  EXPECT_CALL(*mock_tracker_.get(), TrackIceGatheringStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateGathering));
  EXPECT_CALL(*mock_client_.get(), didChangeICEGatheringState(
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateGathering));
  EXPECT_CALL(*mock_tracker_.get(), TrackIceGatheringStateChange(
      pc_handler_.get(),
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateComplete));
  EXPECT_CALL(*mock_client_.get(), didChangeICEGatheringState(
      WebRTCPeerConnectionHandlerClient::ICEGatheringStateComplete));

  webrtc::PeerConnectionInterface::IceGatheringState new_state =
        webrtc::PeerConnectionInterface::kIceGatheringNew;
  pc_handler_->OnIceGatheringChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceGatheringGathering;
  pc_handler_->OnIceGatheringChange(new_state);

  new_state = webrtc::PeerConnectionInterface::kIceGatheringComplete;
  pc_handler_->OnIceGatheringChange(new_state);

  // Check NULL candidate after ice gathering is completed.
  EXPECT_EQ("", mock_client_->candidate_mid());
  EXPECT_EQ(-1, mock_client_->candidate_mlineindex());
  EXPECT_EQ("", mock_client_->candidate_sdp());
}

TEST_F(RTCPeerConnectionHandlerTest, OnAddAndOnRemoveStream) {
  std::string remote_stream_label("remote_stream");
  scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));

  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(), TrackAddStream(
      pc_handler_.get(),
      testing::Property(&blink::WebMediaStream::id,
                        base::UTF8ToUTF16(remote_stream_label)),
      PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_client_.get(), didAddRemoteStream(
      testing::Property(&blink::WebMediaStream::id,
                        base::UTF8ToUTF16(remote_stream_label))));

  EXPECT_CALL(*mock_tracker_.get(), TrackRemoveStream(
      pc_handler_.get(),
      testing::Property(&blink::WebMediaStream::id,
                        base::UTF8ToUTF16(remote_stream_label)),
      PeerConnectionTracker::SOURCE_REMOTE));
  EXPECT_CALL(*mock_client_.get(), didRemoveRemoteStream(
      testing::Property(&blink::WebMediaStream::id,
                        base::UTF8ToUTF16(remote_stream_label))));

  pc_handler_->OnAddStream(remote_stream.get());
  pc_handler_->OnRemoveStream(remote_stream.get());
}

// This test that WebKit is notified about remote track state changes.
TEST_F(RTCPeerConnectionHandlerTest, RemoteTrackState) {
  std::string remote_stream_label("remote_stream");
  scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));

  testing::InSequence sequence;
  EXPECT_CALL(*mock_client_.get(), didAddRemoteStream(
      testing::Property(&blink::WebMediaStream::id,
                        base::UTF8ToUTF16(remote_stream_label))));
  pc_handler_->OnAddStream(remote_stream.get());
  const blink::WebMediaStream& webkit_stream = mock_client_->remote_stream();

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  webkit_stream.audioTracks(audio_tracks);
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive,
            audio_tracks[0].source().readyState());

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    webkit_stream.videoTracks(video_tracks);
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateLive,
            video_tracks[0].source().readyState());

  remote_stream->GetAudioTracks()[0]->set_state(
      webrtc::MediaStreamTrackInterface::kEnded);
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded,
            audio_tracks[0].source().readyState());

  remote_stream->GetVideoTracks()[0]->set_state(
      webrtc::MediaStreamTrackInterface::kEnded);
  EXPECT_EQ(blink::WebMediaStreamSource::ReadyStateEnded,
            video_tracks[0].source().readyState());
}

TEST_F(RTCPeerConnectionHandlerTest, RemoveAndAddAudioTrackFromRemoteStream) {
  std::string remote_stream_label("remote_stream");
  scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "audio"));

  EXPECT_CALL(*mock_client_.get(), didAddRemoteStream(
      testing::Property(&blink::WebMediaStream::id,
                        base::UTF8ToUTF16(remote_stream_label))));
  pc_handler_->OnAddStream(remote_stream.get());
  const blink::WebMediaStream& webkit_stream = mock_client_->remote_stream();

  {
    // Test in a small scope so that  |audio_tracks| don't hold on to destroyed
    // source later.
    blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
    webkit_stream.audioTracks(audio_tracks);
    EXPECT_EQ(1u, audio_tracks.size());
  }

  // Remove the Webrtc audio track from the Webrtc MediaStream.
  scoped_refptr<webrtc::AudioTrackInterface> webrtc_track =
      remote_stream->GetAudioTracks()[0].get();
  remote_stream->RemoveTrack(webrtc_track.get());

  {
    blink::WebVector<blink::WebMediaStreamTrack> modified_audio_tracks1;
    webkit_stream.audioTracks(modified_audio_tracks1);
    EXPECT_EQ(0u, modified_audio_tracks1.size());
  }

  // Add the WebRtc audio track again.
  remote_stream->AddTrack(webrtc_track.get());
  blink::WebVector<blink::WebMediaStreamTrack> modified_audio_tracks2;
  webkit_stream.audioTracks(modified_audio_tracks2);
  EXPECT_EQ(1u, modified_audio_tracks2.size());
}

TEST_F(RTCPeerConnectionHandlerTest, RemoveAndAddVideoTrackFromRemoteStream) {
  std::string remote_stream_label("remote_stream");
  scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video", "video"));

  EXPECT_CALL(*mock_client_.get(), didAddRemoteStream(
      testing::Property(&blink::WebMediaStream::id,
                        base::UTF8ToUTF16(remote_stream_label))));
  pc_handler_->OnAddStream(remote_stream.get());
  const blink::WebMediaStream& webkit_stream = mock_client_->remote_stream();

  {
    // Test in a small scope so that  |video_tracks| don't hold on to destroyed
    // source later.
    blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
    webkit_stream.videoTracks(video_tracks);
    EXPECT_EQ(1u, video_tracks.size());
  }

  // Remove the Webrtc video track from the Webrtc MediaStream.
  scoped_refptr<webrtc::VideoTrackInterface> webrtc_track =
      remote_stream->GetVideoTracks()[0].get();
  remote_stream->RemoveTrack(webrtc_track.get());
  {
    blink::WebVector<blink::WebMediaStreamTrack> modified_video_tracks1;
    webkit_stream.videoTracks(modified_video_tracks1);
    EXPECT_EQ(0u, modified_video_tracks1.size());
  }

  // Add the WebRtc video track again.
  remote_stream->AddTrack(webrtc_track.get());
  blink::WebVector<blink::WebMediaStreamTrack> modified_video_tracks2;
  webkit_stream.videoTracks(modified_video_tracks2);
  EXPECT_EQ(1u, modified_video_tracks2.size());
}

TEST_F(RTCPeerConnectionHandlerTest, OnIceCandidate) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackAddIceCandidate(pc_handler_.get(), _,
                                   PeerConnectionTracker::SOURCE_LOCAL));
  EXPECT_CALL(*mock_client_.get(), didGenerateICECandidate(_));

  scoped_ptr<webrtc::IceCandidateInterface> native_candidate(
      mock_dependency_factory_->CreateIceCandidate("mid", 1, kDummySdp));
  pc_handler_->OnIceCandidate(native_candidate.get());
  EXPECT_EQ("mid", mock_client_->candidate_mid());
  EXPECT_EQ(1, mock_client_->candidate_mlineindex());
  EXPECT_EQ(kDummySdp, mock_client_->candidate_sdp());
}

TEST_F(RTCPeerConnectionHandlerTest, OnRenegotiationNeeded) {
  testing::InSequence sequence;
  EXPECT_CALL(*mock_tracker_.get(),
              TrackOnRenegotiationNeeded(pc_handler_.get()));
  EXPECT_CALL(*mock_client_.get(), negotiationNeeded());
  pc_handler_->OnRenegotiationNeeded();
}

TEST_F(RTCPeerConnectionHandlerTest, CreateDataChannel) {
  blink::WebString label = "d1";
  EXPECT_CALL(*mock_tracker_.get(),
              TrackCreateDataChannel(pc_handler_.get(),
                                     testing::NotNull(),
                                     PeerConnectionTracker::SOURCE_LOCAL));
  scoped_ptr<blink::WebRTCDataChannelHandler> channel(
      pc_handler_->createDataChannel("d1", blink::WebRTCDataChannelInit()));
  EXPECT_TRUE(channel.get() != NULL);
  EXPECT_EQ(label, channel->label());
}

TEST_F(RTCPeerConnectionHandlerTest, CreateDtmfSender) {
  std::string stream_label = "local_stream";
  blink::WebMediaStream local_stream(CreateLocalMediaStream(stream_label));
  blink::WebMediaConstraints constraints;
  pc_handler_->addStream(local_stream, constraints);

  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  local_stream.videoTracks(tracks);

  ASSERT_LE(1ul, tracks.size());
  EXPECT_FALSE(pc_handler_->createDTMFSender(tracks[0]));

  local_stream.audioTracks(tracks);
  ASSERT_LE(1ul, tracks.size());

  EXPECT_CALL(*mock_tracker_.get(),
              TrackCreateDTMFSender(pc_handler_.get(),
                                    testing::Ref(tracks[0])));

  scoped_ptr<blink::WebRTCDTMFSenderHandler> sender(
      pc_handler_->createDTMFSender(tracks[0]));
  EXPECT_TRUE(sender.get());
}

}  // namespace content
