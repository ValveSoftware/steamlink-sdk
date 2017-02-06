// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_peer_connection_handler.h"

#include <string.h>

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/peer_connection_tracker.h"
#include "content/renderer/media/remote_media_stream_impl.h"
#include "content/renderer/media/rtc_certificate.h"
#include "content/renderer/media/rtc_data_channel_handler.h"
#include "content/renderer/media/rtc_dtmf_sender_handler.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_adapter.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_uma_histograms.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/media_switches.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebRTCAnswerOptions.h"
#include "third_party/WebKit/public/platform/WebRTCConfiguration.h"
#include "third_party/WebKit/public/platform/WebRTCDataChannelInit.h"
#include "third_party/WebKit/public/platform/WebRTCICECandidate.h"
#include "third_party/WebKit/public/platform/WebRTCOfferOptions.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescription.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescriptionRequest.h"
#include "third_party/WebKit/public/platform/WebRTCVoidRequest.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/webrtc/pc/mediasession.h"

using webrtc::DataChannelInterface;
using webrtc::IceCandidateInterface;
using webrtc::MediaStreamInterface;
using webrtc::PeerConnectionInterface;
using webrtc::PeerConnectionObserver;
using webrtc::StatsReport;
using webrtc::StatsReports;

namespace content {
namespace {

// Used to back histogram value of "WebRTC.PeerConnection.RtcpMux",
// so treat as append-only.
enum RtcpMux {
  RTCP_MUX_DISABLED,
  RTCP_MUX_ENABLED,
  RTCP_MUX_NO_MEDIA,
  RTCP_MUX_MAX
};

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

blink::WebRTCPeerConnectionHandlerClient::ICEConnectionState
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

blink::WebRTCPeerConnectionHandlerClient::SignalingState
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

blink::WebRTCSessionDescription CreateWebKitSessionDescription(
    const std::string& sdp, const std::string& type) {
  blink::WebRTCSessionDescription description;
  description.initialize(base::UTF8ToUTF16(type), base::UTF8ToUTF16(sdp));
  return description;
}

blink::WebRTCSessionDescription
CreateWebKitSessionDescription(
    const webrtc::SessionDescriptionInterface* native_desc) {
  if (!native_desc) {
    LOG(ERROR) << "Native session description is null.";
    return blink::WebRTCSessionDescription();
  }

  std::string sdp;
  if (!native_desc->ToString(&sdp)) {
    LOG(ERROR) << "Failed to get SDP string of native session description.";
    return blink::WebRTCSessionDescription();
  }

  return CreateWebKitSessionDescription(sdp, native_desc->type());
}

void RunClosureWithTrace(const base::Closure& closure,
                         const char* trace_event_name) {
  TRACE_EVENT0("webrtc", trace_event_name);
  closure.Run();
}

void RunSynchronousClosure(const base::Closure& closure,
                           const char* trace_event_name,
                           base::WaitableEvent* event) {
  {
    TRACE_EVENT0("webrtc", trace_event_name);
    closure.Run();
  }
  event->Signal();
}

void GetSdpAndTypeFromSessionDescription(
    const base::Callback<const webrtc::SessionDescriptionInterface*()>&
        description_callback,
    std::string* sdp, std::string* type) {
  const webrtc::SessionDescriptionInterface* description =
      description_callback.Run();
  if (description) {
    description->ToString(sdp);
    *type = description->type();
  }
}

// Converter functions from WebKit types to WebRTC types.

void GetNativeRtcConfiguration(
    const blink::WebRTCConfiguration& blink_config,
    webrtc::PeerConnectionInterface::RTCConfiguration* webrtc_config) {
  DCHECK(webrtc_config);
  if (blink_config.isNull())
    return;

  for (size_t i = 0; i < blink_config.numberOfServers(); ++i) {
    webrtc::PeerConnectionInterface::IceServer server;
    const blink::WebRTCICEServer& webkit_server =
        blink_config.server(i);
    server.username =
        base::UTF16ToUTF8(base::StringPiece16(webkit_server.username()));
    server.password =
        base::UTF16ToUTF8(base::StringPiece16(webkit_server.credential()));
    server.uri = webkit_server.uri().string().utf8();
    webrtc_config->servers.push_back(server);
  }

  switch (blink_config.iceTransports()) {
    case blink::WebRTCIceTransportsNone:
      webrtc_config->type = webrtc::PeerConnectionInterface::kNone;
      break;
    case blink::WebRTCIceTransportsRelay:
      webrtc_config->type = webrtc::PeerConnectionInterface::kRelay;
      break;
    case blink::WebRTCIceTransportsAll:
      webrtc_config->type = webrtc::PeerConnectionInterface::kAll;
      break;
    default:
      NOTREACHED();
  }

  switch (blink_config.bundlePolicy()) {
    case blink::WebRTCBundlePolicyBalanced:
      webrtc_config->bundle_policy =
          webrtc::PeerConnectionInterface::kBundlePolicyBalanced;
      break;
    case blink::WebRTCBundlePolicyMaxBundle:
      webrtc_config->bundle_policy =
          webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
      break;
    case blink::WebRTCBundlePolicyMaxCompat:
      webrtc_config->bundle_policy =
          webrtc::PeerConnectionInterface::kBundlePolicyMaxCompat;
      break;
    default:
      NOTREACHED();
  }

  switch (blink_config.rtcpMuxPolicy()) {
    case blink::WebRTCRtcpMuxPolicyNegotiate:
      webrtc_config->rtcp_mux_policy =
          webrtc::PeerConnectionInterface::kRtcpMuxPolicyNegotiate;
      break;
    case blink::WebRTCRtcpMuxPolicyRequire:
      webrtc_config->rtcp_mux_policy =
          webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;
      break;
    default:
      NOTREACHED();
  }

  for (size_t i = 0; i < blink_config.numberOfCertificates(); ++i) {
    webrtc_config->certificates.push_back(
        static_cast<RTCCertificate*>(
            blink_config.certificate(i))->rtcCertificate());
  }
}

void CopyConstraintsIntoRtcConfiguration(
    const blink::WebMediaConstraints constraints,
    webrtc::PeerConnectionInterface::RTCConfiguration* configuration) {
  // Copy info from constraints into configuration, if present.
  if (constraints.isEmpty()) {
    return;
  }

  bool the_value;
  if (GetConstraintValueAsBoolean(
          constraints, &blink::WebMediaTrackConstraintSet::enableIPv6,
          &the_value)) {
    configuration->disable_ipv6 = !the_value;
  } else {
    // Note: IPv6 WebRTC value is "disable" while Blink is "enable".
    configuration->disable_ipv6 = false;
  }

  if (GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::enableDscp,
      &the_value)) {
    configuration->set_dscp(the_value);
  }

  if (GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::googCpuOveruseDetection,
      &the_value)) {
    configuration->set_cpu_adaptation(the_value);
  }

  if (GetConstraintValueAsBoolean(
      constraints,
      &blink::WebMediaTrackConstraintSet::googEnableVideoSuspendBelowMinBitrate,
      &the_value)) {
    configuration->set_suspend_below_min_bitrate(the_value);
  }

