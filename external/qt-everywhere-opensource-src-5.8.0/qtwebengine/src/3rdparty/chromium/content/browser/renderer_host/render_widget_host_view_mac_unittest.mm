// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#include <Cocoa/Cocoa.h>
#include <stddef.h>
#include <stdint.h>
#include <tuple>

#include "base/command_line.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/compositor/test/no_transport_image_transport_factory.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view_mac_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_view_host.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/ocmock_extensions.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/latency_info.h"
#include "ui/events/test/cocoa_test_event_utils.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

// Helper class with methods used to mock -[NSEvent phase], used by
// |MockScrollWheelEventWithPhase()|.
@interface MockPhaseMethods : NSObject {
}

- (NSEventPhase)phaseBegan;
- (NSEventPhase)phaseChanged;
- (NSEventPhase)phaseEnded;
@end

@implementation MockPhaseMethods

- (NSEventPhase)phaseBegan {
  return NSEventPhaseBegan;
}
- (NSEventPhase)phaseChanged {
  return NSEventPhaseChanged;
}
- (NSEventPhase)phaseEnded {
  return NSEventPhaseEnded;
}

@end

@interface MockRenderWidgetHostViewMacDelegate
    : NSObject<RenderWidgetHostViewMacDelegate> {
  BOOL unhandledWheelEventReceived_;
}

@property(nonatomic) BOOL unhandledWheelEventReceived;

@end

@implementation MockRenderWidgetHostViewMacDelegate

@synthesize unhandledWheelEventReceived = unhandledWheelEventReceived_;

- (void)rendererHandledWheelEvent:(const blink::WebMouseWheelEvent&)event
                         consumed:(BOOL)consumed {
  if (!consumed)
    unhandledWheelEventReceived_ = true;
}
- (void)rendererHandledGestureScrollEvent:(const blink::WebGestureEvent&)event
                                 consumed:(BOOL)consumed {
  if (!consumed && event.type == blink::WebInputEvent::GestureScrollUpdate)
    unhandledWheelEventReceived_ = true;
}
- (void)touchesBeganWithEvent:(NSEvent*)event {}
- (void)touchesMovedWithEvent:(NSEvent*)event {}
- (void)touchesCancelledWithEvent:(NSEvent*)event {}
- (void)touchesEndedWithEvent:(NSEvent*)event {}
- (void)beginGestureWithEvent:(NSEvent*)event {}
- (void)endGestureWithEvent:(NSEvent*)event {}
- (BOOL)canRubberbandLeft:(NSView*)view {
  return true;
}
- (BOOL)canRubberbandRight:(NSView*)view {
  return true;
}

@end

namespace content {

namespace {

std::string GetInputMessageTypes(MockRenderProcessHost* process) {
  std::string result;
  for (size_t i = 0; i < process->sink().message_count(); ++i) {
    const IPC::Message* message = process->sink().GetMessageAt(i);
    EXPECT_EQ(InputMsg_HandleInputEvent::ID, message->type());
    InputMsg_HandleInputEvent::Param params;
    EXPECT_TRUE(InputMsg_HandleInputEvent::Read(message, &params));
    const blink::WebInputEvent* event = std::get<0>(params);
    if (i != 0)
      result += " ";
    result += WebInputEventTraits::GetName(event->type);
  }
  process->sink().ClearMessages();
  return result;
}

id MockGestureEvent(NSEventType type, double magnification) {
  id event = [OCMockObject mockForClass:[NSEvent class]];
  NSPoint locationInWindow = NSMakePoint(0, 0);
  CGFloat deltaX = 0;
  CGFloat deltaY = 0;
  NSTimeInterval timestamp = 1;
  NSUInteger modifierFlags = 0;

  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(type)] type];
  [(NSEvent*)[[event stub]
      andReturnValue:OCMOCK_VALUE(locationInWindow)] locationInWindow];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(deltaX)] deltaX];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(deltaY)] deltaY];
  [(NSEvent*)[[event stub] andReturnValue:OCMOCK_VALUE(timestamp)] timestamp];
  [(NSEvent*)[[event stub]
      andReturnValue:OCMOCK_VALUE(modifierFlags)] modifierFlags];
  [(NSEvent*)[[event stub]
      andReturnValue:OCMOCK_VALUE(magnification)] magnification];
  return event;
}

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate() {}
  ~MockRenderWidgetHostDelegate() override {}

 private:
  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void SelectAll() override {}
};

class MockRenderWidgetHostImpl : public RenderWidgetHostImpl {
 public:
  MockRenderWidgetHostImpl(RenderWidgetHostDelegate* delegate,
                           RenderProcessHost* process,
                           int32_t routing_id)
      : RenderWidgetHostImpl(delegate, process, routing_id, false) {
    set_renderer_initialized(true);
    lastWheelEventLatencyInfo = ui::LatencyInfo();
  }

  // Extracts |latency_info| and stores it in |lastWheelEventLatencyInfo|.
  void ForwardWheelEventWithLatencyInfo (
        const blink::WebMouseWheelEvent& wheel_event,
        const ui::LatencyInfo& ui_latency) override {
    RenderWidgetHostImpl::ForwardWheelEventWithLatencyInfo (
        wheel_event, ui_latency);
    lastWheelEventLatencyInfo = ui::LatencyInfo(ui_latency);
  }

