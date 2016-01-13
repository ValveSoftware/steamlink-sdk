// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/peer_connection_identity_service.h"

#include "content/renderer/media/webrtc_identity_service.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

PeerConnectionIdentityService::PeerConnectionIdentityService(const GURL& origin)
    : origin_(origin), pending_observer_(NULL), pending_request_id_(0) {}

PeerConnectionIdentityService::~PeerConnectionIdentityService() {
  if (pending_observer_)
    RenderThreadImpl::current()->get_webrtc_identity_service()
        ->CancelRequest(pending_request_id_);
}

bool PeerConnectionIdentityService::RequestIdentity(
    const std::string& identity_name,
    const std::string& common_name,
    webrtc::DTLSIdentityRequestObserver* observer) {
  DCHECK(observer);
  if (pending_observer_)
    return false;

  pending_observer_ = observer;
  pending_request_id_ = RenderThreadImpl::current()
      ->get_webrtc_identity_service()->RequestIdentity(
          origin_,
          identity_name,
          common_name,
          base::Bind(&PeerConnectionIdentityService::OnIdentityReady,
                     base::Unretained(this)),
          base::Bind(&PeerConnectionIdentityService::OnRequestFailed,
                     base::Unretained(this)));
  return true;
}

void PeerConnectionIdentityService::OnIdentityReady(
    const std::string& certificate,
    const std::string& private_key) {
  pending_observer_->OnSuccess(certificate, private_key);
  ResetPendingRequest();
}

void PeerConnectionIdentityService::OnRequestFailed(int error) {
  pending_observer_->OnFailure(error);
  ResetPendingRequest();
}

void PeerConnectionIdentityService::ResetPendingRequest() {
  pending_observer_ = NULL;
  pending_request_id_ = 0;
}

}  // namespace content
