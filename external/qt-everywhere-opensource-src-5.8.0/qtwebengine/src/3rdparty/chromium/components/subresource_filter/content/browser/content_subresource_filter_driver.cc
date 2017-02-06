// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/content_subresource_filter_driver.h"

#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "content/public/browser/render_frame_host.h"

namespace subresource_filter {

ContentSubresourceFilterDriver::ContentSubresourceFilterDriver(
    content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

ContentSubresourceFilterDriver::~ContentSubresourceFilterDriver() {}

void ContentSubresourceFilterDriver::ActivateForProvisionalLoad(
    ActivationState activation_state) {
  // Must use legacy IPC to ensure the activation message arrives in-order, i.e.
  // before the load is committed on the renderer side.
  render_frame_host_->Send(new SubresourceFilterMsg_ActivateForProvisionalLoad(
      render_frame_host_->GetRoutingID(), activation_state));
}

}  // namespace subresource_filter
