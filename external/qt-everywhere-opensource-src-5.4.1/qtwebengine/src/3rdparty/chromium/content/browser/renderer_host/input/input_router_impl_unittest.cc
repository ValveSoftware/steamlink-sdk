// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_router_client.h"
#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/input/mock_input_ack_handler.h"
#include "content/browser/renderer_host/input/mock_input_router_client.h"
#include "content/common/content_constants_internal.h"
#include "content/common/edit_command.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input/touch_action.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/events/event.h"
#endif

using base::TimeDelta;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebKeyboardEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

const WebInputEvent* GetInputEventFromMessage(const IPC::Message& message) {
  PickleIterator iter(message);
  const char* data;
  int data_length;
  if (!message.ReadData(&iter, &data, &data_length))
    return NULL;
  return reinterpret_cast<const WebInputEvent*>(data);
}

WebInputEvent& GetEventWithType(WebInputEvent::Type type) {
  WebInputEvent* event = NULL;
  if (WebInputEvent::isMouseEventType(type)) {
    static WebMouseEvent mouse;
    event = &mouse;
  } else if (WebInputEvent::isTouchEventType(type)) {
    static WebTouchEvent touch;
    event = &touch;
  } else if (WebInputEvent::isKeyboardEventType(type)) {
    static WebKeyboardEvent key;
    event = &key;
  } else if (WebInputEvent::isGestureEventType(type)) {
    static WebGestureEvent gesture;
    event = &gesture;
  } else if (type == WebInputEvent::MouseWheel) {
    static WebMouseWheelEvent wheel;
    event = &wheel;
  }
  CHECK(event);
  event->type = type;
  return *event;
}

bool GetIsShortcutFromHandleInputEventMessage(const IPC::Message* msg) {
  InputMsg_HandleInputEvent::Schema::Param param;
  InputMsg_HandleInputEvent::Read(msg, &param);
  return param.c;
}

template<typename MSG_T, typename ARG_T1>
void ExpectIPCMessageWithArg1(const IPC::Message* msg, const ARG_T1& arg1) {
  ASSERT_EQ(MSG_T::ID, msg->type());
  typename MSG_T::Schema::Param param;
  ASSERT_TRUE(MSG_T::Read(msg, &param));
  EXPECT_EQ(arg1, param.a);
}

template<typename MSG_T, typename ARG_T1, typename ARG_T2>
void ExpectIPCMessageWithArg2(const IPC::Message* msg,
                              const ARG_T1& arg1,
                              const ARG_T2& arg2) {
  ASSERT_EQ(MSG_T::ID, msg->type());
  typename MSG_T::Schema::Param param;
  ASSERT_TRUE(MSG_T::Read(msg, &param));
  EXPECT_EQ(arg1, param.a);
  EXPECT_EQ(arg2, param.b);
}

#if defined(USE_AURA)
bool TouchEventsAreEquivalent(const ui::TouchEvent& first,
                              const ui::TouchEvent& second) {
  if (first.type() != second.type())
    return false;
  if (first.location() != second.location())
    return false;
  if (first.touch_id() != second.touch_id())
    return false;
  if (second.time_stamp().InSeconds() != first.time_stamp().InSeconds())
    return false;
  return true;
}

bool EventListIsSubset(const ScopedVector<ui::TouchEvent>& subset,
                       const ScopedVector<ui::TouchEvent>& set) {
  if (subset.size() > set.size())
    return false;
  for (size_t i = 0; i < subset.size(); ++i) {
    const ui::TouchEvent* first = subset[i];
    const ui::TouchEvent* second = set[i];
    bool equivalent = TouchEventsAreEquivalent(*first, *second);
    if (!equivalent)
      return false;
  }

  return true;
}
#endif  // defined(USE_AURA)

// Expected function used for converting pinch scales to deltaY values.
float PinchScaleToWheelDelta(float scale) {
  return 100.0 * log(scale);
}

}  // namespace

class InputRouterImplTest : public testing::Test {
 public:
  InputRouterImplTest() {}
  virtual ~InputRouterImplTest() {}

