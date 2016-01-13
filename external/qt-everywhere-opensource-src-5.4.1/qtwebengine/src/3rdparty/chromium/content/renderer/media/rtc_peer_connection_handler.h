// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/media_stream_track_metrics.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/public/platform/WebRTCStatsRequest.h"
#include "third_party/WebKit/public/platform/WebRTCStatsResponse.h"

namespace blink {
class WebFrame;
class WebRTCDataChannelHandler;
}

namespace content {

class PeerConnectionDependencyFactory;
class PeerConnectionTracker;
class RemoteMediaStreamImpl;
class WebRtcMediaStreamAdapter;

// Mockable wrapper for blink::WebRTCStatsResponse
class CONTENT_EXPORT LocalRTCStatsResponse
    : public NON_EXPORTED_BASE(talk_base::RefCountInterface) {
 public:
  explicit LocalRTCStatsResponse(const blink::WebRTCStatsResponse& impl)
      : impl_(impl) {
  }

  virtual blink::WebRTCStatsResponse webKitStatsResponse() const;
  virtual size_t addReport(blink::WebString type, blink::WebString id,
                           double timestamp);
  virtual void addStatistic(size_t report,
                            blink::WebString name, blink::WebString value);

 protected:
  virtual ~LocalRTCStatsResponse() {}
  // Constructor for creating mocks.
  LocalRTCStatsResponse() {}

 private:
  blink::WebRTCStatsResponse impl_;
};

// Mockable wrapper for blink::WebRTCStatsRequest
class CONTENT_EXPORT LocalRTCStatsRequest
    : public NON_EXPORTED_BASE(talk_base::RefCountInterface) {
 public:
  explicit LocalRTCStatsRequest(blink::WebRTCStatsRequest impl);
  // Constructor for testing.
  LocalRTCStatsRequest();

  virtual bool hasSelector() const;
  virtual blink::WebMediaStreamTrack component() const;
  virtual void requestSucceeded(const LocalRTCStatsResponse* response);
  virtual scoped_refptr<LocalRTCStatsResponse> createResponse();

 protected:
  virtual ~LocalRTCStatsRequest();

 private:
  blink::WebRTCStatsRequest impl_;
  talk_base::scoped_refptr<LocalRTCStatsResponse> response_;
};

// RTCPeerConnectionHandler is a delegate for the RTC PeerConnection API
// messages going between WebKit and native PeerConnection in libjingle. It's
// owned by WebKit.
// WebKit calls all of these methods on the main render thread.
// Callbacks to the webrtc::PeerConnectionObserver implementation also occur on
// the main render thread.
class CONTENT_EXPORT RTCPeerConnectionHandler
    : NON_EXPORTED_BASE(public blink::WebRTCPeerConnectionHandler),
      NON_EXPORTED_BASE(public webrtc::PeerConnectionObserver) {
 public:
  RTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client,
      PeerConnectionDependencyFactory* dependency_factory);
  virtual ~RTCPeerConnectionHandler();

  // Destroy all existing RTCPeerConnectionHandler objects.
  static void DestructAllHandlers();

  void associateWithFrame(blink::WebFrame* frame);

  // Initialize method only used for unit test.
  bool InitializeForTest(
      const blink::WebRTCConfiguration& server_configuration,
      const blink::WebMediaConstraints& options,
      PeerConnectionTracker* peer_connection_tracker);

  // blink::WebRTCPeerConnectionHandler implementation
  virtual bool initialize(
      const blink::WebRTCConfiguration& server_configuration,
      const blink::WebMediaConstraints& options) OVERRIDE;

  virtual void createOffer(
      const blink::WebRTCSessionDescriptionRequest& request,
      const blink::WebMediaConstraints& options) OVERRIDE;
  virtual void createAnswer(
      const blink::WebRTCSessionDescriptionRequest& request,
      const blink::WebMediaConstraints& options) OVERRIDE;

  virtual void setLocalDescription(
      const blink::WebRTCVoidRequest& request,
      const blink::WebRTCSessionDescription& description) OVERRIDE;
  virtual void setRemoteDescription(
        const blink::WebRTCVoidRequest& request,
        const blink::WebRTCSessionDescription& description) OVERRIDE;

