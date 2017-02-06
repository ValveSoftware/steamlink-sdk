// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NAVIGATION_PUBLIC_CPP_VIEW_OBSERVER_H_
#define SERVICES_NAVIGATION_PUBLIC_CPP_VIEW_OBSERVER_H_

namespace navigation {

class View;

class ViewObserver {
 public:
  // Called when network activity for the application(s) within |view| starts or
  // stops.
  virtual void LoadingStateChanged(View* view) {}

  // Called when the progress of network activity for the application(s) within
  // |view| changes.
  virtual void LoadProgressChanged(View* view, double progress) {}

  // Called when a navigation occurs within |view|.
  virtual void NavigationStateChanged(View* view) {}

  // Called when the target URL of a mouse click at the current mouse position
  // changes. Will provide an empty URL if no navigation would result from such
  // a click.
  virtual void HoverTargetURLChanged(View* view, const GURL& target_url) {}
};

}  // namespace navigation

#endif  // SERVICES_NAVIGATION_PUBLIC_CPP_VIEW_OBSERVER_H_