 protected:
  // testing::Test
  virtual void SetUp() OVERRIDE {
    browser_context_.reset(new TestBrowserContext());
    process_.reset(new MockRenderProcessHost(browser_context_.get()));
    client_.reset(new MockInputRouterClient());
    ack_handler_.reset(new MockInputAckHandler());
    CommandLine* command_line = CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kValidateInputEventStream);
    input_router_.reset(new InputRouterImpl(process_.get(),
                                            client_.get(),
                                            ack_handler_.get(),
                                            MSG_ROUTING_NONE,
                                            config_));
    client_->set_input_router(input_router());
    ack_handler_->set_input_router(input_router());
  }

  virtual void TearDown() OVERRIDE {
    // Process all pending tasks to avoid leaks.
    base::MessageLoop::current()->RunUntilIdle();

    input_router_.reset();
    client_.reset();
    process_.reset();
    browser_context_.reset();
  }

  void SetUpForTouchAckTimeoutTest(int timeout_ms) {
    config_.touch_config.touch_ack_timeout_delay =
        base::TimeDelta::FromMilliseconds(timeout_ms);
    config_.touch_config.touch_ack_timeout_supported = true;
    TearDown();
    SetUp();
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type, bool is_shortcut) {
    WebKeyboardEvent event = SyntheticWebKeyboardEventBuilder::Build(type);
    NativeWebKeyboardEvent native_event;
    memcpy(&native_event, &event, sizeof(event));
    input_router_->SendKeyboardEvent(
        native_event,
        ui::LatencyInfo(),
        is_shortcut);
  }

  void SimulateWheelEvent(float dX, float dY, int modifiers, bool precise) {
    input_router_->SendWheelEvent(MouseWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(dX, dY, modifiers, precise),
        ui::LatencyInfo()));
  }

  void SimulateMouseEvent(WebInputEvent::Type type, int x, int y) {
    input_router_->SendMouseEvent(MouseEventWithLatencyInfo(
        SyntheticWebMouseEventBuilder::Build(type, x, y, 0),
        ui::LatencyInfo()));
  }

  void SimulateWheelEventWithPhase(WebMouseWheelEvent::Phase phase) {
    input_router_->SendWheelEvent(MouseWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(phase), ui::LatencyInfo()));
  }

  void SimulateGestureEvent(const WebGestureEvent& gesture) {
    input_router_->SendGestureEvent(
        GestureEventWithLatencyInfo(gesture, ui::LatencyInfo()));
  }

  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice sourceDevice) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGestureScrollUpdateEvent(float dX, float dY, int modifiers) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::BuildScrollUpdate(dX, dY, modifiers));
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchorX,
                                       float anchorY,
                                       int modifiers,
                                       WebGestureDevice sourceDevice) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildPinchUpdate(
        scale, anchorX, anchorY, modifiers, sourceDevice));
  }

  void SimulateGestureFlingStartEvent(float velocityX,
                                      float velocityY,
                                      WebGestureDevice sourceDevice) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::BuildFling(velocityX,
                                                    velocityY,
                                                    sourceDevice));
  }

  void SetTouchTimestamp(base::TimeDelta timestamp) {
    touch_event_.SetTimestamp(timestamp);
  }

  void SendTouchEvent() {
    input_router_->SendTouchEvent(
        TouchEventWithLatencyInfo(touch_event_, ui::LatencyInfo()));
    touch_event_.ResetPoints();
  }

  int PressTouchPoint(int x, int y) {
    return touch_event_.PressPoint(x, y);
  }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
  }

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
  }

  void CancelTouchPoint(int index) {
    touch_event_.CancelPoint(index);
  }

  void SendInputEventACK(blink::WebInputEvent::Type type,
                         InputEventAckState ack_result) {
    InputHostMsg_HandleInputEvent_ACK_Params ack;
    ack.type = type;
    ack.state = ack_result;
    input_router_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));
  }

  InputRouterImpl* input_router() const {
    return input_router_.get();
  }

  bool TouchEventQueueEmpty() const {
    return input_router()->touch_event_queue_.empty();
  }

  bool TouchEventTimeoutEnabled() const {
    return input_router()->touch_event_queue_.ack_timeout_enabled();
  }

  void Flush() const {
    return input_router_->Flush();
  }

  size_t GetAndResetDidFlushCount() {
    return client_->GetAndResetDidFlushCount();
  }

  bool HasPendingEvents() const {
    return input_router_->HasPendingEvents();
  }

  void OnHasTouchEventHandlers(bool has_handlers) {
    input_router_->OnMessageReceived(
        ViewHostMsg_HasTouchEventHandlers(0, has_handlers));
  }

  void OnSetTouchAction(content::TouchAction touch_action) {
    input_router_->OnMessageReceived(
        InputHostMsg_SetTouchAction(0, touch_action));
  }

  size_t GetSentMessageCountAndResetSink() {
    size_t count = process_->sink().message_count();
    process_->sink().ClearMessages();
    return count;
  }

  static void RunTasksAndWait(base::TimeDelta delay) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitClosure(), delay);
    base::MessageLoop::current()->Run();
  }

  InputRouterImpl::Config config_;
  scoped_ptr<MockRenderProcessHost> process_;
  scoped_ptr<MockInputRouterClient> client_;
  scoped_ptr<MockInputAckHandler> ack_handler_;
  scoped_ptr<InputRouterImpl> input_router_;

 private:
  base::MessageLoopForUI message_loop_;
  SyntheticWebTouchEvent touch_event_;

  scoped_ptr<TestBrowserContext> browser_context_;
};

TEST_F(InputRouterImplTest, CoalescesRangeSelection) {
  input_router_->SendInput(scoped_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(1, 2), gfx::Point(3, 4))));
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(1, 2),
      gfx::Point(3, 4));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send two more messages without acking.
  input_router_->SendInput(scoped_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(5, 6), gfx::Point(7, 8))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(scoped_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(9, 10), gfx::Point(11, 12))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Now ack the first message.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(9, 10),
      gfx::Point(11, 12));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Acking the coalesced msg should not send any more msg.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(InputRouterImplTest, CoalescesCaretMove) {
  input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_MoveCaret(0, gfx::Point(1, 2))));
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(
      process_->sink().GetMessageAt(0), gfx::Point(1, 2));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send two more messages without acking.
  input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_MoveCaret(0, gfx::Point(5, 6))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(
      scoped_ptr<IPC::Message>(new InputMsg_MoveCaret(0, gfx::Point(9, 10))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Now ack the first message.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(
      process_->sink().GetMessageAt(0), gfx::Point(9, 10));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Acking the coalesced msg should not send any more msg.
  {
    scoped_ptr<IPC::Message> response(new ViewHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(InputRouterImplTest, HandledInputEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);

  // Make sure no input event is sent to the renderer.
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // OnKeyboardEventAck should be triggered without actual ack.
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());

  // As the event was acked already, keyboard event queue should be
  // empty.
  ASSERT_EQ(NULL, input_router_->GetLastKeyboardEvent());
}

TEST_F(InputRouterImplTest, ClientCanceledKeyboardEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Simulate a keyboard event that has no consumer.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);

  // Make sure no input event is sent to the renderer.
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());


  // Simulate a keyboard event that should be dropped.
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_UNKNOWN);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);

  // Make sure no input event is sent to the renderer, and no ack is sent.
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
}

