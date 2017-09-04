// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/interstitial_page_navigator_impl.h"

#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"

namespace content {

InterstitialPageNavigatorImpl::InterstitialPageNavigatorImpl(
    InterstitialPageImpl* interstitial,
    NavigationControllerImpl* navigation_controller)
    : interstitial_(interstitial),
      controller_(navigation_controller) {}

InterstitialPageNavigatorImpl::~InterstitialPageNavigatorImpl() {}

NavigatorDelegate* InterstitialPageNavigatorImpl::GetDelegate() {
  return interstitial_;
}

NavigationController* InterstitialPageNavigatorImpl::GetController() {
  return controller_;
}

void InterstitialPageNavigatorImpl::DidStartProvisionalLoad(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    const base::TimeTicks& navigation_start) {
  // The interstitial page should only navigate once.
  DCHECK(!render_frame_host->navigation_handle());
  render_frame_host->SetNavigationHandle(
      NavigationHandleImpl::Create(url, render_frame_host->frame_tree_node(),
                                   false,  // is_renderer_initiated
                                   false,  // is_synchronous
                                   false,  // is_srcdoc
                                   navigation_start,
                                   0,      // pending_nav_entry_id
                                   false)  // started_in_context_menu
      );
}

void InterstitialPageNavigatorImpl::DidNavigate(
    RenderFrameHostImpl* render_frame_host,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& input_params,
    std::unique_ptr<NavigationHandleImpl> navigation_handle) {
  navigation_handle->DidCommitNavigation(input_params, false,
                                         render_frame_host);
  navigation_handle.reset();

  // TODO(nasko): Move implementation here, but for the time being call out
  // to the interstitial page code.
  interstitial_->DidNavigate(render_frame_host->render_view_host(),
                             input_params);
}

}  // namespace content
