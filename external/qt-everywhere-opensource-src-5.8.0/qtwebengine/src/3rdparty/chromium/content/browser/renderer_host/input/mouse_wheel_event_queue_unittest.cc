// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mouse_wheel_event_queue.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseWheelEvent;

namespace content {
namespace {

const float kWheelScrollX = 10;
const float kWheelScrollY = 12;
const float kWheelScrollGlobalX = 50;
const float kWheelScrollGlobalY = 72;
const int64_t kScrollEndTimeoutMs = 100;

base::TimeDelta DefaultScrollEndTimeoutDelay() {
  return base::TimeDelta::FromMilliseconds(kScrollEndTimeoutMs);
}

#define EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event)              \
  EXPECT_EQ(WebInputEvent::GestureScrollBegin, event->type); \
  EXPECT_EQ(kWheelScrollX, event->x);                        \
  EXPECT_EQ(kWheelScrollY, event->y);                        \
  EXPECT_EQ(kWheelScrollGlobalX, event->globalX);            \
  EXPECT_EQ(kWheelScrollGlobalY, event->globalY);            \
  EXPECT_EQ(scroll_units, event->data.scrollBegin.deltaHintUnits);

#define EXPECT_GESTURE_SCROLL_BEGIN(event)         \
  EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event);         \
  EXPECT_FALSE(event->data.scrollBegin.synthetic); \
  EXPECT_EQ(WebGestureEvent::UnknownMomentumPhase, \
            event->data.scrollBegin.inertialPhase);

#define EXPECT_GESTURE_SCROLL_BEGIN_WITH_PHASE(event) \
  EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event);            \
  EXPECT_FALSE(event->data.scrollBegin.synthetic);    \
  EXPECT_EQ(WebGestureEvent::NonMomentumPhase,        \
            event->data.scrollBegin.inertialPhase);

#define EXPECT_SYNTHETIC_GESTURE_SCROLL_BEGIN(event) \
  EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event);           \
  EXPECT_TRUE(event->data.scrollBegin.synthetic);    \
  EXPECT_EQ(WebGestureEvent::NonMomentumPhase,       \
            event->data.scrollBegin.inertialPhase);

#define EXPECT_INERTIAL_GESTURE_SCROLL_BEGIN(event) \
  EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event);          \
  EXPECT_FALSE(event->data.scrollBegin.synthetic);  \
  EXPECT_EQ(WebGestureEvent::MomentumPhase,         \
            event->data.scrollBegin.inertialPhase);

#define EXPECT_SYNTHETIC_INERTIAL_GESTURE_SCROLL_BEGIN(event) \
  EXPECT_GESTURE_SCROLL_BEGIN_IMPL(event);                    \
  EXPECT_TRUE(event->data.scrollBegin.synthetic);             \
  EXPECT_EQ(WebGestureEvent::MomentumPhase,                   \
            event->data.scrollBegin.inertialPhase);

#define EXPECT_GESTURE_SCROLL_UPDATE_IMPL(event)                \
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, event->type);   \
  EXPECT_EQ(scroll_units, event->data.scrollUpdate.deltaUnits); \
  EXPECT_EQ(kWheelScrollX, event->x);                           \
  EXPECT_EQ(kWheelScrollY, event->y);                           \
  EXPECT_EQ(kWheelScrollGlobalX, event->globalX);               \
  EXPECT_EQ(kWheelScrollGlobalY, event->globalY);

#define EXPECT_GESTURE_SCROLL_UPDATE(event)        \
  EXPECT_GESTURE_SCROLL_UPDATE_IMPL(event);        \
  EXPECT_EQ(WebGestureEvent::UnknownMomentumPhase, \
            event->data.scrollUpdate.inertialPhase);

#define EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(event) \
  EXPECT_GESTURE_SCROLL_UPDATE_IMPL(event);            \
  EXPECT_EQ(WebGestureEvent::NonMomentumPhase,         \
            event->data.scrollUpdate.inertialPhase);

#define EXPECT_INERTIAL_GESTURE_SCROLL_UPDATE(event) \
  EXPECT_GESTURE_SCROLL_UPDATE_IMPL(event);          \
  EXPECT_EQ(WebGestureEvent::MomentumPhase,          \
            event->data.scrollUpdate.inertialPhase);