TEST_F(InputRouterImplTest, ShortcutKeyboardEvent) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, true);
  EXPECT_TRUE(GetIsShortcutFromHandleInputEventMessage(
      process_->sink().GetMessageAt(0)));

  process_->sink().ClearMessages();

  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);
  EXPECT_FALSE(GetIsShortcutFromHandleInputEventMessage(
      process_->sink().GetMessageAt(0)));
}

TEST_F(InputRouterImplTest, NoncorrespondingKeyEvents) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);

  SendInputEventACK(WebInputEvent::KeyUp,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(ack_handler_->unexpected_event_ack_called());
}

// Tests ported from RenderWidgetHostTest --------------------------------------

TEST_F(InputRouterImplTest, HandleKeyEventsWeSent) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown, false);
  ASSERT_TRUE(input_router_->GetLastKeyboardEvent());
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            input_router_->GetLastKeyboardEvent()->type);

  // Make sure we sent the input event to the renderer.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  process_->sink().ClearMessages();

  // Send the simulated response from the renderer back.
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::RawKeyDown,
            ack_handler_->acked_keyboard_event().type);
}

TEST_F(InputRouterImplTest, IgnoreKeyEventsWeDidntSend) {
  // Send a simulated, unrequested key response. We should ignore this.
  SendInputEventACK(WebInputEvent::RawKeyDown,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
}

TEST_F(InputRouterImplTest, CoalescesWheelEvents) {
  // Simulate wheel events.
  SimulateWheelEvent(0, -5, 0, false);  // sent directly
  SimulateWheelEvent(0, -10, 0, false);  // enqueued
  SimulateWheelEvent(8, -6, 0, false);  // coalesced into previous event
  SimulateWheelEvent(9, -7, 1, false);  // enqueued, different modifiers
  SimulateWheelEvent(0, -10, 0, false);  // enqueued, different modifiers
  // Explicitly verify that PhaseEnd isn't coalesced to avoid bugs like
  // https://crbug.com/154740.
  SimulateWheelEventWithPhase(WebMouseWheelEvent::PhaseEnded);  // enqueued

  // Check that only the first event was sent.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(0, wheel_event->deltaX);
  EXPECT_EQ(-5, wheel_event->deltaY);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Check that the ACK sends the second message immediately.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  // The coalesced events can queue up a delayed ack
  // so that additional input events can be processed before
  // we turn off coalescing.
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
          InputMsg_HandleInputEvent::ID));
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(8, wheel_event->deltaX);
  EXPECT_EQ(-10 + -6, wheel_event->deltaY);  // coalesced
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Ack the second event (which had the third coalesced into it).
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(9, wheel_event->deltaX);
  EXPECT_EQ(-7, wheel_event->deltaY);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Ack the fourth event.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(0, wheel_event->deltaX);
  EXPECT_EQ(-10, wheel_event->deltaY);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Ack the fifth event.
  SendInputEventACK(WebInputEvent::MouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(0, wheel_event->deltaX);
  EXPECT_EQ(0, wheel_event->deltaY);
  EXPECT_EQ(WebMouseWheelEvent::PhaseEnded, wheel_event->phase);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // After the final ack, the queue should be empty.
  SendInputEventACK(WebInputEvent::MouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
}

// Tests that touch-events are queued properly.
TEST_F(InputRouterImplTest, TouchEventQueue) {
  OnHasTouchEventHandlers(true);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // The second touch should not be sent since one is already in queue.
  MoveTouchPoint(0, 5, 5);
  SendTouchEvent();
  EXPECT_FALSE(client_->GetAndResetFilterEventCalled());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::TouchStart,
            ack_handler_->acked_touch_event().event.type);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::TouchMove,
            ack_handler_->acked_touch_event().event.type);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
}

// Tests that the touch-queue is emptied if a page stops listening for touch
// events.
TEST_F(InputRouterImplTest, TouchEventQueueFlush) {
  OnHasTouchEventHandlers(true);
  EXPECT_TRUE(client_->has_touch_handler());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(TouchEventQueueEmpty());

  EXPECT_TRUE(input_router_->ShouldForwardTouchEvent());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // The page stops listening for touch-events. The touch-event queue should now
  // be emptied, but none of the queued touch-events should be sent to the
  // renderer.
  OnHasTouchEventHandlers(false);
  EXPECT_FALSE(client_->has_touch_handler());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_FALSE(input_router_->ShouldForwardTouchEvent());
}

#if defined(USE_AURA)
// Tests that the acked events have correct state. (ui::Events are used only on
// windows and aura)
TEST_F(InputRouterImplTest, AckedTouchEventState) {
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_TRUE(input_router_->ShouldForwardTouchEvent());

  // Send a bunch of events, and make sure the ACKed events are correct.
  ScopedVector<ui::TouchEvent> expected_events;

  // Use a custom timestamp for all the events to test that the acked events
  // have the same timestamp;
  base::TimeDelta timestamp = base::Time::NowFromSystemTime() - base::Time();
  timestamp -= base::TimeDelta::FromSeconds(600);

  // Press the first finger.
  PressTouchPoint(1, 1);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
      gfx::Point(1, 1), 0, timestamp));

  // Move the finger.
  timestamp += base::TimeDelta::FromSeconds(10);
  MoveTouchPoint(0, 500, 500);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_MOVED,
      gfx::Point(500, 500), 0, timestamp));

  // Now press a second finger.
  timestamp += base::TimeDelta::FromSeconds(10);
  PressTouchPoint(2, 2);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
      gfx::Point(2, 2), 1, timestamp));

  // Move both fingers.
  timestamp += base::TimeDelta::FromSeconds(10);
  MoveTouchPoint(0, 10, 10);
  MoveTouchPoint(1, 20, 20);
  SetTouchTimestamp(timestamp);
  SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_MOVED,
      gfx::Point(10, 10), 0, timestamp));
  expected_events.push_back(new ui::TouchEvent(ui::ET_TOUCH_MOVED,
      gfx::Point(20, 20), 1, timestamp));

  // Receive the ACKs and make sure the generated events from the acked events
  // are correct.
  WebInputEvent::Type acks[] = { WebInputEvent::TouchStart,
                                 WebInputEvent::TouchMove,
                                 WebInputEvent::TouchStart,
                                 WebInputEvent::TouchMove };

  TouchEventCoordinateSystem coordinate_system = LOCAL_COORDINATES;