  MOCK_METHOD0(Focus, void());
  MOCK_METHOD0(Blur, void());
  ui::LatencyInfo lastWheelEventLatencyInfo;
};

// Generates the |length| of composition rectangle vector and save them to
// |output|. It starts from |origin| and each rectangle contains |unit_size|.
void GenerateCompositionRectArray(const gfx::Point& origin,
                                  const gfx::Size& unit_size,
                                  size_t length,
                                  const std::vector<size_t>& break_points,
                                  std::vector<gfx::Rect>* output) {
  DCHECK(output);
  output->clear();

  std::queue<int> break_point_queue;
  for (size_t i = 0; i < break_points.size(); ++i)
    break_point_queue.push(break_points[i]);
  break_point_queue.push(length);
  size_t next_break_point = break_point_queue.front();
  break_point_queue.pop();

  gfx::Rect current_rect(origin, unit_size);
  for (size_t i = 0; i < length; ++i) {
    if (i == next_break_point) {
      current_rect.set_x(origin.x());
      current_rect.set_y(current_rect.y() + current_rect.height());
      next_break_point = break_point_queue.front();
      break_point_queue.pop();
    }
    output->push_back(current_rect);
    current_rect.set_x(current_rect.right());
  }
}

gfx::Rect GetExpectedRect(const gfx::Point& origin,
                          const gfx::Size& size,
                          const gfx::Range& range,
                          int line_no) {
  return gfx::Rect(
      origin.x() + range.start() * size.width(),
      origin.y() + line_no * size.height(),
      range.length() * size.width(),
      size.height());
}

// Returns NSScrollWheel event that mocks -phase. |mockPhaseSelector| should
// correspond to a method in |MockPhaseMethods| that returns the desired phase.
NSEvent* MockScrollWheelEventWithPhase(SEL mockPhaseSelector, int32_t delta) {
  CGEventRef cg_event =
      CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitLine, 1, delta, 0);
  CGEventTimestamp timestamp = 0;
  CGEventSetTimestamp(cg_event, timestamp);
  NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
  CFRelease(cg_event);
  method_setImplementation(
      class_getInstanceMethod([NSEvent class], @selector(phase)),
      [MockPhaseMethods instanceMethodForSelector:mockPhaseSelector]);
  return event;
}

}  // namespace

class RenderWidgetHostViewMacTest : public RenderViewHostImplTestHarness {
 public:
  RenderWidgetHostViewMacTest() : old_rwhv_(NULL), rwhv_mac_(NULL) {
    std::unique_ptr<base::SimpleTestTickClock> mock_clock(
        new base::SimpleTestTickClock());
    mock_clock->Advance(base::TimeDelta::FromMilliseconds(100));
    ui::SetEventTickClockForTesting(std::move(mock_clock));
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    // TestRenderViewHost's destruction assumes that its view is a
    // TestRenderWidgetHostView, so store its view and reset it back to the
    // stored view in |TearDown()|.
    old_rwhv_ = rvh()->GetWidget()->GetView();

    // Owned by its |cocoa_view()|, i.e. |rwhv_cocoa_|.
    rwhv_mac_ = new RenderWidgetHostViewMac(rvh()->GetWidget(), false);
    rwhv_cocoa_.reset([rwhv_mac_->cocoa_view() retain]);
  }
  void TearDown() override {
    // Make sure the rwhv_mac_ is gone once the superclass's |TearDown()| runs.
    rwhv_cocoa_.reset();
    RecycleAndWait();

    // See comment in SetUp().
    test_rvh()->GetWidget()->SetView(
        static_cast<RenderWidgetHostViewBase*>(old_rwhv_));

    RenderViewHostImplTestHarness::TearDown();
  }

  void RecycleAndWait() {
    pool_.Recycle();
    base::RunLoop().RunUntilIdle();
    pool_.Recycle();
  }

  void DestroyHostViewRetainCocoaView() {
    test_rvh()->GetWidget()->SetView(nullptr);
    rwhv_mac_->Destroy();
  }

 private:
  // This class isn't derived from PlatformTest.
  base::mac::ScopedNSAutoreleasePool pool_;

  RenderWidgetHostView* old_rwhv_;

 protected:
  RenderWidgetHostViewMac* rwhv_mac_;
  base::scoped_nsobject<RenderWidgetHostViewCocoa> rwhv_cocoa_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewMacTest);
};

TEST_F(RenderWidgetHostViewMacTest, Basic) {
}

TEST_F(RenderWidgetHostViewMacTest, AcceptsFirstResponder) {
  // The RWHVCocoa should normally accept first responder status.
  EXPECT_TRUE([rwhv_cocoa_.get() acceptsFirstResponder]);
}

TEST_F(RenderWidgetHostViewMacTest, Fullscreen) {
  rwhv_mac_->InitAsFullscreen(NULL);
  EXPECT_TRUE(rwhv_mac_->pepper_fullscreen_window());

  // Break the reference cycle caused by pepper_fullscreen_window() without
  // an <esc> event. See comment in
  // release_pepper_fullscreen_window_for_testing().
  rwhv_mac_->release_pepper_fullscreen_window_for_testing();
}

