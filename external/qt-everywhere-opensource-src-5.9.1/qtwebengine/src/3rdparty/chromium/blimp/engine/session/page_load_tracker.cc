// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/session/page_load_tracker.h"

#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_view.h"

namespace blimp {
namespace engine {

namespace {

bool ShouldIgnoreNavigation(content::NavigationHandle* navigation_handle) {
  // We change the progress bar for main frame navigations only.
  if (!navigation_handle->IsInMainFrame())
    return true;

  // Same page navigations don't need to trigger a progress bar update.
  if (navigation_handle->IsSamePage())
    return true;

  return false;
}

}  // namespace

PageLoadTracker::PageLoadTracker(content::WebContents* web_contents,
                                 PageLoadTrackerClient* client)
    : client_(client) {
  DCHECK(web_contents);
  Observe(web_contents);
}

PageLoadTracker::~PageLoadTracker() {}

void PageLoadTracker::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (ShouldIgnoreNavigation(navigation_handle))
    return;

  // Cancel any pending callbacks for the previous navigation. We will send an
  // update based on the progress of this navigation.
  did_paint_after_navigation_callback_.Cancel();
  client_->SendPageLoadStatusUpdate(PageLoadStatus::LOADING);
}

void PageLoadTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (ShouldIgnoreNavigation(navigation_handle))
    return;

  if (navigation_handle->HasCommitted()) {
    // Make sure that at least one compositor content update after the
    // navigation commits is sent to the client.
    // Note that a visual state update in our case implies that this callback
    // will be invoked after the update is queued to be sent to the client.
    did_paint_after_navigation_callback_.Reset(
        base::Bind(&PageLoadTracker::DidPaintAfterNavigationCommitted,
                   base::Unretained(this)));
    navigation_handle->GetRenderFrameHost()->InsertVisualStateCallback(
        did_paint_after_navigation_callback_.callback());
  } else {
    // Inform the client to update the progress bar right away.
    client_->SendPageLoadStatusUpdate(PageLoadStatus::LOADED);
  }
}

void PageLoadTracker::DidPaintAfterNavigationCommitted(bool result) {
  client_->SendPageLoadStatusUpdate(PageLoadStatus::LOADED);
}

}  // namespace engine
}  // namespace blimp
