// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/widget/native_widget_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/location.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#import "testing/gtest_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#import "ui/base/cocoa/constrained_window/constrained_window_animation.h"
#import "ui/base/cocoa/window_size_constants.h"
#import "ui/base/test/scoped_fake_full_keyboard_access.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/events/test/event_generator.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/bridged_native_widget.h"
#import "ui/views/cocoa/native_widget_mac_nswindow.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/native_cursor.h"
#include "ui/views/test/native_widget_factory.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/window/dialog_delegate.h"

// Donates an implementation of -[NSAnimation stopAnimation] which calls the
// original implementation, then quits a nested run loop.
@interface TestStopAnimationWaiter : NSObject
@end

@interface ConstrainedWindowAnimationBase (TestingAPI)
- (void)setWindowStateForEnd;
@end

@interface NSWindow (PrivateAPI)
- (BOOL)_isTitleHidden;
@end

// Test NSWindow that provides hooks via method overrides to verify behavior.
@interface NativeWidgetMacTestWindow : NativeWidgetMacNSWindow {
 @private
  int invalidateShadowCount_;
  bool* deallocFlag_;
}
@property(readonly, nonatomic) int invalidateShadowCount;
@property(assign, nonatomic) bool* deallocFlag;
@end

// Used to mock BridgedContentView so that calls to drawRect: can be
// intercepted.
@interface MockBridgedView : NSView {
 @private
  // Number of times -[NSView drawRect:] has been called.
  NSUInteger drawRectCount_;

  // The dirtyRect parameter passed to last invocation of drawRect:.
  NSRect lastDirtyRect_;
}

@property(assign, nonatomic) NSUInteger drawRectCount;
@property(assign, nonatomic) NSRect lastDirtyRect;
@end

@interface FocusableTestNSView : NSView
@end

@interface TestNativeParentWindow : NSWindow
@property(assign, nonatomic) bool* deallocFlag;
@end

namespace views {
namespace test {

// BridgedNativeWidget friend to access private members.
class BridgedNativeWidgetTestApi {
 public:
  explicit BridgedNativeWidgetTestApi(NSWindow* window) {
    bridge_ = NativeWidgetMac::GetBridgeForNativeWindow(window);
  }

  // Simulate a frame swap from the compositor.
  void SimulateFrameSwap(const gfx::Size& size) {
    const float kScaleFactor = 1.0f;
    bridge_->compositor_widget_->GotIOSurfaceFrame(
        base::ScopedCFTypeRef<IOSurfaceRef>(), size, kScaleFactor);
    bridge_->AcceleratedWidgetSwapCompleted();
  }

 private:
  BridgedNativeWidget* bridge_;

  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidgetTestApi);
};

// Custom native_widget to create a NativeWidgetMacTestWindow.
class TestWindowNativeWidgetMac : public NativeWidgetMac {
 public:
  explicit TestWindowNativeWidgetMac(Widget* delegate)
      : NativeWidgetMac(delegate) {}

 protected:
  // NativeWidgetMac:
  NativeWidgetMacNSWindow* CreateNSWindow(
      const Widget::InitParams& params) override {
    NSUInteger style_mask = NSBorderlessWindowMask;
    if (params.type == Widget::InitParams::TYPE_WINDOW) {
      style_mask = NSTexturedBackgroundWindowMask | NSTitledWindowMask |
                   NSClosableWindowMask | NSMiniaturizableWindowMask |
                   NSResizableWindowMask;
    }
    return [[[NativeWidgetMacTestWindow alloc]
        initWithContentRect:ui::kWindowSizeDeterminedLater
                  styleMask:style_mask
                    backing:NSBackingStoreBuffered
                      defer:NO] autorelease];
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowNativeWidgetMac);
};

// Tests for parts of NativeWidgetMac not covered by BridgedNativeWidget, which
// need access to Cocoa APIs.
class NativeWidgetMacTest : public WidgetTest {
 public:
  NativeWidgetMacTest() {}

  // The content size of NSWindows made by MakeNativeParent().
  NSRect ParentRect() const { return NSMakeRect(100, 100, 300, 200); }

  // Make a native NSWindow with the given |style_mask| to use as a parent.
  TestNativeParentWindow* MakeNativeParentWithStyle(int style_mask) {
    native_parent_.reset([[TestNativeParentWindow alloc]
        initWithContentRect:ParentRect()
                  styleMask:style_mask
                    backing:NSBackingStoreBuffered
                      defer:NO]);
    [native_parent_ setReleasedWhenClosed:NO];  // Owned by scoped_nsobject.
    [native_parent_ makeKeyAndOrderFront:nil];
    return native_parent_;
  }

  // Make a borderless, native NSWindow to use as a parent.
  TestNativeParentWindow* MakeNativeParent() {
    return MakeNativeParentWithStyle(NSBorderlessWindowMask);
  }

  // Create a Widget backed by the NativeWidgetMacTestWindow NSWindow subclass.
  Widget* CreateWidgetWithTestWindow(Widget::InitParams params,
                                     NativeWidgetMacTestWindow** window) {
    Widget* widget = new Widget;
    params.native_widget = new TestWindowNativeWidgetMac(widget);
    widget->Init(params);
    widget->Show();
    *window = base::mac::ObjCCastStrict<NativeWidgetMacTestWindow>(
        widget->GetNativeWindow());
    EXPECT_TRUE(*window);
    return widget;
  }

 protected:
  base::scoped_nsobject<TestNativeParentWindow> native_parent_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMacTest);
};

class WidgetChangeObserver : public TestWidgetObserver {
 public:
  WidgetChangeObserver(Widget* widget) : TestWidgetObserver(widget) {}

  void WaitForVisibleCounts(int gained, int lost) {
    if (gained_visible_count_ >= gained && lost_visible_count_ >= lost)
      return;

    target_gained_visible_count_ = gained;
    target_lost_visible_count_ = lost;

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
    run_loop_ = nullptr;
  }

  int gained_visible_count() const { return gained_visible_count_; }
  int lost_visible_count() const { return lost_visible_count_; }

 private:
  // WidgetObserver:
  void OnWidgetVisibilityChanged(Widget* widget,
                                 bool visible) override {
    ++(visible ? gained_visible_count_ : lost_visible_count_);
    if (run_loop_ && gained_visible_count_ >= target_gained_visible_count_ &&
        lost_visible_count_ >= target_lost_visible_count_)
      run_loop_->Quit();
  }

  int gained_visible_count_ = 0;
  int lost_visible_count_ = 0;
  int target_gained_visible_count_ = 0;
  int target_lost_visible_count_ = 0;
  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WidgetChangeObserver);
};

class NativeHostHolder {
 public:
  NativeHostHolder()
      : view_([[NSView alloc] init]), host_(new NativeViewHost()) {
    host_->set_owned_by_client();
  }

  void AttachNativeView() {
    DCHECK(!host_->native_view());
    host_->Attach(view_.get());
  }

  void Detach() { host_->Detach(); }

  gfx::NativeView view() const { return view_.get(); }
  NativeViewHost* host() const { return host_.get(); }

 private:
  base::scoped_nsobject<NSView> view_;
  std::unique_ptr<NativeViewHost> host_;

  DISALLOW_COPY_AND_ASSIGN(NativeHostHolder);
};

// This class gives public access to the protected ctor of
// BubbleDialogDelegateView.
class SimpleBubbleView : public BubbleDialogDelegateView {
 public:
  SimpleBubbleView() {}
  ~SimpleBubbleView() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleBubbleView);
};

