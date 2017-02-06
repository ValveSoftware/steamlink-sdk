// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_impl.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/input_router_client.h"
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
  base::PickleIterator iter(message);
  const char* data;
  int data_length;
  if (!iter.ReadData(&data, &data_length))
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

template<typename MSG_T, typename ARG_T1>
void ExpectIPCMessageWithArg1(const IPC::Message* msg, const ARG_T1& arg1) {
  ASSERT_EQ(MSG_T::ID, msg->type());
  typename MSG_T::Schema::Param param;
  ASSERT_TRUE(MSG_T::Read(msg, &param));
  EXPECT_EQ(arg1, std::get<0>(param));
}

template<typename MSG_T, typename ARG_T1, typename ARG_T2>
void ExpectIPCMessageWithArg2(const IPC::Message* msg,
                              const ARG_T1& arg1,
                              const ARG_T2& arg2) {
  ASSERT_EQ(MSG_T::ID, msg->type());
  typename MSG_T::Schema::Param param;
  ASSERT_TRUE(MSG_T::Read(msg, &param));
  EXPECT_EQ(arg1, std::get<0>(param));
  EXPECT_EQ(arg2, std::get<1>(param));
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
  if (second.time_stamp() != first.time_stamp())
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

}  // namespace

class InputRouterImplTest : public testing::Test {
 public:
  InputRouterImplTest() {}
  ~InputRouterImplTest() override {}

 protected:
  // testing::Test
  void SetUp() override {
    browser_context_.reset(new TestBrowserContext());
    process_.reset(new MockRenderProcessHost(browser_context_.get()));
    client_.reset(new MockInputRouterClient());
    ack_handler_.reset(new MockInputAckHandler());
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kValidateInputEventStream);
    input_router_.reset(new InputRouterImpl(process_.get(),
                                            client_.get(),
                                            ack_handler_.get(),
                                            MSG_ROUTING_NONE,
                                            config_));
    client_->set_input_router(input_router());
    ack_handler_->set_input_router(input_router());
  }

  void TearDown() override {
    // Process all pending tasks to avoid leaks.
    base::RunLoop().RunUntilIdle();

    input_router_.reset();
    client_.reset();
    process_.reset();
    browser_context_.reset();
  }

  void SetUpForTouchAckTimeoutTest(int desktop_timeout_ms,
                                   int mobile_timeout_ms) {
    config_.touch_config.desktop_touch_ack_timeout_delay =
        base::TimeDelta::FromMilliseconds(desktop_timeout_ms);
    config_.touch_config.mobile_touch_ack_timeout_delay =
        base::TimeDelta::FromMilliseconds(mobile_timeout_ms);
    config_.touch_config.touch_ack_timeout_supported = true;
    TearDown();
    SetUp();
    input_router()->NotifySiteIsMobileOptimized(false);
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
    WebKeyboardEvent event = SyntheticWebKeyboardEventBuilder::Build(type);
    NativeWebKeyboardEvent native_event;
    memcpy(&native_event, &event, sizeof(event));
    NativeWebKeyboardEventWithLatencyInfo key_event(native_event);
    input_router_->SendKeyboardEvent(key_event);
  }

  void SimulateWheelEvent(float x,
                          float y,
                          float dX,
                          float dY,
                          int modifiers,
                          bool precise) {
    input_router_->SendWheelEvent(MouseWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(x, y, dX, dY, modifiers,
                                                  precise)));
  }

  void SimulateMouseEvent(WebInputEvent::Type type, int x, int y) {
    input_router_->SendMouseEvent(MouseEventWithLatencyInfo(
        SyntheticWebMouseEventBuilder::Build(type, x, y, 0)));
  }

  void SimulateWheelEventWithPhase(WebMouseWheelEvent::Phase phase) {
    input_router_->SendWheelEvent(MouseWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(phase)));
  }

  void SimulateGestureEvent(WebGestureEvent gesture) {
    // Ensure non-zero touchscreen fling velocities, as the router will
    // validate aganst such.
    if (gesture.type == WebInputEvent::GestureFlingStart &&
        gesture.sourceDevice == blink::WebGestureDeviceTouchscreen &&
        !gesture.data.flingStart.velocityX &&
        !gesture.data.flingStart.velocityY) {
      gesture.data.flingStart.velocityX = 5.f;
    }

    input_router_->SendGestureEvent(GestureEventWithLatencyInfo(gesture));
  }

  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice source_device) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, source_device));
  }

  void SimulateGestureScrollUpdateEvent(float dX,
                                        float dY,
                                        int modifiers,
                                        WebGestureDevice source_device) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollUpdate(
        dX, dY, modifiers, source_device));
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchor_x,
                                       float anchor_y,
                                       int modifiers,
                                       WebGestureDevice source_device) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildPinchUpdate(
        scale, anchor_x, anchor_y, modifiers, source_device));
  }

  void SimulateGestureFlingStartEvent(float velocity_x,
                                      float velocity_y,
                                      WebGestureDevice source_device) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildFling(
        velocity_x, velocity_y, source_device));
  }

  void SetTouchTimestamp(base::TimeTicks timestamp) {
    touch_event_.SetTimestamp(timestamp);
  }

  uint32_t SendTouchEvent() {
    uint32_t touch_event_id = touch_event_.uniqueTouchEventId;
    input_router_->SendTouchEvent(TouchEventWithLatencyInfo(touch_event_));
    touch_event_.ResetPoints();
    return touch_event_id;
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
    DCHECK(!WebInputEvent::isTouchEventType(type));
    InputEventAck ack(type, ack_result);
    input_router_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));
  }

  void SendTouchEventACK(blink::WebInputEvent::Type type,
                         InputEventAckState ack_result,
                         uint32_t touch_event_id) {
    DCHECK(WebInputEvent::isTouchEventType(type));
    InputEventAck ack(type, ack_result, touch_event_id);
    input_router_->OnMessageReceived(InputHostMsg_HandleInputEvent_ACK(0, ack));
  }

  InputRouterImpl* input_router() const {
    return input_router_.get();
  }

  bool TouchEventQueueEmpty() const {
    return input_router()->touch_event_queue_.empty();
  }

  bool TouchEventTimeoutEnabled() const {
    return input_router()->touch_event_queue_.IsAckTimeoutEnabled();
  }

  void RequestNotificationWhenFlushed() const {
    return input_router_->RequestNotificationWhenFlushed();
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
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(), delay);
    base::MessageLoop::current()->Run();
  }

  InputRouterImpl::Config config_;
  std::unique_ptr<MockRenderProcessHost> process_;
  std::unique_ptr<MockInputRouterClient> client_;
  std::unique_ptr<MockInputAckHandler> ack_handler_;
  std::unique_ptr<InputRouterImpl> input_router_;

 private:
  base::MessageLoopForUI message_loop_;
  SyntheticWebTouchEvent touch_event_;

  std::unique_ptr<TestBrowserContext> browser_context_;
};

