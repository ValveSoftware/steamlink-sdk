// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_peer_connection_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/remote_media_stream_impl.h"
#include "content/renderer/media/rtc_data_channel_handler.h"
#include "content/renderer/media/rtc_dtmf_sender_handler.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebRTCConfiguration.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelInit.h"
#include "third_party/WebKit/public/platform/WebRTCICECandidate.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescription.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescriptionRequest.h"
#include "third_party/WebKit/public/platform/WebRTCVoidRequest.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

// Converter functions from libjingle types to WebKit types.
blink::WebRTCPeerConnectionHandlerClient::ICEGatheringState
GetWebKitIceGatheringState(
    webrtc::PeerConnectionInterface::IceGatheringState state) {
  using blink::WebRTCPeerConnectionHandlerClient;
  switch (state) {
    case webrtc::PeerConnectionInterface::kIceGatheringNew:
      return WebRTCPeerConnectionHandlerClient::ICEGatheringStateNew;
    case webrtc::PeerConnectionInterface::kIceGatheringGathering:
      return WebRTCPeerConnectionHandlerClient::ICEGatheringStateGathering;
    case webrtc::PeerConnectionInterface::kIceGatheringComplete:
      return WebRTCPeerConnectionHandlerClient::ICEGatheringStateComplete;
    default:
      NOTREACHED();
      return WebRTCPeerConnectionHandlerClient::ICEGatheringStateNew;
  }
}

static blink::WebRTCPeerConnectionHandlerClient::ICEConnectionState
GetWebKitIceConnectionState(
    webrtc::PeerConnectionInterface::IceConnectionState ice_state) {
  using blink::WebRTCPeerConnectionHandlerClient;
  switch (ice_state) {
    case webrtc::PeerConnectionInterface::kIceConnectionNew:
      return WebRTCPeerConnectionHandlerClient::ICEConnectionStateStarting;
    case webrtc::PeerConnectionInterface::kIceConnectionChecking:
      return WebRTCPeerConnectionHandlerClient::ICEConnectionStateChecking;
    case webrtc::PeerConnectionInterface::kIceConnectionConnected:
      return WebRTCPeerConnectionHandlerClient::ICEConnectionStateConnected;
    case webrtc::PeerConnectionInterface::kIceConnectionCompleted:
      return WebRTCPeerConnectionHandlerClient::ICEConnectionStateCompleted;
    case webrtc::PeerConnectionInterface::kIceConnectionFailed:
      return WebRTCPeerConnectionHandlerClient::ICEConnectionStateFailed;
    case webrtc::PeerConnectionInterface::kIceConnectionDisconnected:
      return WebRTCPeerConnectionHandlerClient::ICEConnectionStateDisconnected;
    case webrtc::PeerConnectionInterface::kIceConnectionClosed:
      return WebRTCPeerConnectionHandlerClient::ICEConnectionStateClosed;
    default:
      NOTREACHED();
      return WebRTCPeerConnectionHandlerClient::ICEConnectionStateClosed;
  }
}

static blink::WebRTCPeerConnectionHandlerClient::SignalingState
GetWebKitSignalingState(webrtc::PeerConnectionInterface::SignalingState state) {
  using blink::WebRTCPeerConnectionHandlerClient;
  switch (state) {
    case webrtc::PeerConnectionInterface::kStable:
      return WebRTCPeerConnectionHandlerClient::SignalingStateStable;
    case webrtc::PeerConnectionInterface::kHaveLocalOffer:
      return WebRTCPeerConnectionHandlerClient::SignalingStateHaveLocalOffer;
    case webrtc::PeerConnectionInterface::kHaveLocalPrAnswer:
      return WebRTCPeerConnectionHandlerClient::SignalingStateHaveLocalPrAnswer;
    case webrtc::PeerConnectionInterface::kHaveRemoteOffer:
      return WebRTCPeerConnectionHandlerClient::SignalingStateHaveRemoteOffer;
    case webrtc::PeerConnectionInterface::kHaveRemotePrAnswer:
      return
          WebRTCPeerConnectionHandlerClient::SignalingStateHaveRemotePrAnswer;
    case webrtc::PeerConnectionInterface::kClosed:
      return WebRTCPeerConnectionHandlerClient::SignalingStateClosed;
    default:
      NOTREACHED();
      return WebRTCPeerConnectionHandlerClient::SignalingStateClosed;
  }
}