#if !defined(OS_WIN)
  coordinate_system = SCREEN_COORDINATES;
#endif
  for (size_t i = 0; i < arraysize(acks); ++i) {
    SendInputEventACK(acks[i],
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(acks[i], ack_handler_->acked_touch_event().event.type);
    ScopedVector<ui::TouchEvent> acked;

    MakeUITouchEventsFromWebTouchEvents(
        ack_handler_->acked_touch_event(), &acked, coordinate_system);
    bool success = EventListIsSubset(acked, expected_events);
    EXPECT_TRUE(success) << "Failed on step: " << i;
    if (!success)
      break;
    expected_events.erase(expected_events.begin(),
                          expected_events.begin() + acked.size());
  }

  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_EQ(0U, expected_events.size());
}
#endif  // defined(USE_AURA)

TEST_F(InputRouterImplTest, UnhandledWheelEvent) {
  // Simulate wheel events.
  SimulateWheelEvent(0, -5, 0, false);  // sent directly
  SimulateWheelEvent(0, -10, 0, false);  // enqueued

  // Check that only the first event was sent.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Indicate that the wheel event was unhandled.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Check that the correct unhandled wheel event was received.
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_handler_->ack_state());
  EXPECT_EQ(ack_handler_->acked_wheel_event().deltaY, -5);

  // Check that the second event was sent.
  EXPECT_TRUE(process_->sink().GetUniqueMessageMatching(
                  InputMsg_HandleInputEvent::ID));
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Check that the correct unhandled wheel event was received.
  EXPECT_EQ(ack_handler_->acked_wheel_event().deltaY, -5);
}