TEST_F(InputRouterImplTest, CoalescesRangeSelection) {
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(1, 2), gfx::Point(3, 4))));
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(1, 2),
      gfx::Point(3, 4));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send two more messages without acking.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(5, 6), gfx::Point(7, 8))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(9, 10), gfx::Point(11, 12))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Now ack the first message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
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
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(InputRouterImplTest, CoalescesMoveRangeSelectionExtent) {
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(1, 2))));
  ExpectIPCMessageWithArg1<InputMsg_MoveRangeSelectionExtent>(
      process_->sink().GetMessageAt(0),
      gfx::Point(1, 2));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send two more messages without acking.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(3, 4))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(5, 6))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Now ack the first message.
  {
    std::unique_ptr<IPC::Message> response(
        new InputHostMsg_MoveRangeSelectionExtent_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  ExpectIPCMessageWithArg1<InputMsg_MoveRangeSelectionExtent>(
      process_->sink().GetMessageAt(0),
      gfx::Point(5, 6));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Acking the coalesced msg should not send any more msg.
  {
    std::unique_ptr<IPC::Message> response(
        new InputHostMsg_MoveRangeSelectionExtent_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(InputRouterImplTest, InterleaveSelectRangeAndMoveRangeSelectionExtent) {
  // Send first message: SelectRange.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(1, 2), gfx::Point(3, 4))));
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(1, 2),
      gfx::Point(3, 4));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send second message: MoveRangeSelectionExtent.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(5, 6))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Send third message: SelectRange.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(7, 8), gfx::Point(9, 10))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Ack the messages and verify that they're not coalesced and that they're in
  // correct order.

  // Ack the first message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  ExpectIPCMessageWithArg1<InputMsg_MoveRangeSelectionExtent>(
      process_->sink().GetMessageAt(0),
      gfx::Point(5, 6));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Ack the second message.
  {
    std::unique_ptr<IPC::Message> response(
        new InputHostMsg_MoveRangeSelectionExtent_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(7, 8),
      gfx::Point(9, 10));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Ack the third message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(InputRouterImplTest,
       CoalescesInterleavedSelectRangeAndMoveRangeSelectionExtent) {
  // Send interleaved SelectRange and MoveRangeSelectionExtent messages. They
  // should be coalesced as shown by the arrows.
  //  > SelectRange
  //    MoveRangeSelectionExtent
  //    MoveRangeSelectionExtent
  //  > MoveRangeSelectionExtent
  //    SelectRange
  //  > SelectRange
  //  > MoveRangeSelectionExtent

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(1, 2), gfx::Point(3, 4))));
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(1, 2),
      gfx::Point(3, 4));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(5, 6))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(7, 8))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(9, 10))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(11, 12), gfx::Point(13, 14))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_SelectRange(0, gfx::Point(15, 16), gfx::Point(17, 18))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveRangeSelectionExtent(0, gfx::Point(19, 20))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Ack the first message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the three MoveRangeSelectionExtent messages are coalesced into
  // one message.
  ExpectIPCMessageWithArg1<InputMsg_MoveRangeSelectionExtent>(
      process_->sink().GetMessageAt(0),
      gfx::Point(9, 10));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Ack the second message.
  {
    std::unique_ptr<IPC::Message> response(
        new InputHostMsg_MoveRangeSelectionExtent_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two SelectRange messages are coalesced into one message.
  ExpectIPCMessageWithArg2<InputMsg_SelectRange>(
      process_->sink().GetMessageAt(0),
      gfx::Point(15, 16),
      gfx::Point(17, 18));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Ack the third message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_SelectRange_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify the fourth message.
  ExpectIPCMessageWithArg1<InputMsg_MoveRangeSelectionExtent>(
      process_->sink().GetMessageAt(0),
      gfx::Point(19, 20));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Ack the fourth message.
  {
    std::unique_ptr<IPC::Message> response(
        new InputHostMsg_MoveRangeSelectionExtent_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(InputRouterImplTest, CoalescesCaretMove) {
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveCaret(0, gfx::Point(1, 2))));
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(
      process_->sink().GetMessageAt(0), gfx::Point(1, 2));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Send two more messages without acking.
  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveCaret(0, gfx::Point(5, 6))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  input_router_->SendInput(std::unique_ptr<IPC::Message>(
      new InputMsg_MoveCaret(0, gfx::Point(9, 10))));
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());

  // Now ack the first message.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }

  // Verify that the two messages are coalesced into one message.
  ExpectIPCMessageWithArg1<InputMsg_MoveCaret>(
      process_->sink().GetMessageAt(0), gfx::Point(9, 10));
  EXPECT_EQ(1u, GetSentMessageCountAndResetSink());

  // Acking the coalesced msg should not send any more msg.
  {
    std::unique_ptr<IPC::Message> response(new InputHostMsg_MoveCaret_ACK(0));
    input_router_->OnMessageReceived(*response);
  }
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
}

TEST_F(InputRouterImplTest, HandledInputEvent) {
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

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
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure no input event is sent to the renderer.
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());


  // Simulate a keyboard event that should be dropped.
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_UNKNOWN);
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  // Make sure no input event is sent to the renderer, and no ack is sent.
  EXPECT_EQ(0u, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
}

TEST_F(InputRouterImplTest, NoncorrespondingKeyEvents) {
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);

  SendInputEventACK(WebInputEvent::KeyUp,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(ack_handler_->unexpected_event_ack_called());
}

// Tests ported from RenderWidgetHostTest --------------------------------------

TEST_F(InputRouterImplTest, HandleKeyEventsWeSent) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::RawKeyDown);
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
  SimulateWheelEvent(0, 0, 0, -5, 0, false);   // sent directly
  SimulateWheelEvent(0, 0, 0, -10, 0, false);  // enqueued
  SimulateWheelEvent(0, 0, 8, -6, 0, false);   // coalesced into previous event
  SimulateWheelEvent(0, 0, 9, -7, 1, false);   // enqueued, different modifiers
  SimulateWheelEvent(0, 0, 0, -10, 0, false);  // enqueued, different modifiers
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
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
}