  if (!GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::enableRtpDataChannels,
      &configuration->enable_rtp_data_channel)) {
    configuration->enable_rtp_data_channel = false;
  }
  int rate;
  if (GetConstraintValueAsInteger(
          constraints,
          &blink::WebMediaTrackConstraintSet::googScreencastMinBitrate,
          &rate)) {
    configuration->screencast_min_bitrate = rtc::Optional<int>(rate);
  }
  configuration->combined_audio_video_bwe = ConstraintToOptional(
      constraints,
      &blink::WebMediaTrackConstraintSet::googCombinedAudioVideoBwe);
  configuration->enable_dtls_srtp = ConstraintToOptional(
      constraints, &blink::WebMediaTrackConstraintSet::enableDtlsSrtp);
}

class SessionDescriptionRequestTracker {
 public:
  SessionDescriptionRequestTracker(
      const base::WeakPtr<RTCPeerConnectionHandler>& handler,
      const base::WeakPtr<PeerConnectionTracker>& tracker,
      PeerConnectionTracker::Action action)
      : handler_(handler), tracker_(tracker), action_(action) {}

  void TrackOnSuccess(const webrtc::SessionDescriptionInterface* desc) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (tracker_ && handler_) {
      std::string value;
      if (desc) {
        desc->ToString(&value);
        value = "type: " + desc->type() + ", sdp: " + value;
      }
      tracker_->TrackSessionDescriptionCallback(
          handler_.get(), action_, "OnSuccess", value);
    }
  }

  void TrackOnFailure(const std::string& error) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (handler_ && tracker_) {
      tracker_->TrackSessionDescriptionCallback(
          handler_.get(), action_, "OnFailure", error);
    }
  }

 private:
  const base::WeakPtr<RTCPeerConnectionHandler> handler_;
  const base::WeakPtr<PeerConnectionTracker> tracker_;
  PeerConnectionTracker::Action action_;
  base::ThreadChecker thread_checker_;
};

// Class mapping responses from calls to libjingle CreateOffer/Answer and
// the blink::WebRTCSessionDescriptionRequest.
class CreateSessionDescriptionRequest
    : public webrtc::CreateSessionDescriptionObserver {
 public:
  explicit CreateSessionDescriptionRequest(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const blink::WebRTCSessionDescriptionRequest& request,
      const base::WeakPtr<RTCPeerConnectionHandler>& handler,
      const base::WeakPtr<PeerConnectionTracker>& tracker,
      PeerConnectionTracker::Action action)
      : main_thread_(main_thread),
        webkit_request_(request),
        tracker_(handler, tracker, action) {
  }

  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(FROM_HERE,
          base::Bind(&CreateSessionDescriptionRequest::OnSuccess, this, desc));
      return;
    }

    tracker_.TrackOnSuccess(desc);
    webkit_request_.requestSucceeded(CreateWebKitSessionDescription(desc));
    webkit_request_.reset();
    delete desc;
  }
  void OnFailure(const std::string& error) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(FROM_HERE,
          base::Bind(&CreateSessionDescriptionRequest::OnFailure, this, error));
      return;
    }

    tracker_.TrackOnFailure(error);
    webkit_request_.requestFailed(base::UTF8ToUTF16(error));
    webkit_request_.reset();
  }

 protected:
  ~CreateSessionDescriptionRequest() override {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DLOG_IF(ERROR, !webkit_request_.isNull())
        << "CreateSessionDescriptionRequest not completed. Shutting down?";
  }

  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  blink::WebRTCSessionDescriptionRequest webkit_request_;
  SessionDescriptionRequestTracker tracker_;
};

// Class mapping responses from calls to libjingle
// SetLocalDescription/SetRemoteDescription and a blink::WebRTCVoidRequest.
class SetSessionDescriptionRequest
    : public webrtc::SetSessionDescriptionObserver {
 public:
  explicit SetSessionDescriptionRequest(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const blink::WebRTCVoidRequest& request,
      const base::WeakPtr<RTCPeerConnectionHandler>& handler,
      const base::WeakPtr<PeerConnectionTracker>& tracker,
      PeerConnectionTracker::Action action)
      : main_thread_(main_thread),
        webkit_request_(request),
        tracker_(handler, tracker, action) {
  }

  void OnSuccess() override {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(FROM_HERE,
          base::Bind(&SetSessionDescriptionRequest::OnSuccess, this));
      return;
    }
    tracker_.TrackOnSuccess(NULL);
    webkit_request_.requestSucceeded();
    webkit_request_.reset();
  }
  void OnFailure(const std::string& error) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(FROM_HERE,
          base::Bind(&SetSessionDescriptionRequest::OnFailure, this, error));
      return;
    }
    tracker_.TrackOnFailure(error);
    webkit_request_.requestFailed(base::UTF8ToUTF16(error));
    webkit_request_.reset();
  }

 protected:
  ~SetSessionDescriptionRequest() override {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DLOG_IF(ERROR, !webkit_request_.isNull())
        << "SetSessionDescriptionRequest not completed. Shutting down?";
  }

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  blink::WebRTCVoidRequest webkit_request_;
  SessionDescriptionRequestTracker tracker_;
};

// Class mapping responses from calls to libjingle
// GetStats into a blink::WebRTCStatsCallback.
class StatsResponse : public webrtc::StatsObserver {
 public:
  explicit StatsResponse(const scoped_refptr<LocalRTCStatsRequest>& request)
      : request_(request.get()),
        main_thread_(base::ThreadTaskRunnerHandle::Get()) {
    // Measure the overall time it takes to satisfy a getStats request.
    TRACE_EVENT_ASYNC_BEGIN0("webrtc", "getStats_Native", this);
    signaling_thread_checker_.DetachFromThread();
  }

  void OnComplete(const StatsReports& reports) override {
    DCHECK(signaling_thread_checker_.CalledOnValidThread());
    TRACE_EVENT0("webrtc", "StatsResponse::OnComplete");
    // We can't use webkit objects directly since they use a single threaded
    // heap allocator.
    std::vector<Report*>* report_copies = new std::vector<Report*>();
    report_copies->reserve(reports.size());
    for (auto* r : reports)
      report_copies->push_back(new Report(r));

    main_thread_->PostTaskAndReply(FROM_HERE,
        base::Bind(&StatsResponse::DeliverCallback, this,
                   base::Unretained(report_copies)),
        base::Bind(&StatsResponse::DeleteReports,
                   base::Unretained(report_copies)));
  }

 private:
  struct Report {
    Report(const StatsReport* report)
        : thread_checker(), id(report->id()->ToString()),
          type(report->TypeToString()),  timestamp(report->timestamp()),
          values(report->values()) {
    }

    ~Report() {
      // Since the values vector holds pointers to const objects that are bound
      // to the signaling thread, they must be released on the same thread.
      DCHECK(thread_checker.CalledOnValidThread());
    }

    const base::ThreadChecker thread_checker;
    const std::string id, type;
    const double timestamp;
    const StatsReport::Values values;
  };

  static void DeleteReports(std::vector<Report*>* reports) {
    TRACE_EVENT0("webrtc", "StatsResponse::DeleteReports");
    for (auto* p : *reports)
      delete p;
    delete reports;
  }

  void DeliverCallback(const std::vector<Report*>* reports) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    TRACE_EVENT0("webrtc", "StatsResponse::DeliverCallback");

    rtc::scoped_refptr<LocalRTCStatsResponse> response(
        request_->createResponse().get());
    for (const auto* report : *reports) {
      if (report->values.size() > 0)
        AddReport(response.get(), *report);
    }