TEST_F(InputRouterImplTest, TouchTypesIgnoringAck) {
  OnHasTouchEventHandlers(true);
  // Only acks for TouchCancel should always be ignored.
  ASSERT_FALSE(WebInputEventTraits::IgnoresAckDisposition(
      GetEventWithType(WebInputEvent::TouchStart)));
  ASSERT_FALSE(WebInputEventTraits::IgnoresAckDisposition(
      GetEventWithType(WebInputEvent::TouchMove)));
  ASSERT_FALSE(WebInputEventTraits::IgnoresAckDisposition(
      GetEventWithType(WebInputEvent::TouchEnd)));

  // Precede the TouchCancel with an appropriate TouchStart;
  PressTouchPoint(1, 1);
  SendTouchEvent();
  SendInputEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  ASSERT_EQ(1U, ack_handler_->GetAndResetAckCount());
  ASSERT_EQ(0, client_->in_flight_event_count());

  // The TouchCancel ack is always ignored.
  CancelTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
  EXPECT_FALSE(HasPendingEvents());
  SendInputEventACK(WebInputEvent::TouchCancel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_FALSE(HasPendingEvents());
}

TEST_F(InputRouterImplTest, GestureTypesIgnoringAck) {
  // We test every gesture type, ensuring that the stream of gestures is valid.
  const int kEventTypesLength = 29;
  WebInputEvent::Type eventTypes[kEventTypesLength] = {
      WebInputEvent::GestureTapDown,
      WebInputEvent::GestureShowPress,
      WebInputEvent::GestureTapCancel,
      WebInputEvent::GestureScrollBegin,
      WebInputEvent::GestureFlingStart,
      WebInputEvent::GestureFlingCancel,
      WebInputEvent::GestureTapDown,
      WebInputEvent::GestureTap,
      WebInputEvent::GestureTapDown,
      WebInputEvent::GestureLongPress,
      WebInputEvent::GestureTapCancel,
      WebInputEvent::GestureLongTap,
      WebInputEvent::GestureTapDown,
      WebInputEvent::GestureTapUnconfirmed,
      WebInputEvent::GestureTapCancel,
      WebInputEvent::GestureTapDown,
      WebInputEvent::GestureDoubleTap,
      WebInputEvent::GestureTapDown,
      WebInputEvent::GestureTapCancel,
      WebInputEvent::GestureTwoFingerTap,
      WebInputEvent::GestureTapDown,
      WebInputEvent::GestureTapCancel,
      WebInputEvent::GestureScrollBegin,
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollUpdateWithoutPropagation,
      WebInputEvent::GesturePinchBegin,
      WebInputEvent::GesturePinchUpdate,
      WebInputEvent::GesturePinchEnd,
      WebInputEvent::GestureScrollEnd};
  for (int i = 0; i < kEventTypesLength; ++i) {
    WebInputEvent::Type type = eventTypes[i];
    if (!WebInputEventTraits::IgnoresAckDisposition(GetEventWithType(type))) {
      SimulateGestureEvent(type, blink::WebGestureDeviceTouchscreen);
      EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
      EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
      EXPECT_EQ(1, client_->in_flight_event_count());
      EXPECT_TRUE(HasPendingEvents());

      SendInputEventACK(type, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
      EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
      EXPECT_EQ(0, client_->in_flight_event_count());
      EXPECT_FALSE(HasPendingEvents());
      continue;
    }

    SimulateGestureEvent(type, blink::WebGestureDeviceTouchscreen);
    EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
    EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
    EXPECT_EQ(0, client_->in_flight_event_count());
    EXPECT_FALSE(HasPendingEvents());
  }
}

TEST_F(InputRouterImplTest, MouseTypesIgnoringAck) {
  int start_type = static_cast<int>(WebInputEvent::MouseDown);
  int end_type = static_cast<int>(WebInputEvent::ContextMenu);
  ASSERT_LT(start_type, end_type);
  for (int i = start_type; i <= end_type; ++i) {
    WebInputEvent::Type type = static_cast<WebInputEvent::Type>(i);
    int expected_in_flight_event_count =
        WebInputEventTraits::IgnoresAckDisposition(GetEventWithType(type)) ? 0
                                                                           : 1;

    // Note: Mouse event acks are never forwarded to the ack handler, so the key
    // result here is that ignored ack types don't affect the in-flight count.
    SimulateMouseEvent(type, 0, 0);
    EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
    EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
    EXPECT_EQ(expected_in_flight_event_count, client_->in_flight_event_count());
    if (expected_in_flight_event_count) {
      SendInputEventACK(type, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
      EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
      EXPECT_EQ(0, client_->in_flight_event_count());
    }
  }
}

// Guard against breaking changes to the list of ignored event ack types in
// |WebInputEventTraits::IgnoresAckDisposition|.
TEST_F(InputRouterImplTest, RequiredEventAckTypes) {
  const WebInputEvent::Type kRequiredEventAckTypes[] = {
    WebInputEvent::MouseMove,
    WebInputEvent::MouseWheel,
    WebInputEvent::RawKeyDown,
    WebInputEvent::KeyDown,
    WebInputEvent::KeyUp,
    WebInputEvent::Char,
    WebInputEvent::GestureScrollUpdate,
    WebInputEvent::GestureFlingStart,
    WebInputEvent::GestureFlingCancel,
    WebInputEvent::GesturePinchUpdate,
    WebInputEvent::TouchStart,
    WebInputEvent::TouchMove
  };
  for (size_t i = 0; i < arraysize(kRequiredEventAckTypes); ++i) {
    const WebInputEvent::Type required_ack_type = kRequiredEventAckTypes[i];
    EXPECT_FALSE(WebInputEventTraits::IgnoresAckDisposition(
        GetEventWithType(required_ack_type)));
  }
}

// Test that GestureShowPress, GestureTapDown and GestureTapCancel events don't
// wait for ACKs.
TEST_F(InputRouterImplTest, GestureTypesIgnoringAckInterleaved) {
  // Interleave a few events that do and do not ignore acks, ensuring that
  // ack-ignoring events aren't dispatched until all prior events which observe
  // their ack disposition have been dispatched.

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchscreen);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureShowPress,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureTapCancel,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  // Now ack each ack-respecting event. Ack-ignoring events should not be
  // dispatched until all prior events which observe ack disposition have been
  // fired, at which point they should be sent immediately.  They should also
  // have no effect on the in-flight event count.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(2U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(2U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(2U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(2U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(2U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test that GestureShowPress events don't get out of order due to
// ignoring their acks.
TEST_F(InputRouterImplTest, GestureShowPressIsInOrder) {
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());


  // GesturePinchBegin ignores its ack.
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());

  // GesturePinchUpdate waits for an ack.
  // This also verifies that GesturePinchUpdates for touchscreen are sent
  // to the renderer (in contrast to the TrackpadPinchUpdate test).
  SimulateGestureEvent(WebInputEvent::GesturePinchUpdate,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureShowPress,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  // The ShowPress, though it ignores ack, is still stuck in the queue
  // behind the PinchUpdate which requires an ack.
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SimulateGestureEvent(WebInputEvent::GestureShowPress,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  // ShowPress has entered the queue.
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  // Now that the Tap has been ACKed, the ShowPress events should receive
  // synthetic acks, and fire immediately.
  EXPECT_EQ(2U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(3U, ack_handler_->GetAndResetAckCount());
}

// Test that touch ack timeout behavior is properly toggled by view update flags
// and allowed touch actions.
TEST_F(InputRouterImplTest, TouchAckTimeoutConfigured) {
  const int timeout_ms = 1;
  SetUpForTouchAckTimeoutTest(timeout_ms);
  ASSERT_TRUE(TouchEventTimeoutEnabled());

  // Verify that the touch ack timeout fires upon the delayed ack.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(timeout_ms + 1));

  // The timed-out event should have been ack'ed.
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Ack'ing the timed-out event should fire a TouchCancel.
  SendInputEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // The remainder of the touch sequence should be dropped.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  ASSERT_TRUE(TouchEventTimeoutEnabled());

  // A fixed page scale or mobile viewport should disable the touch timeout.
  input_router()->OnViewUpdated(InputRouter::FIXED_PAGE_SCALE);
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  input_router()->OnViewUpdated(InputRouter::VIEW_FLAGS_NONE);
  EXPECT_TRUE(TouchEventTimeoutEnabled());

  input_router()->OnViewUpdated(InputRouter::MOBILE_VIEWPORT);
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  input_router()->OnViewUpdated(InputRouter::MOBILE_VIEWPORT |
                                InputRouter::FIXED_PAGE_SCALE);
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  input_router()->OnViewUpdated(InputRouter::VIEW_FLAGS_NONE);
  EXPECT_TRUE(TouchEventTimeoutEnabled());

  // TOUCH_ACTION_NONE (and no other touch-action) should disable the timeout.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_PAN_Y);
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  SendInputEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED);

  PressTouchPoint(1, 1);
  SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  ReleaseTouchPoint(0);
  SendTouchEvent();
  SendInputEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED);

  // As the touch-action is reset by a new touch sequence, the timeout behavior
  // should be restored.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(TouchEventTimeoutEnabled());
}

// Test that a touch sequenced preceded by TOUCH_ACTION_NONE is not affected by
// the touch timeout.
TEST_F(InputRouterImplTest,
       TouchAckTimeoutDisabledForTouchSequenceAfterTouchActionNone) {
  const int timeout_ms = 1;
  SetUpForTouchAckTimeoutTest(timeout_ms);
  ASSERT_TRUE(TouchEventTimeoutEnabled());
  OnHasTouchEventHandlers(true);

  // Start a touch sequence.
  PressTouchPoint(1, 1);
  SendTouchEvent();

  // TOUCH_ACTION_NONE should disable the timeout.
  OnSetTouchAction(TOUCH_ACTION_NONE);
  SendInputEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  // End the touch sequence.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  SendInputEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  ack_handler_->GetAndResetAckCount();
  GetSentMessageCountAndResetSink();

  // Start another touch sequence.  While this does restore the touch timeout
  // the timeout will not apply until the *next* touch sequence. This affords a
  // touch-action response from the renderer without racing against the timeout.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Delay the ack.  The timeout should *not* fire.
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(timeout_ms + 1));
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Finally send the ack.  The touch sequence should not have been cancelled.
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // End the sequence.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  SendInputEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // A new touch sequence should (finally) be subject to the timeout.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Wait for the touch ack timeout to fire.
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(timeout_ms + 1));
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
}

