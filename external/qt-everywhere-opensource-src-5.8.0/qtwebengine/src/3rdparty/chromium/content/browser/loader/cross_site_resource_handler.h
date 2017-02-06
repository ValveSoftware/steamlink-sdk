// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_CROSS_SITE_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_CROSS_SITE_RESOURCE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/common/content_export.h"
#include "net/url_request/url_request_status.h"

namespace net {
class URLRequest;
}

namespace content {

struct TransitionLayerData;

// Ensures that responses are delayed for navigations that must be transferred
// to a different process.  This handler wraps an AsyncEventHandler, and it sits
// inside SafeBrowsing and Buffered event handlers.  This is important, so that
// it can intercept OnResponseStarted after we determine that a response is safe
// and not a download.
class CrossSiteResourceHandler : public LayeredResourceHandler {
 public:
  enum class NavigationDecision {
    TRANSFER_REQUIRED,
    USE_EXISTING_RENDERER,
    CANCEL_REQUEST
  };

  CrossSiteResourceHandler(std::unique_ptr<ResourceHandler> next_handler,
                           net::URLRequest* request);
  ~CrossSiteResourceHandler() override;

  // ResourceHandler implementation:
  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override;
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnReadCompleted(int bytes_read, bool* defer) override;
  void OnResponseCompleted(const net::URLRequestStatus& status,
                           const std::string& security_info,
                           bool* defer) override;

  // We can now send the response to the new renderer, which will cause
  // WebContentsImpl to swap in the new renderer and destroy the old one.
  void ResumeResponse();

  // When set to true, requests are leaked when they can't be passed to a
  // RenderViewHost, for unit tests.
  CONTENT_EXPORT static void SetLeakRequestsForTesting(
      bool leak_requests_for_testing);

 private:
  // Prepare to transfer the cross-site response to a new RenderFrameHost, by
  // asking it to issue an identical request (on the UI thread).
  void StartCrossSiteTransition(ResourceResponse* response);

  // Defer the navigation to the UI thread to check whether transfer is required
  // or not. Currently only used in --site-per-process.
  bool DeferForNavigationPolicyCheck(ResourceRequestInfoImpl* info,
                                     ResourceResponse* response,
                                     bool* defer);

  bool OnNormalResponseStarted(ResourceResponse* response,
                               bool* defer);

  void ResumeOrTransfer(NavigationDecision decision);
  void ResumeIfDeferred();

  // Called when about to defer a request.  Sets |did_defer_| and logs the
  // defferral
  void OnDidDefer();

  bool has_started_response_;
  bool in_cross_site_transition_;
  bool completed_during_transition_;
  bool did_defer_;
  net::URLRequestStatus completed_status_;
  std::string completed_security_info_;
  scoped_refptr<ResourceResponse> response_;

  // TODO(nasko): WeakPtr is needed in --site-per-process, since all navigations
  // are deferred to the UI thread and come back to IO thread via
  // PostTaskAndReplyWithResult. If a transfer is needed, it goes back to the UI
  // thread. This can be removed once the code is changed to only do one hop.
  base::WeakPtrFactory<CrossSiteResourceHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrossSiteResourceHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_CROSS_SITE_RESOURCE_HANDLER_H_