static blink::WebRTCSessionDescription
CreateWebKitSessionDescription(
    const webrtc::SessionDescriptionInterface* native_desc) {
  blink::WebRTCSessionDescription description;
  if (!native_desc) {
    LOG(ERROR) << "Native session description is null.";
    return description;
  }

  std::string sdp;
  if (!native_desc->ToString(&sdp)) {
    LOG(ERROR) << "Failed to get SDP string of native session description.";
    return description;
  }

  description.initialize(base::UTF8ToUTF16(native_desc->type()),
                         base::UTF8ToUTF16(sdp));
  return description;
}

// Converter functions from WebKit types to libjingle types.

static void GetNativeIceServers(
    const blink::WebRTCConfiguration& server_configuration,
    webrtc::PeerConnectionInterface::IceServers* servers) {
  if (server_configuration.isNull() || !servers)
    return;
  for (size_t i = 0; i < server_configuration.numberOfServers(); ++i) {
    webrtc::PeerConnectionInterface::IceServer server;
    const blink::WebRTCICEServer& webkit_server =
        server_configuration.server(i);
    server.username = base::UTF16ToUTF8(webkit_server.username());
    server.password = base::UTF16ToUTF8(webkit_server.credential());
    server.uri = webkit_server.uri().spec();
    servers->push_back(server);
  }
}

class SessionDescriptionRequestTracker {
 public:
  SessionDescriptionRequestTracker(RTCPeerConnectionHandler* handler,
                                   PeerConnectionTracker::Action action)
      : handler_(handler), action_(action) {}

  void TrackOnSuccess(const webrtc::SessionDescriptionInterface* desc) {
    std::string value;
    if (desc) {
      desc->ToString(&value);
      value = "type: " + desc->type() + ", sdp: " + value;
    }
    if (handler_->peer_connection_tracker())
      handler_->peer_connection_tracker()->TrackSessionDescriptionCallback(
          handler_, action_, "OnSuccess", value);
  }

  void TrackOnFailure(const std::string& error) {
    if (handler_->peer_connection_tracker())
      handler_->peer_connection_tracker()->TrackSessionDescriptionCallback(
          handler_, action_, "OnFailure", error);
  }

 private:
  RTCPeerConnectionHandler* handler_;
  PeerConnectionTracker::Action action_;
};

// Class mapping responses from calls to libjingle CreateOffer/Answer and
// the blink::WebRTCSessionDescriptionRequest.
class CreateSessionDescriptionRequest
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  explicit CreateSessionDescriptionRequest(
      const blink::WebRTCSessionDescriptionRequest& request,
      RTCPeerConnectionHandler* handler,
      PeerConnectionTracker::Action action)
      : webkit_request_(request), tracker_(handler, action) {}

  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc) OVERRIDE {
    tracker_.TrackOnSuccess(desc);
    webkit_request_.requestSucceeded(CreateWebKitSessionDescription(desc));
    delete desc;
  }
  virtual void OnFailure(const std::string& error) OVERRIDE {
    tracker_.TrackOnFailure(error);
    webkit_request_.requestFailed(base::UTF8ToUTF16(error));
  }

 protected:
  virtual ~CreateSessionDescriptionRequest() {}

 private:
  blink::WebRTCSessionDescriptionRequest webkit_request_;
  SessionDescriptionRequestTracker tracker_;
};

// Class mapping responses from calls to libjingle
// SetLocalDescription/SetRemoteDescription and a blink::WebRTCVoidRequest.
class SetSessionDescriptionRequest
    : public webrtc::SetSessionDescriptionObserver {
 public:
  explicit SetSessionDescriptionRequest(
      const blink::WebRTCVoidRequest& request,
      RTCPeerConnectionHandler* handler,
      PeerConnectionTracker::Action action)
      : webkit_request_(request), tracker_(handler, action) {}

  virtual void OnSuccess() OVERRIDE {
    tracker_.TrackOnSuccess(NULL);
    webkit_request_.requestSucceeded();
  }
  virtual void OnFailure(const std::string& error) OVERRIDE {
    tracker_.TrackOnFailure(error);
    webkit_request_.requestFailed(base::UTF8ToUTF16(error));
  }

 protected:
  virtual ~SetSessionDescriptionRequest() {}

 private:
  blink::WebRTCVoidRequest webkit_request_;
  SessionDescriptionRequestTracker tracker_;
};