// Tests that touch-events are queued properly.
TEST_F(InputRouterImplTest, TouchEventQueue) {
  OnHasTouchEventHandlers(true);

  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  EXPECT_TRUE(client_->GetAndResetFilterEventCalled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // The second touch should not be sent since one is already in queue.
  MoveTouchPoint(0, 5, 5);
  uint32_t touch_move_event_id = SendTouchEvent();
  EXPECT_FALSE(client_->GetAndResetFilterEventCalled());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // Receive an ACK for the first touch-event.
  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id);
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::TouchStart,
            ack_handler_->acked_touch_event().event.type);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  SendTouchEventACK(WebInputEvent::TouchMove, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_move_event_id);
  EXPECT_TRUE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::TouchMove,
            ack_handler_->acked_touch_event().event.type);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
}

// Tests that the touch-queue is emptied after a page stops listening for touch
// events and the outstanding ack is received.
TEST_F(InputRouterImplTest, TouchEventQueueFlush) {
  OnHasTouchEventHandlers(true);
  EXPECT_TRUE(client_->has_touch_handler());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(TouchEventQueueEmpty());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  MoveTouchPoint(0, 2, 2);
  MoveTouchPoint(0, 3, 3);
  EXPECT_FALSE(TouchEventQueueEmpty());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // The page stops listening for touch-events. Note that flushing is deferred
  // until the outstanding ack is received.
  OnHasTouchEventHandlers(false);
  EXPECT_FALSE(client_->has_touch_handler());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_FALSE(TouchEventQueueEmpty());

  // After the ack, the touch-event queue should be empty, and none of the
  // flushed touch-events should have been sent to the renderer.
  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(TouchEventQueueEmpty());
}

#if defined(USE_AURA)
// Tests that the acked events have correct state. (ui::Events are used only on
// windows and aura)
TEST_F(InputRouterImplTest, AckedTouchEventState) {
  input_router_->OnMessageReceived(ViewHostMsg_HasTouchEventHandlers(0, true));
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_TRUE(TouchEventQueueEmpty());

  // Send a bunch of events, and make sure the ACKed events are correct.
  ScopedVector<ui::TouchEvent> expected_events;

  // Use a custom timestamp for all the events to test that the acked events
  // have the same timestamp;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  timestamp -= base::TimeDelta::FromSeconds(600);

  // Press the first finger.
  PressTouchPoint(1, 1);
  SetTouchTimestamp(timestamp);
  uint32_t touch_press_event_id1 = SendTouchEvent();
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  expected_events.push_back(
      new ui::TouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(1, 1), 0, timestamp));

  // Move the finger.
  timestamp += base::TimeDelta::FromSeconds(10);
  MoveTouchPoint(0, 500, 500);
  SetTouchTimestamp(timestamp);
  uint32_t touch_move_event_id1 = SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  expected_events.push_back(new ui::TouchEvent(
      ui::ET_TOUCH_MOVED, gfx::Point(500, 500), 0, timestamp));

  // Now press a second finger.
  timestamp += base::TimeDelta::FromSeconds(10);
  PressTouchPoint(2, 2);
  SetTouchTimestamp(timestamp);
  uint32_t touch_press_event_id2 = SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  expected_events.push_back(
      new ui::TouchEvent(ui::ET_TOUCH_PRESSED, gfx::Point(2, 2), 1, timestamp));

  // Move both fingers.
  timestamp += base::TimeDelta::FromSeconds(10);
  MoveTouchPoint(0, 10, 10);
  MoveTouchPoint(1, 20, 20);
  SetTouchTimestamp(timestamp);
  uint32_t touch_move_event_id2 = SendTouchEvent();
  EXPECT_FALSE(TouchEventQueueEmpty());
  expected_events.push_back(
      new ui::TouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(10, 10), 0, timestamp));
  expected_events.push_back(
      new ui::TouchEvent(ui::ET_TOUCH_MOVED, gfx::Point(20, 20), 1, timestamp));

  // Receive the ACKs and make sure the generated events from the acked events
  // are correct.
  WebInputEvent::Type acks[] = { WebInputEvent::TouchStart,
                                 WebInputEvent::TouchMove,
                                 WebInputEvent::TouchStart,
                                 WebInputEvent::TouchMove };

  uint32_t touch_event_ids[] = {touch_press_event_id1, touch_move_event_id1,
                                touch_press_event_id2, touch_move_event_id2};

  TouchEventCoordinateSystem coordinate_system = LOCAL_COORDINATES;
#if !defined(OS_WIN)
  coordinate_system = SCREEN_COORDINATES;
#endif
  for (size_t i = 0; i < arraysize(acks); ++i) {
    SendTouchEventACK(acks[i], INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
                      touch_event_ids[i]);
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
  SimulateWheelEvent(0, 0, 0, -5, 0, false);   // sent directly
  SimulateWheelEvent(0, 0, 0, -10, 0, false);  // enqueued

  // Check that only the first event was sent.
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(InputMsg_HandleInputEvent::ID));
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Indicate that the wheel event was unhandled.
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Check that the ack for the MouseWheel and ScrollBegin
  // were processed.
  EXPECT_EQ(2U, ack_handler_->GetAndResetAckCount());

  // There should be a ScrollBegin and ScrollUpdate, MouseWheel sent
  EXPECT_EQ(3U, GetSentMessageCountAndResetSink());

  EXPECT_EQ(ack_handler_->acked_wheel_event().deltaY, -5);
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Check that the correct unhandled wheel event was received.
  EXPECT_EQ(2U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_handler_->ack_state());
  EXPECT_EQ(ack_handler_->acked_wheel_event().deltaY, -10);
}