// Verify that escape key down in fullscreen mode suppressed the keyup event on
// the parent.
TEST_F(RenderWidgetHostViewMacTest, FullscreenCloseOnEscape) {
  // Use our own RWH since we need to destroy it.
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  int32_t routing_id = process_host->GetNextRoutingID();
  // Owned by its |cocoa_view()|.
  RenderWidgetHostImpl* rwh =
      new RenderWidgetHostImpl(&delegate, process_host, routing_id, false);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(rwh, false);

  view->InitAsFullscreen(rwhv_mac_);

  WindowedNotificationObserver observer(
      NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(rwh));
  EXPECT_FALSE([rwhv_mac_->cocoa_view() suppressNextEscapeKeyUp]);

  // Escape key down. Should close window and set |suppressNextEscapeKeyUp| on
  // the parent.
  [view->cocoa_view() keyEvent:
      cocoa_test_event_utils::KeyEventWithKeyCode(53, 27, NSKeyDown, 0)];
  observer.Wait();
  EXPECT_TRUE([rwhv_mac_->cocoa_view() suppressNextEscapeKeyUp]);

  // Escape key up on the parent should clear |suppressNextEscapeKeyUp|.
  [rwhv_mac_->cocoa_view() keyEvent:
      cocoa_test_event_utils::KeyEventWithKeyCode(53, 27, NSKeyUp, 0)];
  EXPECT_FALSE([rwhv_mac_->cocoa_view() suppressNextEscapeKeyUp]);
}

// Test that command accelerators which destroy the fullscreen window
// don't crash when forwarded via the window's responder machinery.
TEST_F(RenderWidgetHostViewMacTest, AcceleratorDestroy) {
  // Use our own RWH since we need to destroy it.
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  int32_t routing_id = process_host->GetNextRoutingID();
  // Owned by its |cocoa_view()|.
  RenderWidgetHostImpl* rwh =
      new RenderWidgetHostImpl(&delegate, process_host, routing_id, false);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(rwh, false);

  view->InitAsFullscreen(rwhv_mac_);

  WindowedNotificationObserver observer(
      NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      Source<RenderWidgetHost>(rwh));

  // Command-ESC will destroy the view, while the window is still in
  // |-performKeyEquivalent:|.  There are other cases where this can
  // happen, Command-ESC is the easiest to trigger.
  [[view->cocoa_view() window] performKeyEquivalent:
      cocoa_test_event_utils::KeyEventWithKeyCode(
          53, 27, NSKeyDown, NSCommandKeyMask)];
  observer.Wait();
}

// Test that NSEvent of private use character won't generate keypress event
// http://crbug.com/459089
TEST_F(RenderWidgetHostViewMacTest, FilterNonPrintableCharacter) {
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      new MockRenderWidgetHostImpl(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);

  // Simulate ctrl+F12, will produce a private use character but shouldn't
  // fire keypress event
  process_host->sink().ClearMessages();
  EXPECT_EQ(0U, process_host->sink().message_count());
  [view->cocoa_view() keyEvent:
      cocoa_test_event_utils::KeyEventWithKeyCode(
          0x7B, 0xF70F, NSKeyDown, NSControlKeyMask)];
  EXPECT_EQ(1U, process_host->sink().message_count());
  EXPECT_EQ("RawKeyDown", GetInputMessageTypes(process_host));

  // Simulate ctrl+delete, will produce a private use character but shouldn't
  // fire keypress event
  process_host->sink().ClearMessages();
  EXPECT_EQ(0U, process_host->sink().message_count());
  [view->cocoa_view() keyEvent:
      cocoa_test_event_utils::KeyEventWithKeyCode(
          0x2E, 0xF728, NSKeyDown, NSControlKeyMask)];
  EXPECT_EQ(1U, process_host->sink().message_count());
  EXPECT_EQ("RawKeyDown", GetInputMessageTypes(process_host));

  // Simulate a printable char, should generate keypress event
  process_host->sink().ClearMessages();
  EXPECT_EQ(0U, process_host->sink().message_count());
  [view->cocoa_view() keyEvent:
      cocoa_test_event_utils::KeyEventWithKeyCode(
          0x58, 'x', NSKeyDown, NSControlKeyMask)];
  EXPECT_EQ(2U, process_host->sink().message_count());
  EXPECT_EQ("RawKeyDown Char", GetInputMessageTypes(process_host));

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

// Test that invalid |keyCode| shouldn't generate key events.
// https://crbug.com/601964
TEST_F(RenderWidgetHostViewMacTest, InvalidKeyCode) {
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      new MockRenderWidgetHostImpl(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);

  // Simulate "Convert" key on JIS PC keyboard, will generate a |NSFlagsChanged|
  // NSEvent with |keyCode| == 0xFF.
  process_host->sink().ClearMessages();
  EXPECT_EQ(0U, process_host->sink().message_count());
  [view->cocoa_view() keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                                   0xFF, 0, NSFlagsChanged, 0)];
  EXPECT_EQ(0U, process_host->sink().message_count());

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacTest, GetFirstRectForCharacterRangeCaretCase) {
  const base::string16 kDummyString = base::UTF8ToUTF16("hogehoge");
  const size_t kDummyOffset = 0;

  gfx::Rect caret_rect(10, 11, 0, 10);
  gfx::Range caret_range(0, 0);
  ViewHostMsg_SelectionBounds_Params params;

  NSRect rect;
  NSRange actual_range;
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  params.anchor_rect = params.focus_rect = caret_rect;
  params.anchor_dir = params.focus_dir = blink::WebTextDirectionLeftToRight;
  rwhv_mac_->SelectionBoundsChanged(params);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        caret_range.ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_EQ(caret_rect, gfx::Rect(NSRectToCGRect(rect)));
  EXPECT_EQ(caret_range, gfx::Range(actual_range));

  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(0, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(1, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(2, 3).ToNSRange(),
        &rect,
        &actual_range));

  // Caret moved.
  caret_rect = gfx::Rect(20, 11, 0, 10);
  caret_range = gfx::Range(1, 1);
  params.anchor_rect = params.focus_rect = caret_rect;
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  rwhv_mac_->SelectionBoundsChanged(params);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        caret_range.ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_EQ(caret_rect, gfx::Rect(NSRectToCGRect(rect)));
  EXPECT_EQ(caret_range, gfx::Range(actual_range));

  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(0, 0).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(1, 2).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(2, 3).ToNSRange(),
        &rect,
        &actual_range));

  // No caret.
  caret_range = gfx::Range(1, 2);
  rwhv_mac_->SelectionChanged(kDummyString, kDummyOffset, caret_range);
  params.anchor_rect = caret_rect;
  params.focus_rect = gfx::Rect(30, 11, 0, 10);
  rwhv_mac_->SelectionBoundsChanged(params);
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(0, 0).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(0, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(1, 1).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(1, 2).ToNSRange(),
        &rect,
        &actual_range));
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
        gfx::Range(2, 2).ToNSRange(),
        &rect,
        &actual_range));
}

