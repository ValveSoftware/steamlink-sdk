// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_THROTTLE_H_
#define CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/resource_throttle.h"

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
      ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate);
  ~NavigationResourceThrottle() override;

  // ResourceThrottle overrides:
  void WillStartRequest(bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  void WillProcessResponse(bool* defer) override;
  const char* GetNameForLogging() const override;

 private:
  void OnUIChecksPerformed(NavigationThrottle::ThrottleCheckResult result);

  net::URLRequest* request_;
  ResourceDispatcherHostDelegate* resource_dispatcher_host_delegate_;
  base::WeakPtrFactory<NavigationResourceThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NavigationResourceThrottle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_NAVIGATION_RESOURCE_THROTTLE_H_
