// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_
#define CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"

namespace content {

class MockPeerConnectionDependencyFactory;
class MockStreamCollection;

class MockPeerConnectionImpl : public webrtc::PeerConnectionInterface {
 public:
  explicit MockPeerConnectionImpl(MockPeerConnectionDependencyFactory* factory);

  // PeerConnectionInterface implementation.
  virtual talk_base::scoped_refptr<webrtc::StreamCollectionInterface>
      local_streams() OVERRIDE;
  virtual talk_base::scoped_refptr<webrtc::StreamCollectionInterface>
      remote_streams() OVERRIDE;
  virtual bool AddStream(
      webrtc::MediaStreamInterface* local_stream,
      const webrtc::MediaConstraintsInterface* constraints) OVERRIDE;
  virtual void RemoveStream(
      webrtc::MediaStreamInterface* local_stream) OVERRIDE;
  virtual talk_base::scoped_refptr<webrtc::DtmfSenderInterface>
      CreateDtmfSender(webrtc::AudioTrackInterface* track) OVERRIDE;
  virtual talk_base::scoped_refptr<webrtc::DataChannelInterface>
      CreateDataChannel(const std::string& label,
                        const webrtc::DataChannelInit* config) OVERRIDE;

  virtual bool GetStats(webrtc::StatsObserver* observer,
                        webrtc::MediaStreamTrackInterface* track) {
    return false;
  }
  virtual bool GetStats(webrtc::StatsObserver* observer,
                        webrtc::MediaStreamTrackInterface* track,
                        StatsOutputLevel level) OVERRIDE;

  // Set Call this function to make sure next call to GetStats fail.
  void SetGetStatsResult(bool result) { getstats_result_ = result; }

  virtual SignalingState signaling_state() OVERRIDE {
    NOTIMPLEMENTED();
    return PeerConnectionInterface::kStable;
  }
  virtual IceState ice_state() OVERRIDE {
    NOTIMPLEMENTED();
    return PeerConnectionInterface::kIceNew;
  }
  virtual IceConnectionState ice_connection_state() OVERRIDE {
    NOTIMPLEMENTED();
    return PeerConnectionInterface::kIceConnectionNew;
  }
  virtual IceGatheringState ice_gathering_state() OVERRIDE {
    NOTIMPLEMENTED();
    return PeerConnectionInterface::kIceGatheringNew;
  }
  virtual void Close() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual const webrtc::SessionDescriptionInterface* local_description()
      const OVERRIDE;
  virtual const webrtc::SessionDescriptionInterface* remote_description()
      const OVERRIDE;

  // JSEP01 APIs
  virtual void CreateOffer(
      webrtc::CreateSessionDescriptionObserver* observer,
      const webrtc::MediaConstraintsInterface* constraints) OVERRIDE;
  virtual void CreateAnswer(
      webrtc::CreateSessionDescriptionObserver* observer,
      const webrtc::MediaConstraintsInterface* constraints) OVERRIDE;
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
  virtual bool UpdateIce(
      const IceServers& configuration,
      const webrtc::MediaConstraintsInterface* constraints) OVERRIDE;
  virtual bool AddIceCandidate(
      const webrtc::IceCandidateInterface* candidate) OVERRIDE;
  virtual void RegisterUMAObserver(webrtc::UMAObserver* observer) OVERRIDE;

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
  static const char kDummyOffer[];
  static const char kDummyAnswer[];

 protected:
  virtual ~MockPeerConnectionImpl();

 private:
  // Used for creating MockSessionDescription.
  MockPeerConnectionDependencyFactory* dependency_factory_;

  std::string stream_label_;
  talk_base::scoped_refptr<MockStreamCollection> local_streams_;
  talk_base::scoped_refptr<MockStreamCollection> remote_streams_;
  scoped_ptr<webrtc::SessionDescriptionInterface> local_desc_;
  scoped_ptr<webrtc::SessionDescriptionInterface> remote_desc_;
  scoped_ptr<webrtc::SessionDescriptionInterface> created_sessiondescription_;
  bool hint_audio_;
  bool hint_video_;
  bool getstats_result_;
  std::string description_sdp_;
  std::string sdp_mid_;
  int sdp_mline_index_;
  std::string ice_sdp_;

  DISALLOW_COPY_AND_ASSIGN(MockPeerConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_PEER_CONNECTION_IMPL_H_