    // Record the getStats operation as done before calling into Blink so that
    // we don't skew the perf measurements of the native code with whatever the
    // callback might be doing.
    TRACE_EVENT_ASYNC_END0("webrtc", "getStats_Native", this);
    request_->requestSucceeded(response);
    request_ = nullptr;  // must be freed on the main thread.
  }

  void AddReport(LocalRTCStatsResponse* response, const Report& report) {
    int idx = response->addReport(blink::WebString::fromUTF8(report.id),
                                  blink::WebString::fromUTF8(report.type),
                                  report.timestamp);
    blink::WebString name, value_str;
    for (const auto& value : report.values) {
      const StatsReport::ValuePtr& v = value.second;
      name = blink::WebString::fromUTF8(value.second->display_name());

      if (v->type() == StatsReport::Value::kString)
        value_str = blink::WebString::fromUTF8(v->string_val());
      if (v->type() == StatsReport::Value::kStaticString)
        value_str = blink::WebString::fromUTF8(v->static_string_val());
      else
        value_str = blink::WebString::fromUTF8(v->ToString());

      response->addStatistic(idx, name, value_str);
    }
  }

  rtc::scoped_refptr<LocalRTCStatsRequest> request_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  base::ThreadChecker signaling_thread_checker_;
};

void GetStatsOnSignalingThread(
    const scoped_refptr<webrtc::PeerConnectionInterface>& pc,
    webrtc::PeerConnectionInterface::StatsOutputLevel level,
    const scoped_refptr<webrtc::StatsObserver>& observer,
    const std::string& track_id,
    blink::WebMediaStreamSource::Type track_type) {
  TRACE_EVENT0("webrtc", "GetStatsOnSignalingThread");

  scoped_refptr<webrtc::MediaStreamTrackInterface> track;
  if (!track_id.empty()) {
    if (track_type == blink::WebMediaStreamSource::TypeAudio) {
      track = pc->local_streams()->FindAudioTrack(track_id);
      if (!track.get())
        track = pc->remote_streams()->FindAudioTrack(track_id);
    } else {
      DCHECK_EQ(blink::WebMediaStreamSource::TypeVideo, track_type);
      track = pc->local_streams()->FindVideoTrack(track_id);
      if (!track.get())
        track = pc->remote_streams()->FindVideoTrack(track_id);
    }

    if (!track.get()) {
      DVLOG(1) << "GetStats: Track not found.";
      observer->OnComplete(StatsReports());
      return;
    }
  }

  if (!pc->GetStats(observer.get(), track.get(), level)) {
    DVLOG(1) << "GetStats failed.";
    observer->OnComplete(StatsReports());
  }
}

class PeerConnectionUMAObserver : public webrtc::UMAObserver {
 public:
  PeerConnectionUMAObserver() {}
  ~PeerConnectionUMAObserver() override {}
  void IncrementEnumCounter(webrtc::PeerConnectionEnumCounterType counter_type,
                            int counter,
                            int counter_max) override {
    switch (counter_type) {
      case webrtc::kEnumCounterAddressFamily:
        UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.IPMetrics", counter,
                                  counter_max);
        break;
      case webrtc::kEnumCounterIceCandidatePairTypeUdp:
        UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.CandidatePairType_UDP",
                                  counter, counter_max);
        break;
      case webrtc::kEnumCounterIceCandidatePairTypeTcp:
        UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.CandidatePairType_TCP",
                                  counter, counter_max);
        break;
      default:
        // The default clause is expected to reach when new enum types are
        // added.
        break;
    }
  }

  void IncrementSparseEnumCounter(
      webrtc::PeerConnectionEnumCounterType counter_type,
      int counter) override {
    switch (counter_type) {
      case webrtc::kEnumCounterAudioSrtpCipher:
        UMA_HISTOGRAM_SPARSE_SLOWLY(
            "WebRTC.PeerConnection.SrtpCryptoSuite.Audio", counter);
        break;
      case webrtc::kEnumCounterAudioSslCipher:
        UMA_HISTOGRAM_SPARSE_SLOWLY(
            "WebRTC.PeerConnection.SslCipherSuite.Audio", counter);
        break;
      case webrtc::kEnumCounterVideoSrtpCipher:
        UMA_HISTOGRAM_SPARSE_SLOWLY(
            "WebRTC.PeerConnection.SrtpCryptoSuite.Video", counter);
        break;
      case webrtc::kEnumCounterVideoSslCipher:
        UMA_HISTOGRAM_SPARSE_SLOWLY(
            "WebRTC.PeerConnection.SslCipherSuite.Video", counter);
        break;
      case webrtc::kEnumCounterDataSrtpCipher:
        UMA_HISTOGRAM_SPARSE_SLOWLY(
            "WebRTC.PeerConnection.SrtpCryptoSuite.Data", counter);
        break;
      case webrtc::kEnumCounterDataSslCipher:
        UMA_HISTOGRAM_SPARSE_SLOWLY("WebRTC.PeerConnection.SslCipherSuite.Data",
                                    counter);
        break;
      default:
        // The default clause is expected to reach when new enum types are
        // added.
        break;
    }
  }

  void AddHistogramSample(webrtc::PeerConnectionUMAMetricsName type,
                          int value) override {
    // Runs on libjingle's signaling thread.
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
        // The default clause is expected to reach when new enum types are
        // added.
        break;
    }
  }
};

void ConvertOfferOptionsToWebrtcOfferOptions(
    const blink::WebRTCOfferOptions& options,
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions* output) {
  output->offer_to_receive_audio = options.offerToReceiveAudio();
  output->offer_to_receive_video = options.offerToReceiveVideo();
  output->voice_activity_detection = options.voiceActivityDetection();
  output->ice_restart = options.iceRestart();
}

void ConvertAnswerOptionsToWebrtcAnswerOptions(
    const blink::WebRTCAnswerOptions& options,
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions* output) {
  output->voice_activity_detection = options.voiceActivityDetection();
}

void ConvertConstraintsToWebrtcOfferOptions(
    const blink::WebMediaConstraints& constraints,
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions* output) {
  if (constraints.isEmpty()) {
    return;
  }
  std::string failing_name;
  if (constraints.basic().hasMandatoryOutsideSet(
          {constraints.basic().offerToReceiveAudio.name(),
           constraints.basic().offerToReceiveVideo.name(),
           constraints.basic().voiceActivityDetection.name(),
           constraints.basic().iceRestart.name()},
          failing_name)) {
    // TODO(hta): Reject the calling operation with "constraint error"
    // https://crbug.com/594894
    DLOG(ERROR) << "Invalid mandatory constraint to CreateOffer/Answer: "
                << failing_name;
  }
  GetConstraintValueAsInteger(
      constraints, &blink::WebMediaTrackConstraintSet::offerToReceiveAudio,
      &output->offer_to_receive_audio);
  GetConstraintValueAsInteger(
      constraints, &blink::WebMediaTrackConstraintSet::offerToReceiveVideo,
      &output->offer_to_receive_video);
  GetConstraintValueAsBoolean(
      constraints, &blink::WebMediaTrackConstraintSet::voiceActivityDetection,
      &output->voice_activity_detection);
  GetConstraintValueAsBoolean(constraints,
                              &blink::WebMediaTrackConstraintSet::iceRestart,
                              &output->ice_restart);
}

base::LazyInstance<std::set<RTCPeerConnectionHandler*> >::Leaky
    g_peer_connection_handlers = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Implementation of LocalRTCStatsRequest.
