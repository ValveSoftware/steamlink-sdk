// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_popup_menu_helper_mac.h"

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"

namespace content {

BrowserPluginPopupMenuHelper::BrowserPluginPopupMenuHelper(
    RenderViewHost* embedder_rvh, RenderViewHost* guest_rvh)
    : PopupMenuHelper(guest_rvh),
      embedder_rvh_(static_cast<RenderViewHostImpl*>(embedder_rvh)) {
}

RenderWidgetHostViewMac*
    BrowserPluginPopupMenuHelper::GetRenderWidgetHostView() const {
  return static_cast<RenderWidgetHostViewMac*>(embedder_rvh_->GetView());
}

}  // namespace content
