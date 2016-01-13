// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/renderer/media/peer_connection_tracker.h"

#include "base/strings/utf_string_conversions.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/rtc_peer_connection_handler.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebRTCICECandidate.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandlerClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebUserMediaRequest.h"

using std::string;
using webrtc::MediaConstraintsInterface;
using blink::WebRTCPeerConnectionHandlerClient;

namespace content {

static string SerializeServers(
    const std::vector<webrtc::PeerConnectionInterface::IceServer>& servers) {
  string result = "[";
  for (size_t i = 0; i < servers.size(); ++i) {
    result += servers[i].uri;
    if (i != servers.size() - 1)
      result += ", ";
  }
  result += "]";
  return result;
}

static string SerializeMediaConstraints(
    const RTCMediaConstraints& constraints) {
  string result;
  MediaConstraintsInterface::Constraints mandatory = constraints.GetMandatory();
  if (!mandatory.empty()) {
    result += "mandatory: {";
    for (size_t i = 0; i < mandatory.size(); ++i) {
      result += mandatory[i].key + ":" + mandatory[i].value;
      if (i != mandatory.size() - 1)
        result += ", ";
    }
    result += "}";
  }
  MediaConstraintsInterface::Constraints optional = constraints.GetOptional();
  if (!optional.empty()) {
    if (!result.empty())
      result += ", ";
    result += "optional: {";
    for (size_t i = 0; i < optional.size(); ++i) {
      result += optional[i].key + ":" + optional[i].value;
      if (i != optional.size() - 1)
        result += ", ";
    }
    result += "}";
  }
  return result;
}

static string SerializeMediaStreamComponent(
    const blink::WebMediaStreamTrack component) {
  string id = base::UTF16ToUTF8(component.source().id());
  return id;
}

static string SerializeMediaDescriptor(
    const blink::WebMediaStream& stream) {
  string label = base::UTF16ToUTF8(stream.id());
  string result = "label: " + label;
  blink::WebVector<blink::WebMediaStreamTrack> tracks;
  stream.audioTracks(tracks);
  if (!tracks.isEmpty()) {
    result += ", audio: [";
    for (size_t i = 0; i < tracks.size(); ++i) {
      result += SerializeMediaStreamComponent(tracks[i]);
      if (i != tracks.size() - 1)
        result += ", ";
    }
    result += "]";
  }
  stream.videoTracks(tracks);
  if (!tracks.isEmpty()) {
    result += ", video: [";
    for (size_t i = 0; i < tracks.size(); ++i) {
      result += SerializeMediaStreamComponent(tracks[i]);
      if (i != tracks.size() - 1)
        result += ", ";
    }
    result += "]";
  }
  return result;
}

#define GET_STRING_OF_STATE(state)                \
  case WebRTCPeerConnectionHandlerClient::state:  \
    result = #state;                              \
    break;

static string GetSignalingStateString(
    WebRTCPeerConnectionHandlerClient::SignalingState state) {
  string result;
  switch (state) {
    GET_STRING_OF_STATE(SignalingStateStable)
    GET_STRING_OF_STATE(SignalingStateHaveLocalOffer)
    GET_STRING_OF_STATE(SignalingStateHaveRemoteOffer)
    GET_STRING_OF_STATE(SignalingStateHaveLocalPrAnswer)
    GET_STRING_OF_STATE(SignalingStateHaveRemotePrAnswer)
    GET_STRING_OF_STATE(SignalingStateClosed)
    default:
      NOTREACHED();
      break;
  }
  return result;
}

static string GetIceConnectionStateString(
    WebRTCPeerConnectionHandlerClient::ICEConnectionState state) {
  string result;
  switch (state) {
    GET_STRING_OF_STATE(ICEConnectionStateStarting)
    GET_STRING_OF_STATE(ICEConnectionStateChecking)
    GET_STRING_OF_STATE(ICEConnectionStateConnected)
    GET_STRING_OF_STATE(ICEConnectionStateCompleted)
    GET_STRING_OF_STATE(ICEConnectionStateFailed)
    GET_STRING_OF_STATE(ICEConnectionStateDisconnected)
    GET_STRING_OF_STATE(ICEConnectionStateClosed)
    default:
      NOTREACHED();
      break;
  }
  return result;
}

static string GetIceGatheringStateString(
    WebRTCPeerConnectionHandlerClient::ICEGatheringState state) {
  string result;
  switch (state) {
    GET_STRING_OF_STATE(ICEGatheringStateNew)
    GET_STRING_OF_STATE(ICEGatheringStateGathering)
    GET_STRING_OF_STATE(ICEGatheringStateComplete)
    default:
      NOTREACHED();
      break;
  }
  return result;
}

// Builds a DictionaryValue from the StatsReport.
// The caller takes the ownership of the returned value.
// Note:
// The format must be consistent with what webrtc_internals.js expects.
// If you change it here, you must change webrtc_internals.js as well.
static base::DictionaryValue* GetDictValueStats(
    const webrtc::StatsReport& report) {
  if (report.values.empty())
    return NULL;

  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetDouble("timestamp", report.timestamp);

  base::ListValue* values = new base::ListValue();
  dict->Set("values", values);

  for (size_t i = 0; i < report.values.size(); ++i) {
    values->AppendString(report.values[i].name);
    values->AppendString(report.values[i].value);
  }
  return dict;
}

// Builds a DictionaryValue from the StatsReport.
// The caller takes the ownership of the returned value.
static base::DictionaryValue* GetDictValue(const webrtc::StatsReport& report) {
  scoped_ptr<base::DictionaryValue> stats, result;

  stats.reset(GetDictValueStats(report));
  if (!stats)
    return NULL;

  result.reset(new base::DictionaryValue());
  // Note:
  // The format must be consistent with what webrtc_internals.js expects.
  // If you change it here, you must change webrtc_internals.js as well.
  result->Set("stats", stats.release());
  result->SetString("id", report.id);
  result->SetString("type", report.type);

  return result.release();
}

class InternalStatsObserver : public webrtc::StatsObserver {
 public:
  InternalStatsObserver(int lid)
      : lid_(lid){}

