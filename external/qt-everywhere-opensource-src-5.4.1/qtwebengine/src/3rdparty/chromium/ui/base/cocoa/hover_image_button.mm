// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/hover_image_button.h"

@implementation HoverImageButton

- (void)drawRect:(NSRect)rect {
  if (hoverState_ == kHoverStateMouseDown && pressedImage_) {
    [super setImage:pressedImage_.get()];
  } else if (hoverState_ == kHoverStateMouseOver && hoverImage_) {
    [super setImage:hoverImage_.get()];
  } else {
    [super setImage:defaultImage_.get()];
  }

  [super drawRect:rect];
}

- (void)setDefaultImage:(NSImage*)image {
  defaultImage_.reset([image retain]);
}

- (void)setHoverImage:(NSImage*)image {
  hoverImage_.reset([image retain]);
}

- (void)setPressedImage:(NSImage*)image {
  pressedImage_.reset([image retain]);
}

@end