// Test that TouchActionFilter::ResetTouchAction is called before the
// first touch event for a touch sequence reaches the renderer.
TEST_F(InputRouterImplTest, TouchActionResetBeforeEventReachesRenderer) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_NONE);
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  ReleaseTouchPoint(0);
  SendTouchEvent();

  // Sequence 2.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  ReleaseTouchPoint(0);
  SendTouchEvent();

  SendInputEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::TouchMove, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Ensure touch action is still none, as the next touch start hasn't been
  // acked yet. ScrollBegin and ScrollEnd don't require acks.
  EXPECT_EQ(3U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // This allows the next touch sequence to start.
  SendInputEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Ensure touch action has been set to auto, as a new touch sequence has
  // started.
  SendInputEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::TouchMove, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(3U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SendInputEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED);
}

// Test that TouchActionFilter::ResetTouchAction is called when a new touch
// sequence has no consumer.
TEST_F(InputRouterImplTest, TouchActionResetWhenTouchHasNoConsumer) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_NONE);
  SendInputEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::TouchMove, INPUT_EVENT_ACK_STATE_CONSUMED);

  ReleaseTouchPoint(0);
  SendTouchEvent();

  // Sequence 2
  PressTouchPoint(1, 1);
  SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  ReleaseTouchPoint(0);
  SendTouchEvent();

  // Ensure we have touch-action:none. ScrollBegin and ScrollEnd don't require
  // acks.
  EXPECT_EQ(3U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  SendInputEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Ensure touch action has been set to auto, as the touch had no consumer.
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
}

// Test that TouchActionFilter::ResetTouchAction is called when the touch
// handler is removed.
TEST_F(InputRouterImplTest, TouchActionResetWhenTouchHandlerRemoved) {
  // Touch sequence with touch handler.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_NONE);
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Ensure we have touch-action:none, suppressing scroll events.
  SendInputEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  SendInputEventACK(WebInputEvent::TouchEnd,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Sequence without a touch handler. Note that in this case, the view may not
  // necessarily forward touches to the router (as no touch handler exists).
  OnHasTouchEventHandlers(false);

  // Ensure touch action has been set to auto, as the touch handler has been
  // removed.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
}

// Test that the double tap gesture depends on the touch action of the first
// tap.
TEST_F(InputRouterImplTest, DoubleTapGestureDependsOnFirstTap) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_NONE);
  SendInputEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED);

  ReleaseTouchPoint(0);
  SendTouchEvent();

  // Sequence 2
  PressTouchPoint(1, 1);
  SendTouchEvent();

  // First tap.
  EXPECT_EQ(2U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // The GestureTapUnconfirmed is converted into a tap, as the touch action is
  // none.
  SimulateGestureEvent(WebInputEvent::GestureTapUnconfirmed,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  // This test will become invalid if GestureTap stops requiring an ack.
  ASSERT_FALSE(WebInputEventTraits::IgnoresAckDisposition(
      GetEventWithType(WebInputEvent::GestureTap)));
  EXPECT_EQ(2, client_->in_flight_event_count());
  SendInputEventACK(WebInputEvent::GestureTap,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1, client_->in_flight_event_count());

  // This tap gesture is dropped, since the GestureTapUnconfirmed was turned
  // into a tap.
  SimulateGestureEvent(WebInputEvent::GestureTap,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  SendInputEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);

  // Second Tap.
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Although the touch-action is now auto, the double tap still won't be
  // dispatched, because the first tap occured when the touch-action was none.
  SimulateGestureEvent(WebInputEvent::GestureDoubleTap,
                       blink::WebGestureDeviceTouchscreen);
  // This test will become invalid if GestureDoubleTap stops requiring an ack.
  ASSERT_FALSE(WebInputEventTraits::IgnoresAckDisposition(
      GetEventWithType(WebInputEvent::GestureDoubleTap)));
  EXPECT_EQ(1, client_->in_flight_event_count());
  SendInputEventACK(WebInputEvent::GestureTap, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test that the router will call the client's |DidFlush| after all events have
// been dispatched following a call to |Flush|.
TEST_F(InputRouterImplTest, InputFlush) {
  EXPECT_FALSE(HasPendingEvents());

  // Flushing an empty router should immediately trigger DidFlush.
  Flush();
  EXPECT_EQ(1U, GetAndResetDidFlushCount());
  EXPECT_FALSE(HasPendingEvents());

  // Queue a TouchStart.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(HasPendingEvents());

  // DidFlush should be called only after the event is ack'ed.
  Flush();
  EXPECT_EQ(0U, GetAndResetDidFlushCount());
  SendInputEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetDidFlushCount());

  // Ensure different types of enqueued events will prevent the DidFlush call
  // until all such events have been fully dispatched.
  MoveTouchPoint(0, 50, 50);
  SendTouchEvent();
  ASSERT_TRUE(HasPendingEvents());
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GesturePinchUpdate,
                       blink::WebGestureDeviceTouchscreen);
  Flush();
  EXPECT_EQ(0U, GetAndResetDidFlushCount());

  // Repeated flush calls should have no effect.
  Flush();
  EXPECT_EQ(0U, GetAndResetDidFlushCount());

  // There are still pending gestures.
  SendInputEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetAndResetDidFlushCount());
  EXPECT_TRUE(HasPendingEvents());

  // One more gesture to go.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetDidFlushCount());
  EXPECT_TRUE(HasPendingEvents());

  // The final ack'ed gesture should trigger the DidFlush.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetDidFlushCount());
  EXPECT_FALSE(HasPendingEvents());
}

