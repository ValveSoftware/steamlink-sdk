// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEERCONNECTION_TRACKER_H_
#define CONTENT_RENDERER_MEDIA_PEERCONNECTION_TRACKER_H_

#include <map>

#include "base/compiler_specific.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescription.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"

namespace blink {
class WebFrame;
class WebRTCICECandidate;
class WebString;
class WebRTCSessionDescription;
class WebUserMediaRequest;
}  // namespace blink

namespace webrtc {
class DataChannelInterface;
}  // namespace webrtc

namespace content {
class RTCMediaConstraints;
class RTCPeerConnectionHandler;

// This class collects data about each peer connection,
// sends it to the browser process, and handles messages
// from the browser process.
class CONTENT_EXPORT PeerConnectionTracker : public RenderProcessObserver {
 public:
  PeerConnectionTracker();
  virtual ~PeerConnectionTracker();

  enum Source {
    SOURCE_LOCAL,
    SOURCE_REMOTE
  };

  enum Action {
    ACTION_SET_LOCAL_DESCRIPTION,
    ACTION_SET_REMOTE_DESCRIPTION,
    ACTION_CREATE_OFFER,
    ACTION_CREATE_ANSWER
  };

  // RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  //
  // The following methods send an update to the browser process when a
  // PeerConnection update happens. The caller should call the Track* methods
  // after calling RegisterPeerConnection and before calling
  // UnregisterPeerConnection, otherwise the Track* call has no effect.
  //

  // Sends an update when a PeerConnection has been created in Javascript.
  // This should be called once and only once for each PeerConnection.
  // The |pc_handler| is the handler object associated with the PeerConnection,
  // the |servers| are the server configurations used to establish the
  // connection, the |constraints| are the media constraints used to initialize
  // the PeerConnection, the |frame| is the WebFrame object representing the
  // page in which the PeerConnection is created.
  void RegisterPeerConnection(
      RTCPeerConnectionHandler* pc_handler,
      const std::vector<webrtc::PeerConnectionInterface::IceServer>& servers,
      const RTCMediaConstraints& constraints,
      const blink::WebFrame* frame);

  // Sends an update when a PeerConnection has been destroyed.
  virtual void UnregisterPeerConnection(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when createOffer/createAnswer has been called.
  // The |pc_handler| is the handler object associated with the PeerConnection,
  // the |constraints| is the media constraints used to create the offer/answer.
  virtual void TrackCreateOffer(RTCPeerConnectionHandler* pc_handler,
                                const RTCMediaConstraints& constraints);
  virtual void TrackCreateAnswer(RTCPeerConnectionHandler* pc_handler,
                                 const RTCMediaConstraints& constraints);

  // Sends an update when setLocalDescription or setRemoteDescription is called.
  virtual void TrackSetSessionDescription(
      RTCPeerConnectionHandler* pc_handler,
      const blink::WebRTCSessionDescription& desc, Source source);

  // Sends an update when Ice candidates are updated.
  virtual void TrackUpdateIce(
      RTCPeerConnectionHandler* pc_handler,
      const std::vector<webrtc::PeerConnectionInterface::IceServer>& servers,
      const RTCMediaConstraints& options);

  // Sends an update when an Ice candidate is added.
  virtual void TrackAddIceCandidate(
      RTCPeerConnectionHandler* pc_handler,
      const blink::WebRTCICECandidate& candidate, Source source);

  // Sends an update when a media stream is added.
  virtual void TrackAddStream(
      RTCPeerConnectionHandler* pc_handler,
      const blink::WebMediaStream& stream, Source source);

  // Sends an update when a media stream is removed.
  virtual void TrackRemoveStream(
      RTCPeerConnectionHandler* pc_handler,
      const blink::WebMediaStream& stream, Source source);

  // Sends an update when a DataChannel is created.
  virtual void TrackCreateDataChannel(
      RTCPeerConnectionHandler* pc_handler,
      const webrtc::DataChannelInterface* data_channel, Source source);

  // Sends an update when a PeerConnection has been stopped.
  virtual void TrackStop(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when the signaling state of a PeerConnection has changed.
  virtual void TrackSignalingStateChange(
      RTCPeerConnectionHandler* pc_handler,
      blink::WebRTCPeerConnectionHandlerClient::SignalingState state);

  // Sends an update when the Ice connection state
  // of a PeerConnection has changed.
  virtual void TrackIceConnectionStateChange(
      RTCPeerConnectionHandler* pc_handler,
      blink::WebRTCPeerConnectionHandlerClient::ICEConnectionState state);

  // Sends an update when the Ice gathering state
  // of a PeerConnection has changed.
  virtual void TrackIceGatheringStateChange(
      RTCPeerConnectionHandler* pc_handler,
      blink::WebRTCPeerConnectionHandlerClient::ICEGatheringState state);

  // Sends an update when the SetSessionDescription or CreateOffer or
  // CreateAnswer callbacks are called.
  virtual void TrackSessionDescriptionCallback(
      RTCPeerConnectionHandler* pc_handler, Action action,
      const std::string& type, const std::string& value);

  // Sends an update when onRenegotiationNeeded is called.
  virtual void TrackOnRenegotiationNeeded(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when a DTMFSender is created.
  virtual void TrackCreateDTMFSender(
      RTCPeerConnectionHandler* pc_handler,
      const blink::WebMediaStreamTrack& track);

  // Sends an update when getUserMedia is called.
  virtual void TrackGetUserMedia(
      const blink::WebUserMediaRequest& user_media_request);

 private:
  // Assign a local ID to a peer connection so that the browser process can
  // uniquely identify a peer connection in the renderer process.
  int GetNextLocalID();

  // IPC Message handler for getting all stats.
  void OnGetAllStats();

  void SendPeerConnectionUpdate(RTCPeerConnectionHandler* pc_handler,
                                const std::string& callback_type,
                                const std::string& value);

  // This map stores the local ID assigned to each RTCPeerConnectionHandler.
  typedef std::map<RTCPeerConnectionHandler*, int> PeerConnectionIdMap;
  PeerConnectionIdMap peer_connection_id_map_;

  // This keeps track of the next available local ID.
  int next_lid_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionTracker);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_PEERCONNECTION_TRACKER_H_