TEST_F(InputRouterImplTest, TouchTypesIgnoringAck) {
  OnHasTouchEventHandlers(true);
  // Only acks for TouchCancel should always be ignored.
  ASSERT_TRUE(WebInputEventTraits::ShouldBlockEventStream(
      GetEventWithType(WebInputEvent::TouchStart)));
  ASSERT_TRUE(WebInputEventTraits::ShouldBlockEventStream(
      GetEventWithType(WebInputEvent::TouchMove)));
  ASSERT_TRUE(WebInputEventTraits::ShouldBlockEventStream(
      GetEventWithType(WebInputEvent::TouchEnd)));

  // Precede the TouchCancel with an appropriate TouchStart;
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  ASSERT_EQ(1U, ack_handler_->GetAndResetAckCount());
  ASSERT_EQ(0, client_->in_flight_event_count());

  // The TouchCancel ack is always ignored.
  CancelTouchPoint(0);
  uint32_t touch_cancel_event_id = SendTouchEvent();
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0, client_->in_flight_event_count());
  EXPECT_FALSE(HasPendingEvents());
  SendTouchEventACK(WebInputEvent::TouchCancel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED, touch_cancel_event_id);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_FALSE(HasPendingEvents());
}

TEST_F(InputRouterImplTest, GestureTypesIgnoringAck) {
  // We test every gesture type, ensuring that the stream of gestures is valid.
  const WebInputEvent::Type eventTypes[] = {
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
      WebInputEvent::GesturePinchBegin,
      WebInputEvent::GesturePinchUpdate,
      WebInputEvent::GesturePinchEnd,
      WebInputEvent::GestureScrollEnd};
  for (size_t i = 0; i < arraysize(eventTypes); ++i) {
    WebInputEvent::Type type = eventTypes[i];
    if (WebInputEventTraits::ShouldBlockEventStream(GetEventWithType(type))) {
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
        !WebInputEventTraits::ShouldBlockEventStream(GetEventWithType(type))
            ? 0
            : 1;

    // Note: Only MouseMove ack is forwarded to the ack handler.
    SimulateMouseEvent(type, 0, 0);
    EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
    EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
    EXPECT_EQ(expected_in_flight_event_count, client_->in_flight_event_count());
    if (expected_in_flight_event_count) {
      SendInputEventACK(type, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
      EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
      uint32_t expected_ack_count = type == WebInputEvent::MouseMove ? 1 : 0;
      EXPECT_EQ(expected_ack_count, ack_handler_->GetAndResetAckCount());
      EXPECT_EQ(0, client_->in_flight_event_count());
    }
  }
}

// Guard against breaking changes to the list of ignored event ack types in
// |WebInputEventTraits::ShouldBlockEventStream|.
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
    ASSERT_TRUE(WebInputEventTraits::ShouldBlockEventStream(
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

// Test that touch ack timeout behavior is properly configured for
// mobile-optimized sites and allowed touch actions.
TEST_F(InputRouterImplTest, TouchAckTimeoutConfigured) {
  const int kDesktopTimeoutMs = 1;
  const int kMobileTimeoutMs = 0;
  SetUpForTouchAckTimeoutTest(kDesktopTimeoutMs, kMobileTimeoutMs);
  ASSERT_TRUE(TouchEventTimeoutEnabled());

  // Verify that the touch ack timeout fires upon the delayed ack.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id1 = SendTouchEvent();
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));

  // The timed-out event should have been ack'ed.
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  // Ack'ing the timed-out event should fire a TouchCancel.
  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id1);
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // The remainder of the touch sequence should be dropped.
  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  ASSERT_TRUE(TouchEventTimeoutEnabled());

  // A mobile-optimized site should use the mobile timeout. For this test that
  // timeout value is 0, which disables the timeout.
  input_router()->NotifySiteIsMobileOptimized(true);
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  input_router()->NotifySiteIsMobileOptimized(false);
  EXPECT_TRUE(TouchEventTimeoutEnabled());

  // TOUCH_ACTION_NONE (and no other touch-action) should disable the timeout.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id2 = SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_PAN_Y);
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id2 = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id2);
  SendTouchEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id2);

  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id3 = SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_NONE);
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id3 = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id3);
  SendTouchEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id3);

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
  const int kDesktopTimeoutMs = 1;
  const int kMobileTimeoutMs = 2;
  SetUpForTouchAckTimeoutTest(kDesktopTimeoutMs, kMobileTimeoutMs);
  ASSERT_TRUE(TouchEventTimeoutEnabled());
  OnHasTouchEventHandlers(true);

  // Start a touch sequence.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // TOUCH_ACTION_NONE should disable the timeout.
  OnSetTouchAction(TOUCH_ACTION_NONE);
  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_FALSE(TouchEventTimeoutEnabled());

  MoveTouchPoint(0, 1, 2);
  uint32_t touch_move_event_id = SendTouchEvent();
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Delay the move ack. The timeout should not fire.
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());
  SendTouchEventACK(WebInputEvent::TouchMove, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_move_event_id);
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());

  // End the touch sequence.
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id);
  EXPECT_FALSE(TouchEventTimeoutEnabled());
  ack_handler_->GetAndResetAckCount();
  GetSentMessageCountAndResetSink();

  // Start another touch sequence.  This should restore the touch timeout.
  PressTouchPoint(1, 1);
  SendTouchEvent();
  EXPECT_TRUE(TouchEventTimeoutEnabled());
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(0U, ack_handler_->GetAndResetAckCount());

  // Wait for the touch ack timeout to fire.
  RunTasksAndWait(base::TimeDelta::FromMilliseconds(kDesktopTimeoutMs + 1));
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
}

// Test that TouchActionFilter::ResetTouchAction is called before the
// first touch event for a touch sequence reaches the renderer.
TEST_F(InputRouterImplTest, TouchActionResetBeforeEventReachesRenderer) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id1 = SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_NONE);
  MoveTouchPoint(0, 50, 50);
  uint32_t touch_move_event_id1 = SendTouchEvent();
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id1 = SendTouchEvent();

  // Sequence 2.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id2 = SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  uint32_t touch_move_event_id2 = SendTouchEvent();
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id2 = SendTouchEvent();

  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id1);
  SendTouchEventACK(WebInputEvent::TouchMove, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_move_event_id1);

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
  SendTouchEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id1);

  // Ensure touch action has been set to auto, as a new touch sequence has
  // started.
  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id2);
  SendTouchEventACK(WebInputEvent::TouchMove, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_move_event_id2);
  EXPECT_EQ(3U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SendTouchEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id2);
}

