// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_

#include "base/strings/string16.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

class GURL;
struct FrameHostMsg_DidCommitProvisionalLoad_Params;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;

namespace content {

class RenderFrameHostImpl;
struct LoadCommittedDetails;
struct OpenURLParams;

// A delegate API used by Navigator to notify its embedder of navigation
// related events.
class CONTENT_EXPORT NavigatorDelegate {
 public:
  // The RenderFrameHost started a provisional load for the frame
  // represented by |render_frame_host|.
  virtual void DidStartProvisionalLoad(
      RenderFrameHostImpl* render_frame_host,
      int parent_routing_id,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) {}

  // A provisional load in |render_frame_host| failed.
  virtual void DidFailProvisionalLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params) {}

  // Document load in |render_frame_host| failed.
  virtual void DidFailLoadWithError(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      int error_code,
      const base::string16& error_description) {}

  // A redirect was processed in |render_frame_host| during a provisional load.
  virtual void DidRedirectProvisionalLoad(
      RenderFrameHostImpl* render_frame_host,
      const GURL& validated_target_url) {}

  // A navigation was committed in |render_frame_host|.
  virtual void DidCommitProvisionalLoad(
      RenderFrameHostImpl* render_frame_host,
      const base::string16& frame_unique_name,
      bool is_main_frame,
      const GURL& url,
      PageTransition transition_type) {}

  // Handles post-navigation tasks in navigation BEFORE the entry has been
  // committed to the NavigationController.
  virtual void DidNavigateMainFramePreCommit(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {}

  // Handles post-navigation tasks in navigation AFTER the entry has been
  // committed to the NavigationController. Note that the NavigationEntry is
  // not provided since it may be invalid/changed after being committed. The
  // NavigationController's last committed entry is for this navigation.
  virtual void DidNavigateMainFramePostCommit(
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {}
  virtual void DidNavigateAnyFramePostCommit(
      RenderFrameHostImpl* render_frame_host,
      const LoadCommittedDetails& details,
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {}

  virtual void SetMainFrameMimeType(const std::string& mime_type) {}
  virtual bool CanOverscrollContent() const;

  // Notification to the Navigator embedder that navigation state has
  // changed. This method corresponds to
  // WebContents::NotifyNavigationStateChanged.
  virtual void NotifyChangedNavigationState(InvalidateTypes changed_flags) {}

  // Notifies the Navigator embedder that it is beginning to navigate a frame.
  virtual void AboutToNavigateRenderFrame(
      RenderFrameHostImpl* render_frame_host) {}

  // Notifies the Navigator embedder that a navigation to pending
  // NavigationEntry has started in the browser process.
  virtual void DidStartNavigationToPendingEntry(
      RenderFrameHostImpl* render_frame_host,
      const GURL& url,
      NavigationController::ReloadType reload_type) {}

  // Opens a URL with the given parameters. See PageNavigator::OpenURL, which
  // this forwards to.
  virtual void RequestOpenURL(RenderFrameHostImpl* render_frame_host,
                              const OpenURLParams& params) {}

  // Returns whether URLs for aborted browser-initiated navigations should be
  // preserved in the omnibox.  Defaults to false.
  virtual bool ShouldPreserveAbortedURLs();
};

}  // namspace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATOR_DELEGATE_H_
