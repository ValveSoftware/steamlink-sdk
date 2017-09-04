// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PAGE_NAVIGATION_THROTTLE_H_
#define CONTENT_BROWSER_DEVTOOLS_PAGE_NAVIGATION_THROTTLE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
namespace devtools {
namespace page {
class PageHandler;
}  // namespace page

// Used to allow the DevTools client to optionally cancel navigations via the
// Page.setControlNavigations and Page.processNavigation commands.
class PageNavigationThrottle : public content::NavigationThrottle {
 public:
  PageNavigationThrottle(base::WeakPtr<page::PageHandler> page_handler,
                         int navigation_id,
                         content::NavigationHandle* navigation_handle);
  ~PageNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override;

  int navigation_id() const { return navigation_id_; }

  // Tells the PageNavigationThrottle to not throttle anything!
  void AlwaysProceed();

  // Resumes a deferred navigation request. Does nothing if a response isn't
  // expected.
  void Resume();

  // Cancels a deferred navigation request. Does nothing if a response isn't
  // expected.
  void CancelDeferredNavigation(NavigationThrottle::ThrottleCheckResult result);

 private:
  // An opaque ID assigned by the PageHandler, used to allow the protocol client
  // to refer to this navigation throttle.
  const int navigation_id_;

  // The PageHandler that this navigation throttle is associated with.
  base::WeakPtr<page::PageHandler> page_handler_;

  // Whether or not a navigation was deferred. If deferred we expect a
  // subsequent call to AlwaysProceed, Resume or CancelDeferredNavigation.
  bool navigation_deferred_;

  DISALLOW_COPY_AND_ASSIGN(PageNavigationThrottle);
};

}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PAGE_NAVIGATION_THROTTLE_H_
