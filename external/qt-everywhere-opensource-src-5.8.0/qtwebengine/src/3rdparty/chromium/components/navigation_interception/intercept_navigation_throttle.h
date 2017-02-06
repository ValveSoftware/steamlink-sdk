// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_THROTTLE_H_
#define COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_THROTTLE_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

class GURL;

namespace content {
class NavigationHandle;
class WebContents;
}

namespace navigation_interception {

class NavigationParams;

// This class allows the provider of the Callback to selectively ignore top
// level navigations. This is a UI thread class.
class InterceptNavigationThrottle : public content::NavigationThrottle {
 public:
  typedef base::Callback<bool(content::WebContents* /* source */,
                              const NavigationParams& /* navigation_params */)>
      CheckCallback;

  InterceptNavigationThrottle(content::NavigationHandle* navigation_handle,
                              CheckCallback should_ignore_callback,
                              bool run_callback_synchronously);
  ~InterceptNavigationThrottle() override;

  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

 private:
  ThrottleCheckResult CheckIfShouldIgnoreNavigation(bool is_redirect);

  // Called to perform the checks asynchronously
  void RunCallbackAsynchronously(const NavigationParams& navigation_params);
  // TODO(clamy): remove |throttle_was_destroyed| once crbug.com/570200 is
  // fixed.
  void OnAsynchronousChecksPerformed(bool should_ignore_navigation,
                                     bool throttle_was_destroyed);

  CheckCallback should_ignore_callback_;

  // Whether the callback will be run synchronously or not. If the callback can
  // lead to the destruction of the WebContents, this should be false.
  // Otherwise this should be true.
  const bool run_callback_synchronously_;

  base::WeakPtrFactory<InterceptNavigationThrottle> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterceptNavigationThrottle);
};

}  // namespace navigation_interception

#endif  // COMPONENTS_NAVIGATION_INTERCEPTION_INTERCEPT_NAVIGATION_THROTTLE_H_