TEST_F(RenderWidgetHostViewMacTest, UpdateCompositionSinglelineCase) {
  const gfx::Point kOrigin(10, 11);
  const gfx::Size kBoundsUnit(10, 20);

  NSRect rect;
  // Make sure not crashing by passing NULL pointer instead of |actual_range|.
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 0).ToNSRange(),
      &rect,
      NULL));

  // If there are no update from renderer, always returned caret position.
  NSRange actual_range;
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 0).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 1).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 0).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 1).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 2).ToNSRange(),
      &rect,
      &actual_range));

  // If the firstRectForCharacterRange is failed in renderer, empty rect vector
  // is sent. Make sure this does not crash.
  rwhv_mac_->ImeCompositionRangeChanged(gfx::Range(10, 12),
                                        std::vector<gfx::Rect>());
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(10, 11).ToNSRange(),
      &rect,
      NULL));

  const int kCompositionLength = 10;
  std::vector<gfx::Rect> composition_bounds;
  const int kCompositionStart = 3;
  const gfx::Range kCompositionRange(kCompositionStart,
                                    kCompositionStart + kCompositionLength);
  GenerateCompositionRectArray(kOrigin,
                               kBoundsUnit,
                               kCompositionLength,
                               std::vector<size_t>(),
                               &composition_bounds);
  rwhv_mac_->ImeCompositionRangeChanged(kCompositionRange, composition_bounds);

  // Out of range requests will return caret position.
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(0, 0).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 1).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(1, 2).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(2, 2).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(13, 14).ToNSRange(),
      &rect,
      &actual_range));
  EXPECT_FALSE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
      gfx::Range(14, 15).ToNSRange(),
      &rect,
      &actual_range));

  for (int i = 0; i <= kCompositionLength; ++i) {
    for (int j = 0; j <= kCompositionLength - i; ++j) {
      const gfx::Range range(i, i + j);
      const gfx::Rect expected_rect = GetExpectedRect(kOrigin,
                                                      kBoundsUnit,
                                                      range,
                                                      0);
      const NSRange request_range = gfx::Range(
          kCompositionStart + range.start(),
          kCompositionStart + range.end()).ToNSRange();
      EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
            request_range,
            &rect,
            &actual_range));
      EXPECT_EQ(gfx::Range(request_range), gfx::Range(actual_range));
      EXPECT_EQ(expected_rect, gfx::Rect(NSRectToCGRect(rect)));

      // Make sure not crashing by passing NULL pointer instead of
      // |actual_range|.
      EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(
            request_range,
            &rect,
            NULL));
    }
  }
}