// Test that GesturePinchUpdate is handled specially for trackpad
TEST_F(InputRouterImplTest, TouchpadPinchUpdate) {
  // GesturePinchUpdate for trackpad sends synthetic wheel events.
  // Note that the Touchscreen case is verified as NOT doing this as
  // part of the ShowPressIsInOrder test.

  SimulateGesturePinchUpdateEvent(
      1.5f, 20, 25, 0, blink::WebGestureDeviceTouchpad);

  // Verify we actually sent a special wheel event to the renderer.
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(20, wheel_event->x);
  EXPECT_EQ(25, wheel_event->y);
  EXPECT_EQ(20, wheel_event->globalX);
  EXPECT_EQ(25, wheel_event->globalY);
  EXPECT_EQ(20, wheel_event->windowX);
  EXPECT_EQ(25, wheel_event->windowY);
  EXPECT_EQ(PinchScaleToWheelDelta(1.5), wheel_event->deltaY);
  EXPECT_EQ(0, wheel_event->deltaX);
  EXPECT_EQ(1, wheel_event->hasPreciseScrollingDeltas);
  EXPECT_EQ(1, wheel_event->wheelTicksY);
  EXPECT_EQ(0, wheel_event->wheelTicksX);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Indicate that the wheel event was unhandled.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Check that the correct unhandled pinch event was received.
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  ASSERT_EQ(WebInputEvent::GesturePinchUpdate, ack_handler_->ack_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_handler_->ack_state());
  EXPECT_EQ(1.5f, ack_handler_->acked_gesture_event().data.pinchUpdate.scale);
  EXPECT_EQ(0, client_->in_flight_event_count());

  // Second a second pinch event.
  SimulateGesturePinchUpdateEvent(
      0.3f, 20, 25, 0, blink::WebGestureDeviceTouchpad);
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_FLOAT_EQ(PinchScaleToWheelDelta(0.3f), wheel_event->deltaY);
  EXPECT_EQ(1, wheel_event->hasPreciseScrollingDeltas);
  EXPECT_EQ(-1, wheel_event->wheelTicksY);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Indicate that the wheel event was handled this time.
  SendInputEventACK(WebInputEvent::MouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the correct HANDLED pinch event was received.
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, ack_handler_->ack_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, ack_handler_->ack_state());
  EXPECT_FLOAT_EQ(0.3f,
                  ack_handler_->acked_gesture_event().data.pinchUpdate.scale);
}

// Test that touchpad pinch events are coalesced property, with their synthetic
// wheel events getting the right ACKs.
TEST_F(InputRouterImplTest, TouchpadPinchCoalescing) {
  // Send the first pinch.
  SimulateGesturePinchUpdateEvent(
      1.5f, 20, 25, 0, blink::WebGestureDeviceTouchpad);

  // Verify we sent the wheel event to the renderer.
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(PinchScaleToWheelDelta(1.5f), wheel_event->deltaY);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Send a second pinch, this should be queued in the GestureEventQueue.
  SimulateGesturePinchUpdateEvent(
      1.6f, 20, 25, 0, blink::WebGestureDeviceTouchpad);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  // Send a third pinch, this should be coalesced into the second in the
  // GestureEventQueue.
  SimulateGesturePinchUpdateEvent(
      1.7f, 20, 25, 0, blink::WebGestureDeviceTouchpad);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  // Indicate that the first wheel event was unhandled and verify the ACK.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, ack_handler_->ack_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_handler_->ack_state());
  EXPECT_EQ(1.5f, ack_handler_->acked_gesture_event().data.pinchUpdate.scale);

  // Verify a second wheel event was sent representing the 2nd and 3rd pinch
  // events.
  EXPECT_EQ(1, client_->in_flight_event_count());
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_FLOAT_EQ(PinchScaleToWheelDelta(1.6f * 1.7f),
                  PinchScaleToWheelDelta(1.6f) + PinchScaleToWheelDelta(1.7f));
  EXPECT_FLOAT_EQ(PinchScaleToWheelDelta(1.6f * 1.7f), wheel_event->deltaY);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  // Indicate that the second wheel event was handled and verify the ACK.
  SendInputEventACK(WebInputEvent::MouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, ack_handler_->ack_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, ack_handler_->ack_state());
  EXPECT_FLOAT_EQ(1.6f * 1.7f,
                  ack_handler_->acked_gesture_event().data.pinchUpdate.scale);
}