#define EXPECT_GESTURE_SCROLL_END_IMPL(event)                \
  EXPECT_EQ(WebInputEvent::GestureScrollEnd, event->type);   \
  EXPECT_EQ(scroll_units, event->data.scrollEnd.deltaUnits); \
  EXPECT_EQ(kWheelScrollX, event->x);                        \
  EXPECT_EQ(kWheelScrollY, event->y);                        \
  EXPECT_EQ(kWheelScrollGlobalX, event->globalX);            \
  EXPECT_EQ(kWheelScrollGlobalY, event->globalY);

#define EXPECT_GESTURE_SCROLL_END(event)           \
  EXPECT_GESTURE_SCROLL_END_IMPL(event);           \
  EXPECT_FALSE(event->data.scrollEnd.synthetic);   \
  EXPECT_EQ(WebGestureEvent::UnknownMomentumPhase, \
            event->data.scrollEnd.inertialPhase);

#define EXPECT_GESTURE_SCROLL_END_WITH_PHASE(event) \
  EXPECT_GESTURE_SCROLL_END_IMPL(event);            \
  EXPECT_FALSE(event->data.scrollEnd.synthetic);    \
  EXPECT_EQ(WebGestureEvent::NonMomentumPhase,      \
            event->data.scrollEnd.inertialPhase);

#define EXPECT_SYNTHETIC_GESTURE_SCROLL_END(event) \
  EXPECT_GESTURE_SCROLL_END_IMPL(event);           \
  EXPECT_TRUE(event->data.scrollEnd.synthetic);    \
  EXPECT_EQ(WebGestureEvent::NonMomentumPhase,     \
            event->data.scrollEnd.inertialPhase);

#define EXPECT_INERTIAL_GESTURE_SCROLL_END(event) \
  EXPECT_GESTURE_SCROLL_END_IMPL(event);          \
  EXPECT_FALSE(event->data.scrollEnd.synthetic);  \
  EXPECT_EQ(WebGestureEvent::MomentumPhase,       \
            event->data.scrollEnd.inertialPhase);

#define EXPECT_SYNTHETIC_INERTIAL_GESTURE_SCROLL_END(event) \
  EXPECT_GESTURE_SCROLL_END_IMPL(event);                    \
  EXPECT_TRUE(event->data.scrollEnd.synthetic);             \
  EXPECT_EQ(WebGestureEvent::MomentumPhase,                 \
            event->data.scrollEnd.inertialPhase);

#define EXPECT_MOUSE_WHEEL(event) \
  EXPECT_EQ(WebInputEvent::MouseWheel, event->type);

}  // namespace

