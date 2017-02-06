// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_dispatcher_host.h"

#include "base/callback.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
void ResourceDispatcherHost::BlockRequestsForFrameFromUI(
    RenderFrameHost* root_frame_host) {
  ResourceDispatcherHostImpl::BlockRequestsForFrameFromUI(root_frame_host);
}

// static
void ResourceDispatcherHost::ResumeBlockedRequestsForFrameFromUI(
    RenderFrameHost* root_frame_host) {
  ResourceDispatcherHostImpl::ResumeBlockedRequestsForFrameFromUI(
      root_frame_host);
}

}  // namespace content