// Test visibility states triggered externally.
TEST_F(NativeWidgetMacTest, HideAndShowExternally) {
  Widget* widget = CreateTopLevelPlatformWidget();
  NSWindow* ns_window = widget->GetNativeWindow();
  WidgetChangeObserver observer(widget);

  // Should initially be hidden.
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(0, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());

  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());

  widget->Hide();
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());

  widget->Show();
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());

  // Test when hiding individual windows.
  [ns_window orderOut:nil];
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());

  [ns_window orderFront:nil];
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());

  // Test when hiding the entire application. This doesn't send an orderOut:
  // to the NSWindow.
  [NSApp hide:nil];
  // When the activation policy is NSApplicationActivationPolicyRegular, the
  // calls via NSApp are asynchronous, and the run loop needs to be flushed.
  // With NSApplicationActivationPolicyProhibited, the following
  // WaitForVisibleCounts calls are superfluous, but don't hurt.
  observer.WaitForVisibleCounts(3, 3);
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(3, observer.lost_visible_count());

  [NSApp unhideWithoutActivation];
  observer.WaitForVisibleCounts(4, 3);
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(4, observer.gained_visible_count());
  EXPECT_EQ(3, observer.lost_visible_count());

  // Hide again to test unhiding with an activation.
  [NSApp hide:nil];
  observer.WaitForVisibleCounts(4, 4);
  EXPECT_EQ(4, observer.lost_visible_count());
  [NSApp unhide:nil];
  observer.WaitForVisibleCounts(5, 4);
  EXPECT_EQ(5, observer.gained_visible_count());

  // Hide again to test makeKeyAndOrderFront:.
  [ns_window orderOut:nil];
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE([ns_window isVisible]);
  EXPECT_EQ(5, observer.gained_visible_count());
  EXPECT_EQ(5, observer.lost_visible_count());

  [ns_window makeKeyAndOrderFront:nil];
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE([ns_window isVisible]);
  EXPECT_EQ(6, observer.gained_visible_count());
  EXPECT_EQ(5, observer.lost_visible_count());

  // No change when closing.
  widget->CloseNow();
  EXPECT_EQ(5, observer.lost_visible_count());
  EXPECT_EQ(6, observer.gained_visible_count());
}

// A view that counts calls to OnPaint().
class PaintCountView : public View {
 public:
  PaintCountView() { SetBounds(0, 0, 100, 100); }

  // View:
  void OnPaint(gfx::Canvas* canvas) override {
    EXPECT_TRUE(GetWidget()->IsVisible());
    ++paint_count_;
    if (run_loop_ && paint_count_ == target_paint_count_)
      run_loop_->Quit();
  }

  void WaitForPaintCount(int target) {
    if (paint_count_ == target)
      return;

    target_paint_count_ = target;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  int paint_count() { return paint_count_; }

 private:
  int paint_count_ = 0;
  int target_paint_count_ = 0;
  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PaintCountView);
};

// Test for correct child window restore when parent window is minimized
// and restored using -makeKeyAndOrderFront:.
// Parent-child window relationships in AppKit are not supported when window
// visibility changes.
// Disabled because it relies on cocoa occlusion APIs
// and state changes that are unavoidably flaky.
TEST_F(NativeWidgetMacTest, DISABLED_OrderFrontAfterMiniaturize) {
  Widget* widget = CreateTopLevelPlatformWidget();
  NSWindow* ns_window = widget->GetNativeWindow();

  Widget* child_widget = CreateChildPlatformWidget(widget->GetNativeView());
  NSWindow* child_ns_window = child_widget->GetNativeWindow();

  // Set parent bounds that overlap child.
  widget->SetBounds(gfx::Rect(100, 100, 300, 300));
  child_widget->SetBounds(gfx::Rect(110, 110, 100, 100));

  widget->Show();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(widget->IsMinimized());

  // Minimize parent.
  [ns_window performMiniaturize:nil];
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_FALSE(child_widget->IsVisible());

  // Restore parent window as AppController does.
  [ns_window makeKeyAndOrderFront:nil];

  // Wait and check that child is really visible.
  // TODO(kirr): remove the fixed delay.
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(),
      base::TimeDelta::FromSeconds(2));
  base::MessageLoop::current()->Run();

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_TRUE(child_widget->IsVisible());
  // Check that child window is visible.
  EXPECT_TRUE([child_ns_window occlusionState] & NSWindowOcclusionStateVisible);
  EXPECT_TRUE(IsWindowStackedAbove(child_widget, widget));
  widget->Close();
}

// Test minimized states triggered externally, implied visibility and restored
// bounds whilst minimized.
TEST_F(NativeWidgetMacTest, MiniaturizeExternally) {
  Widget* widget = new Widget;
  Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW);
  widget->Init(init_params);

  PaintCountView* view = new PaintCountView();
  widget->GetContentsView()->AddChildView(view);
  NSWindow* ns_window = widget->GetNativeWindow();
  WidgetChangeObserver observer(widget);

  widget->SetBounds(gfx::Rect(100, 100, 300, 300));

  EXPECT_TRUE(view->IsDrawn());
  EXPECT_EQ(0, view->paint_count());
  widget->Show();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(0, observer.lost_visible_count());
  const gfx::Rect restored_bounds = widget->GetRestoredBounds();
  EXPECT_FALSE(restored_bounds.IsEmpty());
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());

  // Showing should paint.
  view->WaitForPaintCount(1);

  // First try performMiniaturize:, which requires a minimize button. Note that
  // Cocoa just blocks the UI thread during the animation, so no need to do
  // anything fancy to wait for it finish.
  [ns_window performMiniaturize:nil];
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_FALSE(widget->IsVisible());  // Minimizing also makes things invisible.
  EXPECT_EQ(1, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());

  // No repaint when minimizing. But note that this is partly due to not calling
  // [NSView setNeedsDisplay:YES] on the content view. The superview, which is
  // an NSThemeFrame, would repaint |view| if we had, because the miniaturize
  // button is highlighted for performMiniaturize.
  EXPECT_EQ(1, view->paint_count());

  [ns_window deminiaturize:nil];
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_TRUE(widget->IsVisible());
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(1, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());

  view->WaitForPaintCount(2);  // A single paint when deminiaturizing.
  EXPECT_FALSE([ns_window isMiniaturized]);

  widget->Minimize();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_TRUE([ns_window isMiniaturized]);
  EXPECT_EQ(2, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(2, view->paint_count());  // No paint when miniaturizing.

  widget->Restore();  // If miniaturized, should deminiaturize.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  view->WaitForPaintCount(3);

  widget->Restore();  // If not miniaturized, does nothing.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE([ns_window isMiniaturized]);
  EXPECT_EQ(3, observer.gained_visible_count());
  EXPECT_EQ(2, observer.lost_visible_count());
  EXPECT_EQ(restored_bounds, widget->GetRestoredBounds());
  EXPECT_EQ(3, view->paint_count());

  widget->CloseNow();

  // Create a widget without a minimize button.
  widget = CreateTopLevelFramelessPlatformWidget();
  ns_window = widget->GetNativeWindow();
  widget->SetBounds(gfx::Rect(100, 100, 300, 300));
  widget->Show();
  EXPECT_FALSE(widget->IsMinimized());

  // This should fail, since performMiniaturize: requires a minimize button.
  [ns_window performMiniaturize:nil];
  EXPECT_FALSE(widget->IsMinimized());

  // But this should work.
  widget->Minimize();
  EXPECT_TRUE(widget->IsMinimized());

  // Test closing while minimized.
  widget->CloseNow();
}

// Simple view for the SetCursor test that overrides View::GetCursor().
class CursorView : public View {
 public:
  CursorView(int x, NSCursor* cursor) : cursor_(cursor) {
    SetBounds(x, 0, 100, 300);
  }

  // View:
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override {
    return cursor_;
  }

 private:
  NSCursor* cursor_;

  DISALLOW_COPY_AND_ASSIGN(CursorView);
};

