// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_

#include "content/browser/renderer_host/popup_menu_helper_mac.h"

namespace content {
class RenderViewHost;
class RenderViewHostImpl;

// This class is similiar to PopupMenuHelperMac but positions the popup relative
// to the embedder, and issues a reply to the guest.
class BrowserPluginPopupMenuHelper : public PopupMenuHelper {
 public:
  // Creates a BrowserPluginPopupMenuHelper that positions popups relative to
  // |embedder_rvh| and will notify |guest_rvh| when a user selects or cancels
  // the popup.
  BrowserPluginPopupMenuHelper(RenderViewHost* embedder_rvh,
                               RenderViewHost* guest_rvh);

 private:
  virtual RenderWidgetHostViewMac* GetRenderWidgetHostView() const OVERRIDE;

  RenderViewHostImpl* embedder_rvh_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPopupMenuHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_POPUP_MENU_HELPER_MAC_H_