class MouseWheelEventQueueTest : public testing::Test,
                                 public MouseWheelEventQueueClient {
 public:
  MouseWheelEventQueueTest()
      : acked_event_count_(0),
        last_acked_event_state_(INPUT_EVENT_ACK_STATE_UNKNOWN) {
    queue_.reset(new MouseWheelEventQueue(this, kScrollEndTimeoutMs));
  }

  ~MouseWheelEventQueueTest() override {}

  // MouseWheelEventQueueClient
  void SendMouseWheelEventImmediately(
      const MouseWheelEventWithLatencyInfo& event) override {
    WebMouseWheelEvent* cloned_event = new WebMouseWheelEvent();
    std::unique_ptr<WebInputEvent> cloned_event_holder(cloned_event);
    *cloned_event = event.event;
    sent_events_.push_back(std::move(cloned_event_holder));
  }

  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& event,
      const ui::LatencyInfo& latency_info) override {
    WebGestureEvent* cloned_event = new WebGestureEvent();
    std::unique_ptr<WebInputEvent> cloned_event_holder(cloned_event);
    *cloned_event = event;
    sent_events_.push_back(std::move(cloned_event_holder));
  }

  void OnMouseWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                            InputEventAckState ack_result) override {
    ++acked_event_count_;
    last_acked_event_ = event.event;
    last_acked_event_state_ = ack_result;
  }

 protected:
  size_t queued_event_count() const { return queue_->queued_size(); }

  bool event_in_flight() const { return queue_->event_in_flight(); }

  std::vector<std::unique_ptr<WebInputEvent>>& all_sent_events() {
    return sent_events_;
  }

  const std::unique_ptr<WebInputEvent>& sent_input_event(size_t index) {
    return sent_events_[index];
  }
  const WebGestureEvent* sent_gesture_event(size_t index) {
    return static_cast<WebGestureEvent*>(sent_events_[index].get());
  }

  const WebMouseWheelEvent& acked_event() const { return last_acked_event_; }

  size_t GetAndResetSentEventCount() {
    size_t count = sent_events_.size();
    sent_events_.clear();
    return count;
  }

  size_t GetAndResetAckedEventCount() {
    size_t count = acked_event_count_;
    acked_event_count_ = 0;
    return count;
  }

  void SendMouseWheelEventAck(InputEventAckState ack_result) {
    queue_->ProcessMouseWheelAck(ack_result, ui::LatencyInfo());
  }

  void SendMouseWheel(float x,
                      float y,
                      float global_x,
                      float global_y,
                      float dX,
                      float dY,
                      int modifiers,
                      bool high_precision,
                      WebInputEvent::RailsMode rails_mode) {
    WebMouseWheelEvent event = SyntheticWebMouseWheelEventBuilder::Build(
        x, y, global_x, global_y, dX, dY, modifiers, high_precision);
    event.railsMode = rails_mode;
    queue_->QueueEvent(MouseWheelEventWithLatencyInfo(event));
  }

  void SendMouseWheel(float x,
                      float y,
                      float global_x,
                      float global_y,
                      float dX,
                      float dY,
                      int modifiers,
                      bool high_precision) {
    SendMouseWheel(x, y, global_x, global_y, dX, dY, modifiers, high_precision,
                   WebInputEvent::RailsModeFree);
  }
  void SendMouseWheelWithPhase(
      float x,
      float y,
      float global_x,
      float global_y,
      float dX,
      float dY,
      int modifiers,
      bool high_precision,
      blink::WebMouseWheelEvent::Phase phase,
      blink::WebMouseWheelEvent::Phase momentum_phase) {
    WebMouseWheelEvent event = SyntheticWebMouseWheelEventBuilder::Build(
        x, y, global_x, global_y, dX, dY, modifiers, high_precision);
    event.phase = phase;
    event.momentumPhase = momentum_phase;
    queue_->QueueEvent(MouseWheelEventWithLatencyInfo(event));
  }

  void SendGestureEvent(WebInputEvent::Type type) {
    WebGestureEvent event;
    event.type = type;
    event.sourceDevice = blink::WebGestureDeviceTouchscreen;
    queue_->OnGestureScrollEvent(
        GestureEventWithLatencyInfo(event, ui::LatencyInfo()));
  }

  static void RunTasksAndWait(base::TimeDelta delay) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(), delay);
    base::MessageLoop::current()->Run();
  }

  void GestureSendingTest(bool high_precision) {
    const WebGestureEvent::ScrollUnits scroll_units =
        high_precision ? WebGestureEvent::PrecisePixels
                       : WebGestureEvent::Pixels;
    SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                   kWheelScrollGlobalY, 1, 1, 0, high_precision);
    EXPECT_EQ(0U, queued_event_count());
    EXPECT_TRUE(event_in_flight());
    EXPECT_EQ(1U, GetAndResetSentEventCount());

    // The second mouse wheel should not be sent since one is already in
    // queue.
    SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                   kWheelScrollGlobalY, 5, 5, 0, high_precision);
    EXPECT_EQ(1U, queued_event_count());
    EXPECT_TRUE(event_in_flight());
    EXPECT_EQ(0U, GetAndResetSentEventCount());

    // Receive an ACK for the mouse wheel event and release the next
    // mouse wheel event.
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(0U, queued_event_count());
    EXPECT_TRUE(event_in_flight());
    EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
    EXPECT_EQ(1U, GetAndResetAckedEventCount());
    EXPECT_EQ(3U, all_sent_events().size());
    EXPECT_GESTURE_SCROLL_BEGIN(sent_gesture_event(0));
    EXPECT_GESTURE_SCROLL_UPDATE(sent_gesture_event(1));
    EXPECT_MOUSE_WHEEL(sent_input_event(2));
    EXPECT_EQ(3U, GetAndResetSentEventCount());

    RunTasksAndWait(DefaultScrollEndTimeoutDelay() * 2);
    EXPECT_EQ(1U, all_sent_events().size());
    EXPECT_GESTURE_SCROLL_END(sent_gesture_event(0));
  }

  void PhaseGestureSendingTest(bool high_precision) {
    const WebGestureEvent::ScrollUnits scroll_units =
        high_precision ? WebGestureEvent::PrecisePixels
                       : WebGestureEvent::Pixels;

    SendMouseWheelWithPhase(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                            kWheelScrollGlobalY, 1, 1, 0, high_precision,
                            WebMouseWheelEvent::PhaseBegan,
                            WebMouseWheelEvent::PhaseNone);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(3U, all_sent_events().size());
    EXPECT_GESTURE_SCROLL_BEGIN_WITH_PHASE(sent_gesture_event(0));
    EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(1));
    EXPECT_SYNTHETIC_GESTURE_SCROLL_END(sent_gesture_event(2));
    EXPECT_EQ(3U, GetAndResetSentEventCount());

    SendMouseWheelWithPhase(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                            kWheelScrollGlobalY, 5, 5, 0, high_precision,
                            WebMouseWheelEvent::PhaseChanged,
                            WebMouseWheelEvent::PhaseNone);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(3U, all_sent_events().size());
    EXPECT_SYNTHETIC_GESTURE_SCROLL_BEGIN(sent_gesture_event(0));
    EXPECT_GESTURE_SCROLL_UPDATE_WITH_PHASE(sent_gesture_event(1));
    EXPECT_SYNTHETIC_GESTURE_SCROLL_END(sent_gesture_event(2));
    EXPECT_EQ(3U, GetAndResetSentEventCount());

    SendMouseWheelWithPhase(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                            kWheelScrollGlobalY, 0, 0, 0, high_precision,
                            WebMouseWheelEvent::PhaseEnded,
                            WebMouseWheelEvent::PhaseNone);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(2U, all_sent_events().size());
    EXPECT_SYNTHETIC_GESTURE_SCROLL_BEGIN(sent_gesture_event(0));
    EXPECT_GESTURE_SCROLL_END_WITH_PHASE(sent_gesture_event(1));
    EXPECT_EQ(2U, GetAndResetSentEventCount());

    // Send a double phase end; OSX does it consistently.
    SendMouseWheelWithPhase(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                            kWheelScrollGlobalY, 0, 0, 0, high_precision,
                            WebMouseWheelEvent::PhaseEnded,
                            WebMouseWheelEvent::PhaseNone);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(0U, all_sent_events().size());
    EXPECT_EQ(0U, GetAndResetSentEventCount());

    SendMouseWheelWithPhase(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                            kWheelScrollGlobalY, 5, 5, 0, high_precision,
                            WebMouseWheelEvent::PhaseNone,
                            WebMouseWheelEvent::PhaseBegan);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(3U, all_sent_events().size());
    EXPECT_INERTIAL_GESTURE_SCROLL_BEGIN(sent_gesture_event(0));
    EXPECT_INERTIAL_GESTURE_SCROLL_UPDATE(sent_gesture_event(1));
    EXPECT_SYNTHETIC_INERTIAL_GESTURE_SCROLL_END(sent_gesture_event(2));
    EXPECT_EQ(3U, GetAndResetSentEventCount());

    SendMouseWheelWithPhase(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                            kWheelScrollGlobalY, 5, 5, 0, high_precision,
                            WebMouseWheelEvent::PhaseNone,
                            WebMouseWheelEvent::PhaseChanged);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(3U, all_sent_events().size());
    EXPECT_SYNTHETIC_INERTIAL_GESTURE_SCROLL_BEGIN(sent_gesture_event(0));
    EXPECT_INERTIAL_GESTURE_SCROLL_UPDATE(sent_gesture_event(1));
    EXPECT_SYNTHETIC_INERTIAL_GESTURE_SCROLL_END(sent_gesture_event(2));
    EXPECT_EQ(3U, GetAndResetSentEventCount());

    SendMouseWheelWithPhase(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                            kWheelScrollGlobalY, 0, 0, 0, high_precision,
                            WebMouseWheelEvent::PhaseNone,
                            WebMouseWheelEvent::PhaseEnded);
    EXPECT_EQ(1U, GetAndResetSentEventCount());
    SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
    EXPECT_EQ(2U, all_sent_events().size());
    EXPECT_SYNTHETIC_INERTIAL_GESTURE_SCROLL_BEGIN(sent_gesture_event(0));
    EXPECT_INERTIAL_GESTURE_SCROLL_END(sent_gesture_event(1));
    EXPECT_EQ(2U, GetAndResetSentEventCount());
  }

  std::unique_ptr<MouseWheelEventQueue> queue_;
  std::vector<std::unique_ptr<WebInputEvent>> sent_events_;
  size_t acked_event_count_;
  InputEventAckState last_acked_event_state_;
  base::MessageLoopForUI message_loop_;
  WebMouseWheelEvent last_acked_event_;
};

