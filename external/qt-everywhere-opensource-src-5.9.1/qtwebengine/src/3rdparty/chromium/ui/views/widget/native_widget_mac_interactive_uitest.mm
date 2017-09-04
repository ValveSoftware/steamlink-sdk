// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "ui/base/test/ui_controls.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/widget_test.h"

namespace views {
namespace test {

// Tests for NativeWidgetMac that rely on global window manager state, and can
// not be parallelized.
class NativeWidgetMacInteractiveUITest
    : public WidgetTest,
      public ::testing::WithParamInterface<bool> {
 public:
  class Observer;

  NativeWidgetMacInteractiveUITest()
      : activationCount_(0), deactivationCount_(0) {
    // TODO(tapted): Remove this when these are absorbed into Chrome's
    // interactive_ui_tests target. See http://crbug.com/403679.
    ui_controls::EnableUIControls();
  }

  Widget* MakeWidget() {
    return GetParam() ? CreateTopLevelFramelessPlatformWidget()
                      : CreateTopLevelPlatformWidget();
  }

 protected:
  std::unique_ptr<Observer> observer_;
  int activationCount_;
  int deactivationCount_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMacInteractiveUITest);
};

class NativeWidgetMacInteractiveUITest::Observer : public TestWidgetObserver {
 public:
  Observer(NativeWidgetMacInteractiveUITest* parent, Widget* widget)
      : TestWidgetObserver(widget), parent_(parent) {}

  void OnWidgetActivationChanged(Widget* widget, bool active) override {
    if (active)
      parent_->activationCount_++;
    else
      parent_->deactivationCount_++;
  }

 private:
  NativeWidgetMacInteractiveUITest* parent_;

  DISALLOW_COPY_AND_ASSIGN(Observer);
};

// Test that showing a window causes it to attain global keyWindow status.
TEST_P(NativeWidgetMacInteractiveUITest, ShowAttainsKeyStatus) {
  Widget* widget = MakeWidget();
  observer_.reset(new Observer(this, widget));

  EXPECT_FALSE(widget->IsActive());
  EXPECT_EQ(0, activationCount_);
  widget->Show();
  EXPECT_TRUE(widget->IsActive());
  RunPendingMessages();
  EXPECT_TRUE([widget->GetNativeWindow() isKeyWindow]);
  EXPECT_EQ(1, activationCount_);
  EXPECT_EQ(0, deactivationCount_);

  // Now check that losing and gaining key status due events outside of Widget
  // works correctly.
  Widget* widget2 = MakeWidget();  // Note: not observed.
  EXPECT_EQ(0, deactivationCount_);
  widget2->Show();
  EXPECT_EQ(1, deactivationCount_);

  RunPendingMessages();
  EXPECT_FALSE(widget->IsActive());
  EXPECT_EQ(1, deactivationCount_);
  EXPECT_EQ(1, activationCount_);

  [widget->GetNativeWindow() makeKeyAndOrderFront:nil];
  RunPendingMessages();
  EXPECT_TRUE(widget->IsActive());
  EXPECT_EQ(1, deactivationCount_);
  EXPECT_EQ(2, activationCount_);

  widget2->CloseNow();
  widget->CloseNow();

  EXPECT_EQ(1, deactivationCount_);
  EXPECT_EQ(2, activationCount_);
}

// Test that ShowInactive does not take keyWindow status.
TEST_P(NativeWidgetMacInteractiveUITest, ShowInactiveIgnoresKeyStatus) {
  Widget* widget = MakeWidget();

  base::scoped_nsobject<WindowedNSNotificationObserver> waiter(
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidBecomeKeyNotification
                       object:widget->GetNativeWindow()]);

  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([widget->GetNativeWindow() isVisible]);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE([widget->GetNativeWindow() isKeyWindow]);
  widget->ShowInactive();

  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([widget->GetNativeWindow() isVisible]);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE([widget->GetNativeWindow() isKeyWindow]);

  // If the window were to become active, this would activate it.
  RunPendingMessages();
  EXPECT_FALSE(widget->IsActive());
  EXPECT_FALSE([widget->GetNativeWindow() isKeyWindow]);
  EXPECT_EQ(0, [waiter notificationCount]);

