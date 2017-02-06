// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/rpc/rpc_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
// TODO(ckehoe): time.h includes windows.h, which #defines DeviceCapabilities
// to DeviceCapabilitiesW. This breaks the pb.h headers below. For now,
// we fix this with an #undef.
#include "base/time/time.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#undef DeviceCapabilities
#endif

#include "components/audio_modem/public/audio_modem_types.h"
#include "components/copresence/copresence_state_impl.h"
#include "components/copresence/copresence_switches.h"
#include "components/copresence/handlers/directive_handler.h"
#include "components/copresence/handlers/gcm_handler.h"
#include "components/copresence/proto/codes.pb.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/public/copresence_constants.h"
#include "components/copresence/public/copresence_delegate.h"
#include "components/copresence/rpc/http_post.h"
#include "net/http/http_status_code.h"

using google::protobuf::MessageLite;

using audio_modem::AUDIBLE;
using audio_modem::AudioToken;
using audio_modem::INAUDIBLE;

// TODO(ckehoe): Return error messages for bad requests.

namespace copresence {

const char RpcHandler::kReportRequestRpcName[] = "report";

namespace {

const int kTokenLoggingSuffix = 5;
const int kInvalidTokenExpiryTimeMinutes = 10;
const int kMaxInvalidTokens = 10000;
const char kRegisterDeviceRpcName[] = "registerdevice";
const char kDefaultCopresenceServer[] =
    "https://www.googleapis.com/copresence/v2/copresence";

// UrlSafe is defined as:
// '/' represented by a '_' and '+' represented by a '-'
// TODO(rkc): Move this to the wrapper.
std::string ToUrlSafe(std::string token) {
  base::ReplaceChars(token, "+", "-", &token);
  base::ReplaceChars(token, "/", "_", &token);
  return token;
}

// Logging

// Checks for a copresence error. If there is one, logs it and returns true.
bool IsErrorStatus(const Status& status) {
  if (status.code() != OK) {
    LOG(ERROR) << "Copresence error code " << status.code()
               << (status.message().empty() ? "" : ": " + status.message());
  }
  return status.code() != OK;
}

void LogIfErrorStatus(const util::error::Code& code,
                      const std::string& context) {
  LOG_IF(ERROR, code != util::error::OK)
      << context << " error " << code << ". See "
      << "cs/google3/util/task/codes.proto for more info.";
}

// If any errors occurred, logs them and returns true.
bool ReportErrorLogged(const ReportResponse& response) {
  bool result = IsErrorStatus(response.header().status());

  // The Report fails or succeeds as a unit. If any responses had errors,
  // the header will too. Thus we don't need to propagate individual errors.
  if (response.has_update_signals_response())
    LogIfErrorStatus(response.update_signals_response().status(), "Update");
  if (response.has_manage_messages_response())
    LogIfErrorStatus(response.manage_messages_response().status(), "Publish");
  if (response.has_manage_subscriptions_response()) {
    LogIfErrorStatus(response.manage_subscriptions_response().status(),
                     "Subscribe");
  }

  return result;
}

const std::string LoggingStrForToken(const std::string& auth_token) {
  if (auth_token.empty())
    return "anonymous";

  std::string token_suffix = auth_token.substr(
      auth_token.length() - kTokenLoggingSuffix, kTokenLoggingSuffix);
  return "token ..." + token_suffix;
}


// Request construction

template <typename T>
BroadcastScanConfiguration GetBroadcastScanConfig(const T& msg) {
  if (msg.has_token_exchange_strategy() &&
      msg.token_exchange_strategy().has_broadcast_scan_configuration()) {
    return msg.token_exchange_strategy().broadcast_scan_configuration();
  }
  return BROADCAST_SCAN_CONFIGURATION_UNKNOWN;
}

std::unique_ptr<DeviceState> GetDeviceCapabilities(
    const ReportRequest& request) {
  std::unique_ptr<DeviceState> state(new DeviceState);

  TokenTechnology* ultrasound =
      state->mutable_capabilities()->add_token_technology();
  ultrasound->set_medium(AUDIO_ULTRASOUND_PASSBAND);
  ultrasound->add_instruction_type(TRANSMIT);
  ultrasound->add_instruction_type(RECEIVE);

  TokenTechnology* audible =
      state->mutable_capabilities()->add_token_technology();
  audible->set_medium(AUDIO_AUDIBLE_DTMF);
  audible->add_instruction_type(TRANSMIT);
  audible->add_instruction_type(RECEIVE);

  return state;
}

// TODO(ckehoe): We're keeping this code in a separate function for now
// because we get a version string from Chrome, but the proto expects
// an int64_t version. We should probably change the version proto
// to handle a more detailed version.
ClientVersion* CreateVersion(const std::string& client,
                             const std::string& version_name) {
  ClientVersion* version = new ClientVersion;
  version->set_client(client);
  version->set_version_name(version_name);
  return version;
}

void AddTokenToRequest(const AudioToken& token, ReportRequest* request) {
  TokenObservation* token_observation =
      request->mutable_update_signals_request()->add_token_observation();
  token_observation->set_token_id(ToUrlSafe(token.token));

  TokenSignals* signals = token_observation->add_signals();
  signals->set_medium(token.audible ? AUDIO_AUDIBLE_DTMF
                                    : AUDIO_ULTRASOUND_PASSBAND);
  signals->set_observed_time_millis(base::Time::Now().ToJsTime());
}

}  // namespace


// Public functions.

RpcHandler::RpcHandler(CopresenceDelegate* delegate,
                       DirectiveHandler* directive_handler,
                       CopresenceStateImpl* state,
                       GCMHandler* gcm_handler,
                       const MessagesCallback& new_messages_callback,
                       const PostCallback& server_post_callback)
    : delegate_(delegate),
      directive_handler_(directive_handler),
      state_(state),
      gcm_handler_(gcm_handler),
      new_messages_callback_(new_messages_callback),
      server_post_callback_(server_post_callback),
      invalid_audio_token_cache_(
          base::TimeDelta::FromMinutes(kInvalidTokenExpiryTimeMinutes),
          kMaxInvalidTokens) {
    DCHECK(delegate_);
    DCHECK(directive_handler_);
    // |gcm_handler_| is optional.

    if (server_post_callback_.is_null()) {
      server_post_callback_ =
          base::Bind(&RpcHandler::SendHttpPost, base::Unretained(this));
    }

    if (gcm_handler_) {
      gcm_handler_->GetGcmId(
          base::Bind(&RpcHandler::RegisterGcmId, base::Unretained(this)));
    }
  }

RpcHandler::~RpcHandler() {
  // TODO(ckehoe): Cancel the GCM callback?
  for (HttpPost* post : pending_posts_)
    delete post;
}

void RpcHandler::SendReportRequest(std::unique_ptr<ReportRequest> request,
                                   const std::string& app_id,
                                   const std::string& auth_token,
                                   const StatusCallback& status_callback) {
  DCHECK(request.get());

  // Check that the app, if any, has some kind of authentication token.
  // Don't allow it to piggyback on Chrome's credentials.
  if (!app_id.empty() && delegate_->GetAPIKey(app_id).empty() &&
      auth_token.empty()) {
    LOG(ERROR) << "App " << app_id << " has no API key or auth token";
    status_callback.Run(FAIL);
    return;
  }

  // Store just one auth token since we should have only one account
  // per instance of the copresence component.
  // TODO(ckehoe): We may eventually need to support multiple auth tokens.
  const bool authenticated = !auth_token.empty();
  if (authenticated && auth_token != auth_token_) {
    LOG_IF(ERROR, !auth_token_.empty())
        << "Overwriting old auth token: " << LoggingStrForToken(auth_token);
    auth_token_ = auth_token;
  }

  // Check that we have a "device" registered for this authentication state.
  bool queue_request;
  const std::string device_id = delegate_->GetDeviceId(authenticated);
  if (device_id.empty()) {
    queue_request = true;
    if (pending_registrations_.count(authenticated) == 0)
      RegisterDevice(authenticated);
    // else, registration is already in progress.
  } else {
    queue_request = false;
  }

  // We're not registered, or registration is in progress.
  if (queue_request) {
    pending_requests_queue_.push_back(new PendingRequest(
        std::move(request), app_id, authenticated, status_callback));
    return;
  }

  DVLOG(3) << "Sending ReportRequest to server.";

  // If we are unpublishing or unsubscribing, we need to stop those publish or
  // subscribes right away, we don't need to wait for the server to tell us.
  ProcessRemovedOperations(*request);

  request->mutable_update_signals_request()->set_allocated_state(
      GetDeviceCapabilities(*request).release());

  AddPlayingTokens(request.get());

  request->set_allocated_header(CreateRequestHeader(app_id, device_id));
  server_post_callback_.Run(
      delegate_->GetRequestContext(), kReportRequestRpcName,
      delegate_->GetAPIKey(app_id), auth_token,
      base::WrapUnique<MessageLite>(request.release()),
      // On destruction, this request will be cancelled.
      base::Bind(&RpcHandler::ReportResponseHandler, base::Unretained(this),
                 status_callback));
}

void RpcHandler::ReportTokens(const std::vector<AudioToken>& tokens) {
  DCHECK(!tokens.empty());

  std::unique_ptr<ReportRequest> request(new ReportRequest);
  for (const AudioToken& token : tokens) {
    if (invalid_audio_token_cache_.HasKey(ToUrlSafe(token.token)))
      continue;
    DVLOG(3) << "Sending token " << token.token << " to server";
    AddTokenToRequest(token, request.get());
  }

  ReportOnAllDevices(std::move(request));
}


// Private functions.

RpcHandler::PendingRequest::PendingRequest(
    std::unique_ptr<ReportRequest> report,
    const std::string& app_id,
    bool authenticated,
    const StatusCallback& callback)
    : report(std::move(report)),
      app_id(app_id),
      authenticated(authenticated),
      callback(callback) {}

RpcHandler::PendingRequest::~PendingRequest() {}

void RpcHandler::RegisterDevice(const bool authenticated) {
  DVLOG(2) << "Sending " << (authenticated ? "authenticated" : "anonymous")
           << " registration to server.";

  std::unique_ptr<RegisterDeviceRequest> request(new RegisterDeviceRequest);

  // Add a GCM ID for authenticated registration, if we have one.
  if (!authenticated || gcm_id_.empty()) {
    request->mutable_push_service()->set_service(PUSH_SERVICE_NONE);
  } else {
    DVLOG(2) << "Registering GCM ID with " << LoggingStrForToken(auth_token_);
    request->mutable_push_service()->set_service(GCM);
    request->mutable_push_service()->mutable_gcm_registration()
        ->set_device_token(gcm_id_);
  }

  // Only identify as a Chrome device if we're in anonymous mode.
  // Authenticated calls come from a "GAIA device".
  if (!authenticated) {
    // Make sure this isn't a duplicate anonymous registration.
    // Duplicate authenticated registrations are allowed, to update the GCM ID.
    DCHECK(delegate_->GetDeviceId(false).empty())
        << "Attempted anonymous re-registration";

    Identity* identity =
        request->mutable_device_identifiers()->mutable_registrant();
    identity->set_type(CHROME);
  }

  bool gcm_pending = authenticated && gcm_handler_ && gcm_id_.empty();
  pending_registrations_.insert(authenticated);
  request->set_allocated_header(CreateRequestHeader(
     // The device is empty on first registration.
     // When re-registering to pass on the GCM ID, it will be present.
     std::string(), delegate_->GetDeviceId(authenticated)));
  if (authenticated)
    DCHECK(!auth_token_.empty());
  server_post_callback_.Run(
      delegate_->GetRequestContext(), kRegisterDeviceRpcName, std::string(),
      authenticated ? auth_token_ : std::string(),
      base::WrapUnique<MessageLite>(request.release()),
      // On destruction, this request will be cancelled.
      base::Bind(&RpcHandler::RegisterResponseHandler, base::Unretained(this),
                 authenticated, gcm_pending));
}

void RpcHandler::ProcessQueuedRequests(const bool authenticated) {
  // If there is no device ID for this auth state, registration failed.
  bool registration_failed = delegate_->GetDeviceId(authenticated).empty();

  // We momentarily take ownership of all the pointers in the queue.
  // They are either deleted here or passed on to a new queue.
  ScopedVector<PendingRequest> requests_being_processed;
  std::swap(requests_being_processed, pending_requests_queue_);
  for (PendingRequest* request : requests_being_processed) {
    if (request->authenticated == authenticated) {
      if (registration_failed) {
        request->callback.Run(FAIL);
      } else {
        if (request->authenticated)
          DCHECK(!auth_token_.empty());
        SendReportRequest(std::move(request->report), request->app_id,
                          request->authenticated ? auth_token_ : std::string(),
                          request->callback);
      }
      delete request;
    } else {
      // The request is in a different auth state.
      pending_requests_queue_.push_back(request);
    }
  }

  // Only keep the requests that weren't processed.
  // All the pointers in the queue are now spoken for.
  requests_being_processed.weak_clear();
}

void RpcHandler::ReportOnAllDevices(std::unique_ptr<ReportRequest> request) {
  std::vector<bool> auth_states;
  if (!auth_token_.empty() && !delegate_->GetDeviceId(true).empty())
    auth_states.push_back(true);
  if (!delegate_->GetDeviceId(false).empty())
    auth_states.push_back(false);
  if (auth_states.empty()) {
    VLOG(2) << "Skipping reporting because no device IDs are registered";
    return;
  }

  for (bool authenticated : auth_states) {
    SendReportRequest(
        base::WrapUnique(new ReportRequest(*request)), std::string(),
        authenticated ? auth_token_ : std::string(), StatusCallback());
  }
}

// Store a GCM ID and send it to the server if needed. The constructor passes
// this callback to the GCMHandler to receive the ID whenever it's ready.
// It may be returned immediately, if the ID is cached, or require a server
// round-trip. This ID must then be passed along to the copresence server.
// There are a few ways this can happen:
//
// 1. The GCM ID is available when we first register, and is passed along
//    with the RegisterDeviceRequest.
//
// 2. The GCM ID becomes available after the RegisterDeviceRequest has
//    completed. Then this function will invoke RegisterDevice()
//    again to pass on the ID.
//
// 3. The GCM ID becomes available after the RegisterDeviceRequest is sent,
//    but before it completes. In this case, the gcm_pending flag is passed
//    through to the RegisterResponseHandler, which invokes RegisterDevice()
//    again to pass on the ID. This function must skip pending registrations,
//    as the device ID will be empty.
//
// TODO(ckehoe): Add tests for these scenarios.
void RpcHandler::RegisterGcmId(const std::string& gcm_id) {
  gcm_id_ = gcm_id;
  if (!gcm_id.empty()) {
    const std::string& device_id = delegate_->GetDeviceId(true);
    if (!auth_token_.empty() && !device_id.empty())
      RegisterDevice(true);
  }
}

void RpcHandler::RegisterResponseHandler(
    bool authenticated,
    bool gcm_pending,
    HttpPost* completed_post,
    int http_status_code,
    const std::string& response_data) {
  if (completed_post) {
    size_t elements_erased = pending_posts_.erase(completed_post);
    DCHECK_GT(elements_erased, 0u);
  }

  size_t registrations_completed = pending_registrations_.erase(authenticated);
  DCHECK_GT(registrations_completed, 0u);

  RegisterDeviceResponse response;
  const std::string token_str =
      LoggingStrForToken(authenticated ? auth_token_ : std::string());
  if (http_status_code != net::HTTP_OK) {
    // TODO(ckehoe): Retry registration if appropriate.
    LOG(ERROR) << token_str << " device registration failed";
  } else if (!response.ParseFromString(response_data)) {
    LOG(ERROR) << "Invalid RegisterDeviceResponse:\n" << response_data;
  } else if (!IsErrorStatus(response.header().status())) {
    const std::string& device_id = response.registered_device_id();
    DCHECK(!device_id.empty());
    delegate_->SaveDeviceId(authenticated, device_id);
    DVLOG(2) << token_str << " device registration successful. Id: "
             << device_id;

    // If we have a GCM ID now, and didn't before, pass it on to the server.
    if (gcm_pending && !gcm_id_.empty())
      RegisterDevice(authenticated);
  }

  // Send or fail requests on this auth token.
  ProcessQueuedRequests(authenticated);
}

void RpcHandler::ReportResponseHandler(const StatusCallback& status_callback,
                                       HttpPost* completed_post,
                                       int http_status_code,
                                       const std::string& response_data) {
  if (completed_post) {
    size_t elements_erased = pending_posts_.erase(completed_post);
    DCHECK_GT(elements_erased, 0u);
  }

  if (http_status_code != net::HTTP_OK) {
    if (!status_callback.is_null())
      status_callback.Run(FAIL);
    return;
  }

  DVLOG(3) << "Received ReportResponse.";
  ReportResponse response;
  if (!response.ParseFromString(response_data)) {
    LOG(ERROR) << "Invalid ReportResponse";
    if (!status_callback.is_null())
      status_callback.Run(FAIL);
    return;
  }

  if (ReportErrorLogged(response)) {
    if (!status_callback.is_null())
      status_callback.Run(FAIL);
    return;
  }

  for (const MessageResult& result :
      response.manage_messages_response().published_message_result()) {
    DVLOG(2) << "Published message with id " << result.published_message_id();
  }

  for (const SubscriptionResult& result :
      response.manage_subscriptions_response().subscription_result()) {
    DVLOG(2) << "Created subscription with id " << result.subscription_id();
  }

  if (response.has_update_signals_response()) {
    const UpdateSignalsResponse& update_response =
        response.update_signals_response();
    new_messages_callback_.Run(update_response.message());

    for (const Directive& directive : update_response.directive())
      directive_handler_->AddDirective(directive);

    for (const Token& token : update_response.token()) {
      if (state_)
        state_->UpdateTokenStatus(token.id(), token.status());
      switch (token.status()) {
        case VALID:
          // TODO(rkc/ckehoe): Store the token in a |valid_token_cache_| with a
          // short TTL (like 10s) and send it up with every report request.
          // Then we'll still get messages while we're waiting to hear it again.
          VLOG(1) << "Got valid token " << token.id();
          break;
        case INVALID:
          DVLOG(3) << "Discarding invalid token " << token.id();
          invalid_audio_token_cache_.Add(token.id(), true);
          break;
        default:
          DVLOG(2) << "Token " << token.id() << " has status code "
                   << token.status();
      }
    }
  }

  // TODO(ckehoe): Return a more detailed status response.
  if (!status_callback.is_null())
    status_callback.Run(SUCCESS);
}

void RpcHandler::ProcessRemovedOperations(const ReportRequest& request) {
  // Remove unpublishes.
  if (request.has_manage_messages_request()) {
    for (const std::string& unpublish :
        request.manage_messages_request().id_to_unpublish()) {
      directive_handler_->RemoveDirectives(unpublish);
    }
  }

  // Remove unsubscribes.
  if (request.has_manage_subscriptions_request()) {
    for (const std::string& unsubscribe :
        request.manage_subscriptions_request().id_to_unsubscribe()) {
      directive_handler_->RemoveDirectives(unsubscribe);
    }
  }
}

void RpcHandler::AddPlayingTokens(ReportRequest* request) {
  const std::string& audible_token =
      directive_handler_->GetCurrentAudioToken(AUDIBLE);
  const std::string& inaudible_token =
      directive_handler_->GetCurrentAudioToken(INAUDIBLE);

  if (!audible_token.empty())
    AddTokenToRequest(AudioToken(audible_token, true), request);
  if (!inaudible_token.empty())
    AddTokenToRequest(AudioToken(inaudible_token, false), request);
}

// TODO(ckehoe): Pass in the version string and
// group this with the local functions up top.
RequestHeader* RpcHandler::CreateRequestHeader(
    const std::string& app_id,
    const std::string& device_id) const {
  RequestHeader* header = new RequestHeader;

  header->set_allocated_framework_version(CreateVersion(
      "Chrome", delegate_->GetPlatformVersionString()));
  if (!app_id.empty())
    header->set_allocated_client_version(CreateVersion(app_id, std::string()));
  header->set_current_time_millis(base::Time::Now().ToJsTime());
  if (!device_id.empty())
    header->set_registered_device_id(device_id);

  DeviceFingerprint* fingerprint = new DeviceFingerprint;
  fingerprint->set_platform_version(delegate_->GetPlatformVersionString());
  fingerprint->set_type(CHROME_PLATFORM_TYPE);
  header->set_allocated_device_fingerprint(fingerprint);

  return header;
}

void RpcHandler::SendHttpPost(net::URLRequestContextGetter* url_context_getter,
                              const std::string& rpc_name,
                              const std::string& api_key,
                              const std::string& auth_token,
                              std::unique_ptr<MessageLite> request_proto,
                              const PostCleanupCallback& callback) {
  // Create the base URL to call.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const std::string copresence_server_host =
      command_line->HasSwitch(switches::kCopresenceServer) ?
      command_line->GetSwitchValueASCII(switches::kCopresenceServer) :
      kDefaultCopresenceServer;

  // Create the request and keep a pointer until it completes.
  HttpPost* http_post = new HttpPost(
      url_context_getter,
      copresence_server_host,
      rpc_name,
      api_key,
      auth_token,
      command_line->GetSwitchValueASCII(switches::kCopresenceTracingToken),
      *request_proto);

  http_post->Start(base::Bind(callback, http_post));
  pending_posts_.insert(http_post);
}

}  // namespace copresence