// Test that TouchActionFilter::ResetTouchAction is called when a new touch
// sequence has no consumer.
TEST_F(InputRouterImplTest, TouchActionResetWhenTouchHasNoConsumer) {
  OnHasTouchEventHandlers(true);

  // Sequence 1.
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id1 = SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  uint32_t touch_move_event_id1 = SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_NONE);
  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id1);
  SendTouchEventACK(WebInputEvent::TouchMove, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_move_event_id1);

  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id1 = SendTouchEvent();

  // Sequence 2
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id2 = SendTouchEvent();
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

  SendTouchEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id1);
  SendTouchEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                    touch_press_event_id2);

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
  uint32_t touch_press_event_id = SendTouchEvent();
  MoveTouchPoint(0, 50, 50);
  uint32_t touch_move_event_id = SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_NONE);
  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id = SendTouchEvent();
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Ensure we have touch-action:none, suppressing scroll events.
  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SendTouchEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED, touch_move_event_id);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetSentMessageCountAndResetSink());

  SendTouchEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
                    touch_release_event_id);
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
  uint32_t touch_press_event_id1 = SendTouchEvent();
  OnSetTouchAction(TOUCH_ACTION_NONE);
  SendTouchEventACK(WebInputEvent::TouchStart, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_press_event_id1);

  ReleaseTouchPoint(0);
  uint32_t touch_release_event_id = SendTouchEvent();

  // Sequence 2
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id2 = SendTouchEvent();

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
  ASSERT_TRUE(WebInputEventTraits::ShouldBlockEventStream(
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

  SendTouchEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                    touch_release_event_id);
  SendTouchEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                    touch_press_event_id2);

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
  ASSERT_TRUE(WebInputEventTraits::ShouldBlockEventStream(
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
  RequestNotificationWhenFlushed();
  EXPECT_EQ(1U, GetAndResetDidFlushCount());
  EXPECT_FALSE(HasPendingEvents());

  // Queue a TouchStart.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  EXPECT_TRUE(HasPendingEvents());

  // DidFlush should be called only after the event is ack'ed.
  RequestNotificationWhenFlushed();
  EXPECT_EQ(0U, GetAndResetDidFlushCount());
  SendTouchEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED, touch_press_event_id);
  EXPECT_EQ(1U, GetAndResetDidFlushCount());

  // Ensure different types of enqueued events will prevent the DidFlush call
  // until all such events have been fully dispatched.
  MoveTouchPoint(0, 50, 50);
  uint32_t touch_move_event_id = SendTouchEvent();
  ASSERT_TRUE(HasPendingEvents());
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GesturePinchUpdate,
                       blink::WebGestureDeviceTouchscreen);
  RequestNotificationWhenFlushed();
  EXPECT_EQ(0U, GetAndResetDidFlushCount());

  // Repeated flush calls should have no effect.
  RequestNotificationWhenFlushed();
  EXPECT_EQ(0U, GetAndResetDidFlushCount());

  // There are still pending gestures.
  SendTouchEventACK(WebInputEvent::TouchMove,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED, touch_move_event_id);
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

// Test that the router will call the client's |DidFlush| after all fling
// animations have completed.
TEST_F(InputRouterImplTest, InputFlushAfterFling) {
  EXPECT_FALSE(HasPendingEvents());

  // Simulate a fling.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GestureFlingStart,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_TRUE(HasPendingEvents());

  // If the fling is unconsumed, the flush is complete.
  RequestNotificationWhenFlushed();
  EXPECT_EQ(0U, GetAndResetDidFlushCount());
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  SendInputEventACK(WebInputEvent::GestureFlingStart,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(HasPendingEvents());
  EXPECT_EQ(1U, GetAndResetDidFlushCount());

  // Simulate a second fling.
  SimulateGestureEvent(WebInputEvent::GestureFlingStart,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_TRUE(HasPendingEvents());

  // If the fling is consumed, the flush is complete only when the renderer
  // reports that is has ended.
  RequestNotificationWhenFlushed();
  EXPECT_EQ(0U, GetAndResetDidFlushCount());
  SendInputEventACK(WebInputEvent::GestureFlingStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_TRUE(HasPendingEvents());
  EXPECT_EQ(0U, GetAndResetDidFlushCount());

  // The fling end notification should signal that the router is flushed.
  input_router()->OnMessageReceived(InputHostMsg_DidStopFlinging(0));
  EXPECT_EQ(1U, GetAndResetDidFlushCount());

  // Even flings consumed by the client require a fling-end notification.
  client_->set_filter_state(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GestureFlingStart,
                       blink::WebGestureDeviceTouchscreen);
  ASSERT_TRUE(HasPendingEvents());
  RequestNotificationWhenFlushed();
  EXPECT_EQ(0U, GetAndResetDidFlushCount());
  input_router()->OnMessageReceived(InputHostMsg_DidStopFlinging(0));
  EXPECT_EQ(1U, GetAndResetDidFlushCount());
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
  ASSERT_EQ(WebInputEvent::GesturePinchUpdate, input_event->type);
  const WebGestureEvent* gesture_event =
      static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(20, gesture_event->x);
  EXPECT_EQ(25, gesture_event->y);
  EXPECT_EQ(20, gesture_event->globalX);
  EXPECT_EQ(25, gesture_event->globalY);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Indicate that the wheel event was unhandled.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
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
  ASSERT_EQ(WebInputEvent::GesturePinchUpdate, input_event->type);
  gesture_event = static_cast<const WebGestureEvent*>(input_event);
  EXPECT_EQ(1U, GetSentMessageCountAndResetSink());

  // Indicate that the wheel event was handled this time.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Check that the correct HANDLED pinch event was received.
  EXPECT_EQ(1U, ack_handler_->GetAndResetAckCount());
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, ack_handler_->ack_event_type());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, ack_handler_->ack_state());
  EXPECT_FLOAT_EQ(0.3f,
                  ack_handler_->acked_gesture_event().data.pinchUpdate.scale);
}

// Test proper handling of touchpad Gesture{Pinch,Scroll}Update sequences.
TEST_F(InputRouterImplTest, TouchpadPinchAndScrollUpdate) {
  // The first scroll should be sent immediately.
  SimulateGestureScrollUpdateEvent(1.5f, 0.f, 0,
                                   blink::WebGestureDeviceTouchpad);
  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchpad);
  ASSERT_EQ(1U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1, client_->in_flight_event_count());

  // Subsequent scroll and pinch events should remain queued, coalescing as
  // more trackpad events arrive.
  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::WebGestureDeviceTouchpad);
  ASSERT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureScrollUpdateEvent(1.5f, 1.5f, 0,
                                   blink::WebGestureDeviceTouchpad);
  ASSERT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGesturePinchUpdateEvent(1.5f, 20, 25, 0,
                                  blink::WebGestureDeviceTouchpad);
  ASSERT_EQ(0U, GetSentMessageCountAndResetSink());
  EXPECT_EQ(1, client_->in_flight_event_count());

  SimulateGestureScrollUpdateEvent(0.f, 1.5f, 0,
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
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
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

  SimulateWheelEvent(0, 0, 3, 0, 0, false);
  InputEventAck ack(WebInputEvent::MouseWheel,
                    INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
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

// Tests that touch event stream validation passes when events are filtered
// out. See crbug.com/581231 for details.
TEST_F(InputRouterImplTest, TouchValidationPassesWithFilteredInputEvents) {
  // Touch sequence with touch handler.
  OnHasTouchEventHandlers(true);
  PressTouchPoint(1, 1);
  uint32_t touch_press_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                    touch_press_event_id);

  PressTouchPoint(1, 1);
  touch_press_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                    touch_press_event_id);

  // This event will be filtered out, since no consumer exists.
  ReleaseTouchPoint(1);
  uint32_t touch_release_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
                    touch_release_event_id);

  // If the validator didn't see the filtered out release event, it will crash
  // now, upon seeing a press for a touch which it believes to be still pressed.
  PressTouchPoint(1, 1);
  touch_press_event_id = SendTouchEvent();
  SendTouchEventACK(WebInputEvent::TouchStart,
                    INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS,
                    touch_press_event_id);
}

