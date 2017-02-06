// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/native_widget_mac_nswindow.h"

#include "base/mac/foundation_util.h"
#import "ui/views/cocoa/bridged_native_widget.h"
#import "ui/base/cocoa/user_interface_item_command_handler.h"
#import "ui/views/cocoa/views_nswindow_delegate.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/widget_delegate.h"

@interface NSWindow (Private)
- (BOOL)hasKeyAppearance;
@end

@interface NativeWidgetMacNSWindow ()
- (ViewsNSWindowDelegate*)viewsNSWindowDelegate;
- (views::Widget*)viewsWidget;
- (BOOL)hasViewsMenuActive;

// Private API on NSWindow, determines whether the title is drawn on the title
// bar. The title is still visible in menus, Expose, etc.
- (BOOL)_isTitleHidden;
@end

@implementation NativeWidgetMacNSWindow {
 @private
  base::scoped_nsobject<CommandDispatcher> commandDispatcher_;
  base::scoped_nsprotocol<id<UserInterfaceItemCommandHandler>> commandHandler_;
}

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation {
  if ((self = [super initWithContentRect:contentRect
                               styleMask:windowStyle
                                 backing:bufferingType
                                   defer:deferCreation])) {
    commandDispatcher_.reset([[CommandDispatcher alloc] initWithOwner:self]);
  }
  return self;
}

// This override doesn't do anything, but keeping it helps diagnose lifetime
// issues in crash stacktraces by inserting a symbol on NativeWidgetMacNSWindow.
- (void)dealloc {
  [super dealloc];
}

// Public methods.

- (void)setCommandDispatcherDelegate:(id<CommandDispatcherDelegate>)delegate {
  [commandDispatcher_ setDelegate:delegate];
}

// Private methods.

- (ViewsNSWindowDelegate*)viewsNSWindowDelegate {
  return base::mac::ObjCCastStrict<ViewsNSWindowDelegate>([self delegate]);
}

- (views::Widget*)viewsWidget {
  return [[self viewsNSWindowDelegate] nativeWidgetMac]->GetWidget();
}

- (BOOL)hasViewsMenuActive {
  views::MenuController* menuController =
      views::MenuController::GetActiveInstance();
  return menuController && menuController->owner() == [self viewsWidget];
}

// NSWindow overrides.

- (BOOL)_isTitleHidden {
  if (![self delegate])
    return NO;

  return ![self viewsWidget]->widget_delegate()->ShouldShowWindowTitle();
}

// Ignore [super canBecome{Key,Main}Window]. The default is NO for windows with
// NSBorderlessWindowMask, which is not the desired behavior.
// Note these can be called via -[NSWindow close] while the widget is being torn
// down, so check for a delegate.
- (BOOL)canBecomeKeyWindow {
  return [self delegate] && [self viewsWidget]->CanActivate();
}

- (BOOL)canBecomeMainWindow {
  if (![self delegate])
    return NO;

  // Dialogs and bubbles shouldn't take large shadows away from their parent.
  views::Widget* widget = [self viewsWidget];
  return widget->CanActivate() &&
         !views::NativeWidgetMac::GetBridgeForNativeWindow(self)->parent();
}

// Lets the traffic light buttons on the parent window keep their active state.
- (BOOL)hasKeyAppearance {
  if ([self delegate] && [self viewsWidget]->IsAlwaysRenderAsActive())
    return YES;
  return [super hasKeyAppearance];
}

// Override sendEvent to allow key events to be forwarded to a toolkit-views
// menu while it is active, and while still allowing any native subview to
// retain firstResponder status.
- (void)sendEvent:(NSEvent*)event {
  // Let CommandDispatcher check if this is a redispatched event.
  if ([commandDispatcher_ preSendEvent:event])
    return;

  NSEventType type = [event type];
  if ((type != NSKeyDown && type != NSKeyUp) || ![self hasViewsMenuActive]) {
    [super sendEvent:event];
    return;
  }

  // Send to the menu, after converting the event into an action message using
  // the content view.
  if (type == NSKeyDown)
    [[self contentView] keyDown:event];
  else
    [[self contentView] keyUp:event];
}

// Override window order functions to intercept other visibility changes. This
// is needed in addition to the -[NSWindow display] override because Cocoa
// hardly ever calls display, and reports -[NSWindow isVisible] incorrectly
// when ordering in a window for the first time.
- (void)orderWindow:(NSWindowOrderingMode)orderingMode
         relativeTo:(NSInteger)otherWindowNumber {
  [super orderWindow:orderingMode relativeTo:otherWindowNumber];
  [[self viewsNSWindowDelegate] onWindowOrderChanged:nil];
}

// NSResponder implementation.

- (BOOL)performKeyEquivalent:(NSEvent*)event {
  return [commandDispatcher_ performKeyEquivalent:event];
}

- (void)cursorUpdate:(NSEvent*)theEvent {
  // The cursor provided by the delegate should only be applied within the
  // content area. This is because we rely on the contentView to track the
  // mouse cursor and forward cursorUpdate: messages up the responder chain.
  // The cursorUpdate: isn't handled in BridgedContentView because views-style
  // SetCapture() conflicts with the way tracking events are processed for
  // the view during a drag. Since the NSWindow is still in the responder chain
  // overriding cursorUpdate: here handles both cases.
  if (!NSPointInRect([theEvent locationInWindow], [[self contentView] frame])) {
    [super cursorUpdate:theEvent];
    return;
  }

  NSCursor* cursor = [[self viewsNSWindowDelegate] cursor];
  if (cursor)
    [cursor set];
  else
    [super cursorUpdate:theEvent];
}

// CommandDispatchingWindow implementation.

- (void)setCommandHandler:(id<UserInterfaceItemCommandHandler>)commandHandler {
  commandHandler_.reset([commandHandler retain]);
}

- (BOOL)redispatchKeyEvent:(NSEvent*)event {
  return [commandDispatcher_ redispatchKeyEvent:event];
}

- (BOOL)defaultPerformKeyEquivalent:(NSEvent*)event {
  return [super performKeyEquivalent:event];
}

- (void)commandDispatch:(id)sender {
  [commandHandler_ commandDispatch:sender window:self];
}

- (void)commandDispatchUsingKeyModifiers:(id)sender {
  [commandHandler_ commandDispatchUsingKeyModifiers:sender window:self];
}

// NSWindow overrides (NSUserInterfaceItemValidations implementation)

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  // Since this class implements these selectors, |super| will always say they
  // are enabled. Only use [super] to validate other selectors. If there is no
  // command handler, defer to AppController.
  if ([item action] == @selector(commandDispatch:) ||
      [item action] == @selector(commandDispatchUsingKeyModifiers:)) {
    if (commandHandler_)
      return [commandHandler_ validateUserInterfaceItem:item window:self];

    id appController = [NSApp delegate];
    DCHECK([appController
        conformsToProtocol:@protocol(NSUserInterfaceValidations)]);
    return [appController validateUserInterfaceItem:item];
  }

  return [super validateUserInterfaceItem:item];
}

@end
