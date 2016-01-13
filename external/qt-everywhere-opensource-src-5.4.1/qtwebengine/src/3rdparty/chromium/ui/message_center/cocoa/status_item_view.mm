// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/status_item_view.h"

#include <cmath>

#include "base/format_macros.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"

// The width of the status bar item when it's just the icon.
const CGFloat kStatusItemLength = 26;

// The amount of space between the left and right edges and the content of the
// status item.
const CGFloat kMargin = 5;


@interface MCStatusItemView (Private)
// Whether or not the status item should be drawn highlighted.
- (BOOL)shouldHighlight;

- (int)getTrayResourceId;
@end

@implementation MCStatusItemView

@synthesize highlight = highlight_;

- (id)init {
  statusItem_.reset([[[NSStatusBar systemStatusBar] statusItemWithLength:
      NSVariableStatusItemLength] retain]);
  CGFloat thickness = [[statusItem_ statusBar] thickness];

  NSRect frame = NSMakeRect(0, 0, kStatusItemLength, thickness);
  if ((self = [super initWithFrame:frame])) {
    [statusItem_ setView:self];
  }
  return self;
}

- (void)removeItem {
  [[NSStatusBar systemStatusBar] removeStatusItem:statusItem_];
  statusItem_.reset();
}

- (size_t)unreadCount {
  return unreadCount_;
}

- (message_center::StatusItemClickedCallback)callback {
  return callback_.get();
}

- (void)setCallback:(message_center::StatusItemClickedCallback)callback {
  callback_.reset(callback, base::scoped_policy::RETAIN);
}

- (void)setUnreadCount:(size_t)unreadCount withQuietMode:(BOOL)quietMode {
  unreadCount_ = unreadCount;
  quietMode_ = quietMode;

  NSRect frame = [self frame];
  frame.size.width = kStatusItemLength;
  [self setFrame:frame];

  [self setNeedsDisplay:YES];
}

- (void)setHighlight:(BOOL)highlight {
  highlight_ = highlight;
  [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent*)event {
  inMouseEventSequence_ = YES;
  [self setNeedsDisplay:YES];

  if (callback_)
    callback_.get()();
}

- (void)mouseUp:(NSEvent*)event {
  inMouseEventSequence_ = NO;
  [self setNeedsDisplay:YES];
}

- (void)rightMouseDown:(NSEvent*)event {
  [self mouseDown:event];
}

- (void)rightMouseUp:(NSEvent*)event {
  [self mouseUp:event];
}

- (void)otherMouseDown:(NSEvent*)event {
  [self mouseDown:event];
}

- (void)otherMouseUp:(NSEvent*)event {
  [self mouseUp:event];
}

- (void)drawRect:(NSRect)dirtyRect {
  NSRect frame = [self bounds];

  // Draw the background color.
  BOOL highlight = [self shouldHighlight];
  [statusItem_ drawStatusBarBackgroundInRect:frame
                               withHighlight:highlight];

  int resource_id = [self getTrayResourceId];
  // Draw the icon.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  NSImage* image = rb.GetNativeImageNamed(resource_id).ToNSImage();
  NSSize size = [image size];
  NSRect drawRect = NSMakeRect(kMargin,
                               floorf((NSHeight(frame) - size.height) / 2),
                               size.width,
                               size.height);
  [image drawInRect:drawRect
           fromRect:NSZeroRect
          operation:NSCompositeSourceOver
           fraction:1.0];
}

- (NSArray*)accessibilityActionNames {
  return @[ NSAccessibilityPressAction ];
}

- (void)accessibilityPerformAction:(NSString*)action {
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    if (callback_)
      callback_.get()();
    return;
  }
  [super accessibilityPerformAction:action];
}

// Private /////////////////////////////////////////////////////////////////////

- (BOOL)shouldHighlight {
  return highlight_ || inMouseEventSequence_;
}

- (int)getTrayResourceId {
  BOOL highlight = [self shouldHighlight];
  BOOL hasUnreadItems = unreadCount_ > 0;
  int kResourceIds[2][2][2] = {
    {
      { IDR_TRAY_EMPTY, IDR_TRAY_EMPTY_PRESSED },
      { IDR_TRAY_ATTENTION, IDR_TRAY_ATTENTION_PRESSED },
    },
    {
      { IDR_TRAY_DO_NOT_DISTURB_EMPTY,
        IDR_TRAY_DO_NOT_DISTURB_EMPTY_PRESSED },
      { IDR_TRAY_DO_NOT_DISTURB_ATTENTION,
        IDR_TRAY_DO_NOT_DISTURB_ATTENTION_PRESSED },
    },
  };
  return kResourceIds[quietMode_][hasUnreadItems][highlight];
}

@end
