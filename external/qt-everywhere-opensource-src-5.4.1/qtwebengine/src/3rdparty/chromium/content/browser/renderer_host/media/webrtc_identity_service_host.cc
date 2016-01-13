// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/webrtc_identity_service_host.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/media/webrtc_identity_store.h"
#include "content/common/media/webrtc_identity_messages.h"
#include "net/base/net_errors.h"

namespace content {

WebRTCIdentityServiceHost::WebRTCIdentityServiceHost(
    int renderer_process_id,
    scoped_refptr<WebRTCIdentityStore> identity_store)
    : BrowserMessageFilter(WebRTCIdentityMsgStart),
      renderer_process_id_(renderer_process_id),
      identity_store_(identity_store),
      weak_factory_(this) {}

WebRTCIdentityServiceHost::~WebRTCIdentityServiceHost() {
  if (!cancel_callback_.is_null())
    cancel_callback_.Run();
}

bool WebRTCIdentityServiceHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebRTCIdentityServiceHost, message)
    IPC_MESSAGE_HANDLER(WebRTCIdentityMsg_RequestIdentity, OnRequestIdentity)
    IPC_MESSAGE_HANDLER(WebRTCIdentityMsg_CancelRequest, OnCancelRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebRTCIdentityServiceHost::OnRequestIdentity(
    int sequence_number,
    const GURL& origin,
    const std::string& identity_name,
    const std::string& common_name) {
  if (!cancel_callback_.is_null()) {
    DLOG(WARNING)
        << "Request rejected because the previous request has not finished.";
    SendErrorMessage(sequence_number, net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessCookiesForOrigin(renderer_process_id_, origin)) {
    DLOG(WARNING) << "Request rejected because origin access is denied.";
    SendErrorMessage(sequence_number, net::ERR_ACCESS_DENIED);
    return;
  }

  cancel_callback_ = identity_store_->RequestIdentity(
      origin,
      identity_name,
      common_name,
      base::Bind(&WebRTCIdentityServiceHost::OnComplete,
                 weak_factory_.GetWeakPtr(),
                 sequence_number));
  if (cancel_callback_.is_null()) {
    SendErrorMessage(sequence_number, net::ERR_UNEXPECTED);
  }
}

void WebRTCIdentityServiceHost::OnCancelRequest() {
  // cancel_callback_ may be null if we have sent the reponse to the renderer
  // but the renderer has not received it.
  if (!cancel_callback_.is_null())
    base::ResetAndReturn(&cancel_callback_).Run();
}

void WebRTCIdentityServiceHost::OnComplete(int sequence_number,
                                           int status,
                                           const std::string& certificate,
                                           const std::string& private_key) {
  cancel_callback_.Reset();
  if (status == net::OK) {
    Send(new WebRTCIdentityHostMsg_IdentityReady(
        sequence_number, certificate, private_key));
  } else {
    SendErrorMessage(sequence_number, status);
  }
}

void WebRTCIdentityServiceHost::SendErrorMessage(int sequence_number,
                                                 int error) {
  Send(new WebRTCIdentityHostMsg_RequestFailed(sequence_number, error));
}

}  // namespace content