namespace {

class InputRouterImplScaleEventTest : public InputRouterImplTest {
 public:
  InputRouterImplScaleEventTest() {}

  void SetUp() override {
    InputRouterImplTest::SetUp();
    input_router_->SetDeviceScaleFactor(2.f);
  }

  template <typename T>
  const T* GetSentWebInputEvent() const {
    EXPECT_EQ(1u, process_->sink().message_count());

    InputMsg_HandleInputEvent::Schema::Param param;
    InputMsg_HandleInputEvent::Read(process_->sink().GetMessageAt(0), &param);
    return static_cast<const T*>(std::get<0>(param));
  }

  template <typename T>
  const T* GetFilterWebInputEvent() const {
    return static_cast<const T*>(client_->last_filter_event());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleEventTest);
};

class InputRouterImplScaleMouseEventTest
    : public InputRouterImplScaleEventTest {
 public:
  InputRouterImplScaleMouseEventTest() {}

  void RunMouseEventTest(const std::string& name, WebInputEvent::Type type) {
    SCOPED_TRACE(name);
    SimulateMouseEvent(type, 10, 10);
    const WebMouseEvent* sent_event = GetSentWebInputEvent<WebMouseEvent>();
    EXPECT_EQ(20, sent_event->x);
    EXPECT_EQ(20, sent_event->y);

    const WebMouseEvent* filter_event = GetFilterWebInputEvent<WebMouseEvent>();
    EXPECT_EQ(10, filter_event->x);
    EXPECT_EQ(10, filter_event->y);

    process_->sink().ClearMessages();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleMouseEventTest);
};

}  // namespace

TEST_F(InputRouterImplScaleMouseEventTest, ScaleMouseEventTest) {
  RunMouseEventTest("Enter", WebInputEvent::MouseEnter);
  RunMouseEventTest("Down", WebInputEvent::MouseDown);
  RunMouseEventTest("Move", WebInputEvent::MouseMove);
  RunMouseEventTest("Up", WebInputEvent::MouseUp);
}

TEST_F(InputRouterImplScaleEventTest, ScaleMouseWheelEventTest) {
  ASSERT_EQ(0u, process_->sink().message_count());
  SimulateWheelEvent(5, 5, 10, 10, 0, false);
  ASSERT_EQ(1u, process_->sink().message_count());

  const WebMouseWheelEvent* sent_event =
      GetSentWebInputEvent<WebMouseWheelEvent>();
  EXPECT_EQ(10, sent_event->x);
  EXPECT_EQ(10, sent_event->y);
  EXPECT_EQ(20, sent_event->deltaX);
  EXPECT_EQ(20, sent_event->deltaY);
  EXPECT_EQ(2, sent_event->wheelTicksX);
  EXPECT_EQ(2, sent_event->wheelTicksY);

  const WebMouseWheelEvent* filter_event =
      GetFilterWebInputEvent<WebMouseWheelEvent>();
  EXPECT_EQ(5, filter_event->x);
  EXPECT_EQ(5, filter_event->y);
  EXPECT_EQ(10, filter_event->deltaX);
  EXPECT_EQ(10, filter_event->deltaY);
  EXPECT_EQ(1, filter_event->wheelTicksX);
  EXPECT_EQ(1, filter_event->wheelTicksY);

  EXPECT_EQ(sent_event->accelerationRatioX, filter_event->accelerationRatioX);
  EXPECT_EQ(sent_event->accelerationRatioY, filter_event->accelerationRatioY);
}