// Class mapping responses from calls to libjingle
// GetStats into a blink::WebRTCStatsCallback.
class StatsResponse : public webrtc::StatsObserver {
 public:
  explicit StatsResponse(const scoped_refptr<LocalRTCStatsRequest>& request)
      : request_(request.get()), response_(request_->createResponse().get()) {
    // Measure the overall time it takes to satisfy a getStats request.
    TRACE_EVENT_ASYNC_BEGIN0("webrtc", "getStats_Native", this);
  }

  virtual void OnComplete(
      const std::vector<webrtc::StatsReport>& reports) OVERRIDE {
    TRACE_EVENT0("webrtc", "StatsResponse::OnComplete")
    for (std::vector<webrtc::StatsReport>::const_iterator it = reports.begin();
         it != reports.end(); ++it) {
      if (it->values.size() > 0) {
        AddReport(*it);
      }
    }

    // Record the getSync operation as done before calling into Blink so that
    // we don't skew the perf measurements of the native code with whatever the
    // callback might be doing.
    TRACE_EVENT_ASYNC_END0("webrtc", "getStats_Native", this);

    request_->requestSucceeded(response_);
  }

 private:
  void AddReport(const webrtc::StatsReport& report) {
    int idx = response_->addReport(blink::WebString::fromUTF8(report.id),
                                   blink::WebString::fromUTF8(report.type),
                                   report.timestamp);
    for (webrtc::StatsReport::Values::const_iterator value_it =
         report.values.begin();
         value_it != report.values.end(); ++value_it) {
      AddStatistic(idx, value_it->name, value_it->value);
    }
  }

  void AddStatistic(int idx, const std::string& name,
                    const std::string& value) {
    response_->addStatistic(idx,
                            blink::WebString::fromUTF8(name),
                            blink::WebString::fromUTF8(value));
  }

  talk_base::scoped_refptr<LocalRTCStatsRequest> request_;
  talk_base::scoped_refptr<LocalRTCStatsResponse> response_;
};

// Implementation of LocalRTCStatsRequest.
LocalRTCStatsRequest::LocalRTCStatsRequest(blink::WebRTCStatsRequest impl)
    : impl_(impl),
      response_(NULL) {
}

LocalRTCStatsRequest::LocalRTCStatsRequest() {}
LocalRTCStatsRequest::~LocalRTCStatsRequest() {}

bool LocalRTCStatsRequest::hasSelector() const {
  return impl_.hasSelector();
}

blink::WebMediaStreamTrack LocalRTCStatsRequest::component() const {
  return impl_.component();
}

scoped_refptr<LocalRTCStatsResponse> LocalRTCStatsRequest::createResponse() {
  DCHECK(!response_);
  response_ = new talk_base::RefCountedObject<LocalRTCStatsResponse>(
      impl_.createResponse());
  return response_.get();
}

void LocalRTCStatsRequest::requestSucceeded(
    const LocalRTCStatsResponse* response) {
  impl_.requestSucceeded(response->webKitStatsResponse());
}

// Implementation of LocalRTCStatsResponse.
blink::WebRTCStatsResponse LocalRTCStatsResponse::webKitStatsResponse() const {
  return impl_;
}

size_t LocalRTCStatsResponse::addReport(blink::WebString type,
                                        blink::WebString id,
                                        double timestamp) {
  return impl_.addReport(type, id, timestamp);
}

void LocalRTCStatsResponse::addStatistic(size_t report,
                                         blink::WebString name,
                                         blink::WebString value) {
  impl_.addStatistic(report, name, value);
}

namespace {

class PeerConnectionUMAObserver : public webrtc::UMAObserver {
 public:
  PeerConnectionUMAObserver() {}
  virtual ~PeerConnectionUMAObserver() {}

  virtual void IncrementCounter(
      webrtc::PeerConnectionUMAMetricsCounter counter) OVERRIDE {
    UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.IPMetrics",
                              counter,
                              webrtc::kBoundary);
  }

  virtual void AddHistogramSample(
      webrtc::PeerConnectionUMAMetricsName type, int value) OVERRIDE {
    switch (type) {
      case webrtc::kTimeToConnect:
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "WebRTC.PeerConnection.TimeToConnect",
            base::TimeDelta::FromMilliseconds(value));
        break;
      case webrtc::kNetworkInterfaces_IPv4:
        UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv4Interfaces",
                                 value);
        break;
      case webrtc::kNetworkInterfaces_IPv6:
        UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv6Interfaces",
                                 value);
        break;
      default:
        NOTREACHED();
    }
  }
};