// Tests that mouse wheel events are queued properly.
TEST_F(MouseWheelEventQueueTest, Basic) {
  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The second mouse wheel should not be sent since one is already in queue.
  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 5, 5, 0, false);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Receive an ACK for the first mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);

  // Receive an ACK for the second mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
}

TEST_F(MouseWheelEventQueueTest, GestureSending) {
  GestureSendingTest(false);
}

TEST_F(MouseWheelEventQueueTest, GestureSendingPrecisePixels) {
  GestureSendingTest(false);
}

TEST_F(MouseWheelEventQueueTest, GestureSendingWithPhaseInformation) {
  PhaseGestureSendingTest(false);
}

TEST_F(MouseWheelEventQueueTest,
       GestureSendingWithPhaseInformationPrecisePixels) {
  PhaseGestureSendingTest(true);
}

TEST_F(MouseWheelEventQueueTest, GestureSendingInterrupted) {
  const WebGestureEvent::ScrollUnits scroll_units = WebGestureEvent::Pixels;

  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_BEGIN(sent_gesture_event(0));
  EXPECT_GESTURE_SCROLL_UPDATE(sent_gesture_event(1));
  EXPECT_EQ(2U, GetAndResetSentEventCount());

  // Ensure that a gesture scroll begin terminates the current scroll event.
  SendGestureEvent(WebInputEvent::GestureScrollBegin);
  EXPECT_EQ(1U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_END(sent_gesture_event(0));
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // New mouse wheel events won't cause gestures because a scroll
  // is already in progress by another device.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, all_sent_events().size());

  SendGestureEvent(WebInputEvent::GestureScrollEnd);
  EXPECT_EQ(0U, all_sent_events().size());

  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_BEGIN(sent_gesture_event(0));
  EXPECT_GESTURE_SCROLL_UPDATE(sent_gesture_event(1));
  EXPECT_EQ(2U, GetAndResetSentEventCount());
}

TEST_F(MouseWheelEventQueueTest, GestureRailScrolling) {
  const WebGestureEvent::ScrollUnits scroll_units = WebGestureEvent::Pixels;

  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false,
                 WebInputEvent::RailsModeHorizontal);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_BEGIN(sent_gesture_event(0));
  EXPECT_GESTURE_SCROLL_UPDATE(sent_gesture_event(1));
  EXPECT_EQ(1U, sent_gesture_event(1)->data.scrollUpdate.deltaX);
  EXPECT_EQ(0U, sent_gesture_event(1)->data.scrollUpdate.deltaY);
  EXPECT_EQ(2U, GetAndResetSentEventCount());

  RunTasksAndWait(DefaultScrollEndTimeoutDelay() * 2);
  EXPECT_EQ(1U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_END(sent_gesture_event(0));
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  SendMouseWheel(kWheelScrollX, kWheelScrollY, kWheelScrollGlobalX,
                 kWheelScrollGlobalY, 1, 1, 0, false,
                 WebInputEvent::RailsModeVertical);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(event_in_flight());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive an ACK for the mouse wheel event.
  SendMouseWheelEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(event_in_flight());
  EXPECT_EQ(WebInputEvent::MouseWheel, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_GESTURE_SCROLL_BEGIN(sent_gesture_event(0));
  EXPECT_GESTURE_SCROLL_UPDATE(sent_gesture_event(1));
  EXPECT_EQ(0U, sent_gesture_event(1)->data.scrollUpdate.deltaX);
  EXPECT_EQ(1U, sent_gesture_event(1)->data.scrollUpdate.deltaY);
  EXPECT_EQ(2U, GetAndResetSentEventCount());
}

}  // namespace content
