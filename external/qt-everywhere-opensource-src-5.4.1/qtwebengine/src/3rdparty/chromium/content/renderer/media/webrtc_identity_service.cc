// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_identity_service.h"

#include "content/common/media/webrtc_identity_messages.h"
#include "content/public/renderer/render_thread.h"
#include "net/base/net_errors.h"

namespace content {

WebRTCIdentityService::RequestInfo::RequestInfo(
    int request_id,
    const GURL& origin,
    const std::string& identity_name,
    const std::string& common_name,
    const SuccessCallback& success_callback,
    const FailureCallback& failure_callback)
    : request_id(request_id),
      origin(origin),
      identity_name(identity_name),
      common_name(common_name),
      success_callback(success_callback),
      failure_callback(failure_callback) {}

WebRTCIdentityService::RequestInfo::~RequestInfo() {}

WebRTCIdentityService::WebRTCIdentityService() : next_request_id_(1) {
  // RenderThread::Get() could be NULL in unit tests.
  if (RenderThread::Get())
    RenderThread::Get()->AddObserver(this);
}

WebRTCIdentityService::~WebRTCIdentityService() {
  // RenderThread::Get() could be NULL in unit tests.
  if (RenderThread::Get()) {
    RenderThread::Get()->RemoveObserver(this);

    if (!pending_requests_.empty()) {
      RenderThread::Get()->Send(new WebRTCIdentityMsg_CancelRequest());
    }
  }
}

int WebRTCIdentityService::RequestIdentity(
    const GURL& origin,
    const std::string& identity_name,
    const std::string& common_name,
    const SuccessCallback& success_callback,
    const FailureCallback& failure_callback) {
  int request_id = next_request_id_++;

  RequestInfo request_info(request_id,
                           origin,
                           identity_name,
                           common_name,
                           success_callback,
                           failure_callback);

  pending_requests_.push_back(request_info);
  if (pending_requests_.size() == 1)
    SendRequest(request_info);

  return request_id;
}

void WebRTCIdentityService::CancelRequest(int request_id) {
  std::deque<RequestInfo>::iterator it;
  for (it = pending_requests_.begin(); it != pending_requests_.end(); ++it) {
    if (it->request_id != request_id)
      continue;
    if (it != pending_requests_.begin()) {
      pending_requests_.erase(it);
    } else {
      Send(new WebRTCIdentityMsg_CancelRequest());
      OnOutstandingRequestReturned();
    }
    break;
  }
}

bool WebRTCIdentityService::Send(IPC::Message* message) {
  // Unit tests should override this method to avoid null-ptr-deref.
  return RenderThread::Get()->Send(message);
}

bool WebRTCIdentityService::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebRTCIdentityService, message)
    IPC_MESSAGE_HANDLER(WebRTCIdentityHostMsg_IdentityReady, OnIdentityReady)
    IPC_MESSAGE_HANDLER(WebRTCIdentityHostMsg_RequestFailed, OnRequestFailed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void WebRTCIdentityService::OnIdentityReady(int request_id,
                                            const std::string& certificate,
                                            const std::string& private_key) {
  // The browser process may have sent the response before it receives the
  // message to cancel the request. So we need to check if the returned response
  // matches the request on the top of the queue.
  if (pending_requests_.empty() ||
      pending_requests_.front().request_id != request_id)
    return;

  pending_requests_.front().success_callback.Run(certificate, private_key);
  OnOutstandingRequestReturned();
}

void WebRTCIdentityService::OnRequestFailed(int request_id, int error) {
  // The browser process may have sent the response before it receives the
  // message to cancel the request. So we need to check if the returned response
  // matches the request on the top of the queue.
  if (pending_requests_.empty() ||
      pending_requests_.front().request_id != request_id)
    return;

  pending_requests_.front().failure_callback.Run(error);
  OnOutstandingRequestReturned();
}

void WebRTCIdentityService::SendRequest(const RequestInfo& request_info) {
  if (!Send(new WebRTCIdentityMsg_RequestIdentity(request_info.request_id,
                                                  request_info.origin,
                                                  request_info.identity_name,
                                                  request_info.common_name))) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&WebRTCIdentityService::OnRequestFailed,
                   base::Unretained(this),
                   request_info.request_id,
                   net::ERR_UNEXPECTED));
  }
}

void WebRTCIdentityService::OnOutstandingRequestReturned() {
  pending_requests_.pop_front();

  if (!pending_requests_.empty())
    SendRequest(pending_requests_.front());
}

}  // namespace content
