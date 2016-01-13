// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_content_view.h"

#include "base/logging.h"
#include "ui/gfx/canvas_paint_mac.h"
#include "ui/views/view.h"

@implementation BridgedContentView

@synthesize hostedView = hostedView_;

- (id)initWithView:(views::View*)viewToHost {
  DCHECK(viewToHost);
  gfx::Rect bounds = viewToHost->bounds();
  // To keep things simple, assume the origin is (0, 0) until there exists a use
  // case for something other than that.
  DCHECK(bounds.origin().IsOrigin());
  NSRect initialFrame = NSMakeRect(0, 0, bounds.width(), bounds.height());
  if ((self = [super initWithFrame:initialFrame]))
    hostedView_ = viewToHost;

  return self;
}

- (void)clearView {
  hostedView_ = NULL;
}

// NSView implementation.

- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];
  if (!hostedView_)
    return;

  hostedView_->SetSize(gfx::Size(newSize.width, newSize.height));
}

- (void)drawRect:(NSRect)dirtyRect {
  if (!hostedView_)
    return;

  gfx::CanvasSkiaPaint canvas(dirtyRect, false /* opaque */);
  hostedView_->Paint(&canvas, views::CullSet());
}

@end