// Test for Widget::SetCursor(). There is no Widget::GetCursor(), so this uses
// -[NSCursor currentCursor] to validate expectations. Note that currentCursor
// is just "the top cursor on the application's cursor stack.", which is why it
// is safe to use this in a non-interactive UI test with the EventGenerator.
TEST_F(NativeWidgetMacTest, SetCursor) {
  NSCursor* arrow = [NSCursor arrowCursor];
  NSCursor* hand = GetNativeHandCursor();
  NSCursor* ibeam = GetNativeIBeamCursor();

  Widget* widget = CreateTopLevelPlatformWidget();
  widget->SetBounds(gfx::Rect(0, 0, 300, 300));
  widget->GetContentsView()->AddChildView(new CursorView(0, hand));
  widget->GetContentsView()->AddChildView(new CursorView(100, ibeam));
  widget->Show();

  // Events used to simulate tracking rectangle updates. These are not passed to
  // toolkit-views, so it only matters whether they are inside or outside the
  // content area.
  NSEvent* event_in_content = cocoa_test_event_utils::MouseEventAtPoint(
      NSMakePoint(100, 100), NSMouseMoved, 0);
  NSEvent* event_out_of_content = cocoa_test_event_utils::MouseEventAtPoint(
      NSMakePoint(-50, -50), NSMouseMoved, 0);

  EXPECT_NE(arrow, hand);
  EXPECT_NE(arrow, ibeam);

  // Make arrow the current cursor.
  [arrow set];
  EXPECT_EQ(arrow, [NSCursor currentCursor]);

  // Use an event generator to ask views code to set the cursor. However, note
  // that this does not cause Cocoa to generate tracking rectangle updates.
  ui::test::EventGenerator event_generator(GetContext(),
                                           widget->GetNativeWindow());

  // Move the mouse over the first view, then simulate a tracking rectangle
  // update. Verify that the cursor changed from arrow to hand type.
  event_generator.MoveMouseTo(gfx::Point(50, 50));
  [widget->GetNativeWindow() cursorUpdate:event_in_content];
  EXPECT_EQ(hand, [NSCursor currentCursor]);

  // A tracking rectangle update not in the content area should forward to
  // the native NSWindow implementation, which sets the arrow cursor.
  [widget->GetNativeWindow() cursorUpdate:event_out_of_content];
  EXPECT_EQ(arrow, [NSCursor currentCursor]);

  // Now move to the second view.
  event_generator.MoveMouseTo(gfx::Point(150, 50));
  [widget->GetNativeWindow() cursorUpdate:event_in_content];
  EXPECT_EQ(ibeam, [NSCursor currentCursor]);

  // Moving to the third view (but remaining in the content area) should also
  // forward to the native NSWindow implementation.
  event_generator.MoveMouseTo(gfx::Point(250, 50));
  [widget->GetNativeWindow() cursorUpdate:event_in_content];
  EXPECT_EQ(arrow, [NSCursor currentCursor]);

  widget->CloseNow();
}

// Tests that an accessibility request from the system makes its way through to
// a views::Label filling the window.
TEST_F(NativeWidgetMacTest, AccessibilityIntegration) {
  Widget* widget = CreateTopLevelPlatformWidget();
  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  const base::string16 test_string = base::ASCIIToUTF16("Green");
  views::Label* label = new views::Label(test_string);
  label->SetBounds(0, 0, 100, 100);
  widget->GetContentsView()->AddChildView(label);
  widget->Show();

  // Accessibility hit tests come in Cocoa screen coordinates.
  NSRect nsrect = gfx::ScreenRectToNSRect(screen_rect);
  NSPoint midpoint = NSMakePoint(NSMidX(nsrect), NSMidY(nsrect));

  id hit = [widget->GetNativeWindow() accessibilityHitTest:midpoint];
  id title = [hit accessibilityAttributeValue:NSAccessibilityTitleAttribute];
  EXPECT_NSEQ(title, @"Green");

  widget->CloseNow();
}

// Tests creating a views::Widget parented off a native NSWindow.
TEST_F(NativeWidgetMacTest, NonWidgetParent) {
  NSWindow* native_parent = MakeNativeParent();

  base::scoped_nsobject<NSView> anchor_view(
      [[NSView alloc] initWithFrame:[[native_parent contentView] bounds]]);
  [[native_parent contentView] addSubview:anchor_view];

  // Note: Don't use WidgetTest::CreateChildPlatformWidget because that makes
  // windows of TYPE_CONTROL which need a parent Widget to obtain the focus
  // manager.
  Widget* child = new Widget;
  Widget::InitParams init_params;
  init_params.parent = anchor_view;
  init_params.type = Widget::InitParams::TYPE_POPUP;
  child->Init(init_params);

  TestWidgetObserver child_observer(child);

  // GetTopLevelNativeWidget() only goes as far as there exists a Widget (i.e.
  // must stop at |child|.
  internal::NativeWidgetPrivate* top_level_widget =
      internal::NativeWidgetPrivate::GetTopLevelNativeWidget(
          child->GetNativeView());
  EXPECT_EQ(child, top_level_widget->GetWidget());

  // To verify the parent, we need to use NativeWidgetMac APIs.
  BridgedNativeWidget* bridged_native_widget =
      NativeWidgetMac::GetBridgeForNativeWindow(child->GetNativeWindow());
  EXPECT_EQ(native_parent, bridged_native_widget->parent()->GetNSWindow());

  const gfx::Rect child_bounds(50, 50, 200, 100);
  child->SetBounds(child_bounds);
  EXPECT_FALSE(child->IsVisible());
  EXPECT_EQ(0u, [[native_parent childWindows] count]);

  child->Show();
  EXPECT_TRUE(child->IsVisible());
  EXPECT_EQ(1u, [[native_parent childWindows] count]);
  EXPECT_EQ(child->GetNativeWindow(),
            [[native_parent childWindows] objectAtIndex:0]);
  EXPECT_EQ(native_parent, [child->GetNativeWindow() parentWindow]);

  // Only non-toplevel Widgets are positioned relative to the parent, so the
  // bounds set above should be in screen coordinates.
  EXPECT_EQ(child_bounds, child->GetWindowBoundsInScreen());

  // Removing the anchor_view from its view hierarchy is permitted. This should
  // not break the relationship between the two windows.
  [anchor_view removeFromSuperview];
  anchor_view.reset();
  EXPECT_EQ(native_parent, bridged_native_widget->parent()->GetNSWindow());

  // Closing the parent should close and destroy the child.
  EXPECT_FALSE(child_observer.widget_closed());
  [native_parent close];
  EXPECT_TRUE(child_observer.widget_closed());

  EXPECT_EQ(0u, [[native_parent childWindows] count]);
}

// Tests closing the last remaining NSWindow reference via -windowWillClose:.
// This is a regression test for http://crbug.com/616701.
TEST_F(NativeWidgetMacTest, NonWidgetParentLastReference) {
  bool child_dealloced = false;
  bool native_parent_dealloced = false;
  {
    base::mac::ScopedNSAutoreleasePool pool;
    TestNativeParentWindow* native_parent = MakeNativeParent();
    [native_parent setDeallocFlag:&native_parent_dealloced];

    NativeWidgetMacTestWindow* window;
    Widget::InitParams init_params =
        CreateParams(Widget::InitParams::TYPE_POPUP);
    init_params.parent = [native_parent_ contentView];
    init_params.bounds = gfx::Rect(0, 0, 100, 200);
    CreateWidgetWithTestWindow(init_params, &window);
    [window setDeallocFlag:&child_dealloced];
  }
  {
    // On 10.11, closing a weak reference on the parent window works, but older
    // versions of AppKit get upset if things are released inside -[NSWindow
    // close]. This test tries to establish a situation where the last reference
    // to the child window is released inside WidgetOwnerNSWindowAdapter::
    // OnWindowWillClose().
    base::mac::ScopedNSAutoreleasePool pool;
    [native_parent_.autorelease() close];
    EXPECT_TRUE(child_dealloced);
  }
  EXPECT_TRUE(native_parent_dealloced);
}

