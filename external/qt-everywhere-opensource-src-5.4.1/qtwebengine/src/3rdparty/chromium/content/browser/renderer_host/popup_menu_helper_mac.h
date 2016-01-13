// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_HELPER_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_HELPER_MAC_H_

#include <vector>

#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/rect.h"

#ifdef __OBJC__
@class WebMenuRunner;
#else
class WebMenuRunner;
#endif

namespace content {
class RenderViewHost;
class RenderViewHostImpl;
class RenderWidgetHostViewMac;
struct MenuItem;

class PopupMenuHelper : public NotificationObserver {
 public:
  // Creates a PopupMenuHelper that will notify |render_view_host| when a user
  // selects or cancels the popup.
  explicit PopupMenuHelper(RenderViewHost* render_view_host);
  void Hide();

  // Shows the popup menu and notifies the RenderViewHost of the selection/
  // cancel.
  // This call is blocking.
  void ShowPopupMenu(const gfx::Rect& bounds,
                     int item_height,
                     double item_font_size,
                     int selected_item,
                     const std::vector<MenuItem>& items,
                     bool right_aligned,
                     bool allow_multiple_selection);

  // Immediately return from ShowPopupMenu.
  CONTENT_EXPORT static void DontShowPopupMenuForTesting();

 protected:
  virtual RenderWidgetHostViewMac* GetRenderWidgetHostView() const;

  // NotificationObserver implementation:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar notification_registrar_;
  RenderViewHostImpl* render_view_host_;
  WebMenuRunner* menu_runner_;
  bool popup_was_hidden_;

  DISALLOW_COPY_AND_ASSIGN(PopupMenuHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_POPUP_MENU_HELPER_MAC_H_