LocalRTCStatsRequest::LocalRTCStatsRequest(blink::WebRTCStatsRequest impl)
    : impl_(impl) {
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
  return scoped_refptr<LocalRTCStatsResponse>(
      new rtc::RefCountedObject<LocalRTCStatsResponse>(impl_.createResponse()));
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

// Receives notifications from a PeerConnection object about state changes,
// track addition/removal etc.  The callbacks we receive here come on the
// signaling thread, so this class takes care of delivering them to an
// RTCPeerConnectionHandler instance on the main thread.
// In order to do safe PostTask-ing, the class is reference counted and
// checks for the existence of the RTCPeerConnectionHandler instance before
// delivering callbacks on the main thread.
class RTCPeerConnectionHandler::Observer
    : public base::RefCountedThreadSafe<RTCPeerConnectionHandler::Observer>,
      public PeerConnectionObserver {
 public:
  Observer(const base::WeakPtr<RTCPeerConnectionHandler>& handler)
      : handler_(handler), main_thread_(base::ThreadTaskRunnerHandle::Get()) {}

 protected:
  friend class base::RefCountedThreadSafe<RTCPeerConnectionHandler::Observer>;
  virtual ~Observer() {}

  void OnSignalingChange(
      PeerConnectionInterface::SignalingState new_state) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(FROM_HERE,
          base::Bind(&RTCPeerConnectionHandler::Observer::OnSignalingChange,
              this, new_state));
    } else if (handler_) {
      handler_->OnSignalingChange(new_state);
    }
  }

  void OnAddStream(rtc::scoped_refptr<MediaStreamInterface> stream) override {
    DCHECK(stream);
    std::unique_ptr<RemoteMediaStreamImpl> remote_stream(
        new RemoteMediaStreamImpl(main_thread_, stream));

    // The webkit object owned by RemoteMediaStreamImpl, will be initialized
    // asynchronously and the posted task will execude after that initialization
    // is done.
    main_thread_->PostTask(FROM_HERE,
        base::Bind(&RTCPeerConnectionHandler::Observer::OnAddStreamImpl,
            this, base::Passed(&remote_stream)));
  }

  void OnRemoveStream(
      rtc::scoped_refptr<MediaStreamInterface> stream) override {
    main_thread_->PostTask(
        FROM_HERE,
        base::Bind(&RTCPeerConnectionHandler::Observer::OnRemoveStreamImpl,
                   this, make_scoped_refptr(stream.get())));
  }

  void OnDataChannel(
      rtc::scoped_refptr<DataChannelInterface> data_channel) override {
    std::unique_ptr<RtcDataChannelHandler> handler(
        new RtcDataChannelHandler(main_thread_, data_channel));
    main_thread_->PostTask(FROM_HERE,
        base::Bind(&RTCPeerConnectionHandler::Observer::OnDataChannelImpl,
            this, base::Passed(&handler)));
  }

  void OnRenegotiationNeeded() override {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(FROM_HERE,
          base::Bind(&RTCPeerConnectionHandler::Observer::OnRenegotiationNeeded,
              this));
    } else if (handler_) {
      handler_->OnRenegotiationNeeded();
    }
  }

  void OnIceConnectionChange(
      PeerConnectionInterface::IceConnectionState new_state) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(FROM_HERE,
          base::Bind(
              &RTCPeerConnectionHandler::Observer::OnIceConnectionChange, this,
              new_state));
    } else if (handler_) {
      handler_->OnIceConnectionChange(new_state);
    }
  }

  void OnIceGatheringChange(
      PeerConnectionInterface::IceGatheringState new_state) override {
    if (!main_thread_->BelongsToCurrentThread()) {
      main_thread_->PostTask(FROM_HERE,
          base::Bind(&RTCPeerConnectionHandler::Observer::OnIceGatheringChange,
              this, new_state));
    } else if (handler_) {
      handler_->OnIceGatheringChange(new_state);
    }
  }

  void OnIceCandidate(const IceCandidateInterface* candidate) override {
    std::string sdp;
    if (!candidate->ToString(&sdp)) {
      NOTREACHED() << "OnIceCandidate: Could not get SDP string.";
      return;
    }

    main_thread_->PostTask(FROM_HERE,
        base::Bind(&RTCPeerConnectionHandler::Observer::OnIceCandidateImpl,
            this, sdp, candidate->sdp_mid(), candidate->sdp_mline_index(),
            candidate->candidate().component(),
            candidate->candidate().address().family()));
  }

  void OnAddStreamImpl(std::unique_ptr<RemoteMediaStreamImpl> stream) {
    DCHECK(stream->webkit_stream().getExtraData()) << "Initialization not done";
    if (handler_)
      handler_->OnAddStream(std::move(stream));
  }

  void OnRemoveStreamImpl(const scoped_refptr<MediaStreamInterface>& stream) {
    if (handler_)
      handler_->OnRemoveStream(stream);
  }

  void OnDataChannelImpl(std::unique_ptr<RtcDataChannelHandler> handler) {
    if (handler_)
      handler_->OnDataChannel(std::move(handler));
  }

  void OnIceCandidateImpl(const std::string& sdp, const std::string& sdp_mid,
      int sdp_mline_index, int component, int address_family) {
    if (handler_) {
      handler_->OnIceCandidate(sdp, sdp_mid, sdp_mline_index, component,
          address_family);
    }
  }

 private:
  const base::WeakPtr<RTCPeerConnectionHandler> handler_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
};

RTCPeerConnectionHandler::RTCPeerConnectionHandler(
    blink::WebRTCPeerConnectionHandlerClient* client,
    PeerConnectionDependencyFactory* dependency_factory)
    : client_(client),
      is_closed_(false),
      dependency_factory_(dependency_factory),
      weak_factory_(this) {
  CHECK(client_),
  g_peer_connection_handlers.Get().insert(this);
}

RTCPeerConnectionHandler::~RTCPeerConnectionHandler() {
  DCHECK(thread_checker_.CalledOnValidThread());

  stop();

  g_peer_connection_handlers.Get().erase(this);
  if (peer_connection_tracker_)
    peer_connection_tracker_->UnregisterPeerConnection(this);
  STLDeleteValues(&remote_streams_);

  UMA_HISTOGRAM_COUNTS_10000(
      "WebRTC.NumDataChannelsPerPeerConnection", num_data_channels_created_);
}

// static
void RTCPeerConnectionHandler::DestructAllHandlers() {
  // Copy g_peer_connection_handlers since releasePeerConnectionHandler will
  // remove an item.
  std::set<RTCPeerConnectionHandler*> handlers(
      g_peer_connection_handlers.Get().begin(),
      g_peer_connection_handlers.Get().end());
  for (auto* handler : handlers)
    handler->client_->releasePeerConnectionHandler();
}

void RTCPeerConnectionHandler::associateWithFrame(blink::WebFrame* frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(frame);
  frame_ = frame;
}

