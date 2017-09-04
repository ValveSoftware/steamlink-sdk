// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Carbon/Carbon.h>

#include "content/browser/frame_host/popup_menu_helper_mac.h"

#include "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_sending_event.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/webmenurunner_mac.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#import "ui/base/cocoa/base_view.h"

namespace content {

namespace {

bool g_allow_showing_popup_menus = true;

}  // namespace

PopupMenuHelper::PopupMenuHelper(RenderFrameHost* render_frame_host)
    : render_frame_host_(static_cast<RenderFrameHostImpl*>(render_frame_host)),
      menu_runner_(nil),
      popup_was_hidden_(false) {
  notification_registrar_.Add(
      this, NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(
          render_frame_host->GetRenderViewHost()->GetWidget()));
  notification_registrar_.Add(
      this, NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
      Source<RenderWidgetHost>(
          render_frame_host->GetRenderViewHost()->GetWidget()));
}

void PopupMenuHelper::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<MenuItem>& items,
    bool right_aligned,
    bool allow_multiple_selection) {
  // Only single selection list boxes show a popup on Mac.
  DCHECK(!allow_multiple_selection);

  if (!g_allow_showing_popup_menus)
    return;

  // Retain the Cocoa view for the duration of the pop-up so that it can't be
  // dealloced if my Destroy() method is called while the pop-up's up (which
  // would in turn delete me, causing a crash once the -runMenuInView
  // call returns. That's what was happening in <http://crbug.com/33250>).
  RenderWidgetHostViewMac* rwhvm =
      static_cast<RenderWidgetHostViewMac*>(GetRenderWidgetHostView());
  base::scoped_nsobject<RenderWidgetHostViewCocoa> cocoa_view(
      [rwhvm->cocoa_view() retain]);

  // Display the menu.
  menu_runner_ = [[WebMenuRunner alloc] initWithItems:items
                                             fontSize:item_font_size
                                         rightAligned:right_aligned];

  {
    // Make sure events can be pumped while the menu is up.
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());

    // One of the events that could be pumped is |window.close()|.
    // User-initiated event-tracking loops protect against this by
    // setting flags in -[CrApplication sendEvent:], but since
    // web-content menus are initiated by IPC message the setup has to
    // be done manually.
    base::mac::ScopedSendingEvent sending_event_scoper;

    // Now run a SYNCHRONOUS NESTED EVENT LOOP until the pop-up is finished.
    [menu_runner_ runMenuInView:cocoa_view
                     withBounds:[cocoa_view flipRectToNSRect:bounds]
                   initialIndex:selected_item];
  }

  if (!render_frame_host_) {
    // Bad news, the RenderFrameHost got deleted while we were off running the
    // menu. Nothing to do.
    [menu_runner_ release];
    menu_runner_ = nil;
    return;
  }

#if defined(USE_EXTERNAL_POPUP_MENU)
  if (!popup_was_hidden_) {
    if ([menu_runner_ menuItemWasChosen]) {
      render_frame_host_->DidSelectPopupMenuItem(
          [menu_runner_ indexOfSelectedItem]);
    } else {
      render_frame_host_->DidCancelPopupMenu();
    }
  }
#endif
  [menu_runner_ release];
  menu_runner_ = nil;
}

void PopupMenuHelper::Hide() {
  if (menu_runner_)
    [menu_runner_ hide];
  popup_was_hidden_ = true;
}

// static
void PopupMenuHelper::DontShowPopupMenuForTesting() {
  g_allow_showing_popup_menus = false;
}

RenderWidgetHostViewMac* PopupMenuHelper::GetRenderWidgetHostView() const {
  return static_cast<RenderWidgetHostViewMac*>(
      render_frame_host_->frame_tree_node()
          ->frame_tree()
          ->root()
          ->current_frame_host()
          ->GetView());
}

void PopupMenuHelper::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK_EQ(Source<RenderWidgetHost>(source).ptr(),
            render_frame_host_->GetRenderViewHost()->GetWidget());
  switch (type) {
    case NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED: {
      render_frame_host_ = NULL;
      break;
    }
    case NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED: {
      bool is_visible = *Details<bool>(details).ptr();
      if (!is_visible)
        Hide();
      break;
    }
  }
}

}  // namespace content
