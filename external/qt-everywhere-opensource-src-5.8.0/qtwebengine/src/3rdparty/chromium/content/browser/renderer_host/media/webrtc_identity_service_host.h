// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEBRTC_IDENTITY_SERVICE_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEBRTC_IDENTITY_SERVICE_HOST_H_

#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"

class GURL;

struct WebRTCIdentityMsg_RequestIdentity_Params;

namespace content {

class ResourceContext;
class WebRTCIdentityStore;

// This class is the host for WebRTCIdentityService in the browser process.
// It converts the IPC messages for requesting a WebRTC DTLS identity and
// cancelling a pending request into calls of WebRTCIdentityStore. It also sends
// the request result back to the renderer through IPC.
// Only one outstanding request is allowed per renderer at a time. If a second
// request is made before the first one completes, an IPC with error
// ERR_INSUFFICIENT_RESOURCES will be sent back to the renderer.
class CONTENT_EXPORT WebRTCIdentityServiceHost : public BrowserMessageFilter {
 public:
  WebRTCIdentityServiceHost(int renderer_process_id,
                            scoped_refptr<WebRTCIdentityStore> identity_store,
                            ResourceContext* resource_context);

 protected:
  ~WebRTCIdentityServiceHost() override;

  // content::BrowserMessageFilter override.
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  // |request_id| is the same as in the OnRequestIdentity call.
  // See WebRTCIdentityStore for the meaning of the parameters.
  void OnComplete(int request_id,
                  int status,
                  const std::string& certificate,
                  const std::string& private_key);

  // |request_id| is a renderer wide unique number for each request and
  // will be echoed in the response to handle the possibility that the renderer
  // cancels the request after the browser sends the response and before it's
  // received by the renderer.
  // See WebRTCIdentityStore for the meaning of the other parameters.
  void OnRequestIdentity(
      const WebRTCIdentityMsg_RequestIdentity_Params& params);

  void OnCancelRequest();

  void SendErrorMessage(int request_id, int error);

  int renderer_process_id_;
  base::Closure cancel_callback_;
  scoped_refptr<WebRTCIdentityStore> identity_store_;
  ResourceContext* resource_context_;

  base::WeakPtrFactory<WebRTCIdentityServiceHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebRTCIdentityServiceHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_WEBRTC_IDENTITY_SERVICE_HOST_H_