  // Activating the inactive widget should make it key, asynchronously.
  widget->Activate();
  [waiter wait];
  EXPECT_EQ(1, [waiter notificationCount]);
  EXPECT_TRUE(widget->IsActive());
  EXPECT_TRUE([widget->GetNativeWindow() isKeyWindow]);

  widget->CloseNow();
}

namespace {

// Show |widget| and wait for it to become the key window.
void ShowKeyWindow(Widget* widget) {
  base::scoped_nsobject<WindowedNSNotificationObserver> waiter(
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidBecomeKeyNotification
                       object:widget->GetNativeWindow()]);
  widget->Show();
  EXPECT_TRUE([waiter wait]);
  EXPECT_TRUE([widget->GetNativeWindow() isKeyWindow]);
}

NSData* ViewAsTIFF(NSView* view) {
  NSBitmapImageRep* bitmap =
      [view bitmapImageRepForCachingDisplayInRect:[view bounds]];
  [view cacheDisplayInRect:[view bounds] toBitmapImageRep:bitmap];
  return [bitmap TIFFRepresentation];
}

class TestBubbleView : public BubbleDialogDelegateView {
 public:
  explicit TestBubbleView(Widget* parent) {
    SetAnchorView(parent->GetContentsView());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBubbleView);
};

}  // namespace

// Test that parent windows keep their traffic lights enabled when showing
// dialogs.
TEST_F(NativeWidgetMacInteractiveUITest, ParentWindowTrafficLights) {
  Widget* parent_widget = CreateTopLevelPlatformWidget();
  parent_widget->SetBounds(gfx::Rect(100, 100, 100, 100));
  ShowKeyWindow(parent_widget);

  NSWindow* parent = parent_widget->GetNativeWindow();
  EXPECT_TRUE([parent isMainWindow]);

  NSButton* button = [parent standardWindowButton:NSWindowCloseButton];
  EXPECT_TRUE(button);
  NSData* active_button_image = ViewAsTIFF(button);
  EXPECT_TRUE(active_button_image);

  // Pop open a bubble on the parent Widget. When the visibility of Bubbles with
  // an anchor View changes, BubbleDialogDelegateView::HandleVisibilityChanged()
  // updates Widget::SetAlwaysRenderAsActive(..) accordingly.
  ShowKeyWindow(BubbleDialogDelegateView::CreateBubble(
      new TestBubbleView(parent_widget)));

  // Ensure the button instance is still valid.
  EXPECT_EQ(button, [parent standardWindowButton:NSWindowCloseButton]);

  // Parent window should still be main, and have its traffic lights active.
  EXPECT_TRUE([parent isMainWindow]);
  EXPECT_FALSE([parent isKeyWindow]);

  // Enabled status doesn't actually change, but check anyway.
  EXPECT_TRUE([button isEnabled]);
  NSData* button_image_with_child = ViewAsTIFF(button);
  EXPECT_TRUE([active_button_image isEqualToData:button_image_with_child]);

  // Verify that activating some other random window does change the button.
  // When the bubble loses activation, it will dismiss itself and update
  // Widget::SetAlwaysRenderAsActive().
  Widget* other_widget = CreateTopLevelPlatformWidget();
  other_widget->SetBounds(gfx::Rect(200, 200, 100, 100));
  ShowKeyWindow(other_widget);
  EXPECT_FALSE([parent isMainWindow]);
  EXPECT_FALSE([parent isKeyWindow]);
  EXPECT_TRUE([button isEnabled]);
  NSData* inactive_button_image = ViewAsTIFF(button);
  EXPECT_FALSE([active_button_image isEqualToData:inactive_button_image]);

  other_widget->CloseNow();
  parent_widget->CloseNow();
}

INSTANTIATE_TEST_CASE_P(NativeWidgetMacInteractiveUITestInstance,
                        NativeWidgetMacInteractiveUITest,
                        ::testing::Bool());

}  // namespace test
}  // namespace views