// Use Native APIs to query the tooltip text that would be shown once the
// tooltip delay had elapsed.
base::string16 TooltipTextForWidget(Widget* widget) {
  // For Mac, the actual location doesn't matter, since there is only one native
  // view and it fills the window. This just assumes the window is at least big
  // big enough for a constant coordinate to be within it.
  NSPoint point = NSMakePoint(30, 30);
  NSView* view = [widget->GetNativeView() hitTest:point];
  NSString* text =
      [view view:view stringForToolTip:0 point:point userData:nullptr];
  return base::SysNSStringToUTF16(text);
}

// Tests tooltips. The test doesn't wait for tooltips to appear. That is, the
// test assumes Cocoa calls stringForToolTip: at appropriate times and that,
// when a tooltip is already visible, changing it causes an update. These were
// tested manually by inserting a base::RunLoop.Run().
TEST_F(NativeWidgetMacTest, Tooltips) {
  Widget* widget = CreateTopLevelPlatformWidget();
  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  const base::string16 tooltip_back = base::ASCIIToUTF16("Back");
  const base::string16 tooltip_front = base::ASCIIToUTF16("Front");
  const base::string16 long_tooltip(2000, 'W');

  // Create a nested layout to test corner cases.
  LabelButton* back = new LabelButton(nullptr, base::string16());
  back->SetBounds(10, 10, 80, 80);
  widget->GetContentsView()->AddChildView(back);
  widget->Show();

  ui::test::EventGenerator event_generator(GetContext(),
                                           widget->GetNativeWindow());

  // Initially, there should be no tooltip.
  event_generator.MoveMouseTo(gfx::Point(50, 50));
  EXPECT_TRUE(TooltipTextForWidget(widget).empty());

  // Create a new button for the "front", and set the tooltip, but don't add it
  // to the view hierarchy yet.
  LabelButton* front = new LabelButton(nullptr, base::string16());
  front->SetBounds(20, 20, 40, 40);
  front->SetTooltipText(tooltip_front);

  // Changing the tooltip text shouldn't require an additional mousemove to take
  // effect.
  EXPECT_TRUE(TooltipTextForWidget(widget).empty());
  back->SetTooltipText(tooltip_back);
  EXPECT_EQ(tooltip_back, TooltipTextForWidget(widget));

  // Adding a new view under the mouse should also take immediate effect.
  back->AddChildView(front);
  EXPECT_EQ(tooltip_front, TooltipTextForWidget(widget));

  // A long tooltip will be wrapped by Cocoa, but the full string should appear.
  // Note that render widget hosts clip at 1024 to prevent DOS, but in toolkit-
  // views the UI is more trusted.
  front->SetTooltipText(long_tooltip);
  EXPECT_EQ(long_tooltip, TooltipTextForWidget(widget));

  // Move the mouse to a different view - tooltip should change.
  event_generator.MoveMouseTo(gfx::Point(15, 15));
  EXPECT_EQ(tooltip_back, TooltipTextForWidget(widget));

  // Move the mouse off of any view, tooltip should clear.
  event_generator.MoveMouseTo(gfx::Point(5, 5));
  EXPECT_TRUE(TooltipTextForWidget(widget).empty());

  widget->CloseNow();
}

namespace {

// Delegate to make Widgets of a provided ui::ModalType.
class ModalDialogDelegate : public DialogDelegateView {
 public:
  explicit ModalDialogDelegate(ui::ModalType modal_type)
      : modal_type_(modal_type) {}

  // WidgetDelegate:
  ui::ModalType GetModalType() const override { return modal_type_; }

 private:
  const ui::ModalType modal_type_;

  DISALLOW_COPY_AND_ASSIGN(ModalDialogDelegate);
};

// While in scope, waits for a call to a swizzled objective C method, then quits
// a nested run loop.
class ScopedSwizzleWaiter {
 public:
  explicit ScopedSwizzleWaiter(Class target)
      : swizzler_(target,
                  [TestStopAnimationWaiter class],
                  @selector(setWindowStateForEnd)) {
    DCHECK(!instance_);
    instance_ = this;
  }

  ~ScopedSwizzleWaiter() { instance_ = nullptr; }

  static IMP GetMethodAndMarkCalled() {
    return instance_->GetMethodInternal();
  }

  void WaitForMethod() {
    if (method_called_)
      return;

    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  bool method_called() const { return method_called_; }

 private:
  IMP GetMethodInternal() {
    DCHECK(!method_called_);
    method_called_ = true;
    if (run_loop_)
      run_loop_->Quit();
    return swizzler_.GetOriginalImplementation();
  }

  static ScopedSwizzleWaiter* instance_;

  base::mac::ScopedObjCClassSwizzler swizzler_;
  base::RunLoop* run_loop_ = nullptr;
  bool method_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScopedSwizzleWaiter);
};

ScopedSwizzleWaiter* ScopedSwizzleWaiter::instance_ = nullptr;

// Shows a modal widget and waits for the show animation to complete. Waiting is
// not compulsory (calling Close() while animating the show will cancel the show
// animation). However, testing with overlapping swizzlers is tricky.
Widget* ShowChildModalWidgetAndWait(NSWindow* native_parent) {
  Widget* modal_dialog_widget = views::DialogDelegate::CreateDialogWidget(
      new ModalDialogDelegate(ui::MODAL_TYPE_CHILD), nullptr,
      [native_parent contentView]);

  modal_dialog_widget->SetBounds(gfx::Rect(50, 50, 200, 150));
  EXPECT_FALSE(modal_dialog_widget->IsVisible());
  ScopedSwizzleWaiter show_waiter([ConstrainedWindowAnimationShow class]);

  modal_dialog_widget->Show();
  // Visible immediately (although it animates from transparent).
  EXPECT_TRUE(modal_dialog_widget->IsVisible());

  // Run the animation.
  show_waiter.WaitForMethod();
  EXPECT_TRUE(modal_dialog_widget->IsVisible());
  EXPECT_TRUE(show_waiter.method_called());
  return modal_dialog_widget;
}

}  // namespace

// Tests object lifetime for the show/hide animations used for child-modal
// windows. Parents the dialog off a native parent window (not a views::Widget).
TEST_F(NativeWidgetMacTest, NativeWindowChildModalShowHide) {
  NSWindow* native_parent = MakeNativeParent();
  {
    Widget* modal_dialog_widget = ShowChildModalWidgetAndWait(native_parent);
    TestWidgetObserver widget_observer(modal_dialog_widget);

    ScopedSwizzleWaiter hide_waiter([ConstrainedWindowAnimationHide class]);
    EXPECT_TRUE(modal_dialog_widget->IsVisible());
    EXPECT_FALSE(widget_observer.widget_closed());

    // Widget::Close() is always asynchronous, so we can check that the widget
    // is initially visible, but then it's destroyed.
    modal_dialog_widget->Close();
    EXPECT_TRUE(modal_dialog_widget->IsVisible());
    EXPECT_FALSE(hide_waiter.method_called());
    EXPECT_FALSE(widget_observer.widget_closed());

    // Wait for a hide to finish.
    hide_waiter.WaitForMethod();
    EXPECT_TRUE(hide_waiter.method_called());

    // The animation finishing should also mean it has closed the window.
    EXPECT_TRUE(widget_observer.widget_closed());
  }

  {
    // Make a new dialog to test another lifetime flow.
    Widget* modal_dialog_widget = ShowChildModalWidgetAndWait(native_parent);
    TestWidgetObserver widget_observer(modal_dialog_widget);

    // Start an asynchronous close as above.
    ScopedSwizzleWaiter hide_waiter([ConstrainedWindowAnimationHide class]);
    modal_dialog_widget->Close();
    EXPECT_FALSE(widget_observer.widget_closed());
    EXPECT_FALSE(hide_waiter.method_called());

    // Now close the _parent_ window to force a synchronous close of the child.
    [native_parent close];

    // Widget is destroyed immediately. No longer paints, but the animation is
    // still running.
    EXPECT_TRUE(widget_observer.widget_closed());
    EXPECT_FALSE(hide_waiter.method_called());

    // Wait for the hide again. It will call close on its retained copy of the
    // child NSWindow, but that's fine since all the C++ objects are detached.
    hide_waiter.WaitForMethod();
    EXPECT_TRUE(hide_waiter.method_called());
  }
}