bool RTCPeerConnectionHandler::initialize(
    const blink::WebRTCConfiguration& server_configuration,
    const blink::WebMediaConstraints& options) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(frame_);

  peer_connection_tracker_ =
      RenderThreadImpl::current()->peer_connection_tracker()->AsWeakPtr();

  webrtc::PeerConnectionInterface::RTCConfiguration config;
  GetNativeRtcConfiguration(server_configuration, &config);

  // Choose between RTC smoothness algorithm and prerenderer smoothing.
  // Prerenderer smoothing is turned on if RTC smoothness is turned off.
  config.set_prerenderer_smoothing(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRTCSmoothnessAlgorithm));

  // Copy all the relevant constraints into |config|.
  CopyConstraintsIntoRtcConfiguration(options, &config);

  peer_connection_observer_ = new Observer(weak_factory_.GetWeakPtr());
  native_peer_connection_ = dependency_factory_->CreatePeerConnection(
      config, frame_, peer_connection_observer_.get());

  if (!native_peer_connection_.get()) {
    LOG(ERROR) << "Failed to initialize native PeerConnection.";
    return false;
  }

  if (peer_connection_tracker_) {
    peer_connection_tracker_->RegisterPeerConnection(this, config, options,
                                                     frame_);
  }

  uma_observer_ = new rtc::RefCountedObject<PeerConnectionUMAObserver>();
  native_peer_connection_->RegisterUMAObserver(uma_observer_.get());
  return true;
}

bool RTCPeerConnectionHandler::InitializeForTest(
    const blink::WebRTCConfiguration& server_configuration,
    const blink::WebMediaConstraints& options,
    const base::WeakPtr<PeerConnectionTracker>& peer_connection_tracker) {
  DCHECK(thread_checker_.CalledOnValidThread());
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  GetNativeRtcConfiguration(server_configuration, &config);

  peer_connection_observer_ = new Observer(weak_factory_.GetWeakPtr());
  CopyConstraintsIntoRtcConfiguration(options, &config);

  native_peer_connection_ = dependency_factory_->CreatePeerConnection(
      config, nullptr, peer_connection_observer_.get());
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
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createOffer");

  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new rtc::RefCountedObject<CreateSessionDescriptionRequest>(
          base::ThreadTaskRunnerHandle::Get(), request,
          weak_factory_.GetWeakPtr(), peer_connection_tracker_,
          PeerConnectionTracker::ACTION_CREATE_OFFER));

  // TODO(tommi): Do this asynchronously via e.g. PostTaskAndReply.
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions webrtc_options;
  ConvertConstraintsToWebrtcOfferOptions(options, &webrtc_options);
  native_peer_connection_->CreateOffer(description_request.get(),
                                       webrtc_options);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateOffer(this, options);
}

void RTCPeerConnectionHandler::createOffer(
    const blink::WebRTCSessionDescriptionRequest& request,
    const blink::WebRTCOfferOptions& options) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createOffer");

  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new rtc::RefCountedObject<CreateSessionDescriptionRequest>(
          base::ThreadTaskRunnerHandle::Get(), request,
          weak_factory_.GetWeakPtr(), peer_connection_tracker_,
          PeerConnectionTracker::ACTION_CREATE_OFFER));

  // TODO(tommi): Do this asynchronously via e.g. PostTaskAndReply.
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions webrtc_options;
  ConvertOfferOptionsToWebrtcOfferOptions(options, &webrtc_options);
  native_peer_connection_->CreateOffer(description_request.get(),
                                       webrtc_options);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateOffer(this, options);
}

void RTCPeerConnectionHandler::createAnswer(
    const blink::WebRTCSessionDescriptionRequest& request,
    const blink::WebMediaConstraints& options) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createAnswer");
  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new rtc::RefCountedObject<CreateSessionDescriptionRequest>(
          base::ThreadTaskRunnerHandle::Get(), request,
          weak_factory_.GetWeakPtr(), peer_connection_tracker_,
          PeerConnectionTracker::ACTION_CREATE_ANSWER));
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions webrtc_options;
  ConvertConstraintsToWebrtcOfferOptions(options, &webrtc_options);
  // TODO(tommi): Do this asynchronously via e.g. PostTaskAndReply.
  native_peer_connection_->CreateAnswer(description_request.get(),
                                        webrtc_options);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateAnswer(this, options);
}

void RTCPeerConnectionHandler::createAnswer(
    const blink::WebRTCSessionDescriptionRequest& request,
    const blink::WebRTCAnswerOptions& options) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createAnswer");
  scoped_refptr<CreateSessionDescriptionRequest> description_request(
      new rtc::RefCountedObject<CreateSessionDescriptionRequest>(
          base::ThreadTaskRunnerHandle::Get(), request,
          weak_factory_.GetWeakPtr(), peer_connection_tracker_,
          PeerConnectionTracker::ACTION_CREATE_ANSWER));
  // TODO(tommi): Do this asynchronously via e.g. PostTaskAndReply.
  webrtc::PeerConnectionInterface::RTCOfferAnswerOptions webrtc_options;
  ConvertAnswerOptionsToWebrtcAnswerOptions(options, &webrtc_options);
  native_peer_connection_->CreateAnswer(description_request.get(),
                                        webrtc_options);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateAnswer(this, options);
}

bool IsOfferOrAnswer(const webrtc::SessionDescriptionInterface* native_desc) {
  DCHECK(native_desc);
  return native_desc->type() == "offer" || native_desc->type() == "answer";
}

void RTCPeerConnectionHandler::setLocalDescription(
    const blink::WebRTCVoidRequest& request,
    const blink::WebRTCSessionDescription& description) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::setLocalDescription");

  std::string sdp = base::UTF16ToUTF8(base::StringPiece16(description.sdp()));
  std::string type =
      base::UTF16ToUTF8(base::StringPiece16(description.type()));

  webrtc::SdpParseError error;
  // Since CreateNativeSessionDescription uses the dependency factory, we need
  // to make this call on the current thread to be safe.
  webrtc::SessionDescriptionInterface* native_desc =
      CreateNativeSessionDescription(sdp, type, &error);
  if (!native_desc) {
    std::string reason_str = "Failed to parse SessionDescription. ";
    reason_str.append(error.line);
    reason_str.append(" ");
    reason_str.append(error.description);
    LOG(ERROR) << reason_str;
    request.requestFailed(blink::WebString::fromUTF8(reason_str));
    return;
  }

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackSetSessionDescription(
        this, sdp, type, PeerConnectionTracker::SOURCE_LOCAL);
  }

  if (!first_local_description_ && IsOfferOrAnswer(native_desc)) {
    first_local_description_.reset(new FirstSessionDescription(native_desc));
    if (first_remote_description_) {
      ReportFirstSessionDescriptions(
          *first_local_description_,
          *first_remote_description_);
    }
  }

  scoped_refptr<SetSessionDescriptionRequest> set_request(
      new rtc::RefCountedObject<SetSessionDescriptionRequest>(
          base::ThreadTaskRunnerHandle::Get(), request,
          weak_factory_.GetWeakPtr(), peer_connection_tracker_,
          PeerConnectionTracker::ACTION_SET_LOCAL_DESCRIPTION));

  signaling_thread()->PostTask(
      FROM_HERE,
      base::Bind(
          &RunClosureWithTrace,
          base::Bind(&webrtc::PeerConnectionInterface::SetLocalDescription,
                     native_peer_connection_, base::RetainedRef(set_request),
                     base::Unretained(native_desc)),
          "SetLocalDescription"));
}