  virtual void OnComplete(
      const std::vector<webrtc::StatsReport>& reports) OVERRIDE {
    base::ListValue list;

    for (size_t i = 0; i < reports.size(); ++i) {
      base::DictionaryValue* report = GetDictValue(reports[i]);
      if (report)
        list.Append(report);
    }

    if (!list.empty())
      RenderThreadImpl::current()->Send(
          new PeerConnectionTrackerHost_AddStats(lid_, list));
  }

 protected:
  virtual ~InternalStatsObserver() {}

 private:
  int lid_;
};

PeerConnectionTracker::PeerConnectionTracker() : next_lid_(1) {
}

PeerConnectionTracker::~PeerConnectionTracker() {
}

bool PeerConnectionTracker::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PeerConnectionTracker, message)
    IPC_MESSAGE_HANDLER(PeerConnectionTracker_GetAllStats, OnGetAllStats)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PeerConnectionTracker::OnGetAllStats() {
  for (PeerConnectionIdMap::iterator it = peer_connection_id_map_.begin();
       it != peer_connection_id_map_.end(); ++it) {

    talk_base::scoped_refptr<InternalStatsObserver> observer(
        new talk_base::RefCountedObject<InternalStatsObserver>(it->second));

    it->first->GetStats(
        observer,
        NULL,
        webrtc::PeerConnectionInterface::kStatsOutputLevelDebug);
  }
}