base::LazyInstance<std::set<RTCPeerConnectionHandler*> >::Leaky
    g_peer_connection_handlers = LAZY_INSTANCE_INITIALIZER;

}  // namespace

RTCPeerConnectionHandler::RTCPeerConnectionHandler(
    blink::WebRTCPeerConnectionHandlerClient* client,
    PeerConnectionDependencyFactory* dependency_factory)
    : client_(client),
      dependency_factory_(dependency_factory),
      frame_(NULL),
      peer_connection_tracker_(NULL),
      num_data_channels_created_(0) {
  g_peer_connection_handlers.Get().insert(this);
}

RTCPeerConnectionHandler::~RTCPeerConnectionHandler() {
  g_peer_connection_handlers.Get().erase(this);
  if (peer_connection_tracker_)
    peer_connection_tracker_->UnregisterPeerConnection(this);
  STLDeleteValues(&remote_streams_);

  UMA_HISTOGRAM_COUNTS_10000(
      "WebRTC.NumDataChannelsPerPeerConnection", num_data_channels_created_);
}

// static
void RTCPeerConnectionHandler::DestructAllHandlers() {
  std::set<RTCPeerConnectionHandler*> handlers(
      g_peer_connection_handlers.Get().begin(),
      g_peer_connection_handlers.Get().end());
  for (std::set<RTCPeerConnectionHandler*>::iterator handler = handlers.begin();
       handler != handlers.end();
       ++handler) {
    (*handler)->client_->releasePeerConnectionHandler();
  }
}

void RTCPeerConnectionHandler::associateWithFrame(blink::WebFrame* frame) {
  DCHECK(frame);
  frame_ = frame;
}

bool RTCPeerConnectionHandler::initialize(
    const blink::WebRTCConfiguration& server_configuration,
    const blink::WebMediaConstraints& options) {
  DCHECK(frame_);

  peer_connection_tracker_ =
      RenderThreadImpl::current()->peer_connection_tracker();

  webrtc::PeerConnectionInterface::IceServers servers;
  GetNativeIceServers(server_configuration, &servers);

  RTCMediaConstraints constraints(options);

  native_peer_connection_ =
      dependency_factory_->CreatePeerConnection(
          servers, &constraints, frame_, this);
  if (!native_peer_connection_.get()) {
    LOG(ERROR) << "Failed to initialize native PeerConnection.";
    return false;
  }
  if (peer_connection_tracker_)
    peer_connection_tracker_->RegisterPeerConnection(
        this, servers, constraints, frame_);

  uma_observer_ = new talk_base::RefCountedObject<PeerConnectionUMAObserver>();
  native_peer_connection_->RegisterUMAObserver(uma_observer_.get());
  return true;
}

bool RTCPeerConnectionHandler::InitializeForTest(
    const blink::WebRTCConfiguration& server_configuration,
    const blink::WebMediaConstraints& options,
    PeerConnectionTracker* peer_connection_tracker) {
  webrtc::PeerConnectionInterface::IceServers servers;
  GetNativeIceServers(server_configuration, &servers);

  RTCMediaConstraints constraints(options);
  native_peer_connection_ =
      dependency_factory_->CreatePeerConnection(
          servers, &constraints, NULL, this);
  if (!native_peer_connection_.get()) {
    LOG(ERROR) << "Failed to initialize native PeerConnection.";
    return false;
  }
  peer_connection_tracker_ = peer_connection_tracker;
  return true;
}

void RTCPeerConnectionHandler::createOffer(
    const blink::WebRTCSessionDescriptionRequest& request,
    const blink::WebMediaConstraints& options) {
  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new talk_base::RefCountedObject<CreateSessionDescriptionRequest>(
          request, this, PeerConnectionTracker::ACTION_CREATE_OFFER));
  RTCMediaConstraints constraints(options);
  native_peer_connection_->CreateOffer(description_request.get(), &constraints);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateOffer(this, constraints);
}

void RTCPeerConnectionHandler::createAnswer(
    const blink::WebRTCSessionDescriptionRequest& request,
    const blink::WebMediaConstraints& options) {
  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new talk_base::RefCountedObject<CreateSessionDescriptionRequest>(
          request, this, PeerConnectionTracker::ACTION_CREATE_ANSWER));
  RTCMediaConstraints constraints(options);
  native_peer_connection_->CreateAnswer(description_request.get(),
                                        &constraints);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateAnswer(this, constraints);
}