void RTCPeerConnectionHandler::setRemoteDescription(
    const blink::WebRTCVoidRequest& request,
    const blink::WebRTCSessionDescription& description) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::setRemoteDescription");
  std::string sdp = base::UTF16ToUTF8(base::StringPiece16(description.sdp()));
  std::string type =
      base::UTF16ToUTF8(base::StringPiece16(description.type()));

  webrtc::SdpParseError error;
  // Since CreateNativeSessionDescription uses the dependency factory, we need
  // to make this call on the current thread to be safe.
  webrtc::SessionDescriptionInterface* native_desc =
      CreateNativeSessionDescription(sdp, type, &error);
  if (!native_desc) {
    std::string reason_str = "Failed to parse SessionDescription. ";
    reason_str.append(error.line);
    reason_str.append(" ");
    reason_str.append(error.description);
    LOG(ERROR) << reason_str;
    request.requestFailed(blink::WebString::fromUTF8(reason_str));
    return;
  }

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackSetSessionDescription(
        this, sdp, type, PeerConnectionTracker::SOURCE_REMOTE);
  }

  if (!first_remote_description_ && IsOfferOrAnswer(native_desc)) {
    first_remote_description_.reset(new FirstSessionDescription(native_desc));
    if (first_local_description_) {
      ReportFirstSessionDescriptions(
          *first_local_description_,
          *first_remote_description_);
    }
  }

  scoped_refptr<SetSessionDescriptionRequest> set_request(
      new rtc::RefCountedObject<SetSessionDescriptionRequest>(
          base::ThreadTaskRunnerHandle::Get(), request,
          weak_factory_.GetWeakPtr(), peer_connection_tracker_,
          PeerConnectionTracker::ACTION_SET_REMOTE_DESCRIPTION));
  signaling_thread()->PostTask(
      FROM_HERE,
      base::Bind(
          &RunClosureWithTrace,
          base::Bind(&webrtc::PeerConnectionInterface::SetRemoteDescription,
                     native_peer_connection_, base::RetainedRef(set_request),
                     base::Unretained(native_desc)),
          "SetRemoteDescription"));
}

blink::WebRTCSessionDescription
RTCPeerConnectionHandler::localDescription() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::localDescription");

  // Since local_description returns a pointer to a non-reference-counted object
  // that lives on the signaling thread, we cannot fetch a pointer to it and use
  // it directly here. Instead, we access the object completely on the signaling
  // thread.
  std::string sdp, type;
  base::Callback<const webrtc::SessionDescriptionInterface*()> description_cb =
      base::Bind(&webrtc::PeerConnectionInterface::local_description,
                 native_peer_connection_);
  RunSynchronousClosureOnSignalingThread(
      base::Bind(&GetSdpAndTypeFromSessionDescription, description_cb,
          base::Unretained(&sdp), base::Unretained(&type)),
      "localDescription");

  return CreateWebKitSessionDescription(sdp, type);
}

blink::WebRTCSessionDescription
RTCPeerConnectionHandler::remoteDescription() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::remoteDescription");
  // Since local_description returns a pointer to a non-reference-counted object
  // that lives on the signaling thread, we cannot fetch a pointer to it and use
  // it directly here. Instead, we access the object completely on the signaling
  // thread.
  std::string sdp, type;
  base::Callback<const webrtc::SessionDescriptionInterface*()> description_cb =
      base::Bind(&webrtc::PeerConnectionInterface::remote_description,
                 native_peer_connection_);
  RunSynchronousClosureOnSignalingThread(
      base::Bind(&GetSdpAndTypeFromSessionDescription, description_cb,
          base::Unretained(&sdp), base::Unretained(&type)),
      "remoteDescription");

  return CreateWebKitSessionDescription(sdp, type);
}

bool RTCPeerConnectionHandler::updateICE(
    const blink::WebRTCConfiguration& server_configuration) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::updateICE");
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  GetNativeRtcConfiguration(server_configuration, &config);

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackUpdateIce(this, config);

  return native_peer_connection_->UpdateIce(config.servers);
}

void RTCPeerConnectionHandler::logSelectedRtcpMuxPolicy(
    blink::RtcpMuxPolicy selectedRtcpMuxPolicy) {
  UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.SelectedRtcpMuxPolicy",
                            selectedRtcpMuxPolicy, blink::RtcpMuxPolicyMax);
}

bool RTCPeerConnectionHandler::addICECandidate(
    const blink::WebRTCVoidRequest& request,
    const blink::WebRTCICECandidate& candidate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::addICECandidate");
  // Libjingle currently does not accept callbacks for addICECandidate.
  // For that reason we are going to call callbacks from here.

  // TODO(tommi): Instead of calling addICECandidate here, we can do a
  // PostTaskAndReply kind of a thing.
  bool result = addICECandidate(candidate);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&RTCPeerConnectionHandler::OnaddICECandidateResult,
                            weak_factory_.GetWeakPtr(), request, result));
  // On failure callback will be triggered.
  return true;
}

bool RTCPeerConnectionHandler::addICECandidate(
    const blink::WebRTCICECandidate& candidate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::addICECandidate");
  std::unique_ptr<webrtc::IceCandidateInterface> native_candidate(
      dependency_factory_->CreateIceCandidate(
          base::UTF16ToUTF8(base::StringPiece16(candidate.sdpMid())),
          candidate.sdpMLineIndex(),
          base::UTF16ToUTF8(base::StringPiece16(candidate.candidate()))));
  bool return_value = false;

  if (native_candidate) {
    return_value =
        native_peer_connection_->AddIceCandidate(native_candidate.get());
    LOG_IF(ERROR, !return_value) << "Error processing ICE candidate.";
  } else {
    LOG(ERROR) << "Could not create native ICE candidate.";
  }

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackAddIceCandidate(
        this, candidate, PeerConnectionTracker::SOURCE_REMOTE, return_value);
  }
  return return_value;
}

void RTCPeerConnectionHandler::OnaddICECandidateResult(
    const blink::WebRTCVoidRequest& webkit_request, bool result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnaddICECandidateResult");
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
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::addStream");
  for (ScopedVector<WebRtcMediaStreamAdapter>::iterator adapter_it =
      local_streams_.begin(); adapter_it != local_streams_.end();
      ++adapter_it) {
    if ((*adapter_it)->IsEqual(stream)) {
      DVLOG(1) << "RTCPeerConnectionHandler::addStream called with the same "
               << "stream twice. id=" << stream.id().utf8();
      return false;
    }
  }

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackAddStream(
        this, stream, PeerConnectionTracker::SOURCE_LOCAL);
  }

  PerSessionWebRTCAPIMetrics::GetInstance()->IncrementStreamCounter();

  WebRtcMediaStreamAdapter* adapter =
      new WebRtcMediaStreamAdapter(stream, dependency_factory_);
  local_streams_.push_back(adapter);

  webrtc::MediaStreamInterface* webrtc_stream = adapter->webrtc_media_stream();
  track_metrics_.AddStream(MediaStreamTrackMetrics::SENT_STREAM,
                           webrtc_stream);

  if (!options.isEmpty()) {
    // TODO(perkj): |mediaConstraints| is the name of the optional constraints
    // argument in RTCPeerConnection.idl. It has been removed from the spec and
    // should be removed from blink as well.
    LOG(WARNING)
        << "mediaConstraints is not a supported argument to addStream.";
    LOG(WARNING) << "mediaConstraints was " << options.toString().utf8();
  }

  return native_peer_connection_->AddStream(webrtc_stream);
}

void RTCPeerConnectionHandler::removeStream(
    const blink::WebMediaStream& stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::removeStream");
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
  DCHECK(webrtc_stream.get());
  // TODO(tommi): Make this async (PostTaskAndReply).
  native_peer_connection_->RemoveStream(webrtc_stream.get());

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackRemoveStream(
        this, stream, PeerConnectionTracker::SOURCE_LOCAL);
  }
  PerSessionWebRTCAPIMetrics::GetInstance()->DecrementStreamCounter();
  track_metrics_.RemoveStream(MediaStreamTrackMetrics::SENT_STREAM,
                              webrtc_stream.get());
}