// Tests behavior of window-modal dialogs, displayed as sheets.
TEST_F(NativeWidgetMacTest, WindowModalSheet) {
  NSWindow* native_parent =
      MakeNativeParentWithStyle(NSClosableWindowMask | NSTitledWindowMask);

  Widget* sheet_widget = views::DialogDelegate::CreateDialogWidget(
      new ModalDialogDelegate(ui::MODAL_TYPE_WINDOW), nullptr,
      [native_parent contentView]);

  // Retain, to run checks after the Widget is torn down.
  base::scoped_nsobject<NSWindow> sheet_window(
      [sheet_widget->GetNativeWindow() retain]);

  // Although there is no titlebar displayed, sheets need NSTitledWindowMask in
  // order to properly engage window-modal behavior in AppKit.
  EXPECT_EQ(NSTitledWindowMask, [sheet_window styleMask]);

  sheet_widget->SetBounds(gfx::Rect(50, 50, 200, 150));
  EXPECT_FALSE(sheet_widget->IsVisible());
  EXPECT_FALSE(sheet_widget->GetLayer()->IsDrawn());

  NSButton* parent_close_button =
      [native_parent standardWindowButton:NSWindowCloseButton];
  EXPECT_TRUE(parent_close_button);
  EXPECT_TRUE([parent_close_button isEnabled]);

  bool did_observe = false;
  bool* did_observe_ptr = &did_observe;
  id observer = [[NSNotificationCenter defaultCenter]
      addObserverForName:NSWindowWillBeginSheetNotification
                  object:native_parent
                   queue:nil
              usingBlock:^(NSNotification* note) {
                // Ensure that before the sheet runs, the window contents would
                // be drawn.
                EXPECT_TRUE(sheet_widget->IsVisible());
                EXPECT_TRUE(sheet_widget->GetLayer()->IsDrawn());
                *did_observe_ptr = true;
              }];

  sheet_widget->Show();  // Should run the above block, then animate the sheet.
  EXPECT_TRUE(did_observe);
  [[NSNotificationCenter defaultCenter] removeObserver:observer];

  // Modal, so the close button in the parent window should get disabled.
  EXPECT_FALSE([parent_close_button isEnabled]);

  // Trigger the close. Don't use CloseNow, since that tears down the UI before
  // the close sheet animation gets a chance to run (so it's banned).
  sheet_widget->Close();
  EXPECT_TRUE(sheet_widget->IsVisible());

  did_observe = false;

  // Experimentally (on 10.10), this notification is posted from within the
  // -[NSWindow orderOut:] call that is triggered from -[ViewsNSWindowDelegate
  // sheetDidEnd:]. |sheet_widget| will be destroyed next, so it's still safe to
  // use in the block. However, since the orderOut just happened, it's not very
  // interesting.
  observer = [[NSNotificationCenter defaultCenter]
      addObserverForName:NSWindowDidEndSheetNotification
                  object:native_parent
                   queue:nil
              usingBlock:^(NSNotification* note) {
                EXPECT_TRUE([sheet_window delegate]);
                *did_observe_ptr = true;
              }];

  // Pump in order to trigger -[NSWindow endSheet:..], which will block while
  // the animation runs, then delete |sheet_widget|.
  TestWidgetObserver widget_observer(sheet_widget);
  EXPECT_TRUE([sheet_window delegate]);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE([sheet_window delegate]);

  EXPECT_TRUE(did_observe);  // Also ensures the Close() actually uses sheets.
  [[NSNotificationCenter defaultCenter] removeObserver:observer];

  EXPECT_TRUE(widget_observer.widget_closed());
  EXPECT_TRUE([parent_close_button isEnabled]);
}

// Test calls to Widget::ReparentNativeView() that result in a no-op on Mac.
// Tests with both native and non-native parents.
TEST_F(NativeWidgetMacTest, NoopReparentNativeView) {
  NSWindow* parent = MakeNativeParent();
  Widget* dialog = views::DialogDelegate::CreateDialogWidget(
      new DialogDelegateView, nullptr, [parent contentView]);
  BridgedNativeWidget* bridge =
      NativeWidgetMac::GetBridgeForNativeWindow(dialog->GetNativeWindow());

  EXPECT_EQ(bridge->parent()->GetNSWindow(), parent);
  Widget::ReparentNativeView(dialog->GetNativeView(), [parent contentView]);
  EXPECT_EQ(bridge->parent()->GetNSWindow(), parent);

  [parent close];

  Widget* parent_widget = CreateNativeDesktopWidget();
  parent = parent_widget->GetNativeWindow();
  dialog = views::DialogDelegate::CreateDialogWidget(
      new DialogDelegateView, nullptr, [parent contentView]);
  bridge = NativeWidgetMac::GetBridgeForNativeWindow(dialog->GetNativeWindow());

  EXPECT_EQ(bridge->parent()->GetNSWindow(), parent);
  Widget::ReparentNativeView(dialog->GetNativeView(), [parent contentView]);
  EXPECT_EQ(bridge->parent()->GetNSWindow(), parent);

  parent_widget->CloseNow();
}

// Attaches a child window to |parent| that checks its parent's delegate is
// cleared when the child is destroyed. This assumes the child is destroyed via
// destruction of its parent.
class ParentCloseMonitor : public WidgetObserver {
 public:
  explicit ParentCloseMonitor(Widget* parent) {
    Widget* child = new Widget();
    child->AddObserver(this);
    Widget::InitParams init_params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    init_params.parent = parent->GetNativeView();
    init_params.bounds = gfx::Rect(100, 100, 100, 100);
    init_params.native_widget =
        CreatePlatformNativeWidgetImpl(init_params, child, kStubCapture,
                                       nullptr);
    child->Init(init_params);
    child->Show();

    // NSWindow parent/child relationship should be established on Show() and
    // the parent should have a delegate. Retain the parent since it can't be
    // retrieved from the child while it is being destroyed.
    parent_nswindow_.reset([[child->GetNativeWindow() parentWindow] retain]);
    EXPECT_TRUE(parent_nswindow_);
    EXPECT_TRUE([parent_nswindow_ delegate]);
  }

  ~ParentCloseMonitor() override {
    EXPECT_TRUE(child_closed_);  // Otherwise the observer wasn't removed.
  }

  void OnWidgetDestroying(Widget* child) override {
    // Upon a parent-triggered close, the NSWindow relationship will already be
    // removed. The parent should still be open (children are always closed
    // first), but not have a delegate (since it is being torn down).
    EXPECT_FALSE([child->GetNativeWindow() parentWindow]);
    EXPECT_TRUE([parent_nswindow_ isVisible]);
    EXPECT_FALSE([parent_nswindow_ delegate]);

    EXPECT_FALSE(child_closed_);
    child->RemoveObserver(this);
    child_closed_ = true;
  }

  bool child_closed() const { return child_closed_; }

 private:
  base::scoped_nsobject<NSWindow> parent_nswindow_;
  bool child_closed_ = false;

  DISALLOW_COPY_AND_ASSIGN(ParentCloseMonitor);
};