void RTCPeerConnectionHandler::setLocalDescription(
    const blink::WebRTCVoidRequest& request,
    const blink::WebRTCSessionDescription& description) {
  webrtc::SdpParseError error;
  webrtc::SessionDescriptionInterface* native_desc =
      CreateNativeSessionDescription(description, &error);
  if (!native_desc) {
    std::string reason_str = "Failed to parse SessionDescription. ";
    reason_str.append(error.line);
    reason_str.append(" ");
    reason_str.append(error.description);
    LOG(ERROR) << reason_str;
    request.requestFailed(blink::WebString::fromUTF8(reason_str));
    return;
  }
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackSetSessionDescription(
        this, description, PeerConnectionTracker::SOURCE_LOCAL);

  scoped_refptr<SetSessionDescriptionRequest> set_request(
      new talk_base::RefCountedObject<SetSessionDescriptionRequest>(
          request, this, PeerConnectionTracker::ACTION_SET_LOCAL_DESCRIPTION));
  native_peer_connection_->SetLocalDescription(set_request.get(), native_desc);
}

void RTCPeerConnectionHandler::setRemoteDescription(
    const blink::WebRTCVoidRequest& request,
    const blink::WebRTCSessionDescription& description) {
  webrtc::SdpParseError error;
  webrtc::SessionDescriptionInterface* native_desc =
      CreateNativeSessionDescription(description, &error);
  if (!native_desc) {
    std::string reason_str = "Failed to parse SessionDescription. ";
    reason_str.append(error.line);
    reason_str.append(" ");
    reason_str.append(error.description);
    LOG(ERROR) << reason_str;
    request.requestFailed(blink::WebString::fromUTF8(reason_str));
    return;
  }
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackSetSessionDescription(
        this, description, PeerConnectionTracker::SOURCE_REMOTE);

  scoped_refptr<SetSessionDescriptionRequest> set_request(
      new talk_base::RefCountedObject<SetSessionDescriptionRequest>(
          request, this, PeerConnectionTracker::ACTION_SET_REMOTE_DESCRIPTION));
  native_peer_connection_->SetRemoteDescription(set_request.get(), native_desc);
}

blink::WebRTCSessionDescription
RTCPeerConnectionHandler::localDescription() {
  const webrtc::SessionDescriptionInterface* native_desc =
      native_peer_connection_->local_description();
  blink::WebRTCSessionDescription description =
      CreateWebKitSessionDescription(native_desc);
  return description;
}

blink::WebRTCSessionDescription
RTCPeerConnectionHandler::remoteDescription() {
  const webrtc::SessionDescriptionInterface* native_desc =
      native_peer_connection_->remote_description();
  blink::WebRTCSessionDescription description =
      CreateWebKitSessionDescription(native_desc);
  return description;
}

bool RTCPeerConnectionHandler::updateICE(
    const blink::WebRTCConfiguration& server_configuration,
    const blink::WebMediaConstraints& options) {
  webrtc::PeerConnectionInterface::IceServers servers;
  GetNativeIceServers(server_configuration, &servers);
  RTCMediaConstraints constraints(options);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackUpdateIce(this, servers, constraints);

  return native_peer_connection_->UpdateIce(servers,
                                            &constraints);
}

bool RTCPeerConnectionHandler::addICECandidate(
  const blink::WebRTCVoidRequest& request,
  const blink::WebRTCICECandidate& candidate) {
  // Libjingle currently does not accept callbacks for addICECandidate.
  // For that reason we are going to call callbacks from here.
  bool result = addICECandidate(candidate);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RTCPeerConnectionHandler::OnaddICECandidateResult,
                 base::Unretained(this), request, result));
  // On failure callback will be triggered.
  return true;
}

bool RTCPeerConnectionHandler::addICECandidate(
    const blink::WebRTCICECandidate& candidate) {
  scoped_ptr<webrtc::IceCandidateInterface> native_candidate(
      dependency_factory_->CreateIceCandidate(
          base::UTF16ToUTF8(candidate.sdpMid()),
          candidate.sdpMLineIndex(),
          base::UTF16ToUTF8(candidate.candidate())));
  if (!native_candidate) {
    LOG(ERROR) << "Could not create native ICE candidate.";
    return false;
  }

  bool return_value =
      native_peer_connection_->AddIceCandidate(native_candidate.get());
  LOG_IF(ERROR, !return_value) << "Error processing ICE candidate.";

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackAddIceCandidate(
        this, candidate, PeerConnectionTracker::SOURCE_REMOTE);

  return return_value;
}