void RTCPeerConnectionHandler::getStats(
    const blink::WebRTCStatsRequest& request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<LocalRTCStatsRequest> inner_request(
      new rtc::RefCountedObject<LocalRTCStatsRequest>(request));
  getStats(inner_request);
}

void RTCPeerConnectionHandler::getStats(
    const scoped_refptr<LocalRTCStatsRequest>& request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::getStats");


  rtc::scoped_refptr<webrtc::StatsObserver> observer(
      new rtc::RefCountedObject<StatsResponse>(request));

  std::string track_id;
  blink::WebMediaStreamSource::Type track_type =
      blink::WebMediaStreamSource::TypeAudio;
  if (request->hasSelector()) {
    track_type = request->component().source().getType();
    track_id = request->component().id().utf8();
  }

  GetStats(observer, webrtc::PeerConnectionInterface::kStatsOutputLevelStandard,
           track_id, track_type);
}

// TODO(tommi): It's weird to have three {g|G}etStats methods.  Clean this up.
void RTCPeerConnectionHandler::GetStats(
    webrtc::StatsObserver* observer,
    webrtc::PeerConnectionInterface::StatsOutputLevel level,
    const std::string& track_id,
    blink::WebMediaStreamSource::Type track_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  signaling_thread()->PostTask(FROM_HERE,
      base::Bind(&GetStatsOnSignalingThread, native_peer_connection_, level,
                 make_scoped_refptr(observer), track_id, track_type));
}

void RTCPeerConnectionHandler::CloseClientPeerConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!is_closed_)
    client_->closePeerConnection();
}

blink::WebRTCDataChannelHandler* RTCPeerConnectionHandler::createDataChannel(
    const blink::WebString& label, const blink::WebRTCDataChannelInit& init) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createDataChannel");
  DVLOG(1) << "createDataChannel label "
           << base::UTF16ToUTF8(base::StringPiece16(label));

  webrtc::DataChannelInit config;
  // TODO(jiayl): remove the deprecated reliable field once Libjingle is updated
  // to handle that.
  config.reliable = false;
  config.id = init.id;
  config.ordered = init.ordered;
  config.negotiated = init.negotiated;
  config.maxRetransmits = init.maxRetransmits;
  config.maxRetransmitTime = init.maxRetransmitTime;
  config.protocol = base::UTF16ToUTF8(base::StringPiece16(init.protocol));

  rtc::scoped_refptr<webrtc::DataChannelInterface> webrtc_channel(
      native_peer_connection_->CreateDataChannel(
          base::UTF16ToUTF8(base::StringPiece16(label)), &config));
  if (!webrtc_channel) {
    DLOG(ERROR) << "Could not create native data channel.";
    return NULL;
  }
  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackCreateDataChannel(
        this, webrtc_channel.get(), PeerConnectionTracker::SOURCE_LOCAL);
  }

  ++num_data_channels_created_;

  return new RtcDataChannelHandler(base::ThreadTaskRunnerHandle::Get(),
                                   webrtc_channel);
}

blink::WebRTCDTMFSenderHandler* RTCPeerConnectionHandler::createDTMFSender(
    const blink::WebMediaStreamTrack& track) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!track.isNull());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::createDTMFSender");
  DVLOG(1) << "createDTMFSender.";

  // Find the WebRtc track referenced by the blink track's ID.
  webrtc::AudioTrackInterface* webrtc_track = nullptr;
  for (const WebRtcMediaStreamAdapter* s : local_streams_) {
    webrtc_track = s->webrtc_media_stream()->FindAudioTrack(track.id().utf8());
    if (webrtc_track)
      break;
  }
  if (!webrtc_track) {
    DLOG(ERROR) << "Audio track with ID '" << track.id().utf8()
                << "' has no known WebRtc sink.";
    return nullptr;
  }

  rtc::scoped_refptr<webrtc::DtmfSenderInterface> sender(
      native_peer_connection_->CreateDtmfSender(webrtc_track));
  if (!sender) {
    DLOG(ERROR) << "Could not create native DTMF sender.";
    return nullptr;
  }
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackCreateDTMFSender(this, track);

  return new RtcDtmfSenderHandler(sender);
}

void RTCPeerConnectionHandler::stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "RTCPeerConnectionHandler::stop";

  if (is_closed_ || !native_peer_connection_.get())
    return;  // Already stopped.

  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackStop(this);

  native_peer_connection_->Close();

  // This object may no longer forward call backs to blink.
  is_closed_ = true;
}

void RTCPeerConnectionHandler::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnSignalingChange");

  blink::WebRTCPeerConnectionHandlerClient::SignalingState state =
      GetWebKitSignalingState(new_state);
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackSignalingStateChange(this, state);
  if (!is_closed_)
    client_->didChangeSignalingState(state);
}

// Called any time the IceConnectionState changes
void RTCPeerConnectionHandler::OnIceConnectionChange(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnIceConnectionChange");
  DCHECK(thread_checker_.CalledOnValidThread());
  ReportICEState(new_state);
  if (new_state == webrtc::PeerConnectionInterface::kIceConnectionChecking) {
    ice_connection_checking_start_ = base::TimeTicks::Now();
  } else if (new_state ==
      webrtc::PeerConnectionInterface::kIceConnectionConnected) {
    // If the state becomes connected, send the time needed for PC to become
    // connected from checking to UMA. UMA data will help to know how much
    // time needed for PC to connect with remote peer.
    if (ice_connection_checking_start_.is_null()) {
      // From UMA, we have observed a large number of calls falling into the
      // overflow buckets. One possibility is that the Checking is not signaled
      // before Connected. This is to guard against that situation to make the
      // metric more robust.
      UMA_HISTOGRAM_MEDIUM_TIMES("WebRTC.PeerConnection.TimeToConnect",
                                 base::TimeDelta());
    } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "WebRTC.PeerConnection.TimeToConnect",
        base::TimeTicks::Now() - ice_connection_checking_start_);
    }
  }

  track_metrics_.IceConnectionChange(new_state);
  blink::WebRTCPeerConnectionHandlerClient::ICEConnectionState state =
      GetWebKitIceConnectionState(new_state);
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackIceConnectionStateChange(this, state);
  if (!is_closed_)
    client_->didChangeICEConnectionState(state);
}

// Called any time the IceGatheringState changes
void RTCPeerConnectionHandler::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnIceGatheringChange");

  if (new_state == webrtc::PeerConnectionInterface::kIceGatheringComplete) {
    // If ICE gathering is completed, generate a NULL ICE candidate,
    // to signal end of candidates.
    if (!is_closed_) {
      blink::WebRTCICECandidate null_candidate;
      client_->didGenerateICECandidate(null_candidate);
    }

    UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv4LocalCandidates",
                             num_local_candidates_ipv4_);

    UMA_HISTOGRAM_COUNTS_100("WebRTC.PeerConnection.IPv6LocalCandidates",
                             num_local_candidates_ipv6_);
  } else if (new_state ==
             webrtc::PeerConnectionInterface::kIceGatheringGathering) {
    // ICE restarts will change gathering state back to "gathering",
    // reset the counter.
    ResetUMAStats();
  }

  blink::WebRTCPeerConnectionHandlerClient::ICEGatheringState state =
      GetWebKitIceGatheringState(new_state);
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackIceGatheringStateChange(this, state);
  if (!is_closed_)
    client_->didChangeICEGatheringState(state);
}