namespace {

class InputRouterImplScaleTouchEventTest
    : public InputRouterImplScaleEventTest {
 public:
  InputRouterImplScaleTouchEventTest() {}

  // Test tests if two finger touch event at (10, 20) and (100, 200) are
  // properly scaled. The touch event must be generated ans flushed into
  // the message sink prior to this method.
  void RunTouchEventTest(const std::string& name, WebTouchPoint::State state) {
    SCOPED_TRACE(name);
    ASSERT_EQ(1u, process_->sink().message_count());
    const WebTouchEvent* sent_event = GetSentWebInputEvent<WebTouchEvent>();
    ASSERT_EQ(2u, sent_event->touchesLength);
    EXPECT_EQ(state, sent_event->touches[0].state);
    EXPECT_EQ(20, sent_event->touches[0].position.x);
    EXPECT_EQ(40, sent_event->touches[0].position.y);
    EXPECT_EQ(10, sent_event->touches[0].screenPosition.x);
    EXPECT_EQ(20, sent_event->touches[0].screenPosition.y);
    EXPECT_EQ(2, sent_event->touches[0].radiusX);
    EXPECT_EQ(2, sent_event->touches[0].radiusY);

    EXPECT_EQ(200, sent_event->touches[1].position.x);
    EXPECT_EQ(400, sent_event->touches[1].position.y);
    EXPECT_EQ(100, sent_event->touches[1].screenPosition.x);
    EXPECT_EQ(200, sent_event->touches[1].screenPosition.y);
    EXPECT_EQ(2, sent_event->touches[1].radiusX);
    EXPECT_EQ(2, sent_event->touches[1].radiusY);

    const WebTouchEvent* filter_event = GetFilterWebInputEvent<WebTouchEvent>();
    ASSERT_EQ(2u, filter_event->touchesLength);
    EXPECT_EQ(10, filter_event->touches[0].position.x);
    EXPECT_EQ(20, filter_event->touches[0].position.y);
    EXPECT_EQ(10, filter_event->touches[0].screenPosition.x);
    EXPECT_EQ(20, filter_event->touches[0].screenPosition.y);
    EXPECT_EQ(1, filter_event->touches[0].radiusX);
    EXPECT_EQ(1, filter_event->touches[0].radiusY);

    EXPECT_EQ(100, filter_event->touches[1].position.x);
    EXPECT_EQ(200, filter_event->touches[1].position.y);
    EXPECT_EQ(100, filter_event->touches[1].screenPosition.x);
    EXPECT_EQ(200, filter_event->touches[1].screenPosition.y);
    EXPECT_EQ(1, filter_event->touches[1].radiusX);
    EXPECT_EQ(1, filter_event->touches[1].radiusY);
  }

  void FlushTouchEvent(WebInputEvent::Type type) {
    uint32_t touch_event_id = SendTouchEvent();
    SendTouchEventACK(type, INPUT_EVENT_ACK_STATE_CONSUMED, touch_event_id);
    ASSERT_TRUE(TouchEventQueueEmpty());
    ASSERT_NE(0u, process_->sink().message_count());
  }

  void ReleaseTouchPointAndAck(int index) {
    ReleaseTouchPoint(index);
    int release_event_id = SendTouchEvent();
    SendTouchEventACK(WebInputEvent::TouchEnd, INPUT_EVENT_ACK_STATE_CONSUMED,
                      release_event_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleTouchEventTest);
};

}  // namespace

TEST_F(InputRouterImplScaleTouchEventTest, ScaleTouchEventTest) {
  // Press
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::TouchStart);

  RunTouchEventTest("Press", WebTouchPoint::StatePressed);
  ReleaseTouchPointAndAck(1);
  ReleaseTouchPointAndAck(0);
  EXPECT_EQ(3u, GetSentMessageCountAndResetSink());

  // Move
  PressTouchPoint(0, 0);
  PressTouchPoint(0, 0);
  FlushTouchEvent(WebInputEvent::TouchStart);
  process_->sink().ClearMessages();

  MoveTouchPoint(0, 10, 20);
  MoveTouchPoint(1, 100, 200);
  FlushTouchEvent(WebInputEvent::TouchMove);
  RunTouchEventTest("Move", WebTouchPoint::StateMoved);
  ReleaseTouchPointAndAck(1);
  ReleaseTouchPointAndAck(0);
  EXPECT_EQ(3u, GetSentMessageCountAndResetSink());

  // Release
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::TouchMove);
  process_->sink().ClearMessages();

  ReleaseTouchPoint(0);
  ReleaseTouchPoint(1);
  FlushTouchEvent(WebInputEvent::TouchEnd);
  RunTouchEventTest("Release", WebTouchPoint::StateReleased);

  // Cancel
  PressTouchPoint(10, 20);
  PressTouchPoint(100, 200);
  FlushTouchEvent(WebInputEvent::TouchStart);
  process_->sink().ClearMessages();

  CancelTouchPoint(0);
  CancelTouchPoint(1);
  FlushTouchEvent(WebInputEvent::TouchCancel);
  RunTouchEventTest("Cancel", WebTouchPoint::StateCancelled);
}

namespace {

class InputRouterImplScaleGestureEventTest
    : public InputRouterImplScaleEventTest {
 public:
  InputRouterImplScaleGestureEventTest() {}

  WebGestureEvent BuildGestureEvent(WebInputEvent::Type type,
                                    const gfx::Point& point) {
    WebGestureEvent event = SyntheticWebGestureEventBuilder::Build(
        type, blink::WebGestureDeviceTouchpad);
    event.globalX = event.x = point.x();
    event.globalY = event.y = point.y();
    return event;
  }

  void TestTap(const std::string& name, WebInputEvent::Type type) {
    SCOPED_TRACE(name);
    const gfx::Point orig(10, 20), scaled(20, 40);
    WebGestureEvent event = BuildGestureEvent(type, orig);
    event.data.tap.width = 30;
    event.data.tap.height = 40;
    SimulateGestureEvent(event);
    FlushGestureEvent(type);

    const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
    TestLocationInSentEvent(sent_event, orig, scaled);
    EXPECT_EQ(60, sent_event->data.tap.width);
    EXPECT_EQ(80, sent_event->data.tap.height);

    const WebGestureEvent* filter_event =
        GetFilterWebInputEvent<WebGestureEvent>();
    TestLocationInFilterEvent(filter_event, orig);
    EXPECT_EQ(30, filter_event->data.tap.width);
    EXPECT_EQ(40, filter_event->data.tap.height);
    process_->sink().ClearMessages();
  }

  void TestLongPress(const std::string& name, WebInputEvent::Type type) {
    const gfx::Point orig(10, 20), scaled(20, 40);
    WebGestureEvent event = BuildGestureEvent(type, orig);
    event.data.longPress.width = 30;
    event.data.longPress.height = 40;
    SimulateGestureEvent(event);
    FlushGestureEvent(type);
    const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
    TestLocationInSentEvent(sent_event, orig, scaled);
    EXPECT_EQ(60, sent_event->data.longPress.width);
    EXPECT_EQ(80, sent_event->data.longPress.height);

    const WebGestureEvent* filter_event =
        GetFilterWebInputEvent<WebGestureEvent>();
    TestLocationInFilterEvent(filter_event, orig);
    EXPECT_EQ(30, filter_event->data.longPress.width);
    EXPECT_EQ(40, filter_event->data.longPress.height);
    process_->sink().ClearMessages();
  }

  void FlushGestureEvent(WebInputEvent::Type type) {
    SendInputEventACK(type, INPUT_EVENT_ACK_STATE_CONSUMED);
    ASSERT_NE(0u, process_->sink().message_count());
  }

  void TestLocationInSentEvent(const WebGestureEvent* sent_event,
                               const gfx::Point& orig,
                               const gfx::Point& scaled) {
    EXPECT_EQ(20, sent_event->x);
    EXPECT_EQ(40, sent_event->y);
    EXPECT_EQ(10, sent_event->globalX);
    EXPECT_EQ(20, sent_event->globalY);
  }

  void TestLocationInFilterEvent(const WebGestureEvent* filter_event,
                                 const gfx::Point& point) {
    EXPECT_EQ(10, filter_event->x);
    EXPECT_EQ(20, filter_event->y);
    EXPECT_EQ(10, filter_event->globalX);
    EXPECT_EQ(20, filter_event->globalY);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputRouterImplScaleGestureEventTest);
};

}  // namespace

