// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_THROTTLE_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/request_context_type.h"

namespace net {
class URLRequest;
}

namespace content {
class ResourceDispatcherHostDelegate;

// This ResourceThrottle is used to convey throttling information to the UI
// thread during navigations. The UI thread can then use its NavigationThrottle
// mechanism to interact with the navigation.
class NavigationResourceThrottle : public ResourceThrottle {
 public:
  NavigationResourceThrottle(
      net::URLRequest* request,
      ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate,
      RequestContextType request_context_type);
  ~NavigationResourceThrottle() override;

  // ResourceThrottle overrides:
  void WillStartRequest(bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  void WillProcessResponse(bool* defer) override;
  const char* GetNameForLogging() const override;

  // Used in unit tests to make UI checks pass when they would fail due to no
  // NavigationHandle being present in the RenderFrameHost.
  CONTENT_EXPORT static void set_ui_checks_always_succeed_for_testing(
      bool ui_checks_always_succeed);

  // Used in unit tests to make all navigations transfer.
  CONTENT_EXPORT static void set_force_transfer_for_testing(
      bool force_transfer);

 private:
  void OnUIChecksPerformed(NavigationThrottle::ThrottleCheckResult result);

  // Used in transfer navigations.
  void InitiateTransfer();
  void OnTransferComplete();

  net::URLRequest* request_;
  ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate_;
  RequestContextType request_context_type_;
  bool in_cross_site_transition_;
  NavigationThrottle::ThrottleCheckResult on_transfer_done_result_;

  base::WeakPtrFactory<NavigationResourceThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationResourceThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_THROTTLE_H_