TEST_F(RenderWidgetHostViewMacTest, UpdateCompositionMultilineCase) {
  const gfx::Point kOrigin(10, 11);
  const gfx::Size kBoundsUnit(10, 20);
  NSRect rect;

  const int kCompositionLength = 30;
  std::vector<gfx::Rect> composition_bounds;
  const gfx::Range kCompositionRange(0, kCompositionLength);
  // Set breaking point at 10 and 20.
  std::vector<size_t> break_points;
  break_points.push_back(10);
  break_points.push_back(20);
  GenerateCompositionRectArray(kOrigin,
                               kBoundsUnit,
                               kCompositionLength,
                               break_points,
                               &composition_bounds);
  rwhv_mac_->ImeCompositionRangeChanged(kCompositionRange, composition_bounds);

  // Range doesn't contain line breaking point.
  gfx::Range range;
  range = gfx::Range(5, 8);
  NSRange actual_range;
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(range, gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, range, 0),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(15, 18);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(range, gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 8), 1),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(25, 28);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(range, gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 8), 2),
      gfx::Rect(NSRectToCGRect(rect)));

  // Range contains line breaking point.
  range = gfx::Range(8, 12);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(8, 10), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(8, 10), 0),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(18, 22);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(18, 20), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(8, 10), 1),
      gfx::Rect(NSRectToCGRect(rect)));

  // Start point is line breaking point.
  range = gfx::Range(10, 12);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(10, 12), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 2), 1),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(20, 22);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(20, 22), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 2), 2),
      gfx::Rect(NSRectToCGRect(rect)));

  // End point is line breaking point.
  range = gfx::Range(5, 10);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(5, 10), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 10), 0),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(15, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(15, 20), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(5, 10), 1),
      gfx::Rect(NSRectToCGRect(rect)));

  // Start and end point are same line breaking point.
  range = gfx::Range(10, 10);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(10, 10), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 0), 1),
      gfx::Rect(NSRectToCGRect(rect)));
  range = gfx::Range(20, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(20, 20), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 0), 2),
      gfx::Rect(NSRectToCGRect(rect)));

  // Start and end point are different line breaking point.
  range = gfx::Range(10, 20);
  EXPECT_TRUE(rwhv_mac_->GetCachedFirstRectForCharacterRange(range.ToNSRange(),
                                                             &rect,
                                                             &actual_range));
  EXPECT_EQ(gfx::Range(10, 20), gfx::Range(actual_range));
  EXPECT_EQ(
      GetExpectedRect(kOrigin, kBoundsUnit, gfx::Range(0, 10), 1),
      gfx::Rect(NSRectToCGRect(rect)));
}

// Check that events coming from AppKit via -[NSTextInputClient
// firstRectForCharacterRange:actualRange] are handled in a sane manner if they
// arrive after the C++ RenderWidgetHostView is destroyed.
TEST_F(RenderWidgetHostViewMacTest, CompositionEventAfterDestroy) {
  const gfx::Rect composition_bounds(0, 0, 30, 40);
  const gfx::Range range(0, 1);
  rwhv_mac_->ImeCompositionRangeChanged(
      range, std::vector<gfx::Rect>(1, composition_bounds));

  NSRange actual_range = NSMakeRange(0, 0);

  base::scoped_nsobject<CocoaTestHelperWindow> window(
      [[CocoaTestHelperWindow alloc] init]);
  [[window contentView] addSubview:rwhv_cocoa_];
  [rwhv_cocoa_ setFrame:NSMakeRect(0, 0, 400, 400)];

  NSRect rect = [rwhv_cocoa_ firstRectForCharacterRange:range.ToNSRange()
                                            actualRange:&actual_range];
  EXPECT_EQ(30, rect.size.width);
  EXPECT_EQ(40, rect.size.height);
  EXPECT_EQ(range, gfx::Range(actual_range));

  DestroyHostViewRetainCocoaView();
  actual_range = NSMakeRange(0, 0);
  rect = [rwhv_cocoa_ firstRectForCharacterRange:range.ToNSRange()
                                     actualRange:&actual_range];
  EXPECT_NSEQ(NSZeroRect, rect);
  EXPECT_EQ(gfx::Range(), gfx::Range(actual_range));
}

