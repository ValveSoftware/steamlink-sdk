// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_native_widget.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#import "testing/gtest_mac.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"
#import "ui/views/cocoa/bridged_content_view.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/view.h"

namespace views {

class BridgedNativeWidgetTest : public ui::CocoaTest {
 public:
  BridgedNativeWidgetTest();
  virtual ~BridgedNativeWidgetTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  // TODO(tapted): Make this a EventCountView from widget_unittest.cc.
  scoped_ptr<views::View> view_;
  scoped_ptr<BridgedNativeWidget> bridge_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidgetTest);
};

BridgedNativeWidgetTest::BridgedNativeWidgetTest() {
}

BridgedNativeWidgetTest::~BridgedNativeWidgetTest() {
}

void BridgedNativeWidgetTest::SetUp() {
  ui::CocoaTest::SetUp();

  view_.reset(new views::View);
  bridge_.reset(new BridgedNativeWidget);
  base::scoped_nsobject<NSWindow> window([test_window() retain]);
  bridge_->Init(window);
  bridge_->SetRootView(view_.get());

  [test_window() makePretendKeyWindowAndSetFirstResponder:bridge_->ns_view()];
}

void BridgedNativeWidgetTest::TearDown() {
  [test_window() clearPretendKeyWindowAndFirstResponder];

  bridge_.reset();
  view_.reset();

  ui::CocoaTest::TearDown();
}

// The TEST_VIEW macro expects the view it's testing to have a superview. In
// these tests, the NSView bridge is a contentView, at the root. These mimic
// what TEST_VIEW usually does.
TEST_F(BridgedNativeWidgetTest, BridgedNativeWidgetTest_TestViewAddRemove) {
  base::scoped_nsobject<BridgedContentView> view([bridge_->ns_view() retain]);
  EXPECT_NSEQ([test_window() contentView], view);
  EXPECT_NSEQ(test_window(), [view window]);

  // The superview of a contentView is an NSNextStepFrame.
  EXPECT_TRUE([view superview]);
  EXPECT_TRUE([view hostedView]);

  // Destroying the C++ bridge should remove references to any C++ objects in
  // the ObjectiveC object, and remove it from the hierarchy.
  bridge_.reset();
  EXPECT_FALSE([view hostedView]);
  EXPECT_FALSE([view superview]);
  EXPECT_FALSE([view window]);
  EXPECT_FALSE([test_window() contentView]);
}

TEST_F(BridgedNativeWidgetTest, BridgedNativeWidgetTest_TestViewDisplay) {
  [bridge_->ns_view() display];
}

// Test that resizing the window resizes the root view appropriately.
TEST_F(BridgedNativeWidgetTest, ViewSizeTracksWindow) {
  const int kTestNewWidth = 400;
  const int kTestNewHeight = 300;

  // |test_window()| is borderless, so these should align.
  NSSize window_size = [test_window() frame].size;
  EXPECT_EQ(view_->width(), static_cast<int>(window_size.width));
  EXPECT_EQ(view_->height(), static_cast<int>(window_size.height));

  // Make sure a resize actually occurs.
  EXPECT_NE(kTestNewWidth, view_->width());
  EXPECT_NE(kTestNewHeight, view_->height());

  [test_window() setFrame:NSMakeRect(0, 0, kTestNewWidth, kTestNewHeight)
                  display:NO];
  EXPECT_EQ(kTestNewWidth, view_->width());
  EXPECT_EQ(kTestNewHeight, view_->height());
}

TEST_F(BridgedNativeWidgetTest, CreateInputMethod) {
  scoped_ptr<views::InputMethod> input_method(bridge_->CreateInputMethod());
  EXPECT_TRUE(input_method);
}

TEST_F(BridgedNativeWidgetTest, GetHostInputMethod) {
  EXPECT_TRUE(bridge_->GetHostInputMethod());
}

}  // namespace views
