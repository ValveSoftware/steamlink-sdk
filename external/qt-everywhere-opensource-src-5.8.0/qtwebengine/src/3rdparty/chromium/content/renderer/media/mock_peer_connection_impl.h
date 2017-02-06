// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"

namespace content {

class MockPeerConnectionDependencyFactory;
class MockStreamCollection;

class MockPeerConnectionImpl : public webrtc::PeerConnectionInterface {
 public:
  explicit MockPeerConnectionImpl(MockPeerConnectionDependencyFactory* factory,
                                  webrtc::PeerConnectionObserver* observer);

  // PeerConnectionInterface implementation.
  rtc::scoped_refptr<webrtc::StreamCollectionInterface>
      local_streams() override;
  rtc::scoped_refptr<webrtc::StreamCollectionInterface>
      remote_streams() override;
  bool AddStream(
      webrtc::MediaStreamInterface* local_stream) override;
  void RemoveStream(
      webrtc::MediaStreamInterface* local_stream) override;
  rtc::scoped_refptr<webrtc::DtmfSenderInterface>
      CreateDtmfSender(webrtc::AudioTrackInterface* track) override;
  rtc::scoped_refptr<webrtc::DataChannelInterface>
      CreateDataChannel(const std::string& label,
                        const webrtc::DataChannelInit* config) override;
  bool GetStats(webrtc::StatsObserver* observer,
                webrtc::MediaStreamTrackInterface* track,
                StatsOutputLevel level) override;

  // Set Call this function to make sure next call to GetStats fail.
  void SetGetStatsResult(bool result) { getstats_result_ = result; }

  SignalingState signaling_state() override {
    NOTIMPLEMENTED();
    return PeerConnectionInterface::kStable;
  }
  IceState ice_state() override {
    NOTIMPLEMENTED();
    return PeerConnectionInterface::kIceNew;
  }
  IceConnectionState ice_connection_state() override {
    NOTIMPLEMENTED();
    return PeerConnectionInterface::kIceConnectionNew;
  }
  IceGatheringState ice_gathering_state() override {
    NOTIMPLEMENTED();
    return PeerConnectionInterface::kIceGatheringNew;
  }

  MOCK_METHOD0(Close, void());

  const webrtc::SessionDescriptionInterface* local_description() const override;
  const webrtc::SessionDescriptionInterface* remote_description()
      const override;

  // JSEP01 APIs
  void CreateOffer(webrtc::CreateSessionDescriptionObserver* observer,
                   const RTCOfferAnswerOptions& options) override;
  void CreateAnswer(webrtc::CreateSessionDescriptionObserver* observer,
                    const RTCOfferAnswerOptions& options) override;
  MOCK_METHOD2(SetLocalDescription,
               void(webrtc::SetSessionDescriptionObserver* observer,
                    webrtc::SessionDescriptionInterface* desc));
  void SetLocalDescriptionWorker(
      webrtc::SetSessionDescriptionObserver* observer,
      webrtc::SessionDescriptionInterface* desc) ;
  MOCK_METHOD2(SetRemoteDescription,
               void(webrtc::SetSessionDescriptionObserver* observer,
                    webrtc::SessionDescriptionInterface* desc));
  void SetRemoteDescriptionWorker(
      webrtc::SetSessionDescriptionObserver* observer,
      webrtc::SessionDescriptionInterface* desc);
  bool UpdateIce(const IceServers& configuration) override;
  bool AddIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void RegisterUMAObserver(webrtc::UMAObserver* observer) override;

  void AddRemoteStream(webrtc::MediaStreamInterface* stream);

  const std::string& stream_label() const { return stream_label_; }
  bool hint_audio() const { return hint_audio_; }
  bool hint_video() const { return hint_video_; }
  const std::string& description_sdp() const { return description_sdp_; }
  const std::string& sdp_mid() const { return sdp_mid_; }
  int sdp_mline_index() const { return sdp_mline_index_; }
  const std::string& ice_sdp() const { return ice_sdp_; }
  webrtc::SessionDescriptionInterface* created_session_description() const {
    return created_sessiondescription_.get();
  }
  webrtc::PeerConnectionObserver* observer() {
    return observer_;
  }
  static const char kDummyOffer[];
  static const char kDummyAnswer[];

 protected:
  virtual ~MockPeerConnectionImpl();

 private:
  // Used for creating MockSessionDescription.
  MockPeerConnectionDependencyFactory* dependency_factory_;

  std::string stream_label_;
  rtc::scoped_refptr<MockStreamCollection> local_streams_;
  rtc::scoped_refptr<MockStreamCollection> remote_streams_;
  std::unique_ptr<webrtc::SessionDescriptionInterface> local_desc_;
  std::unique_ptr<webrtc::SessionDescriptionInterface> remote_desc_;
  std::unique_ptr<webrtc::SessionDescriptionInterface>
      created_sessiondescription_;
  bool hint_audio_;
  bool hint_video_;
  bool getstats_result_;
  std::string description_sdp_;
  std::string sdp_mid_;
  int sdp_mline_index_;
  std::string ice_sdp_;
  webrtc::PeerConnectionObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(MockPeerConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_
