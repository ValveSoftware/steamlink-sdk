// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/hover_image_button.h"

#import "base/mac/scoped_nsobject.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace {

class HoverImageButtonTest : public ui::CocoaTest {
 public:
  HoverImageButtonTest() {
    NSRect content_frame = [[test_window() contentView] frame];
    base::scoped_nsobject<HoverImageButton> button(
        [[HoverImageButton alloc] initWithFrame:content_frame]);
    button_ = button.get();
    [[test_window() contentView] addSubview:button_];
  }

  void DrawRect() {
    [button_ lockFocus];
    [button_ drawRect:[button_ bounds]];
    [button_ unlockFocus];
  }

  HoverImageButton* button_;
};

// Test mouse events.
TEST_F(HoverImageButtonTest, ImageSwap) {
  NSImage* image = [NSImage imageNamed:NSImageNameStatusAvailable];
  NSImage* hover = [NSImage imageNamed:NSImageNameStatusNone];
  [button_ setDefaultImage:image];
  [button_ setHoverImage:hover];

  [button_ mouseEntered:nil];
  DrawRect();
  EXPECT_EQ([button_ image], hover);
  [button_ mouseExited:nil];
  DrawRect();
  EXPECT_NE([button_ image], hover);
  EXPECT_EQ([button_ image], image);
}

}  // namespace