void RTCPeerConnectionHandler::OnaddICECandidateResult(
    const blink::WebRTCVoidRequest& webkit_request, bool result) {
  if (!result) {
    // We don't have the actual error code from the libjingle, so for now
    // using a generic error string.
    return webkit_request.requestFailed(
        base::UTF8ToUTF16("Error processing ICE candidate"));
  }

  return webkit_request.requestSucceeded();
}

bool RTCPeerConnectionHandler::addStream(
    const blink::WebMediaStream& stream,
    const blink::WebMediaConstraints& options) {

  for (ScopedVector<WebRtcMediaStreamAdapter>::iterator adapter_it =
      local_streams_.begin(); adapter_it != local_streams_.end();
      ++adapter_it) {
    if ((*adapter_it)->IsEqual(stream)) {
      DVLOG(1) << "RTCPeerConnectionHandler::addStream called with the same "
               << "stream twice. id=" << stream.id().utf8();
      return false;
    }
  }

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackAddStream(
        this, stream, PeerConnectionTracker::SOURCE_LOCAL);

  PerSessionWebRTCAPIMetrics::GetInstance()->IncrementStreamCounter();

  WebRtcMediaStreamAdapter* adapter =
      new WebRtcMediaStreamAdapter(stream, dependency_factory_);
  local_streams_.push_back(adapter);

  webrtc::MediaStreamInterface* webrtc_stream = adapter->webrtc_media_stream();
  track_metrics_.AddStream(MediaStreamTrackMetrics::SENT_STREAM,
                           webrtc_stream);

  RTCMediaConstraints constraints(options);
  return native_peer_connection_->AddStream(webrtc_stream, &constraints);
}

void RTCPeerConnectionHandler::removeStream(
    const blink::WebMediaStream& stream) {
  // Find the webrtc stream.
  scoped_refptr<webrtc::MediaStreamInterface> webrtc_stream;
  for (ScopedVector<WebRtcMediaStreamAdapter>::iterator adapter_it =
           local_streams_.begin(); adapter_it != local_streams_.end();
       ++adapter_it) {
    if ((*adapter_it)->IsEqual(stream)) {
      webrtc_stream = (*adapter_it)->webrtc_media_stream();
      local_streams_.erase(adapter_it);
      break;
    }
  }
  DCHECK(webrtc_stream);
  native_peer_connection_->RemoveStream(webrtc_stream);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackRemoveStream(
        this, stream, PeerConnectionTracker::SOURCE_LOCAL);
  PerSessionWebRTCAPIMetrics::GetInstance()->DecrementStreamCounter();
  track_metrics_.RemoveStream(MediaStreamTrackMetrics::SENT_STREAM,
                              webrtc_stream);
}

void RTCPeerConnectionHandler::getStats(
    const blink::WebRTCStatsRequest& request) {
  scoped_refptr<LocalRTCStatsRequest> inner_request(
      new talk_base::RefCountedObject<LocalRTCStatsRequest>(request));
  getStats(inner_request.get());
}

void RTCPeerConnectionHandler::getStats(LocalRTCStatsRequest* request) {
  talk_base::scoped_refptr<webrtc::StatsObserver> observer(
      new talk_base::RefCountedObject<StatsResponse>(request));
  webrtc::MediaStreamTrackInterface* track = NULL;
  if (request->hasSelector()) {
    blink::WebMediaStreamSource::Type type =
        request->component().source().type();
    std::string track_id = request->component().id().utf8();
    if (type == blink::WebMediaStreamSource::TypeAudio) {
      track =
          native_peer_connection_->local_streams()->FindAudioTrack(track_id);
      if (!track) {
        track =
            native_peer_connection_->remote_streams()->FindAudioTrack(track_id);
      }
    } else {
      DCHECK_EQ(blink::WebMediaStreamSource::TypeVideo, type);
      track =
          native_peer_connection_->local_streams()->FindVideoTrack(track_id);
      if (!track) {
        track =
            native_peer_connection_->remote_streams()->FindVideoTrack(track_id);
      }
    }
    if (!track) {
      DVLOG(1) << "GetStats: Track not found.";
      // TODO(hta): Consider how to get an error back.
      std::vector<webrtc::StatsReport> no_reports;
      observer->OnComplete(no_reports);
      return;
    }
  }
  GetStats(observer,
           track,
           webrtc::PeerConnectionInterface::kStatsOutputLevelStandard);
}