void PeerConnectionTracker::RegisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler,
    const std::vector<webrtc::PeerConnectionInterface::IceServer>& servers,
    const RTCMediaConstraints& constraints,
    const blink::WebFrame* frame) {
  DVLOG(1) << "PeerConnectionTracker::RegisterPeerConnection()";
  PeerConnectionInfo info;

  info.lid = GetNextLocalID();
  info.servers = SerializeServers(servers);
  info.constraints = SerializeMediaConstraints(constraints);
  info.url = frame->document().url().spec();
  RenderThreadImpl::current()->Send(
      new PeerConnectionTrackerHost_AddPeerConnection(info));

  DCHECK(peer_connection_id_map_.find(pc_handler) ==
         peer_connection_id_map_.end());
  peer_connection_id_map_[pc_handler] = info.lid;
}

void PeerConnectionTracker::UnregisterPeerConnection(
    RTCPeerConnectionHandler* pc_handler) {
  DVLOG(1) << "PeerConnectionTracker::UnregisterPeerConnection()";

  std::map<RTCPeerConnectionHandler*, int>::iterator it =
      peer_connection_id_map_.find(pc_handler);

  if (it == peer_connection_id_map_.end()) {
    // The PeerConnection might not have been registered if its initilization
    // failed.
    return;
  }

  RenderThreadImpl::current()->Send(
      new PeerConnectionTrackerHost_RemovePeerConnection(it->second));

  peer_connection_id_map_.erase(it);
}

void PeerConnectionTracker::TrackCreateOffer(
    RTCPeerConnectionHandler* pc_handler,
    const RTCMediaConstraints& constraints) {
  SendPeerConnectionUpdate(
      pc_handler, "createOffer",
      "constraints: {" + SerializeMediaConstraints(constraints) + "}");
}

void PeerConnectionTracker::TrackCreateAnswer(
    RTCPeerConnectionHandler* pc_handler,
    const RTCMediaConstraints& constraints) {
  SendPeerConnectionUpdate(
      pc_handler, "createAnswer",
      "constraints: {" + SerializeMediaConstraints(constraints) + "}");
}

void PeerConnectionTracker::TrackSetSessionDescription(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebRTCSessionDescription& desc,
    Source source) {
  string sdp = base::UTF16ToUTF8(desc.sdp());
  string type = base::UTF16ToUTF8(desc.type());

  string value = "type: " + type + ", sdp: " + sdp;
  SendPeerConnectionUpdate(
      pc_handler,
      source == SOURCE_LOCAL ? "setLocalDescription" : "setRemoteDescription",
      value);
}

void PeerConnectionTracker::TrackUpdateIce(
      RTCPeerConnectionHandler* pc_handler,
      const std::vector<webrtc::PeerConnectionInterface::IceServer>& servers,
      const RTCMediaConstraints& options) {
  string servers_string = "servers: " + SerializeServers(servers);
  string constraints =
      "constraints: {" + SerializeMediaConstraints(options) + "}";

  SendPeerConnectionUpdate(
      pc_handler, "updateIce", servers_string + ", " + constraints);
}

void PeerConnectionTracker::TrackAddIceCandidate(
      RTCPeerConnectionHandler* pc_handler,
      const blink::WebRTCICECandidate& candidate,
      Source source) {
  string value = "mid: " + base::UTF16ToUTF8(candidate.sdpMid()) + ", " +
                 "candidate: " + base::UTF16ToUTF8(candidate.candidate());
  SendPeerConnectionUpdate(
      pc_handler,
      source == SOURCE_LOCAL ? "onIceCandidate" : "addIceCandidate", value);
}

void PeerConnectionTracker::TrackAddStream(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebMediaStream& stream,
    Source source){
  SendPeerConnectionUpdate(
      pc_handler, source == SOURCE_LOCAL ? "addStream" : "onAddStream",
      SerializeMediaDescriptor(stream));
}

void PeerConnectionTracker::TrackRemoveStream(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebMediaStream& stream,
    Source source){
  SendPeerConnectionUpdate(
      pc_handler, source == SOURCE_LOCAL ? "removeStream" : "onRemoveStream",
      SerializeMediaDescriptor(stream));
}

