// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_

#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "content/common/content_export.h"

namespace net {
class URLRequest;
}

namespace content {

class DownloadItem;
class ResourceContext;
class ResourceDispatcherHostDelegate;
struct DownloadSaveInfo;
struct Referrer;
class RenderFrameHost;

class CONTENT_EXPORT ResourceDispatcherHost {
 public:
  // Returns the singleton instance of the ResourceDispatcherHost.
  static ResourceDispatcherHost* Get();

  // Causes all new requests for the root RenderFrameHost and its children to be
  // blocked (not being started) until ResumeBlockedRequestsForFrameFromUI is
  // called.
  static void BlockRequestsForFrameFromUI(RenderFrameHost* root_frame_host);

  // Resumes any blocked request for the specified root RenderFrameHost and
  // child frame hosts.
  static void ResumeBlockedRequestsForFrameFromUI(
      RenderFrameHost* root_frame_host);

  // This does not take ownership of the delegate. It is expected that the
  // delegate have a longer lifetime than the ResourceDispatcherHost.
  virtual void SetDelegate(ResourceDispatcherHostDelegate* delegate) = 0;

  // Controls whether third-party sub-content can pop-up HTTP basic auth
  // dialog boxes.
  virtual void SetAllowCrossOriginAuthPrompt(bool value) = 0;

  // Clears the ResourceDispatcherHostLoginDelegate associated with the request.
  virtual void ClearLoginDelegateForRequest(net::URLRequest* request) = 0;

 protected:
  virtual ~ResourceDispatcherHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_