void RTCPeerConnectionHandler::GetStats(
    webrtc::StatsObserver* observer,
    webrtc::MediaStreamTrackInterface* track,
    webrtc::PeerConnectionInterface::StatsOutputLevel level) {
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::GetStats");
  if (!native_peer_connection_->GetStats(observer, track, level)) {
    DVLOG(1) << "GetStats failed.";
    // TODO(hta): Consider how to get an error back.
    std::vector<webrtc::StatsReport> no_reports;
    observer->OnComplete(no_reports);
    return;
  }
}

blink::WebRTCDataChannelHandler* RTCPeerConnectionHandler::createDataChannel(
    const blink::WebString& label, const blink::WebRTCDataChannelInit& init) {
  DVLOG(1) << "createDataChannel label " << base::UTF16ToUTF8(label);

  webrtc::DataChannelInit config;
  // TODO(jiayl): remove the deprecated reliable field once Libjingle is updated
  // to handle that.
  config.reliable = false;
  config.id = init.id;
  config.ordered = init.ordered;
  config.negotiated = init.negotiated;
  config.maxRetransmits = init.maxRetransmits;
  config.maxRetransmitTime = init.maxRetransmitTime;
  config.protocol = base::UTF16ToUTF8(init.protocol);

  talk_base::scoped_refptr<webrtc::DataChannelInterface> webrtc_channel(
      native_peer_connection_->CreateDataChannel(base::UTF16ToUTF8(label),
                                                 &config));
  if (!webrtc_channel) {
    DLOG(ERROR) << "Could not create native data channel.";
    return NULL;
  }
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateDataChannel(
        this, webrtc_channel.get(), PeerConnectionTracker::SOURCE_LOCAL);

  ++num_data_channels_created_;

  return new RtcDataChannelHandler(webrtc_channel);
}

blink::WebRTCDTMFSenderHandler* RTCPeerConnectionHandler::createDTMFSender(
    const blink::WebMediaStreamTrack& track) {
  DVLOG(1) << "createDTMFSender.";

  MediaStreamTrack* native_track = MediaStreamTrack::GetTrack(track);
  if (!native_track ||
      track.source().type() != blink::WebMediaStreamSource::TypeAudio) {
    DLOG(ERROR) << "Could not create DTMF sender from a non-audio track.";
    return NULL;
  }

  webrtc::AudioTrackInterface* audio_track = native_track->GetAudioAdapter();
  talk_base::scoped_refptr<webrtc::DtmfSenderInterface> sender(
      native_peer_connection_->CreateDtmfSender(audio_track));
  if (!sender) {
    DLOG(ERROR) << "Could not create native DTMF sender.";
    return NULL;
  }
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateDTMFSender(this, track);

  return new RtcDtmfSenderHandler(sender);
}

void RTCPeerConnectionHandler::stop() {
  DVLOG(1) << "RTCPeerConnectionHandler::stop";

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackStop(this);
  native_peer_connection_->Close();
}

void RTCPeerConnectionHandler::OnError() {
  // TODO(perkj): Implement.
  NOTIMPLEMENTED();
}

void RTCPeerConnectionHandler::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  blink::WebRTCPeerConnectionHandlerClient::SignalingState state =
      GetWebKitSignalingState(new_state);
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackSignalingStateChange(this, state);
  client_->didChangeSignalingState(state);
}

// Called any time the IceConnectionState changes
void RTCPeerConnectionHandler::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  track_metrics_.IceConnectionChange(new_state);
  blink::WebRTCPeerConnectionHandlerClient::ICEConnectionState state =
      GetWebKitIceConnectionState(new_state);
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackIceConnectionStateChange(this, state);
  client_->didChangeICEConnectionState(state);
}