// Ensures when a parent window is destroyed, and triggers its child windows to
// be closed, that the child windows (via AppKit) do not attempt to call back
// into the parent, whilst it's in the process of being destroyed.
TEST_F(NativeWidgetMacTest, NoParentDelegateDuringTeardown) {
  // First test "normal" windows and AppKit close.
  {
    Widget* parent = CreateTopLevelPlatformWidget();
    parent->SetBounds(gfx::Rect(100, 100, 300, 200));
    parent->Show();
    ParentCloseMonitor monitor(parent);
    [parent->GetNativeWindow() close];
    EXPECT_TRUE(monitor.child_closed());
  }

  // Test the Widget::CloseNow() flow.
  {
    Widget* parent = CreateTopLevelPlatformWidget();
    parent->SetBounds(gfx::Rect(100, 100, 300, 200));
    parent->Show();
    ParentCloseMonitor monitor(parent);
    parent->CloseNow();
    EXPECT_TRUE(monitor.child_closed());
  }

  // Test the WIDGET_OWNS_NATIVE_WIDGET flow.
  {
    std::unique_ptr<Widget> parent(new Widget);
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(100, 100, 300, 200);
    parent->Init(params);
    parent->Show();

    ParentCloseMonitor monitor(parent.get());
    parent.reset();
    EXPECT_TRUE(monitor.child_closed());
  }
}

// Tests Cocoa properties that should be given to particular widget types.
TEST_F(NativeWidgetMacTest, NativeProperties) {
  // Create a regular widget (TYPE_WINDOW).
  Widget* regular_widget = CreateNativeDesktopWidget();
  EXPECT_TRUE([regular_widget->GetNativeWindow() canBecomeKeyWindow]);
  EXPECT_TRUE([regular_widget->GetNativeWindow() canBecomeMainWindow]);

  // Disabling activation should prevent key and main status.
  regular_widget->widget_delegate()->set_can_activate(false);
  EXPECT_FALSE([regular_widget->GetNativeWindow() canBecomeKeyWindow]);
  EXPECT_FALSE([regular_widget->GetNativeWindow() canBecomeMainWindow]);

  // Create a dialog widget (also TYPE_WINDOW), but with a DialogDelegate.
  Widget* dialog_widget = views::DialogDelegate::CreateDialogWidget(
      new ModalDialogDelegate(ui::MODAL_TYPE_CHILD), nullptr,
      regular_widget->GetNativeView());
  EXPECT_TRUE([dialog_widget->GetNativeWindow() canBecomeKeyWindow]);
  // Dialogs shouldn't take main status away from their parent.
  EXPECT_FALSE([dialog_widget->GetNativeWindow() canBecomeMainWindow]);

  // Create a bubble widget with a parent: also shouldn't get main.
  BubbleDialogDelegateView* bubble_view = new SimpleBubbleView();
  bubble_view->set_parent_window(regular_widget->GetNativeView());
  Widget* bubble_widget = BubbleDialogDelegateView::CreateBubble(bubble_view);
  EXPECT_TRUE([bubble_widget->GetNativeWindow() canBecomeKeyWindow]);
  EXPECT_FALSE([bubble_widget->GetNativeWindow() canBecomeMainWindow]);

  // But a bubble without a parent should still be able to become main.
  Widget* toplevel_bubble_widget =
      BubbleDialogDelegateView::CreateBubble(new SimpleBubbleView());
  EXPECT_TRUE([toplevel_bubble_widget->GetNativeWindow() canBecomeKeyWindow]);
  EXPECT_TRUE([toplevel_bubble_widget->GetNativeWindow() canBecomeMainWindow]);

  toplevel_bubble_widget->CloseNow();
  regular_widget->CloseNow();
}

