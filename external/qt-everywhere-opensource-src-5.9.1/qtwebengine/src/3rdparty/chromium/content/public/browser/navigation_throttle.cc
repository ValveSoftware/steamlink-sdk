// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_throttle.h"

namespace content {

NavigationThrottle::NavigationThrottle(NavigationHandle* navigation_handle)
    : navigation_handle_(navigation_handle) {}

NavigationThrottle::~NavigationThrottle() {}

NavigationThrottle::ThrottleCheckResult NavigationThrottle::WillStartRequest() {
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
NavigationThrottle::WillRedirectRequest() {
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
NavigationThrottle::WillProcessResponse() {
  return NavigationThrottle::PROCEED;
}

}  // namespace content