void PeerConnectionTracker::TrackCreateDataChannel(
    RTCPeerConnectionHandler* pc_handler,
    const webrtc::DataChannelInterface* data_channel,
    Source source) {
  string value = "label: " + data_channel->label() +
                 ", reliable: " + (data_channel->reliable() ? "true" : "false");
  SendPeerConnectionUpdate(
      pc_handler,
      source == SOURCE_LOCAL ? "createLocalDataChannel" : "onRemoteDataChannel",
      value);
}

void PeerConnectionTracker::TrackStop(RTCPeerConnectionHandler* pc_handler) {
  SendPeerConnectionUpdate(pc_handler, "stop", std::string());
}

void PeerConnectionTracker::TrackSignalingStateChange(
      RTCPeerConnectionHandler* pc_handler,
      WebRTCPeerConnectionHandlerClient::SignalingState state) {
  SendPeerConnectionUpdate(
      pc_handler, "signalingStateChange", GetSignalingStateString(state));
}

void PeerConnectionTracker::TrackIceConnectionStateChange(
      RTCPeerConnectionHandler* pc_handler,
      WebRTCPeerConnectionHandlerClient::ICEConnectionState state) {
  SendPeerConnectionUpdate(
      pc_handler, "iceConnectionStateChange",
      GetIceConnectionStateString(state));
}

void PeerConnectionTracker::TrackIceGatheringStateChange(
      RTCPeerConnectionHandler* pc_handler,
      WebRTCPeerConnectionHandlerClient::ICEGatheringState state) {
  SendPeerConnectionUpdate(
      pc_handler, "iceGatheringStateChange",
      GetIceGatheringStateString(state));
}

void PeerConnectionTracker::TrackSessionDescriptionCallback(
    RTCPeerConnectionHandler* pc_handler, Action action,
    const string& callback_type, const string& value) {
  string update_type;
  switch (action) {
    case ACTION_SET_LOCAL_DESCRIPTION:
      update_type = "setLocalDescription";
      break;
    case ACTION_SET_REMOTE_DESCRIPTION:
      update_type = "setRemoteDescription";
      break;
    case ACTION_CREATE_OFFER:
      update_type = "createOffer";
      break;
    case ACTION_CREATE_ANSWER:
      update_type = "createAnswer";
      break;
    default:
      NOTREACHED();
      break;
  }
  update_type += callback_type;

  SendPeerConnectionUpdate(pc_handler, update_type, value);
}

void PeerConnectionTracker::TrackOnRenegotiationNeeded(
    RTCPeerConnectionHandler* pc_handler) {
  SendPeerConnectionUpdate(pc_handler, "onRenegotiationNeeded", std::string());
}

void PeerConnectionTracker::TrackCreateDTMFSender(
    RTCPeerConnectionHandler* pc_handler,
    const blink::WebMediaStreamTrack& track) {
  SendPeerConnectionUpdate(pc_handler, "createDTMFSender",
                           base::UTF16ToUTF8(track.id()));
}

void PeerConnectionTracker::TrackGetUserMedia(
    const blink::WebUserMediaRequest& user_media_request) {
  RTCMediaConstraints audio_constraints(user_media_request.audioConstraints());
  RTCMediaConstraints video_constraints(user_media_request.videoConstraints());

  RenderThreadImpl::current()->Send(new PeerConnectionTrackerHost_GetUserMedia(
      user_media_request.securityOrigin().toString().utf8(),
      user_media_request.audio(),
      user_media_request.video(),
      SerializeMediaConstraints(audio_constraints),
      SerializeMediaConstraints(video_constraints)));
}

int PeerConnectionTracker::GetNextLocalID() {
  return next_lid_++;
}

void PeerConnectionTracker::SendPeerConnectionUpdate(
    RTCPeerConnectionHandler* pc_handler,
    const std::string& type,
    const std::string& value) {
  if (peer_connection_id_map_.find(pc_handler) == peer_connection_id_map_.end())
    return;

  RenderThreadImpl::current()->Send(
      new PeerConnectionTrackerHost_UpdatePeerConnection(
          peer_connection_id_map_[pc_handler], type, value));
}

}  // namespace content
