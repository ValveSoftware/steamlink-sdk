// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/navigation_state_impl.h"

namespace content {

NavigationStateImpl::~NavigationStateImpl() {
}

NavigationStateImpl* NavigationStateImpl::CreateBrowserInitiated(
    const CommonNavigationParams& common_params,
    const StartNavigationParams& start_params,
    const RequestNavigationParams& request_params) {
  return new NavigationStateImpl(common_params, start_params, request_params,
                                 false);
}

NavigationStateImpl* NavigationStateImpl::CreateContentInitiated() {
  return new NavigationStateImpl(CommonNavigationParams(),
                                 StartNavigationParams(),
                                 RequestNavigationParams(), true);
}

ui::PageTransition NavigationStateImpl::GetTransitionType() {
  return common_params_.transition;
}

bool NavigationStateImpl::WasWithinSamePage() {
  return was_within_same_page_;
}

bool NavigationStateImpl::IsContentInitiated() {
  return is_content_initiated_;
}

NavigationStateImpl::NavigationStateImpl(
    const CommonNavigationParams& common_params,
    const StartNavigationParams& start_params,
    const RequestNavigationParams& request_params,
    bool is_content_initiated)
    : request_committed_(false),
      was_within_same_page_(false),
      is_content_initiated_(is_content_initiated),
      common_params_(common_params),
      start_params_(start_params),
      request_params_(request_params) {
}

}  // namespace content
