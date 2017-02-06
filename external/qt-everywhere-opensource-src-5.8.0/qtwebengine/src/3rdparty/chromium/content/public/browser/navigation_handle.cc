// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_handle.h"

#include <utility>

#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

WebContents* NavigationHandle::GetWebContents() {
  // The NavigationHandleImpl cannot access the WebContentsImpl as it would be
  // a layering violation, hence the cast here.
  return static_cast<WebContentsImpl*>(
      static_cast<NavigationHandleImpl*>(this)->GetDelegate());
}

// static
std::unique_ptr<NavigationHandle>
NavigationHandle::CreateNavigationHandleForTesting(
    const GURL& url,
    RenderFrameHost* render_frame_host) {
  std::unique_ptr<NavigationHandleImpl> handle_impl =
      NavigationHandleImpl::Create(
          url, static_cast<RenderFrameHostImpl*>(render_frame_host)
                   ->frame_tree_node(),
          true,   // is_renderer_initiated
          false,  // is_synchronous
          false,  // is_srcdoc
          base::TimeTicks::Now(), 0);
  return std::unique_ptr<NavigationHandle>(std::move(handle_impl));
}

}  // namespace content
