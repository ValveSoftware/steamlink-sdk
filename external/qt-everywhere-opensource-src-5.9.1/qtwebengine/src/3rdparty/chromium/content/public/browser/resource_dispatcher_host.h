// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "content/common/content_export.h"

namespace net {
class URLRequest;
}

namespace content {

class ResourceContext;
class ResourceDispatcherHostDelegate;
class RenderFrameHost;

// This callback is invoked when the interceptor finishes processing the
// header.
// Parameter 1 is a bool indicating success or failure.
// Parameter 2 contains the error code in case of failure, else 0.
typedef base::Callback<void(bool, int)> OnHeaderProcessedCallback;

// This callback is registered by interceptors who are interested in being
// notified of certain HTTP headers in outgoing requests. For e.g. Origin.
// Parameter 1 contains the HTTP header.
// Parameter 2 contains its value.
// Parameter 3 contains the child process id.
// Parameter 4 contains the current ResourceContext.
// Parameter 5 contains the callback which needs to be invoked once the
// interceptor finishes its processing.
typedef base::Callback<void(const std::string&,
                            const std::string&,
                            int,
                            ResourceContext*,
                            OnHeaderProcessedCallback)>
    InterceptorCallback;

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

  // Registers the |interceptor| for the |http_header| passed in.
  // The |starts_with| parameter is used to match the prefix of the
  // |http_header| in the response and the interceptor will be invoked if there
  // is a match. If the |starts_with| parameter is empty, the interceptor will
  // be invoked for every occurrence of the |http_header|.
  // Currently only HTTP header based interceptors are supported.
  // At the moment we only support one interceptor per |http_header|.
  virtual void RegisterInterceptor(const std::string& http_header,
                                   const std::string& starts_with,
                                   const InterceptorCallback& interceptor) = 0;

 protected:
  virtual ~ResourceDispatcherHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_DISPATCHER_HOST_H_
