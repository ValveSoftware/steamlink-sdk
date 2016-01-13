// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/tray_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_tray.h"

namespace message_center {

class TrayControllerTest : public ui::CocoaTest {
 public:
  virtual void SetUp() OVERRIDE {
    ui::CocoaTest::SetUp();
    message_center::MessageCenter::Initialize();
    tray_.reset(new message_center::MessageCenterTray(
        NULL, message_center::MessageCenter::Get()));
    controller_.reset(
        [[MCTrayController alloc] initWithMessageCenterTray:tray_.get()]);
  }

  virtual void TearDown() OVERRIDE {
    controller_.reset();
    tray_.reset();
    message_center::MessageCenter::Shutdown();
    ui::CocoaTest::TearDown();
  }

 protected:
  scoped_ptr<message_center::MessageCenterTray> tray_;
  base::scoped_nsobject<MCTrayController> controller_;
};

TEST_F(TrayControllerTest, OpenLeftRight) {
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];
  NSRect screen_frame = [screen visibleFrame];

  const CGFloat y = NSMaxY(screen_frame);

  // With ample room to the right, it should open to the right.
  NSPoint right_point = NSMakePoint(0, y);
  NSPoint left_point = NSMakePoint(NSMaxX(screen_frame), y);

  [controller_ showTrayAtRightOf:right_point atLeftOf:left_point];
  NSRect window_frame = [[controller_ window] frame];
  EXPECT_EQ(right_point.x, NSMinX(window_frame));

  // With little room on the right, it should open to the left.
  right_point = NSMakePoint(NSMaxX(screen_frame) - 10, y);
  [controller_ showTrayAtRightOf:right_point atLeftOf:left_point];
  window_frame = [[controller_ window] frame];
  EXPECT_EQ(left_point.x - NSWidth(window_frame), NSMinX(window_frame));
}

}  // namespace message_center
