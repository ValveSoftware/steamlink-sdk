// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEER_CONNECTION_IDENTITY_SERVICE_H_
#define CONTENT_RENDERER_MEDIA_PEER_CONNECTION_IDENTITY_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "url/gurl.h"

namespace content {

// This class is associated with a peer connection and handles WebRTC DTLS
// identity requests by delegating to the per-renderer WebRTCIdentityProxy.
class PeerConnectionIdentityService
    : public webrtc::DTLSIdentityServiceInterface {
 public:
  explicit PeerConnectionIdentityService(const GURL& origin);

  virtual ~PeerConnectionIdentityService();

  // webrtc::DTLSIdentityServiceInterface implementation.
  virtual bool RequestIdentity(
      const std::string& identity_name,
      const std::string& common_name,
      webrtc::DTLSIdentityRequestObserver* observer) OVERRIDE;

 private:
  void OnIdentityReady(const std::string& certificate,
                       const std::string& private_key);
  void OnRequestFailed(int error);

  void ResetPendingRequest();

  // The origin of the DTLS connection.
  GURL origin_;
  talk_base::scoped_refptr<webrtc::DTLSIdentityRequestObserver>
      pending_observer_;
  int pending_request_id_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionIdentityService);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_PEER_CONNECTION_IDENTITY_SERVICE_H_
