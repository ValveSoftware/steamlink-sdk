// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/widget_owner_nswindow_adapter.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/mac/sdk_forward_declarations.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#import "ui/views/cocoa/bridged_native_widget.h"

// Bridges an AppKit observer to observe when the (non-views) NSWindow owning a
// views::Widget will close.
@interface WidgetOwnerNSWindowAdapterBridge : NSObject {
 @private
  views::WidgetOwnerNSWindowAdapter* adapter_;  // Weak. Owns us.
}
- (instancetype)initWithAdapter:(views::WidgetOwnerNSWindowAdapter*)adapter;
- (void)windowWillClose:(NSNotification*)notification;
@end

@implementation WidgetOwnerNSWindowAdapterBridge

- (instancetype)initWithAdapter:(views::WidgetOwnerNSWindowAdapter*)adapter {
  if ((self = [super init]))
    adapter_ = adapter;
  return self;
}

- (void)windowWillClose:(NSNotification*)notification {
  adapter_->OnWindowWillClose();
}

@end

namespace views {

WidgetOwnerNSWindowAdapter::WidgetOwnerNSWindowAdapter(
    BridgedNativeWidget* child,
    NSView* anchor_view)
    : child_(child),
      anchor_view_([anchor_view retain]),
      observer_bridge_(
          [[WidgetOwnerNSWindowAdapterBridge alloc] initWithAdapter:this]) {

  // Although the |anchor_view| must be in an NSWindow when the child dialog is
  // created, it's permitted for the |anchor_view| to be removed from its view
  // hierarchy before the child dialog window is fully removed from screen. When
  // this happens, [anchor_view_ window] will become nil, so retain both.
  anchor_window_.reset([[anchor_view_ window] retain]);
  DCHECK(anchor_window_);

  [[NSNotificationCenter defaultCenter]
      addObserver:observer_bridge_
         selector:@selector(windowWillClose:)
             name:NSWindowWillCloseNotification
           object:anchor_window_];
}

void WidgetOwnerNSWindowAdapter::OnWindowWillClose() {
  // Retain the child window before closing it. If the last reference to the
  // NSWindow goes away inside -[NSWindow close], then bad stuff can happen.
  // See e.g. http://crbug.com/616701.
  base::scoped_nsobject<NSWindow> child_window(child_->ns_window(),
                                               base::scoped_policy::RETAIN);

  // AppKit child window relationships break when the windows are not visible,
  // so if the child is not visible, it won't currently be a child.
  DCHECK(![child_window isVisible] || [child_window parentWindow]);
  DCHECK([child_window delegate]);

  [child_window close];
  // Note: |this| will be deleted here.

  DCHECK(![child_window parentWindow]);
  DCHECK(![child_window delegate]);
}

NSWindow* WidgetOwnerNSWindowAdapter::GetNSWindow() {
  return anchor_window_;
}

gfx::Vector2d WidgetOwnerNSWindowAdapter::GetChildWindowOffset() const {
  NSRect rect_in_window =
      [anchor_view_ convertRect:[anchor_view_ bounds] toView:nil];
  NSRect rect_in_screen = [anchor_window_ convertRectToScreen:rect_in_window];
  // Ensure we anchor off the top-left of |anchor_view_| (rect_in_screen.origin
  // is the bottom-left of the view).
  NSPoint anchor_in_screen =
      NSMakePoint(NSMinX(rect_in_screen), NSMaxY(rect_in_screen));
  return gfx::ScreenPointFromNSPoint(anchor_in_screen).OffsetFromOrigin();
}

bool WidgetOwnerNSWindowAdapter::IsVisibleParent() const {
  return [anchor_window_ isVisible];
}

void WidgetOwnerNSWindowAdapter::RemoveChildWindow(BridgedNativeWidget* child) {
  DCHECK_EQ(child, child_);
  [GetNSWindow() removeChildWindow:child->ns_window()];
  delete this;
}

WidgetOwnerNSWindowAdapter::~WidgetOwnerNSWindowAdapter() {
  [[NSNotificationCenter defaultCenter] removeObserver:observer_bridge_];
}

}  // namespace views