NSData* WindowContentsAsTIFF(NSWindow* window) {
  NSView* frame_view = [[window contentView] superview];
  EXPECT_TRUE(frame_view);

  // Inset to mask off left and right edges which vary in HighDPI.
  NSRect bounds = NSInsetRect([frame_view bounds], 4, 0);

  // On 10.6, the grippy changes appearance slightly when painted the second
  // time in a textured window. Since this test cares about the window title,
  // cut off the bottom of the window.
  bounds.size.height -= 40;
  bounds.origin.y += 40;

  NSBitmapImageRep* bitmap =
      [frame_view bitmapImageRepForCachingDisplayInRect:bounds];
  EXPECT_TRUE(bitmap);

  [frame_view cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
  NSData* tiff = [bitmap TIFFRepresentation];
  EXPECT_TRUE(tiff);
  return tiff;
}

class CustomTitleWidgetDelegate : public WidgetDelegate {
 public:
  CustomTitleWidgetDelegate(Widget* widget)
      : widget_(widget), should_show_title_(true) {}

  void set_title(const base::string16& title) { title_ = title; }
  void set_should_show_title(bool show) { should_show_title_ = show; }

  // WidgetDelegate:
  base::string16 GetWindowTitle() const override { return title_; }
  bool ShouldShowWindowTitle() const override { return should_show_title_; }
  Widget* GetWidget() override { return widget_; };
  const Widget* GetWidget() const override { return widget_; };

 private:
  Widget* widget_;
  base::string16 title_;
  bool should_show_title_;

  DISALLOW_COPY_AND_ASSIGN(CustomTitleWidgetDelegate);
};

// Test that undocumented title-hiding API we're using does the job.
TEST_F(NativeWidgetMacTest, DoesHideTitle) {
  // Same as CreateTopLevelPlatformWidget but with a custom delegate.
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  Widget* widget = new Widget;
  params.native_widget =
        CreatePlatformNativeWidgetImpl(params, widget, kStubCapture, nullptr);
  CustomTitleWidgetDelegate delegate(widget);
  params.delegate = &delegate;
  params.bounds = gfx::Rect(0, 0, 800, 600);
  widget->Init(params);
  widget->Show();

  NSWindow* ns_window = widget->GetNativeWindow();
  // Disable color correction so we can read unmodified values from the bitmap.
  [ns_window setColorSpace:[NSColorSpace sRGBColorSpace]];

  EXPECT_EQ(base::string16(), delegate.GetWindowTitle());
  EXPECT_NSEQ(@"", [ns_window title]);
  NSData* empty_title_data = WindowContentsAsTIFF(ns_window);

  delegate.set_title(base::ASCIIToUTF16("This is a title"));
  widget->UpdateWindowTitle();
  NSData* this_title_data = WindowContentsAsTIFF(ns_window);

  // The default window with a title should look different from the
  // window with an empty title.
  EXPECT_FALSE([empty_title_data isEqualToData:this_title_data]);

  delegate.set_should_show_title(false);
  delegate.set_title(base::ASCIIToUTF16("This is another title"));
  widget->UpdateWindowTitle();
  NSData* hidden_title_data = WindowContentsAsTIFF(ns_window);

  // With our magic setting, the window with a title should look the
  // same as the window with an empty title.
  EXPECT_TRUE([ns_window _isTitleHidden]);
  EXPECT_TRUE([empty_title_data isEqualToData:hidden_title_data]);

  widget->CloseNow();
}

// Test calls to invalidate the shadow when composited frames arrive.
TEST_F(NativeWidgetMacTest, InvalidateShadow) {
  NativeWidgetMacTestWindow* window;
  const gfx::Rect rect(0, 0, 100, 200);
  Widget::InitParams init_params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  init_params.bounds = rect;
  Widget* widget = CreateWidgetWithTestWindow(init_params, &window);

  // Simulate the initial paint.
  BridgedNativeWidgetTestApi(window).SimulateFrameSwap(rect.size());

  // Default is an opaque window, so shadow doesn't need to be invalidated.
  EXPECT_EQ(0, [window invalidateShadowCount]);
  widget->CloseNow();

  init_params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
  widget = CreateWidgetWithTestWindow(init_params, &window);
  BridgedNativeWidgetTestApi test_api(window);

  // First paint on a translucent window needs to invalidate the shadow. Once.
  EXPECT_EQ(0, [window invalidateShadowCount]);
  test_api.SimulateFrameSwap(rect.size());
  EXPECT_EQ(1, [window invalidateShadowCount]);
  test_api.SimulateFrameSwap(rect.size());
  EXPECT_EQ(1, [window invalidateShadowCount]);

  // Resizing the window also needs to trigger a shadow invalidation.
  [window setContentSize:NSMakeSize(123, 456)];
  // A "late" frame swap at the old size should do nothing.
  test_api.SimulateFrameSwap(rect.size());
  EXPECT_EQ(1, [window invalidateShadowCount]);

  test_api.SimulateFrameSwap(gfx::Size(123, 456));
  EXPECT_EQ(2, [window invalidateShadowCount]);
  test_api.SimulateFrameSwap(gfx::Size(123, 456));
  EXPECT_EQ(2, [window invalidateShadowCount]);

  widget->CloseNow();
}

// Test the expected result of GetWorkAreaBoundsInScreen().
TEST_F(NativeWidgetMacTest, GetWorkAreaBoundsInScreen) {
  Widget widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

  // This is relative to the top-left of the primary screen, so unless the bot's
  // display is smaller than 400x300, the window will be wholly contained there.
  params.bounds = gfx::Rect(100, 100, 300, 200);
  widget.Init(params);
  widget.Show();
  NSRect expected = [[[NSScreen screens] firstObject] visibleFrame];
  NSRect actual = gfx::ScreenRectToNSRect(widget.GetWorkAreaBoundsInScreen());
  EXPECT_FALSE(NSIsEmptyRect(actual));
  EXPECT_NSEQ(expected, actual);

  [widget.GetNativeWindow() close];
  actual = gfx::ScreenRectToNSRect(widget.GetWorkAreaBoundsInScreen());
  EXPECT_TRUE(NSIsEmptyRect(actual));
}

// Test that Widget opacity can be changed.
TEST_F(NativeWidgetMacTest, ChangeOpacity) {
  Widget* widget = CreateTopLevelPlatformWidget();
  NSWindow* ns_window = widget->GetNativeWindow();

  CGFloat old_opacity = [ns_window alphaValue];
  widget->SetOpacity(.7f);
  EXPECT_NE(old_opacity, [ns_window alphaValue]);
  EXPECT_DOUBLE_EQ(.7f, [ns_window alphaValue]);

  widget->CloseNow();
}

// Test that NativeWidgetMac::SchedulePaintInRect correctly passes the dirtyRect
// parameter to BridgedContentView::drawRect, for a titled window (window with a
// toolbar).
TEST_F(NativeWidgetMacTest, SchedulePaintInRect_Titled) {
  Widget* widget = CreateTopLevelPlatformWidget();

  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  // Setup the mock content view for the NSWindow, so that we can intercept
  // drawRect.
  NSWindow* window = widget->GetNativeWindow();
  base::scoped_nsobject<MockBridgedView> mock_bridged_view(
      [[MockBridgedView alloc] init]);
  [window setContentView:mock_bridged_view];

  // Ensure the initial draw of the window is done.
  base::RunLoop().RunUntilIdle();

  // Add a dummy view to the widget. This will cause SchedulePaint to be called
  // on the dummy view.
  View* dummy_view = new View();
  gfx::Rect dummy_bounds(25, 30, 10, 15);
  dummy_view->SetBoundsRect(dummy_bounds);
  // Reset drawRect count.
  [mock_bridged_view setDrawRectCount:0];
  widget->GetContentsView()->AddChildView(dummy_view);

  // SchedulePaint is asyncronous. Wait for drawRect: to be called.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, [mock_bridged_view drawRectCount]);
  int client_area_height = widget->GetClientAreaBoundsInScreen().height();
  // These are expected dummy_view bounds in AppKit coordinate system. The y
  // coordinate of rect origin is calculated as:
  // client_area_height - 30 (dummy_view's y coordinate) - 15 (dummy view's
  // height).
  gfx::Rect expected_appkit_bounds(25, client_area_height - 45, 10, 15);
  EXPECT_NSEQ(expected_appkit_bounds.ToCGRect(),
              [mock_bridged_view lastDirtyRect]);
  widget->CloseNow();
}

// Test that NativeWidgetMac::SchedulePaintInRect correctly passes the dirtyRect
// parameter to BridgedContentView::drawRect, for a borderless window.
TEST_F(NativeWidgetMacTest, SchedulePaintInRect_Borderless) {
  Widget* widget = CreateTopLevelFramelessPlatformWidget();

  gfx::Rect screen_rect(50, 50, 100, 100);
  widget->SetBounds(screen_rect);

  // Setup the mock content view for the NSWindow, so that we can intercept
  // drawRect.
  NSWindow* window = widget->GetNativeWindow();
  base::scoped_nsobject<MockBridgedView> mock_bridged_view(
      [[MockBridgedView alloc] init]);
  [window setContentView:mock_bridged_view];

  // Ensure the initial draw of the window is done.
  base::RunLoop().RunUntilIdle();

  // Add a dummy view to the widget. This will cause SchedulePaint to be called
  // on the dummy view.
  View* dummy_view = new View();
  gfx::Rect dummy_bounds(25, 30, 10, 15);
  dummy_view->SetBoundsRect(dummy_bounds);
  // Reset drawRect count.
  [mock_bridged_view setDrawRectCount:0];
  widget->GetRootView()->AddChildView(dummy_view);

  // SchedulePaint is asyncronous. Wait for drawRect: to be called.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1u, [mock_bridged_view drawRectCount]);
  // These are expected dummy_view bounds in AppKit coordinate system. The y
  // coordinate of rect origin is calculated as:
  // 100(client area height) - 30 (dummy_view's y coordinate) - 15 (dummy view's
  // height).
  gfx::Rect expected_appkit_bounds(25, 55, 10, 15);
  EXPECT_NSEQ(expected_appkit_bounds.ToCGRect(),
              [mock_bridged_view lastDirtyRect]);
  widget->CloseNow();
}

// Ensure traversing NSView focus correctly updates the views::FocusManager.
TEST_F(NativeWidgetMacTest, ChangeFocusOnChangeFirstResponder) {
  Widget* widget = CreateTopLevelPlatformWidget();
  widget->GetRootView()->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  widget->Show();

  base::scoped_nsobject<NSView> child_view([[FocusableTestNSView alloc]
      initWithFrame:[widget->GetNativeView() bounds]]);
  [widget->GetNativeView() addSubview:child_view];
  EXPECT_TRUE([child_view acceptsFirstResponder]);
  EXPECT_TRUE(widget->GetRootView()->IsFocusable());

  FocusManager* manager = widget->GetFocusManager();
  manager->SetFocusedView(widget->GetRootView());
  EXPECT_EQ(manager->GetFocusedView(), widget->GetRootView());

  [widget->GetNativeWindow() makeFirstResponder:child_view];
  EXPECT_FALSE(manager->GetFocusedView());

  [widget->GetNativeWindow() makeFirstResponder:widget->GetNativeView()];
  EXPECT_EQ(manager->GetFocusedView(), widget->GetRootView());

  widget->CloseNow();
}

// Test class for Full Keyboard Access related tests.
class NativeWidgetMacFullKeyboardAccessTest : public NativeWidgetMacTest {
 public:
  NativeWidgetMacFullKeyboardAccessTest() {}

 protected:
  // testing::Test:
  void SetUp() override {
    NativeWidgetMacTest::SetUp();

    widget_ = CreateTopLevelPlatformWidget();
    bridge_ =
        NativeWidgetMac::GetBridgeForNativeWindow(widget_->GetNativeWindow());
    fake_full_keyboard_access_ =
        ui::test::ScopedFakeFullKeyboardAccess::GetInstance();
    DCHECK(fake_full_keyboard_access_);
    widget_->Show();
  }

  void TearDown() override {
    widget_->CloseNow();
    NativeWidgetMacTest::TearDown();
  }

  Widget* widget_ = nullptr;
  BridgedNativeWidget* bridge_ = nullptr;
  ui::test::ScopedFakeFullKeyboardAccess* fake_full_keyboard_access_ = nullptr;
};

// Test that updateFullKeyboardAccess method on BridgedContentView correctly
// sets the keyboard accessibility mode on the associated focus manager.
TEST_F(NativeWidgetMacFullKeyboardAccessTest, FullKeyboardToggle) {
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());
  fake_full_keyboard_access_->set_full_keyboard_access_state(false);
  [bridge_->ns_view() updateFullKeyboardAccess];
  EXPECT_FALSE(widget_->GetFocusManager()->keyboard_accessible());
  fake_full_keyboard_access_->set_full_keyboard_access_state(true);
  [bridge_->ns_view() updateFullKeyboardAccess];
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());
}