void RTCPeerConnectionHandler::OnRenegotiationNeeded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnRenegotiationNeeded");
  if (peer_connection_tracker_)
    peer_connection_tracker_->TrackOnRenegotiationNeeded(this);
  if (!is_closed_)
    client_->negotiationNeeded();
}

void RTCPeerConnectionHandler::OnAddStream(
    std::unique_ptr<RemoteMediaStreamImpl> stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(remote_streams_.find(stream->webrtc_stream().get()) ==
         remote_streams_.end());
  DCHECK(stream->webkit_stream().getExtraData()) << "Initialization not done";
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnAddStreamImpl");

  // Ownership is with remote_streams_ now.
  RemoteMediaStreamImpl* s = stream.release();
  remote_streams_.insert(
      std::pair<webrtc::MediaStreamInterface*, RemoteMediaStreamImpl*> (
          s->webrtc_stream().get(), s));

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackAddStream(
        this, s->webkit_stream(), PeerConnectionTracker::SOURCE_REMOTE);
  }

  PerSessionWebRTCAPIMetrics::GetInstance()->IncrementStreamCounter();

  track_metrics_.AddStream(MediaStreamTrackMetrics::RECEIVED_STREAM,
                           s->webrtc_stream().get());
  if (!is_closed_)
    client_->didAddRemoteStream(s->webkit_stream());
}

void RTCPeerConnectionHandler::OnRemoveStream(
    const scoped_refptr<webrtc::MediaStreamInterface>& stream) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnRemoveStreamImpl");
  RemoteStreamMap::iterator it = remote_streams_.find(stream.get());
  if (it == remote_streams_.end()) {
    NOTREACHED() << "Stream not found";
    return;
  }

  track_metrics_.RemoveStream(MediaStreamTrackMetrics::RECEIVED_STREAM,
                              stream.get());
  PerSessionWebRTCAPIMetrics::GetInstance()->DecrementStreamCounter();

  std::unique_ptr<RemoteMediaStreamImpl> remote_stream(it->second);
  const blink::WebMediaStream& webkit_stream = remote_stream->webkit_stream();
  DCHECK(!webkit_stream.isNull());
  remote_streams_.erase(it);

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackRemoveStream(
        this, webkit_stream, PeerConnectionTracker::SOURCE_REMOTE);
  }

  if (!is_closed_)
    client_->didRemoveRemoteStream(webkit_stream);
}

void RTCPeerConnectionHandler::OnDataChannel(
    std::unique_ptr<RtcDataChannelHandler> handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnDataChannelImpl");

  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackCreateDataChannel(
        this, handler->channel().get(), PeerConnectionTracker::SOURCE_REMOTE);
  }

  if (!is_closed_)
    client_->didAddRemoteDataChannel(handler.release());
}

void RTCPeerConnectionHandler::OnIceCandidate(
    const std::string& sdp, const std::string& sdp_mid, int sdp_mline_index,
    int component, int address_family) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("webrtc", "RTCPeerConnectionHandler::OnIceCandidateImpl");
  blink::WebRTCICECandidate web_candidate;
  web_candidate.initialize(base::UTF8ToUTF16(sdp),
                           base::UTF8ToUTF16(sdp_mid),
                           sdp_mline_index);
  if (peer_connection_tracker_) {
    peer_connection_tracker_->TrackAddIceCandidate(
        this, web_candidate, PeerConnectionTracker::SOURCE_LOCAL, true);
  }

  // Only the first m line's first component is tracked to avoid
  // miscounting when doing BUNDLE or rtcp mux.
  if (sdp_mline_index == 0 && component == 1) {
    if (address_family == AF_INET) {
      ++num_local_candidates_ipv4_;
    } else if (address_family == AF_INET6) {
      ++num_local_candidates_ipv6_;
    } else {
      NOTREACHED();
    }
  }
  if (!is_closed_)
    client_->didGenerateICECandidate(web_candidate);
}

webrtc::SessionDescriptionInterface*
RTCPeerConnectionHandler::CreateNativeSessionDescription(
    const std::string& sdp, const std::string& type,
    webrtc::SdpParseError* error) {
  webrtc::SessionDescriptionInterface* native_desc =
      dependency_factory_->CreateSessionDescription(type, sdp, error);

  LOG_IF(ERROR, !native_desc) << "Failed to create native session description."
                              << " Type: " << type << " SDP: " << sdp;

  return native_desc;
}

RTCPeerConnectionHandler::FirstSessionDescription::FirstSessionDescription(
    const webrtc::SessionDescriptionInterface* sdesc) {
  DCHECK(sdesc);

  for (const auto& content : sdesc->description()->contents()) {
    if (content.type == cricket::NS_JINGLE_RTP) {
      const auto* mdesc =
          static_cast<cricket::MediaContentDescription*>(content.description);
      audio = audio || (mdesc->type() == cricket::MEDIA_TYPE_AUDIO);
      video = video || (mdesc->type() == cricket::MEDIA_TYPE_VIDEO);
      rtcp_mux = rtcp_mux || mdesc->rtcp_mux();
    }
  }
}

void RTCPeerConnectionHandler::ReportFirstSessionDescriptions(
    const FirstSessionDescription& local,
    const FirstSessionDescription& remote) {
  RtcpMux rtcp_mux = RTCP_MUX_ENABLED;
  if ((!local.audio && !local.video) || (!remote.audio && !remote.video)) {
    rtcp_mux = RTCP_MUX_NO_MEDIA;
  } else if (!local.rtcp_mux || !remote.rtcp_mux) {
    rtcp_mux = RTCP_MUX_DISABLED;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "WebRTC.PeerConnection.RtcpMux", rtcp_mux, RTCP_MUX_MAX);

  // TODO(pthatcher): Reports stats about whether we have audio and
  // video or not.
}

scoped_refptr<base::SingleThreadTaskRunner>
RTCPeerConnectionHandler::signaling_thread() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return dependency_factory_->GetWebRtcSignalingThread();
}

void RTCPeerConnectionHandler::RunSynchronousClosureOnSignalingThread(
    const base::Closure& closure,
    const char* trace_event_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  scoped_refptr<base::SingleThreadTaskRunner> thread(signaling_thread());
  if (!thread.get() || thread->BelongsToCurrentThread()) {
    TRACE_EVENT0("webrtc", trace_event_name);
    closure.Run();
  } else {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    thread->PostTask(FROM_HERE,
        base::Bind(&RunSynchronousClosure, closure,
                   base::Unretained(trace_event_name),
                   base::Unretained(&event)));
    event.Wait();
  }
}

void RTCPeerConnectionHandler::ReportICEState(
    webrtc::PeerConnectionInterface::IceConnectionState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (ice_state_seen_[new_state])
    return;
  ice_state_seen_[new_state] = true;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.PeerConnection.ConnectionState", new_state,
                            webrtc::PeerConnectionInterface::kIceConnectionMax);
}

void RTCPeerConnectionHandler::ResetUMAStats() {
  DCHECK(thread_checker_.CalledOnValidThread());
  num_local_candidates_ipv6_ = 0;
  num_local_candidates_ipv4_ = 0;
  ice_connection_checking_start_ = base::TimeTicks();
  memset(ice_state_seen_, 0, sizeof(ice_state_seen_));
}
}  // namespace content