TEST_F(InputRouterImplScaleGestureEventTest, GestureScrollUpdate) {
  SimulateGestureScrollUpdateEvent(10.f, 20, 0,
                                   blink::WebGestureDeviceTouchpad);
  FlushGestureEvent(WebInputEvent::GestureScrollUpdate);
  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();

  EXPECT_EQ(20.f, sent_event->data.scrollUpdate.deltaX);
  EXPECT_EQ(40.f, sent_event->data.scrollUpdate.deltaY);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(10.f, filter_event->data.scrollUpdate.deltaX);
  EXPECT_EQ(20.f, filter_event->data.scrollUpdate.deltaY);
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureScrollBegin) {
  SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildScrollBegin(
      10.f, 20.f, blink::WebGestureDeviceTouchscreen));
  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(20.f, sent_event->data.scrollBegin.deltaXHint);
  EXPECT_EQ(40.f, sent_event->data.scrollBegin.deltaYHint);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(10.f, filter_event->data.scrollBegin.deltaXHint);
  EXPECT_EQ(20.f, filter_event->data.scrollBegin.deltaYHint);
}

TEST_F(InputRouterImplScaleGestureEventTest, GesturePinchUpdate) {
  const gfx::Point orig(10, 20), scaled(20, 40);
  SimulateGesturePinchUpdateEvent(1.5f, orig.x(), orig.y(), 0,
                                  blink::WebGestureDeviceTouchpad);
  FlushGestureEvent(WebInputEvent::GesturePinchUpdate);
  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  TestLocationInSentEvent(sent_event, orig, scaled);
  EXPECT_EQ(1.5f, sent_event->data.pinchUpdate.scale);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  TestLocationInFilterEvent(filter_event, orig);
  EXPECT_EQ(1.5f, filter_event->data.pinchUpdate.scale);
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureTapDown) {
  const gfx::Point orig(10, 20), scaled(20, 40);
  WebGestureEvent event =
      BuildGestureEvent(WebInputEvent::GestureTapDown, orig);
  event.data.tapDown.width = 30;
  event.data.tapDown.height = 40;
  SimulateGestureEvent(event);
  // FlushGestureEvent(WebInputEvent::GestureTapDown);
  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  TestLocationInSentEvent(sent_event, orig, scaled);
  EXPECT_EQ(60, sent_event->data.tapDown.width);
  EXPECT_EQ(80, sent_event->data.tapDown.height);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  TestLocationInFilterEvent(filter_event, orig);
  EXPECT_EQ(30, filter_event->data.tapDown.width);
  EXPECT_EQ(40, filter_event->data.tapDown.height);
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureTapOthers) {
  TestTap("GestureDoubleTap", WebInputEvent::GestureDoubleTap);
  TestTap("GestureTap", WebInputEvent::GestureTap);
  TestTap("GestureTapUnconfirmed", WebInputEvent::GestureTapUnconfirmed);
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureShowPress) {
  const gfx::Point orig(10, 20), scaled(20, 40);
  WebGestureEvent event =
      BuildGestureEvent(WebInputEvent::GestureShowPress, orig);
  event.data.showPress.width = 30;
  event.data.showPress.height = 40;
  SimulateGestureEvent(event);

  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  TestLocationInSentEvent(sent_event, orig, scaled);
  EXPECT_EQ(60, sent_event->data.showPress.width);
  EXPECT_EQ(80, sent_event->data.showPress.height);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  TestLocationInFilterEvent(filter_event, orig);
  EXPECT_EQ(30, filter_event->data.showPress.width);
  EXPECT_EQ(40, filter_event->data.showPress.height);
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureLongPress) {
  TestLongPress("LongPress", WebInputEvent::GestureLongPress);
  TestLongPress("LongPap", WebInputEvent::GestureLongTap);
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureTwoFingerTap) {
  WebGestureEvent event =
      BuildGestureEvent(WebInputEvent::GestureTwoFingerTap, gfx::Point(10, 20));
  event.data.twoFingerTap.firstFingerWidth = 30;
  event.data.twoFingerTap.firstFingerHeight = 40;
  SimulateGestureEvent(event);

  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(20, sent_event->x);
  EXPECT_EQ(40, sent_event->y);
  EXPECT_EQ(60, sent_event->data.twoFingerTap.firstFingerWidth);
  EXPECT_EQ(80, sent_event->data.twoFingerTap.firstFingerHeight);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  EXPECT_EQ(10, filter_event->x);
  EXPECT_EQ(20, filter_event->y);
  EXPECT_EQ(30, filter_event->data.twoFingerTap.firstFingerWidth);
  EXPECT_EQ(40, filter_event->data.twoFingerTap.firstFingerHeight);
}

TEST_F(InputRouterImplScaleGestureEventTest, GestureFlingStart) {
  const gfx::Point orig(10, 20), scaled(20, 40);
  WebGestureEvent event = BuildGestureEvent(
      WebInputEvent::GestureFlingStart, orig);
  event.data.flingStart.velocityX = 30;
  event.data.flingStart.velocityY = 40;
  SimulateGestureEvent(event);

  const WebGestureEvent* sent_event = GetSentWebInputEvent<WebGestureEvent>();
  TestLocationInSentEvent(sent_event, orig, scaled);
  EXPECT_EQ(60, sent_event->data.flingStart.velocityX);
  EXPECT_EQ(80, sent_event->data.flingStart.velocityY);

  const WebGestureEvent* filter_event =
      GetFilterWebInputEvent<WebGestureEvent>();
  TestLocationInFilterEvent(filter_event, orig);
  EXPECT_EQ(30, filter_event->data.flingStart.velocityX);
  EXPECT_EQ(40, filter_event->data.flingStart.velocityY);
}

}  // namespace content
