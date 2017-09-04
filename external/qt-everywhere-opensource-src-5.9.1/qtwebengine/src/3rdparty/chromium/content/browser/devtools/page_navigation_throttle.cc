// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/page_navigation_throttle.h"

#include "base/strings/stringprintf.h"
#include "content/browser/devtools/protocol/page_handler.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace content {
namespace devtools {

PageNavigationThrottle::PageNavigationThrottle(
    base::WeakPtr<page::PageHandler> page_handler,
    int navigation_id,
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      navigation_id_(navigation_id),
      page_handler_(page_handler),
      navigation_deferred_(false) {}

PageNavigationThrottle::~PageNavigationThrottle() {
  if (page_handler_)
    page_handler_->OnPageNavigationThrottleDisposed(navigation_id_);
}

NavigationThrottle::ThrottleCheckResult
PageNavigationThrottle::WillStartRequest() {
  if (!page_handler_)
    return ThrottleCheckResult::PROCEED;
  navigation_deferred_ = true;
  page_handler_->NavigationRequested(this);
  return ThrottleCheckResult::DEFER;
}

NavigationThrottle::ThrottleCheckResult
PageNavigationThrottle::WillRedirectRequest() {
  if (!page_handler_)
    return ThrottleCheckResult::PROCEED;
  navigation_deferred_ = true;
  page_handler_->NavigationRequested(this);
  return ThrottleCheckResult::DEFER;
}

void PageNavigationThrottle::AlwaysProceed() {
  // Makes WillStartRequest and WillRedirectRequest always return
  // ThrottleCheckResult::PROCEED.
  page_handler_.reset();

  if (navigation_deferred_)
    Resume();
}

void PageNavigationThrottle::Resume() {
  if (!navigation_deferred_)
    return;
  navigation_deferred_ = false;
  navigation_handle()->Resume();

  // Do not add code after this as the PageNavigationThrottle may be deleted by
  // the line above.
}

void PageNavigationThrottle::CancelDeferredNavigation(
    NavigationThrottle::ThrottleCheckResult result) {
  if (!navigation_deferred_)
    return;
  navigation_deferred_ = false;
  navigation_handle()->CancelDeferredNavigation(result);

  // Do not add code after this as the PageNavigationThrottle may be deleted by
  // the line above.
}

}  // namespace devtools
}  // namespace content
