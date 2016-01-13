// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/hover_button.h"

@implementation HoverButton

@synthesize hoverState = hoverState_;

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    [self setTrackingEnabled:YES];
    hoverState_ = kHoverStateNone;
    [self updateTrackingAreas];
  }
  return self;
}

- (void)awakeFromNib {
  [self setTrackingEnabled:YES];
  self.hoverState = kHoverStateNone;
  [self updateTrackingAreas];
}

- (void)dealloc {
  [self setTrackingEnabled:NO];
  [super dealloc];
}

- (void)mouseEntered:(NSEvent*)theEvent {
  if (trackingArea_.get())
    self.hoverState = kHoverStateMouseOver;
}

- (void)mouseExited:(NSEvent*)theEvent {
  if (trackingArea_.get())
    self.hoverState = kHoverStateNone;
}

- (void)mouseDown:(NSEvent*)theEvent {
  self.hoverState = kHoverStateMouseDown;
  // The hover button needs to hold onto itself here for a bit.  Otherwise,
  // it can be freed while |super mouseDown:| is in its loop, and the
  // |checkImageState| call will crash.
  // http://crbug.com/28220
  base::scoped_nsobject<HoverButton> myself([self retain]);

  [super mouseDown:theEvent];
  // We need to check the image state after the mouseDown event loop finishes.
  // It's possible that we won't get a mouseExited event if the button was
  // moved under the mouse during tab resize, instead of the mouse moving over
  // the button.
  // http://crbug.com/31279
  [self checkImageState];
}

- (void)setTrackingEnabled:(BOOL)enabled {
  if (enabled) {
    trackingArea_.reset(
        [[CrTrackingArea alloc] initWithRect:NSZeroRect
                                     options:NSTrackingMouseEnteredAndExited |
                                             NSTrackingActiveAlways |
                                             NSTrackingInVisibleRect
                                       owner:self
                                    userInfo:nil]);
    [self addTrackingArea:trackingArea_.get()];

    // If you have a separate window that overlaps the close button, and you
    // move the mouse directly over the close button without entering another
    // part of the tab strip, we don't get any mouseEntered event since the
    // tracking area was disabled when we entered.
    // Done with a delay of 0 because sometimes an event appears to be missed
    // between the activation of the tracking area and the call to
    // checkImageState resulting in the button state being incorrect.
    [self performSelector:@selector(checkImageState)
               withObject:nil
               afterDelay:0];
  } else {
    if (trackingArea_.get()) {
      [self removeTrackingArea:trackingArea_.get()];
      trackingArea_.reset(nil);
    }
  }
}

- (void)updateTrackingAreas {
  [super updateTrackingAreas];
  [self checkImageState];
}

- (void)checkImageState {
  if (!trackingArea_.get())
    return;

  // Update the button's state if the button has moved.
  NSPoint mouseLoc = [[self window] mouseLocationOutsideOfEventStream];
  mouseLoc = [self convertPoint:mouseLoc fromView:nil];
  self.hoverState = NSPointInRect(mouseLoc, [self bounds]) ?
      kHoverStateMouseOver : kHoverStateNone;
}

- (void)setHoverState:(HoverState)state {
  hoverState_ = state;
  [self setNeedsDisplay:YES];
}

@end