// Test interleaving pinch and wheel events.
TEST_F(InputRouterImplTest, TouchpadPinchAndWheel) {
  // Simulate queued wheel and pinch events events.
  // Note that in practice interleaving pinch and wheel events should be rare
  // (eg. requires the use of a mouse and trackpad at the same time).

  // Use the control modifier to match the synthetic wheel events so that
  // they're elligble for coalescing.
  int mod = WebInputEvent::ControlKey;

  // Event 1: sent directly.
  SimulateWheelEvent(0, -5, mod, true);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Event 2: enqueued in InputRouter.
  SimulateWheelEvent(0, -10, mod, true);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Event 3: enqueued in InputRouter, not coalesced into #2.
  SimulateGesturePinchUpdateEvent(
      1.5f, 20, 25, 0, blink::WebGestureDeviceTouchpad);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Event 4: enqueued in GestureEventQueue.
  SimulateGesturePinchUpdateEvent(
      1.2f, 20, 25, 0, blink::WebGestureDeviceTouchpad);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Event 5: coalesced into wheel event for #3.
  SimulateWheelEvent(2, 0, mod, true);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Send ack for #1.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::MouseWheel, ack_handler_->ack_event_type());

  // Verify we sent #2.
  ASSERT_EQ(1U, process_->sink().message_count());
  const WebInputEvent* input_event =
      GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  const WebMouseWheelEvent* wheel_event =
      static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(0, wheel_event->deltaX);
  EXPECT_EQ(-10, wheel_event->deltaY);
  EXPECT_EQ(mod, wheel_event->modifiers);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Send ack for #2.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::MouseWheel, ack_handler_->ack_event_type());

  // Verify we sent #3 (with #5 coalesced in).
  ASSERT_EQ(1U, process_->sink().message_count());
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(2, wheel_event->deltaX);
  EXPECT_EQ(PinchScaleToWheelDelta(1.5f), wheel_event->deltaY);
  EXPECT_EQ(mod, wheel_event->modifiers);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Send ack for #3.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, ack_handler_->ack_event_type());

  // Verify we sent #4.
  ASSERT_EQ(1U, process_->sink().message_count());
  input_event = GetInputEventFromMessage(*process_->sink().GetMessageAt(0));
  ASSERT_EQ(WebInputEvent::MouseWheel, input_event->type);
  wheel_event = static_cast<const WebMouseWheelEvent*>(input_event);
  EXPECT_EQ(0, wheel_event->deltaX);
  EXPECT_FLOAT_EQ(PinchScaleToWheelDelta(1.2f), wheel_event->deltaY);
  EXPECT_EQ(mod, wheel_event->modifiers);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Send ack for #4.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, ack_handler_->ack_event_type());
}

// Test proper handling of touchpad Gesture{Pinch,Scroll}Update sequences.
TEST_F(InputRouterImplTest, TouchpadPinchAndScrollUpdate) {
  // The first scroll should be sent immediately.
  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchpad);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Subsequent scroll and pinch events should remain queued, coalescing as
  // more trackpad events arrive.
  SimulateGestureEvent(WebInputEvent::GesturePinchUpdate,
                       blink::WebGestureDeviceTouchpad);
  ASSERT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchpad);
  ASSERT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::GesturePinchUpdate,
                       blink::WebGestureDeviceTouchpad);
  ASSERT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchpad);
  ASSERT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Ack'ing the first scroll should trigger both the coalesced scroll and the
  // coalesced pinch events (which is sent to the renderer as a wheel event).
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(2U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(2, client_->in_flight_event_count());

  // Ack the second scroll.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Ack the wheel event.
  SendInputEventACK(WebInputEvent::MouseWheel, INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
}

// Test proper routing of overscroll notifications received either from
// event acks or from |DidOverscroll| IPC messages.
TEST_F(InputRouterImplTest, OverscrollDispatch) {
  DidOverscrollParams overscroll;
  overscroll.accumulated_overscroll = gfx::Vector2dF(-14, 14);
  overscroll.latest_overscroll_delta = gfx::Vector2dF(-7, 0);
  overscroll.current_fling_velocity = gfx::Vector2dF(-1, 0);

  input_router_->OnMessageReceived(InputHostMsg_DidOverscroll(0, overscroll));
  DidOverscrollParams client_overscroll = client_->GetAndResetOverscroll();
  EXPECT_EQ(overscroll.accumulated_overscroll,
            client_overscroll.accumulated_overscroll);
  EXPECT_EQ(overscroll.latest_overscroll_delta,
            client_overscroll.latest_overscroll_delta);
  EXPECT_EQ(overscroll.current_fling_velocity,
            client_overscroll.current_fling_velocity);

  DidOverscrollParams wheel_overscroll;
  wheel_overscroll.accumulated_overscroll = gfx::Vector2dF(7, -7);
  wheel_overscroll.latest_overscroll_delta = gfx::Vector2dF(3, 0);
  wheel_overscroll.current_fling_velocity = gfx::Vector2dF(1, 0);

  SimulateWheelEvent(3, 0, 0, false);
  InputHostMsg_HandleInputEvent_ACK_Params ack;
  ack.type = WebInputEvent::MouseWheel;
  ack.state = INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
  ack.overscroll.reset(new DidOverscrollParams(wheel_overscroll));
  input_router_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));

  client_overscroll = client_->GetAndResetOverscroll();
  EXPECT_EQ(wheel_overscroll.accumulated_overscroll,
            client_overscroll.accumulated_overscroll);
  EXPECT_EQ(wheel_overscroll.latest_overscroll_delta,
            client_overscroll.latest_overscroll_delta);
  EXPECT_EQ(wheel_overscroll.current_fling_velocity,
            client_overscroll.current_fling_velocity);
}

}  // namespace content
