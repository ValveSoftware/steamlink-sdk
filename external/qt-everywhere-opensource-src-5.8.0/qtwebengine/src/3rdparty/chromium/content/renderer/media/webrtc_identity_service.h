// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_IDENTITY_SERVICE_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_IDENTITY_SERVICE_H_

#include <deque>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/media/webrtc_identity_messages.h"
#include "content/public/renderer/render_thread_observer.h"
#include "url/gurl.h"

namespace content {

// This class handles WebRTC DTLS identity requests by sending IPC messages to
// the browser process. Only one request is sent to the browser at a time; other
// requests are queued and have to wait for the outstanding request to complete.
class CONTENT_EXPORT WebRTCIdentityService : public RenderThreadObserver {
 public:
  typedef base::Callback<
      void(const std::string& certificate, const std::string& private_key)>
      SuccessCallback;

  typedef base::Callback<void(int error)> FailureCallback;

  WebRTCIdentityService();
  ~WebRTCIdentityService() override;

  // Sends an identity request.
  //
  // |url| is the frame url of the caller;
  // |first_party_for_cookies| is the first party url for checking cookie
  // policies.
  // |identity_name| and |common_name| have the same meaning as in
  // webrtc::DTLSIdentityServiceInterface::RequestIdentity;
  // |success_callback| is the callback if the identity is successfully
  // returned;
  // |failure_callback| is the callback if the identity request fails.
  //
  // The request id is returned. It's unique within the renderer and can be used
  // to cancel the request.
  int RequestIdentity(const GURL& url,
                      const GURL& first_party_for_cookies,
                      const std::string& identity_name,
                      const std::string& common_name,
                      const SuccessCallback& success_callback,
                      const FailureCallback& failure_callback);

  // Cancels a previous request and the callbacks will not be called.
  // If the |request_id| is not associated with the
  // outstanding request or any queued request, this method does nothing.
  //
  // |request_id| is the request id returned from RequestIdentity.
  void CancelRequest(int request_id);

 protected:
  // For unittest to override.
  virtual bool Send(IPC::Message* message);
  // RenderThreadObserver implementation. Protected for testing.
  bool OnControlMessageReceived(const IPC::Message& message) override;

 private:
  struct RequestInfo {
    RequestInfo(const WebRTCIdentityMsg_RequestIdentity_Params& params,
                const SuccessCallback& success_callback,
                const FailureCallback& failure_callback);
    RequestInfo(const RequestInfo& other);
    ~RequestInfo();

    WebRTCIdentityMsg_RequestIdentity_Params params;
    SuccessCallback success_callback;
    FailureCallback failure_callback;
  };

  // IPC message handlers.
  void OnIdentityReady(int request_id,
                       const std::string& certificate,
                       const std::string& private_key);
  void OnRequestFailed(int request_id, int error);

  void SendRequest(const RequestInfo& request_info);
  void OnOutstandingRequestReturned();

  std::deque<RequestInfo> pending_requests_;
  int next_request_id_;

  DISALLOW_COPY_AND_ASSIGN(WebRTCIdentityService);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_IDENTITY_SERVICE_H_