  virtual blink::WebRTCSessionDescription localDescription()
      OVERRIDE;
  virtual blink::WebRTCSessionDescription remoteDescription()
      OVERRIDE;

  virtual bool updateICE(
      const blink::WebRTCConfiguration& server_configuration,
      const blink::WebMediaConstraints& options) OVERRIDE;
  virtual bool addICECandidate(
      const blink::WebRTCICECandidate& candidate) OVERRIDE;
  virtual bool addICECandidate(
      const blink::WebRTCVoidRequest& request,
      const blink::WebRTCICECandidate& candidate) OVERRIDE;
  virtual void OnaddICECandidateResult(const blink::WebRTCVoidRequest& request,
                                       bool result);

  virtual bool addStream(
      const blink::WebMediaStream& stream,
      const blink::WebMediaConstraints& options) OVERRIDE;
  virtual void removeStream(
      const blink::WebMediaStream& stream) OVERRIDE;
  virtual void getStats(
      const blink::WebRTCStatsRequest& request) OVERRIDE;
  virtual blink::WebRTCDataChannelHandler* createDataChannel(
      const blink::WebString& label,
      const blink::WebRTCDataChannelInit& init) OVERRIDE;
  virtual blink::WebRTCDTMFSenderHandler* createDTMFSender(
      const blink::WebMediaStreamTrack& track) OVERRIDE;
  virtual void stop() OVERRIDE;

  // webrtc::PeerConnectionObserver implementation
  virtual void OnError() OVERRIDE;
  // Triggered when the SignalingState changed.
  virtual void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) OVERRIDE;
  virtual void OnAddStream(webrtc::MediaStreamInterface* stream) OVERRIDE;
  virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream) OVERRIDE;
  virtual void OnIceCandidate(
      const webrtc::IceCandidateInterface* candidate) OVERRIDE;
  virtual void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) OVERRIDE;
  virtual void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) OVERRIDE;

  virtual void OnDataChannel(
      webrtc::DataChannelInterface* data_channel) OVERRIDE;
  virtual void OnRenegotiationNeeded() OVERRIDE;

  // Delegate functions to allow for mocking of WebKit interfaces.
  // getStats takes ownership of request parameter.
  virtual void getStats(LocalRTCStatsRequest* request);

  // Calls GetStats on |native_peer_connection_|.
  void GetStats(webrtc::StatsObserver* observer,
                webrtc::MediaStreamTrackInterface* track,
                webrtc::PeerConnectionInterface::StatsOutputLevel level);

  PeerConnectionTracker* peer_connection_tracker();

 protected:
  webrtc::PeerConnectionInterface* native_peer_connection() {
    return native_peer_connection_.get();
  }

 private:
  webrtc::SessionDescriptionInterface* CreateNativeSessionDescription(
      const blink::WebRTCSessionDescription& description,
      webrtc::SdpParseError* error);

  // |client_| is a weak pointer, and is valid until stop() has returned.
  blink::WebRTCPeerConnectionHandlerClient* client_;

  // |dependency_factory_| is a raw pointer, and is valid for the lifetime of
  // RenderThreadImpl.
  PeerConnectionDependencyFactory* dependency_factory_;

  blink::WebFrame* frame_;

  ScopedVector<WebRtcMediaStreamAdapter> local_streams_;

  PeerConnectionTracker* peer_connection_tracker_;

  MediaStreamTrackMetrics track_metrics_;

  // Counter for a UMA stat reported at destruction time.
  int num_data_channels_created_;

  // |native_peer_connection_| is the libjingle native PeerConnection object.
  scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection_;

  typedef std::map<webrtc::MediaStreamInterface*,
      content::RemoteMediaStreamImpl*> RemoteStreamMap;
  RemoteStreamMap remote_streams_;
  scoped_refptr<webrtc::UMAObserver> uma_observer_;

  DISALLOW_COPY_AND_ASSIGN(RTCPeerConnectionHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_
