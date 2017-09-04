// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_SESSION_PAGE_LOAD_TRACKER_H_
#define BLIMP_ENGINE_SESSION_PAGE_LOAD_TRACKER_H_

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_observer.h"

namespace blimp {
namespace engine {

enum class PageLoadStatus { LOADING, LOADED };

class PageLoadTrackerClient {
 public:
  virtual ~PageLoadTrackerClient() {}

  // Called when a navigation status update should be sent to the client.
  // |load_status| indicates the status of the navigation. An update with a
  // |LOADING| value is sent each time a new navigation is initiated. This
  // update will be followed by a |LOADED| value when the navigation either
  // successfully finishes or fails.
  virtual void SendPageLoadStatusUpdate(PageLoadStatus load_status) = 0;
};

// Tracks the page load status for a tab using load and paint notifications
// from the renderer.
class PageLoadTracker : public content::WebContentsObserver {
 public:
  explicit PageLoadTracker(content::WebContents* web_contents,
                           PageLoadTrackerClient* client);
  ~PageLoadTracker() override;

 private:
  // content::WebContentsObserver implementation.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Invoked when the renderer has performed at least one paint after the
  // navigation was committed.
  void DidPaintAfterNavigationCommitted(bool result);

  base::CancelableCallback<void(bool)> did_paint_after_navigation_callback_;

  PageLoadTrackerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadTracker);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_SESSION_PAGE_LOAD_TRACKER_H_