// Test that a Widget's associated FocusManager is initialized with the correct
// keyboard accessibility value.
TEST_F(NativeWidgetMacFullKeyboardAccessTest, Initialization) {
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());

  fake_full_keyboard_access_->set_full_keyboard_access_state(false);
  Widget* widget2 = CreateTopLevelPlatformWidget();
  EXPECT_FALSE(widget2->GetFocusManager()->keyboard_accessible());
  widget2->CloseNow();
}

// Test that the correct keyboard accessibility mode is set when the window
// becomes active.
TEST_F(NativeWidgetMacFullKeyboardAccessTest, Activation) {
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());

  widget_->Hide();
  fake_full_keyboard_access_->set_full_keyboard_access_state(false);
  // [bridge_->ns_view() updateFullKeyboardAccess] is not explicitly called
  // since we may not receive full keyboard access toggle notifications when our
  // application is inactive.

  widget_->Show();
  EXPECT_FALSE(widget_->GetFocusManager()->keyboard_accessible());

  widget_->Hide();
  fake_full_keyboard_access_->set_full_keyboard_access_state(true);

  widget_->Show();
  EXPECT_TRUE(widget_->GetFocusManager()->keyboard_accessible());
}

class NativeWidgetMacViewsOrderTest : public WidgetTest {
 public:
  NativeWidgetMacViewsOrderTest() {}

 protected:
  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();

    widget_ = CreateTopLevelPlatformWidget();

    ASSERT_EQ(1u, [[widget_->GetNativeView() subviews] count]);
    compositor_view_ = [[widget_->GetNativeView() subviews] firstObject];

    native_host_parent_ = new View();
    widget_->GetContentsView()->AddChildView(native_host_parent_);

    const int kNativeViewCount = 3;
    for (int i = 0; i < kNativeViewCount; ++i) {
      std::unique_ptr<NativeHostHolder> holder(new NativeHostHolder());
      native_host_parent_->AddChildView(holder->host());
      holder->AttachNativeView();
      hosts_.push_back(std::move(holder));
    }
    EXPECT_EQ(kNativeViewCount, native_host_parent_->child_count());
    EXPECT_TRUE(([[widget_->GetNativeView() subviews] isEqualToArray:@[
      compositor_view_, hosts_[0]->view(), hosts_[1]->view(), hosts_[2]->view()
    ]]));
  }

  void TearDown() override {
    widget_->CloseNow();
    WidgetTest::TearDown();
  }

  NSView* GetContentNativeView() { return widget_->GetNativeView(); }

  Widget* widget_ = nullptr;
  View* native_host_parent_ = nullptr;
  NSView* compositor_view_ = nil;
  std::vector<std::unique_ptr<NativeHostHolder>> hosts_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeWidgetMacViewsOrderTest);
};

// Test that NativeViewHost::Attach()/Detach() method saves the NativeView
// z-order.
TEST_F(NativeWidgetMacViewsOrderTest, NativeViewAttached) {
  hosts_[1]->Detach();
  EXPECT_TRUE(([[GetContentNativeView() subviews] isEqualToArray:@[
    compositor_view_, hosts_[0]->view(), hosts_[2]->view()
  ]]));

  hosts_[1]->AttachNativeView();
  EXPECT_TRUE(([[GetContentNativeView() subviews] isEqualToArray:@[
    compositor_view_, hosts_[0]->view(), hosts_[1]->view(),
    hosts_[2]->view()
  ]]));
}

// Tests that NativeViews order changes according to views::View hierarchy.
TEST_F(NativeWidgetMacViewsOrderTest, ReorderViews) {
  native_host_parent_->ReorderChildView(hosts_[2]->host(), 1);
  EXPECT_TRUE(([[GetContentNativeView() subviews] isEqualToArray:@[
    compositor_view_, hosts_[0]->view(), hosts_[2]->view(),
    hosts_[1]->view()
  ]]));

  native_host_parent_->RemoveChildView(hosts_[2]->host());
  EXPECT_TRUE(([[GetContentNativeView() subviews] isEqualToArray:@[
    compositor_view_, hosts_[0]->view(), hosts_[1]->view()
  ]]));

  View* new_parent = new View();
  native_host_parent_->RemoveChildView(hosts_[1]->host());
  native_host_parent_->AddChildView(new_parent);
  new_parent->AddChildView(hosts_[1]->host());
  new_parent->AddChildView(hosts_[2]->host());
  EXPECT_TRUE(([[GetContentNativeView() subviews] isEqualToArray:@[
    compositor_view_, hosts_[0]->view(), hosts_[1]->view(),
    hosts_[2]->view()
  ]]));

  native_host_parent_->ReorderChildView(new_parent, 0);
  EXPECT_TRUE(([[GetContentNativeView() subviews] isEqualToArray:@[
    compositor_view_, hosts_[1]->view(), hosts_[2]->view(),
    hosts_[0]->view()
  ]]));
}

// Test that unassociated native views stay on top after reordering.
TEST_F(NativeWidgetMacViewsOrderTest, UnassociatedViewsIsAbove) {
  base::scoped_nsobject<NSView> child_view([[NSView alloc] init]);
  [GetContentNativeView() addSubview:child_view];
  EXPECT_TRUE(([[GetContentNativeView() subviews] isEqualToArray:@[
    compositor_view_, hosts_[0]->view(), hosts_[1]->view(),
    hosts_[2]->view(), child_view
  ]]));

  native_host_parent_->ReorderChildView(hosts_[2]->host(), 1);
  EXPECT_TRUE(([[GetContentNativeView() subviews] isEqualToArray:@[
    compositor_view_, hosts_[0]->view(), hosts_[2]->view(),
    hosts_[1]->view(), child_view
  ]]));
}

}  // namespace test
}  // namespace views

@implementation TestStopAnimationWaiter
- (void)setWindowStateForEnd {
  views::test::ScopedSwizzleWaiter::GetMethodAndMarkCalled()(self, _cmd);
}
@end

@implementation NativeWidgetMacTestWindow

@synthesize invalidateShadowCount = invalidateShadowCount_;
@synthesize deallocFlag = deallocFlag_;

- (void)dealloc {
  if (deallocFlag_) {
    DCHECK(!*deallocFlag_);
    *deallocFlag_ = true;
  }
  [super dealloc];
}

- (void)invalidateShadow {
  ++invalidateShadowCount_;
  [super invalidateShadow];
}

@end

@implementation MockBridgedView

@synthesize drawRectCount = drawRectCount_;
@synthesize lastDirtyRect = lastDirtyRect_;

- (void)drawRect:(NSRect)dirtyRect {
  ++drawRectCount_;
  lastDirtyRect_ = dirtyRect;
}

@end

@implementation FocusableTestNSView
- (BOOL)acceptsFirstResponder {
  return YES;
}
@end

@implementation TestNativeParentWindow {
  bool* deallocFlag_;
}

@synthesize deallocFlag = deallocFlag_;

- (void)dealloc {
  if (deallocFlag_) {
    DCHECK(!*deallocFlag_);
    *deallocFlag_ = true;
  }
  [super dealloc];
}

@end