// Verify that |SetActive()| calls |RenderWidgetHostImpl::Blur()| and
// |RenderWidgetHostImp::Focus()|.
TEST_F(RenderWidgetHostViewMacTest, BlurAndFocusOnSetActive) {
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();

  // Owned by its |cocoa_view()|.
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* rwh =
      new MockRenderWidgetHostImpl(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(rwh, false);

  base::scoped_nsobject<CocoaTestHelperWindow> window(
      [[CocoaTestHelperWindow alloc] init]);
  [[window contentView] addSubview:view->cocoa_view()];

  EXPECT_CALL(*rwh, Focus());
  [window makeFirstResponder:view->cocoa_view()];
  testing::Mock::VerifyAndClearExpectations(rwh);

  EXPECT_CALL(*rwh, Blur());
  view->SetActive(false);
  testing::Mock::VerifyAndClearExpectations(rwh);

  EXPECT_CALL(*rwh, Focus());
  view->SetActive(true);
  testing::Mock::VerifyAndClearExpectations(rwh);

  // Unsetting first responder should blur.
  EXPECT_CALL(*rwh, Blur());
  [window makeFirstResponder:nil];
  testing::Mock::VerifyAndClearExpectations(rwh);

  // |SetActive()| shoud not focus if view is not first responder.
  EXPECT_CALL(*rwh, Focus()).Times(0);
  view->SetActive(true);
  testing::Mock::VerifyAndClearExpectations(rwh);

  // Clean up.
  rwh->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacTest, LastWheelEventLatencyInfoExists) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      new MockRenderWidgetHostImpl(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  process_host->sink().ClearMessages();

  // Send an initial wheel event for scrolling by 3 lines.
  // Verifies that ui::INPUT_EVENT_LATENCY_UI_COMPONENT is added
  // properly in scrollWheel function.
  NSEvent* wheelEvent1 = MockScrollWheelEventWithPhase(@selector(phaseBegan),3);
  [view->cocoa_view() scrollWheel:wheelEvent1];
  ui::LatencyInfo::LatencyComponent ui_component1;
  ASSERT_TRUE(host->lastWheelEventLatencyInfo.FindLatency(
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, &ui_component1) );

  // Send a wheel event with phaseEnded.
  // Verifies that ui::INPUT_EVENT_LATENCY_UI_COMPONENT is added
  // properly in shortCircuitScrollWheelEvent function which is called
  // in scrollWheel.
  NSEvent* wheelEvent2 = MockScrollWheelEventWithPhase(@selector(phaseEnded),0);
  [view->cocoa_view() scrollWheel:wheelEvent2];
  ui::LatencyInfo::LatencyComponent ui_component2;
  ASSERT_TRUE(host->lastWheelEventLatencyInfo.FindLatency(
      ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0, &ui_component2) );

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacTest, ScrollWheelEndEventDelivery) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      new MockRenderWidgetHostImpl(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  process_host->sink().ClearMessages();

  // Send an initial wheel event with NSEventPhaseBegan to the view.
  NSEvent* event1 = MockScrollWheelEventWithPhase(@selector(phaseBegan), 0);
  [view->cocoa_view() scrollWheel:event1];
  ASSERT_EQ(1U, process_host->sink().message_count());

  // Send an ACK for the first wheel event, so that the queue will be flushed.
  InputEventAck ack(blink::WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  std::unique_ptr<IPC::Message> response(
      new InputHostMsg_HandleInputEvent_ACK(0, ack));
  host->OnMessageReceived(*response);

  // Post the NSEventPhaseEnded wheel event to NSApp and check whether the
  // render view receives it.
  NSEvent* event2 = MockScrollWheelEventWithPhase(@selector(phaseEnded), 0);
  [NSApp postEvent:event2 atStart:NO];
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2U, process_host->sink().message_count());

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacTest,
       IgnoreEmptyUnhandledWheelEventWithWheelGestures) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      new MockRenderWidgetHostImpl(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  process_host->sink().ClearMessages();

  // Add a delegate to the view.
  base::scoped_nsobject<MockRenderWidgetHostViewMacDelegate> view_delegate(
      [[MockRenderWidgetHostViewMacDelegate alloc] init]);
  view->SetDelegate(view_delegate.get());

  // Send an initial wheel event for scrolling by 3 lines.
  NSEvent* event1 = MockScrollWheelEventWithPhase(@selector(phaseBegan), 3);
  [view->cocoa_view() scrollWheel:event1];
  ASSERT_EQ(1U, process_host->sink().message_count());
  process_host->sink().ClearMessages();

  // Indicate that the wheel event was unhandled.
  InputEventAck unhandled_ack(blink::WebInputEvent::MouseWheel,
                              INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  std::unique_ptr<IPC::Message> response1(
      new InputHostMsg_HandleInputEvent_ACK(0, unhandled_ack));
  host->OnMessageReceived(*response1);
  ASSERT_EQ(2U, process_host->sink().message_count());
  process_host->sink().ClearMessages();

  InputEventAck unhandled_scroll_ack(blink::WebInputEvent::GestureScrollUpdate,
                                     INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  std::unique_ptr<IPC::Message> scroll_response1(
      new InputHostMsg_HandleInputEvent_ACK(0, unhandled_scroll_ack));
  host->OnMessageReceived(*scroll_response1);

  // Check that the view delegate got an unhandled wheel event.
  ASSERT_EQ(YES, view_delegate.get().unhandledWheelEventReceived);
  view_delegate.get().unhandledWheelEventReceived = NO;

  // Send another wheel event, this time for scrolling by 0 lines (empty event).
  NSEvent* event2 = MockScrollWheelEventWithPhase(@selector(phaseChanged), 0);
  [view->cocoa_view() scrollWheel:event2];
  ASSERT_EQ(2U, process_host->sink().message_count());

  // Indicate that the wheel event was also unhandled.
  std::unique_ptr<IPC::Message> response2(
      new InputHostMsg_HandleInputEvent_ACK(0, unhandled_ack));
  host->OnMessageReceived(*response2);

  // Check that the view delegate ignored the empty unhandled wheel event.
  ASSERT_EQ(NO, view_delegate.get().unhandledWheelEventReceived);

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

// Tests that when view initiated shutdown happens (i.e. RWHView is deleted
// before RWH), we clean up properly and don't leak the RWHVGuest.
TEST_F(RenderWidgetHostViewMacTest, GuestViewDoesNotLeak) {
  MockRenderWidgetHostDelegate delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  int32_t routing_id = process_host->GetNextRoutingID();

  // Owned by its |cocoa_view()|.
  MockRenderWidgetHostImpl* rwh =
      new MockRenderWidgetHostImpl(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(rwh, true);

  // Add a delegate to the view.
  base::scoped_nsobject<MockRenderWidgetHostViewMacDelegate> view_delegate(
      [[MockRenderWidgetHostViewMacDelegate alloc] init]);
  view->SetDelegate(view_delegate.get());

  base::WeakPtr<RenderWidgetHostViewBase> guest_rwhv_weak =
      (new RenderWidgetHostViewGuest(
           rwh, NULL, view->GetWeakPtr()))->GetWeakPtr();

  // Remove the cocoa_view() so |view| also goes away before |rwh|.
  {
    base::scoped_nsobject<RenderWidgetHostViewCocoa> rwhv_cocoa;
    rwhv_cocoa.reset([view->cocoa_view() retain]);
  }
  RecycleAndWait();

  // Clean up.
  rwh->ShutdownAndDestroyWidget(true);

  // Let |guest_rwhv_weak| have a chance to delete itself.
  base::RunLoop run_loop;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_FALSE(guest_rwhv_weak.get());
}

// Tests setting background transparency. See also (disabled on Mac)
// RenderWidgetHostTest.Background. This test has some additional checks for
// Mac.
TEST_F(RenderWidgetHostViewMacTest, Background) {
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      new MockRenderWidgetHostImpl(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);

  EXPECT_TRUE(view->GetBackgroundOpaque());
  EXPECT_TRUE([view->cocoa_view() isOpaque]);

  view->SetBackgroundColor(SK_ColorTRANSPARENT);
  EXPECT_FALSE(view->GetBackgroundOpaque());
  EXPECT_FALSE([view->cocoa_view() isOpaque]);

  const IPC::Message* set_background;
  set_background = process_host->sink().GetUniqueMessageMatching(
      ViewMsg_SetBackgroundOpaque::ID);
  ASSERT_TRUE(set_background);
  std::tuple<bool> sent_background;
  ViewMsg_SetBackgroundOpaque::Read(set_background, &sent_background);
  EXPECT_FALSE(std::get<0>(sent_background));

  // Try setting it back.
  process_host->sink().ClearMessages();
  view->SetBackgroundColor(SK_ColorWHITE);
  EXPECT_TRUE(view->GetBackgroundOpaque());
  EXPECT_TRUE([view->cocoa_view() isOpaque]);
  set_background = process_host->sink().GetUniqueMessageMatching(
      ViewMsg_SetBackgroundOpaque::ID);
  ASSERT_TRUE(set_background);
  ViewMsg_SetBackgroundOpaque::Read(set_background, &sent_background);
  EXPECT_TRUE(std::get<0>(sent_background));

  host->ShutdownAndDestroyWidget(true);
}

class RenderWidgetHostViewMacPinchTest : public RenderWidgetHostViewMacTest {
 public:
  RenderWidgetHostViewMacPinchTest() : process_host_(NULL) {}

  bool ZoomDisabledForPinchUpdateMessage() {
    const IPC::Message* message = NULL;
    // The first message may be a PinchBegin. Go for the second message if
    // there are two.
    switch (process_host_->sink().message_count()) {
      case 1:
        message = process_host_->sink().GetMessageAt(0);
        break;
      case 2:
        message = process_host_->sink().GetMessageAt(1);
        break;
      default:
        NOTREACHED();
        break;
    }
    DCHECK(message);
    std::tuple<IPC::WebInputEventPointer, ui::LatencyInfo,
               InputEventDispatchType>
        data;
    InputMsg_HandleInputEvent::Read(message, &data);
    IPC::WebInputEventPointer ipc_event = std::get<0>(data);
    const blink::WebGestureEvent* gesture_event =
        static_cast<const blink::WebGestureEvent*>(ipc_event);
    return gesture_event->data.pinchUpdate.zoomDisabled;
  }

  MockRenderProcessHost* process_host_;
};

TEST_F(RenderWidgetHostViewMacPinchTest, PinchThresholding) {
  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  process_host_ = new MockRenderProcessHost(&browser_context);
  process_host_->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host_->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      new MockRenderWidgetHostImpl(&delegate, process_host_, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  process_host_->sink().ClearMessages();

  // We'll use this IPC message to ack events.
  InputEventAck ack(blink::WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  std::unique_ptr<IPC::Message> response(
      new InputHostMsg_HandleInputEvent_ACK(0, ack));

  // Do a gesture that crosses the threshold.
  {
    NSEvent* pinchBeginEvent =
        MockGestureEvent(NSEventTypeBeginGesture, 0);
    NSEvent* pinchUpdateEvents[3] = {
        MockGestureEvent(NSEventTypeMagnify, 0.25),
        MockGestureEvent(NSEventTypeMagnify, 0.25),
        MockGestureEvent(NSEventTypeMagnify, 0.25),
    };
    NSEvent* pinchEndEvent =
        MockGestureEvent(NSEventTypeEndGesture, 0);

    [view->cocoa_view() beginGestureWithEvent:pinchBeginEvent];
    EXPECT_EQ(0U, process_host_->sink().message_count());

    // No zoom is sent for the first update event.
    [view->cocoa_view() magnifyWithEvent:pinchUpdateEvents[0]];
    host->OnMessageReceived(*response);
    EXPECT_EQ(2U, process_host_->sink().message_count());
    EXPECT_TRUE(ZoomDisabledForPinchUpdateMessage());
    process_host_->sink().ClearMessages();

    // The second update event crosses the threshold of 0.4, and so zoom is no
    // longer disabled.
    [view->cocoa_view() magnifyWithEvent:pinchUpdateEvents[1]];
    EXPECT_FALSE(ZoomDisabledForPinchUpdateMessage());
    host->OnMessageReceived(*response);
    EXPECT_EQ(1U, process_host_->sink().message_count());
    process_host_->sink().ClearMessages();

    // The third update still has zoom enabled.
    [view->cocoa_view() magnifyWithEvent:pinchUpdateEvents[2]];
    EXPECT_FALSE(ZoomDisabledForPinchUpdateMessage());
    host->OnMessageReceived(*response);
    EXPECT_EQ(1U, process_host_->sink().message_count());
    process_host_->sink().ClearMessages();

    [view->cocoa_view() endGestureWithEvent:pinchEndEvent];
    EXPECT_EQ(1U, process_host_->sink().message_count());
    process_host_->sink().ClearMessages();
  }

  // Do a gesture that doesn't cross the threshold, but happens when we're not
  // at page scale factor one, so it should be sent to the renderer.
  {
    NSEvent* pinchBeginEvent = MockGestureEvent(NSEventTypeBeginGesture, 0);
    NSEvent* pinchUpdateEvent = MockGestureEvent(NSEventTypeMagnify, 0.25);
    NSEvent* pinchEndEvent = MockGestureEvent(NSEventTypeEndGesture, 0);

    view->page_at_minimum_scale_ = false;

    [view->cocoa_view() beginGestureWithEvent:pinchBeginEvent];
    EXPECT_EQ(0U, process_host_->sink().message_count());

    // Expect that a zoom happen because the time threshold has not passed.
    [view->cocoa_view() magnifyWithEvent:pinchUpdateEvent];
    EXPECT_FALSE(ZoomDisabledForPinchUpdateMessage());
    host->OnMessageReceived(*response);
    EXPECT_EQ(2U, process_host_->sink().message_count());
    process_host_->sink().ClearMessages();

    [view->cocoa_view() endGestureWithEvent:pinchEndEvent];
    EXPECT_EQ(1U, process_host_->sink().message_count());
    process_host_->sink().ClearMessages();
  }

  // Do a gesture again, after the page scale is no longer at one, and ensure
  // that it is thresholded again.
  {
    NSEvent* pinchBeginEvent = MockGestureEvent(NSEventTypeBeginGesture, 0);
    NSEvent* pinchUpdateEvent = MockGestureEvent(NSEventTypeMagnify, 0.25);
    NSEvent* pinchEndEvent = MockGestureEvent(NSEventTypeEndGesture, 0);

    view->page_at_minimum_scale_ = true;

    [view->cocoa_view() beginGestureWithEvent:pinchBeginEvent];
    EXPECT_EQ(0U, process_host_->sink().message_count());

    // Get back to zoom one right after the begin event. This should still keep
    // the thresholding in place (it is latched at the begin event).
    view->page_at_minimum_scale_ = false;

    // Expect that zoom be disabled because the time threshold has passed.
    [view->cocoa_view() magnifyWithEvent:pinchUpdateEvent];
    EXPECT_EQ(2U, process_host_->sink().message_count());
    EXPECT_TRUE(ZoomDisabledForPinchUpdateMessage());
    host->OnMessageReceived(*response);
    process_host_->sink().ClearMessages();

    [view->cocoa_view() endGestureWithEvent:pinchEndEvent];
    EXPECT_EQ(1U, process_host_->sink().message_count());
    process_host_->sink().ClearMessages();
  }

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

TEST_F(RenderWidgetHostViewMacTest, EventLatencyOSMouseWheelHistogram) {
  base::HistogramTester histogram_tester;

  // Initialize the view associated with a MockRenderWidgetHostImpl, rather than
  // the MockRenderProcessHost that is set up by the test harness which mocks
  // out |OnMessageReceived()|.
  TestBrowserContext browser_context;
  MockRenderProcessHost* process_host =
      new MockRenderProcessHost(&browser_context);
  process_host->Init();
  MockRenderWidgetHostDelegate delegate;
  int32_t routing_id = process_host->GetNextRoutingID();
  MockRenderWidgetHostImpl* host =
      new MockRenderWidgetHostImpl(&delegate, process_host, routing_id);
  RenderWidgetHostViewMac* view = new RenderWidgetHostViewMac(host, false);
  process_host->sink().ClearMessages();

  // Send an initial wheel event for scrolling by 3 lines.
  // Verify that Event.Latency.OS.MOUSE_WHEEL histogram is computed properly.
  NSEvent* wheelEvent = MockScrollWheelEventWithPhase(@selector(phaseBegan),3);
  [view->cocoa_view() scrollWheel:wheelEvent];
  histogram_tester.ExpectTotalCount("Event.Latency.OS.MOUSE_WHEEL", 1);

  // Clean up.
  host->ShutdownAndDestroyWidget(true);
}

}  // namespace content
