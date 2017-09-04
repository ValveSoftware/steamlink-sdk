// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigator_delegate.h"

namespace content {

bool NavigatorDelegate::CanOverscrollContent() const {
  return false;
}

bool NavigatorDelegate::ShouldTransferNavigation(
    bool is_main_frame_navigation) {
  return true;
}

bool NavigatorDelegate::ShouldPreserveAbortedURLs() {
  return false;
}

ScopedVector<NavigationThrottle>
NavigatorDelegate::CreateThrottlesForNavigation(
    NavigationHandle* navigation_handle) {
  return ScopedVector<NavigationThrottle>();
}

std::unique_ptr<NavigationUIData> NavigatorDelegate::GetNavigationUIData(
    NavigationHandle* navigation_handle) {
  return nullptr;
}

}  // namespace content
