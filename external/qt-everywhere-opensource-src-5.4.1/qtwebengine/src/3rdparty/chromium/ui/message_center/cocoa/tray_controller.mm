// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/cocoa/tray_controller.h"

#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/message_center/cocoa/popup_collection.h"
#import "ui/message_center/cocoa/tray_view_controller.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"

@interface MCTrayWindow : NSPanel
@end

@implementation MCTrayWindow

- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (void)cancelOperation:(id)sender {
  [self orderOut:self];
}

@end

@implementation MCTrayController

- (id)initWithMessageCenterTray:(message_center::MessageCenterTray*)tray {
  base::scoped_nsobject<MCTrayWindow> window(
      [[MCTrayWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                      styleMask:NSBorderlessWindowMask |
                                                NSNonactivatingPanelMask
                                        backing:NSBackingStoreBuffered
                                          defer:NO]);
  if ((self = [super initWithWindow:window])) {
    tray_ = tray;

    [window setDelegate:self];
    [window setHasShadow:YES];
    [window setHidesOnDeactivate:NO];
    [window setLevel:NSFloatingWindowLevel];

    viewController_.reset([[MCTrayViewController alloc] initWithMessageCenter:
        tray_->message_center()]);
    NSView* contentView = [viewController_ view];
    [window setFrame:[contentView frame] display:NO];
    [window setContentView:contentView];

    // The global event monitor will close the tray in response to events
    // delivered to other applications, and -windowDidResignKey: will catch
    // events within the application.
    __block typeof(self) weakSelf = self;
    clickEventMonitor_ =
        [NSEvent addGlobalMonitorForEventsMatchingMask:NSLeftMouseDownMask |
                                                       NSRightMouseDownMask |
                                                       NSOtherMouseDownMask
            handler:^(NSEvent* event) {
                [weakSelf windowDidResignKey:nil];
            }];
  }
  return self;
}

- (void)dealloc {
  [NSEvent removeMonitor:clickEventMonitor_];
  [super dealloc];
}

- (MCTrayViewController*)viewController {
  return viewController_.get();
}

- (void)close {
  [viewController_ onWindowClosing];
  [super close];
}

- (void)showTrayAtRightOf:(NSPoint)rightPoint atLeftOf:(NSPoint)leftPoint {
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect screenFrame = [screen visibleFrame];

  NSRect frame = [[viewController_ view] frame];

  if (rightPoint.x + NSWidth(frame) < NSMaxX(screenFrame)) {
    frame.origin.x = rightPoint.x;
    frame.origin.y = rightPoint.y - NSHeight(frame);
  } else {
    frame.origin.x = leftPoint.x - NSWidth(frame);
    frame.origin.y = leftPoint.y - NSHeight(frame);
  }

  [[self window] setFrame:frame display:YES];
  [viewController_ scrollToTop];
  [self showWindow:nil];
}

- (void)onMessageCenterTrayChanged {
  [viewController_ onMessageCenterTrayChanged];
}

- (void)windowDidResignKey:(NSNotification*)notification {
  // The settings bubble data structures assume that the settings dialog is
  // visible only for short periods of time: There's a fixed list of permissions
  // for example.
  [viewController_ cleanupSettings];

  tray_->HideMessageCenterBubble();
}

@end
