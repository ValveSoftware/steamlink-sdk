// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_
#define COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_vector.h"
#include "components/audio_modem/public/audio_modem_types.h"
#include "components/copresence/proto/enums.pb.h"
#include "components/copresence/public/copresence_constants.h"
#include "components/copresence/public/copresence_delegate.h"
#include "components/copresence/timed_map.h"

namespace copresence {

class CopresenceDelegate;
class CopresenceStateImpl;
class DirectiveHandler;
class GCMHandler;
class HttpPost;
class ReportRequest;
class RequestHeader;
class SubscribedMessage;

// This class handles all communication with the copresence server.
// Clients provide a ReportRequest proto containing publishes, subscribes,
// and token observations they want to send to the server. The RpcHandler
// will fill in details like the RequestHeader and DeviceCapabilities,
// and dispatch the results of the server call to the appropriate parts
// of the system.
//
// To create an RpcHandler, clients will need to provide a few other classes
// that support its functionality. Notable among them is the CopresenceDelegate,
// an interface clients must implement to provide settings and functionality
// that may depend on the environment. See the definition in
// //components/copresence/public/copresence_delegate.h.
//
// Here is an example of creating and using an RpcHandler.
// The GCMHandler and CopresenceStateImpl are optional.
//
// MyDelegate delegate(...);
// copresence::DirectiveHandlerImpl directive_handler;
//
// RpcHandler handler(&delegate,
//                    &directive_handler,
//                    nullptr,
//                    nullptr,
//                    base::Bind(&HandleMessages));
//
// std::unique_ptr<ReportRequest> request(new ReportRequest);
// (Fill in ReportRequest.)
//
// handler.SendReportRequest(std::move(request),
//                           "my_app_id",
//                           "",
//                           base::Bind(&HandleStatus));
//
// The server will respond with directives, which get passed to the
// DirectiveHandlerImpl.
//
// Tokens from the audio modem should also be forwarded
// via ReportTokens() so that messages get delivered properly.
class RpcHandler final {
 public:
  // An HttpPost::ResponseCallback along with an HttpPost object to be deleted.
  // Arguments:
  // HttpPost*: The handler should take ownership of (i.e. delete) this object.
  // int: The HTTP status code of the response.
  // string: The contents of the response.
  using PostCleanupCallback = base::Callback<void(HttpPost*,
                                                  int,
                                                  const std::string&)>;

  // Callback to allow tests to stub out HTTP POST behavior.
  // Arguments:
  // URLRequestContextGetter: Context for the HTTP POST request.
  // string: Name of the rpc to invoke. URL format: server.google.com/rpc_name
  // string: The API key to pass in the request.
  // string: The auth token to pass with the request.
  // MessageLite: Contents of POST request to be sent. This needs to be
  //     a (scoped) pointer to ease handling of the abstract MessageLite class.
  // PostCleanupCallback: Receives the response to the request.
  using PostCallback =
      base::Callback<void(net::URLRequestContextGetter*,
                          const std::string&,
                          const std::string&,
                          const std::string&,
                          std::unique_ptr<google::protobuf::MessageLite>,
                          const PostCleanupCallback&)>;

  // Report rpc name to send to Apiary.
  static const char kReportRequestRpcName[];

  // Constructor. The CopresenceStateImpl and GCMHandler may be null.
  // The first four parameters are owned by the caller and (if not null)
  // must outlive the RpcHandler.
  RpcHandler(CopresenceDelegate* delegate,
             DirectiveHandler* directive_handler,
             CopresenceStateImpl* state,
             GCMHandler* gcm_handler,
             const MessagesCallback& new_messages_callback,
             const PostCallback& server_post_callback = PostCallback());

  // Not copyable.
  RpcHandler(const RpcHandler&) = delete;
  void operator=(const RpcHandler&) = delete;

  ~RpcHandler();

  // Sends a ReportRequest from a specific app, and get notified of completion.
  void SendReportRequest(std::unique_ptr<ReportRequest> request,
                         const std::string& app_id,
                         const std::string& auth_token,
                         const StatusCallback& callback);

  // Reports a set of tokens to the server for a given medium.
  // Uses all active auth tokens (if any).
  void ReportTokens(const std::vector<audio_modem::AudioToken>& tokens);

 private:
  // A queued ReportRequest along with its metadata.
  struct PendingRequest {
    PendingRequest(std::unique_ptr<ReportRequest> report,
                   const std::string& app_id,
                   bool authenticated,
                   const StatusCallback& callback);
    ~PendingRequest();

    std::unique_ptr<ReportRequest> report;
    const std::string app_id;
    const bool authenticated;
    const StatusCallback callback;
  };

  friend class RpcHandlerTest;

  // Before accepting any other calls, the server requires registration,
  // which is tied to the auth token (or lack thereof) used to call Report.
  void RegisterDevice(bool authenticated);

  // Device registration has completed. Send the requests that it was blocking.
  void ProcessQueuedRequests(bool authenticated);

  // Sends a ReportRequest from Chrome itself, i.e. no app id.
  void ReportOnAllDevices(std::unique_ptr<ReportRequest> request);

  // Stores a GCM ID and send it to the server if needed.
  void RegisterGcmId(const std::string& gcm_id);

  // Server call response handlers.
  void RegisterResponseHandler(bool authenticated,
                               bool gcm_pending,
                               HttpPost* completed_post,
                               int http_status_code,
                               const std::string& response_data);
  void ReportResponseHandler(const StatusCallback& status_callback,
                             HttpPost* completed_post,
                             int http_status_code,
                             const std::string& response_data);

  // Removes unpublished or unsubscribed operations from the directive handlers.
  void ProcessRemovedOperations(const ReportRequest& request);

  // Adds all currently playing tokens to the update signals in this report
  // request. This ensures that the server doesn't keep issueing new tokens to
  // us when we're already playing valid tokens.
  void AddPlayingTokens(ReportRequest* request);

  void DispatchMessages(
      const google::protobuf::RepeatedPtrField<SubscribedMessage>&
      subscribed_messages);

  RequestHeader* CreateRequestHeader(const std::string& app_id,
                                     const std::string& device_id) const;

  // Wrapper for the http post constructor. This is the default way
  // to contact the server, but it can be overridden for testing.
  void SendHttpPost(
      net::URLRequestContextGetter* url_context_getter,
      const std::string& rpc_name,
      const std::string& api_key,
      const std::string& auth_token,
      std::unique_ptr<google::protobuf::MessageLite> request_proto,
      const PostCleanupCallback& callback);

  // These belong to the caller.
  CopresenceDelegate* const delegate_;
  DirectiveHandler* const directive_handler_;
  CopresenceStateImpl* state_;
  GCMHandler* const gcm_handler_;

  MessagesCallback new_messages_callback_;
  PostCallback server_post_callback_;

  ScopedVector<PendingRequest> pending_requests_queue_;
  TimedMap<std::string, bool> invalid_audio_token_cache_;
  std::set<HttpPost*> pending_posts_;
  std::set<bool> pending_registrations_;
  std::string auth_token_;
  std::string gcm_id_;
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_RPC_RPC_HANDLER_H_