// Called any time the IceGatheringState changes
void RTCPeerConnectionHandler::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  if (new_state == webrtc::PeerConnectionInterface::kIceGatheringComplete) {
    // If ICE gathering is completed, generate a NULL ICE candidate,
    // to signal end of candidates.
    blink::WebRTCICECandidate null_candidate;
    client_->didGenerateICECandidate(null_candidate);
  }

  blink::WebRTCPeerConnectionHandlerClient::ICEGatheringState state =
      GetWebKitIceGatheringState(new_state);
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackIceGatheringStateChange(this, state);
  client_->didChangeICEGatheringState(state);
}

void RTCPeerConnectionHandler::OnAddStream(
    webrtc::MediaStreamInterface* stream_interface) {
  DCHECK(stream_interface);
  DCHECK(remote_streams_.find(stream_interface) == remote_streams_.end());

  RemoteMediaStreamImpl* remote_stream =
      new RemoteMediaStreamImpl(stream_interface);
  remote_streams_.insert(
      std::pair<webrtc::MediaStreamInterface*, RemoteMediaStreamImpl*> (
          stream_interface, remote_stream));

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackAddStream(
        this, remote_stream->webkit_stream(),
        PeerConnectionTracker::SOURCE_REMOTE);

  PerSessionWebRTCAPIMetrics::GetInstance()->IncrementStreamCounter();

  track_metrics_.AddStream(MediaStreamTrackMetrics::RECEIVED_STREAM,
                           stream_interface);

  client_->didAddRemoteStream(remote_stream->webkit_stream());
}

void RTCPeerConnectionHandler::OnRemoveStream(
    webrtc::MediaStreamInterface* stream_interface) {
  DCHECK(stream_interface);
  RemoteStreamMap::iterator it = remote_streams_.find(stream_interface);
  if (it == remote_streams_.end()) {
    NOTREACHED() << "Stream not found";
    return;
  }

  track_metrics_.RemoveStream(MediaStreamTrackMetrics::RECEIVED_STREAM,
                              stream_interface);
  PerSessionWebRTCAPIMetrics::GetInstance()->DecrementStreamCounter();

  scoped_ptr<RemoteMediaStreamImpl> remote_stream(it->second);
  const blink::WebMediaStream& webkit_stream = remote_stream->webkit_stream();
  DCHECK(!webkit_stream.isNull());
  remote_streams_.erase(it);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackRemoveStream(
        this, webkit_stream, PeerConnectionTracker::SOURCE_REMOTE);

  client_->didRemoveRemoteStream(webkit_stream);
}

void RTCPeerConnectionHandler::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  DCHECK(candidate);
  std::string sdp;
  if (!candidate->ToString(&sdp)) {
    NOTREACHED() << "OnIceCandidate: Could not get SDP string.";
    return;
  }
  blink::WebRTCICECandidate web_candidate;
  web_candidate.initialize(base::UTF8ToUTF16(sdp),
                           base::UTF8ToUTF16(candidate->sdp_mid()),
                           candidate->sdp_mline_index());
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackAddIceCandidate(
        this, web_candidate, PeerConnectionTracker::SOURCE_LOCAL);

  client_->didGenerateICECandidate(web_candidate);
}

void RTCPeerConnectionHandler::OnDataChannel(
    webrtc::DataChannelInterface* data_channel) {
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateDataChannel(
        this, data_channel, PeerConnectionTracker::SOURCE_REMOTE);

  DVLOG(1) << "RTCPeerConnectionHandler::OnDataChannel "
           << data_channel->label();
  client_->didAddRemoteDataChannel(new RtcDataChannelHandler(data_channel));
}

void RTCPeerConnectionHandler::OnRenegotiationNeeded() {
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackOnRenegotiationNeeded(this);
  client_->negotiationNeeded();
}

PeerConnectionTracker* RTCPeerConnectionHandler::peer_connection_tracker() {
  return peer_connection_tracker_;
}

webrtc::SessionDescriptionInterface*
RTCPeerConnectionHandler::CreateNativeSessionDescription(
    const blink::WebRTCSessionDescription& description,
    webrtc::SdpParseError* error) {
  std::string sdp = base::UTF16ToUTF8(description.sdp());
  std::string type = base::UTF16ToUTF8(description.type());
  webrtc::SessionDescriptionInterface* native_desc =
      dependency_factory_->CreateSessionDescription(type, sdp, error);

  LOG_IF(ERROR, !native_desc) << "Failed to create native session description."
                              << " Type: " << type << " SDP: " << sdp;

  return native_desc;
}

}  // namespace content
