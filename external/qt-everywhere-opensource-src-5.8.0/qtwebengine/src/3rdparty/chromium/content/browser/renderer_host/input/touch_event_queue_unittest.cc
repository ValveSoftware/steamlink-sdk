// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touch_event_queue.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input/web_touch_event_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {
namespace {

const double kMinSecondsBetweenThrottledTouchmoves = 0.2;
const float kSlopLengthDips = 10;
const float kHalfSlopLengthDips = kSlopLengthDips / 2;

base::TimeDelta DefaultTouchTimeoutDelay() {
  return base::TimeDelta::FromMilliseconds(1);
}
}  // namespace

class TouchEventQueueTest : public testing::Test,
                            public TouchEventQueueClient {
 public:
  TouchEventQueueTest()
      : acked_event_count_(0),
        last_acked_event_state_(INPUT_EVENT_ACK_STATE_UNKNOWN),
        slop_length_dips_(0) {}

  ~TouchEventQueueTest() override {}

  // testing::Test
  void SetUp() override {
    ResetQueueWithConfig(TouchEventQueue::Config());
    sent_events_ids_.clear();
  }

  void TearDown() override { queue_.reset(); }

  // TouchEventQueueClient
  void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& event) override {
    sent_events_.push_back(event.event);
    sent_events_ids_.push_back(event.event.uniqueTouchEventId);
    if (sync_ack_result_) {
      auto sync_ack_result = std::move(sync_ack_result_);
      SendTouchEventAck(*sync_ack_result);
    }
  }

  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckState ack_result) override {
    ++acked_event_count_;
    last_acked_event_ = event.event;
    last_acked_event_state_ = ack_result;
    if (followup_touch_event_) {
      std::unique_ptr<WebTouchEvent> followup_touch_event =
          std::move(followup_touch_event_);
      SendTouchEvent(*followup_touch_event);
    }
    if (followup_gesture_event_) {
      std::unique_ptr<WebGestureEvent> followup_gesture_event =
          std::move(followup_gesture_event_);
      queue_->OnGestureScrollEvent(
          GestureEventWithLatencyInfo(*followup_gesture_event,
                                      ui::LatencyInfo()));
    }
  }

  void OnFilteringTouchEvent(const blink::WebTouchEvent& touch_event) override {
  }

 protected:
  void SetUpForTouchMoveSlopTesting(double slop_length_dips) {
    slop_length_dips_ = slop_length_dips;
  }

  void SetUpForTimeoutTesting(base::TimeDelta desktop_timeout_delay,
                              base::TimeDelta mobile_timeout_delay) {
    TouchEventQueue::Config config;
    config.desktop_touch_ack_timeout_delay = desktop_timeout_delay;
    config.mobile_touch_ack_timeout_delay = mobile_timeout_delay;
    config.touch_ack_timeout_supported = true;
    ResetQueueWithConfig(config);
  }

  void SetUpForTimeoutTesting() {
    SetUpForTimeoutTesting(DefaultTouchTimeoutDelay(),
                           DefaultTouchTimeoutDelay());
  }

  void SendTouchEvent(WebTouchEvent event) {
    if (slop_length_dips_) {
      event.movedBeyondSlopRegion = false;
      if (WebTouchEventTraits::IsTouchSequenceStart(event))
        anchor_ = event.touches[0].position;
      if (event.type == WebInputEvent::TouchMove) {
        gfx::Vector2dF delta = anchor_ - event.touches[0].position;
        if (delta.LengthSquared() > slop_length_dips_ * slop_length_dips_)
          event.movedBeyondSlopRegion = true;
      }
    } else {
      event.movedBeyondSlopRegion = event.type == WebInputEvent::TouchMove;
    }
    queue_->QueueEvent(TouchEventWithLatencyInfo(event, ui::LatencyInfo()));
  }

  void SendGestureEvent(WebInputEvent::Type type) {
    WebGestureEvent event;
    event.type = type;
    queue_->OnGestureScrollEvent(
        GestureEventWithLatencyInfo(event, ui::LatencyInfo()));
  }

  void SendTouchEventAck(InputEventAckState ack_result) {
    DCHECK(!sent_events_ids_.empty());
    queue_->ProcessTouchAck(ack_result, ui::LatencyInfo(),
                            sent_events_ids_.front());
    sent_events_ids_.pop_front();
  }

  void SendTouchEventAckWithID(InputEventAckState ack_result,
                               int unique_event_id) {
    queue_->ProcessTouchAck(ack_result, ui::LatencyInfo(), unique_event_id);
    sent_events_ids_.erase(std::remove(sent_events_ids_.begin(),
                                       sent_events_ids_.end(), unique_event_id),
                           sent_events_ids_.end());
  }

  void SendGestureEventAck(WebInputEvent::Type type,
                           InputEventAckState ack_result) {
    blink::WebGestureEvent gesture_event;
    gesture_event.type = type;
    GestureEventWithLatencyInfo event(gesture_event, ui::LatencyInfo());
    queue_->OnGestureEventAck(event, ack_result);
  }

  void SetFollowupEvent(const WebTouchEvent& event) {
    followup_touch_event_.reset(new WebTouchEvent(event));
  }

  void SetFollowupEvent(const WebGestureEvent& event) {
    followup_gesture_event_.reset(new WebGestureEvent(event));
  }

  void SetSyncAckResult(InputEventAckState sync_ack_result) {
    sync_ack_result_.reset(new InputEventAckState(sync_ack_result));
  }

  void PressTouchPoint(float x, float y) {
    touch_event_.PressPoint(x, y);
    SendTouchEvent();
  }

  void MoveTouchPoint(int index, float x, float y) {
    touch_event_.MovePoint(index, x, y);
    SendTouchEvent();
  }

  void MoveTouchPoints(int index0,
                       float x0,
                       float y0,
                       int index1,
                       float x1,
                       float y1) {
    touch_event_.MovePoint(index0, x0, y0);
    touch_event_.MovePoint(index1, x1, y1);
    SendTouchEvent();
  }

  void ChangeTouchPointRadius(int index, float radius_x, float radius_y) {
    CHECK_GE(index, 0);
    CHECK_LT(index, touch_event_.touchesLengthCap);
    WebTouchPoint& point = touch_event_.touches[index];
    point.radiusX = radius_x;
    point.radiusY = radius_y;
    touch_event_.touches[index].state = WebTouchPoint::StateMoved;
    touch_event_.movedBeyondSlopRegion = true;
    WebTouchEventTraits::ResetType(WebInputEvent::TouchMove,
                                   touch_event_.timeStampSeconds,
                                   &touch_event_);
    SendTouchEvent();
  }

  void ChangeTouchPointRotationAngle(int index, float rotation_angle) {
    CHECK_GE(index, 0);
    CHECK_LT(index, touch_event_.touchesLengthCap);
    WebTouchPoint& point = touch_event_.touches[index];
    point.rotationAngle = rotation_angle;
    touch_event_.touches[index].state = WebTouchPoint::StateMoved;
    touch_event_.movedBeyondSlopRegion = true;
    WebTouchEventTraits::ResetType(WebInputEvent::TouchMove,
                                   touch_event_.timeStampSeconds,
                                   &touch_event_);
    SendTouchEvent();
  }

  void ChangeTouchPointForce(int index, float force) {
    CHECK_GE(index, 0);
    CHECK_LT(index, touch_event_.touchesLengthCap);
    WebTouchPoint& point = touch_event_.touches[index];
    point.force = force;
    touch_event_.touches[index].state = WebTouchPoint::StateMoved;
    touch_event_.movedBeyondSlopRegion = true;
    WebTouchEventTraits::ResetType(WebInputEvent::TouchMove,
                                   touch_event_.timeStampSeconds,
                                   &touch_event_);
    SendTouchEvent();
  }

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
    SendTouchEvent();
  }

  void CancelTouchPoint(int index) {
    touch_event_.CancelPoint(index);
    SendTouchEvent();
  }

  void PrependTouchScrollNotification() {
    queue_->PrependTouchScrollNotification();
  }

  void AdvanceTouchTime(double seconds) {
    touch_event_.timeStampSeconds += seconds;
  }

  void ResetTouchEvent() {
    touch_event_ = SyntheticWebTouchEvent();
  }

  size_t GetAndResetAckedEventCount() {
    size_t count = acked_event_count_;
    acked_event_count_ = 0;
    return count;
  }

  size_t GetAndResetSentEventCount() {
    size_t count = sent_events_.size();
    sent_events_.clear();
    return count;
  }

  bool IsPendingAckTouchStart() const {
    return queue_->IsPendingAckTouchStart();
  }

  void OnHasTouchEventHandlers(bool has_handlers) {
    queue_->OnHasTouchEventHandlers(has_handlers);
  }

  void SetAckTimeoutDisabled() { queue_->SetAckTimeoutEnabled(false); }

  void SetIsMobileOptimizedSite(bool is_mobile_optimized) {
    queue_->SetIsMobileOptimizedSite(is_mobile_optimized);
  }

  bool IsTimeoutRunning() const { return queue_->IsTimeoutRunningForTesting(); }

  bool HasPendingAsyncTouchMove() const {
    return queue_->HasPendingAsyncTouchMoveForTesting();
  }

  size_t queued_event_count() const {
    return queue_->size();
  }

  const WebTouchEvent& latest_event() const {
    return queue_->GetLatestEventForTesting().event;
  }

  const WebTouchEvent& acked_event() const {
    return last_acked_event_;
  }

  const WebTouchEvent& sent_event() const {
    DCHECK(!sent_events_.empty());
    return sent_events_.back();
  }

  const std::vector<WebTouchEvent>& all_sent_events() const {
    return sent_events_;
  }

  InputEventAckState acked_event_state() const {
    return last_acked_event_state_;
  }

  static void RunTasksAndWait(base::TimeDelta delay) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitWhenIdleClosure(), delay);
    base::MessageLoop::current()->Run();
  }

  size_t uncancelable_touch_moves_pending_ack_count() const {
    return queue_->uncancelable_touch_moves_pending_ack_count();
  }

  int GetUniqueTouchEventID() { return sent_events_ids_.back(); }

 private:
  void SendTouchEvent() {
    SendTouchEvent(touch_event_);
    touch_event_.ResetPoints();
  }

  void ResetQueueWithConfig(const TouchEventQueue::Config& config) {
    queue_.reset(new TouchEventQueue(this, config));
    queue_->OnHasTouchEventHandlers(true);
  }

  std::unique_ptr<TouchEventQueue> queue_;
  size_t acked_event_count_;
  WebTouchEvent last_acked_event_;
  std::vector<WebTouchEvent> sent_events_;
  InputEventAckState last_acked_event_state_;
  SyntheticWebTouchEvent touch_event_;
  std::unique_ptr<WebTouchEvent> followup_touch_event_;
  std::unique_ptr<WebGestureEvent> followup_gesture_event_;
  std::unique_ptr<InputEventAckState> sync_ack_result_;
  double slop_length_dips_;
  gfx::PointF anchor_;
  base::MessageLoopForUI message_loop_;
  std::deque<int> sent_events_ids_;
};


// Tests that touch-events are queued properly.
TEST_F(TouchEventQueueTest, Basic) {
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The second touch should not be sent since one is already in queue.
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Receive an ACK for the first touch-event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);
  EXPECT_EQ(WebInputEvent::Blocking, acked_event().dispatchType);

  // Receive an ACK for the second touch-event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);
  EXPECT_EQ(WebInputEvent::Blocking, acked_event().dispatchType);
}

// Tests that touch-events with multiple points are queued properly.
TEST_F(TouchEventQueueTest, BasicMultiTouch) {
  const size_t kPointerCount = 10;
  for (float i = 0; i < kPointerCount; ++i)
    PressTouchPoint(i, i);

  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(kPointerCount, queued_event_count());

  for (int i = 0; i < static_cast<int>(kPointerCount); ++i)
    MoveTouchPoint(i, 1.f + i, 2.f + i);

  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  // All moves should coalesce.
  EXPECT_EQ(kPointerCount + 1, queued_event_count());

  for (int i = 0; i < static_cast<int>(kPointerCount); ++i)
    ReleaseTouchPoint(kPointerCount - 1 - i);

  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(kPointerCount * 2 + 1, queued_event_count());

  // Ack all presses.
  for (size_t i = 0; i < kPointerCount; ++i)
    SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_EQ(kPointerCount, GetAndResetAckedEventCount());
  EXPECT_EQ(kPointerCount, GetAndResetSentEventCount());

  // Ack the coalesced move.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(kPointerCount, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Ack all releases.
  for (size_t i = 0; i < kPointerCount; ++i)
    SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_EQ(kPointerCount, GetAndResetAckedEventCount());
  EXPECT_EQ(kPointerCount - 1, GetAndResetSentEventCount());
}

// Tests that the touch-queue continues delivering events for an active touch
// sequence after all handlers are removed.
TEST_F(TouchEventQueueTest, TouchesForwardedIfHandlerRemovedDuringSequence) {
  OnHasTouchEventHandlers(true);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Signal that all touch handlers have been removed.
  OnHasTouchEventHandlers(false);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Process the ack for the sent touch, ensuring that it is honored (despite
  // the touch handler having been removed).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, acked_event_state());

  // Try forwarding a new pointer. It should be forwarded as usual.
  PressTouchPoint(2, 2);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Further events for any pointer should be forwarded, even for pointers that
  // reported no consumer.
  MoveTouchPoint(1, 3, 3);
  ReleaseTouchPoint(1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Events for the first pointer, that had a handler, should be forwarded.
  MoveTouchPoint(0, 4, 4);
  ReleaseTouchPoint(0);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, acked_event_state());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, acked_event_state());
}

// Tests that addition of a touch handler during a touch sequence will not cause
// the remaining sequence to be forwarded.
TEST_F(TouchEventQueueTest, ActiveSequenceNotForwardedWhenHandlersAdded) {
  OnHasTouchEventHandlers(false);

  // Send a touch-press event while there is no handler.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());

  OnHasTouchEventHandlers(true);

  // The remaining touch sequence should not be forwarded.
  MoveTouchPoint(0, 5, 5);
  ReleaseTouchPoint(0);
  EXPECT_EQ(2U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // A new touch sequence should resume forwarding.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
}

// Tests that removal of a touch handler during a touch sequence will prevent
// the remaining sequence from being forwarded, even if another touch handler is
// registered during the same touch sequence.
TEST_F(TouchEventQueueTest, ActiveSequenceDroppedWhenHandlersRemoved) {
  // Send a touch-press event.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Queue a touch-move event.
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Unregister all touch handlers.
  OnHasTouchEventHandlers(false);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Repeated registration/unregstration of handlers should have no effect as
  // we're still awaiting the ack arrival.
  OnHasTouchEventHandlers(true);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, queued_event_count());
  OnHasTouchEventHandlers(false);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // The ack should be flush the queue.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(2U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Events should be dropped while there is no touch handler.
  MoveTouchPoint(0, 10, 10);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Simulate touch handler registration in the middle of a touch sequence.
  OnHasTouchEventHandlers(true);

  // The touch end for the interrupted sequence should be dropped.
  ReleaseTouchPoint(0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // A new touch sequence should be forwarded properly.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
}

// Tests that removal/addition of a touch handler without any intervening
// touch activity has no affect on touch forwarding.
TEST_F(TouchEventQueueTest,
       ActiveSequenceUnaffectedByRepeatedHandlerRemovalAndAddition) {
  // Send a touch-press event.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Simulate the case where the touchstart handler removes itself, and adds a
  // touchmove handler.
  OnHasTouchEventHandlers(false);
  OnHasTouchEventHandlers(true);

  // Queue a touch-move event.
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // The ack should trigger forwarding of the touchmove, as if no touch
  // handler registration changes have occurred.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());
}

// Tests that touch-events are coalesced properly in the queue.
TEST_F(TouchEventQueueTest, Coalesce) {
  // Send a touch-press event.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Send a few touch-move events, followed by a touch-release event. All the
  // touch-move events should be coalesced into a single event.
  for (float i = 5; i < 15; ++i)
    MoveTouchPoint(0, i, i);

  EXPECT_EQ(0U, GetAndResetSentEventCount());
  ReleaseTouchPoint(0);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(3U, queued_event_count());

  // ACK the press.  Coalesced touch-move events should be sent.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, acked_event_state());

  // ACK the moves.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(10U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);

  // ACK the release.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchEnd, acked_event().type);
}

// Tests that an event that has already been sent but hasn't been ack'ed yet
// doesn't get coalesced with newer events.
TEST_F(TouchEventQueueTest, SentTouchEventDoesNotCoalesce) {
  // Send a touch-press event.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Send a few touch-move events, followed by a touch-release event. All the
  // touch-move events should be coalesced into a single event.
  for (float i = 5; i < 15; ++i)
    MoveTouchPoint(0, i, i);

  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // The coalesced touch-move event has been sent to the renderer. Any new
  // touch-move event should not be coalesced with the sent event.
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(2U, queued_event_count());

  MoveTouchPoint(0, 7, 7);
  EXPECT_EQ(2U, queued_event_count());
}

// Tests that coalescing works correctly for multi-touch events.
TEST_F(TouchEventQueueTest, MultiTouch) {
  // Press the first finger.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Move the finger.
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(2U, queued_event_count());

  // Now press a second finger.
  PressTouchPoint(2, 2);
  EXPECT_EQ(3U, queued_event_count());

  // Move both fingers.
  MoveTouchPoints(0, 10, 10, 1, 20, 20);
  MoveTouchPoint(1, 20, 20);
  EXPECT_EQ(4U, queued_event_count());

  // Move only one finger now.
  MoveTouchPoint(0, 15, 15);
  EXPECT_EQ(4U, queued_event_count());

  // Move the other finger.
  MoveTouchPoint(1, 25, 25);
  EXPECT_EQ(4U, queued_event_count());

  // Make sure both fingers are marked as having been moved in the coalesced
  // event.
  const WebTouchEvent& event = latest_event();
  EXPECT_EQ(WebTouchPoint::StateMoved, event.touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, event.touches[1].state);
}

// Tests that the touch-event queue is robust to redundant acks.
TEST_F(TouchEventQueueTest, SpuriousAcksIgnored) {
  // Trigger a spurious ack.
  SendTouchEventAckWithID(INPUT_EVENT_ACK_STATE_CONSUMED, 0);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Send and ack a touch press.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Trigger a spurious ack.
  SendTouchEventAckWithID(INPUT_EVENT_ACK_STATE_CONSUMED, 3);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that touch-move events are not sent to the renderer if the preceding
// touch-press event did not have a consumer (and consequently, did not hit the
// main thread in the renderer). Also tests that all queued/coalesced touch
// events are flushed immediately when the ACK for the touch-press comes back
// with NO_CONSUMER status.
TEST_F(TouchEventQueueTest, NoConsumer) {
  // The first touch-press should reach the renderer.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The second touch should not be sent since one is already in queue.
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Receive an ACK for the first touch-event. This should release the queued
  // touch-event, but it should not be sent to the renderer.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);
  EXPECT_EQ(2U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Send a release event. This should not reach the renderer.
  ReleaseTouchPoint(0);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::TouchEnd, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Send a press-event, followed by move and release events, and another press
  // event, before the ACK for the first press event comes back. All of the
  // events should be queued first. After the NO_CONSUMER ack for the first
  // touch-press, all events upto the second touch-press should be flushed.
  PressTouchPoint(10, 10);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  MoveTouchPoint(0, 5, 5);
  MoveTouchPoint(0, 6, 5);
  ReleaseTouchPoint(0);

  PressTouchPoint(6, 5);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  // The queue should hold the first sent touch-press event, the coalesced
  // touch-move event, the touch-end event and the second touch-press event.
  EXPECT_EQ(4U, queued_event_count());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::TouchEnd, acked_event().type);
  EXPECT_EQ(4U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // ACK the second press event as NO_CONSUMER too.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Send a second press event. Even though the first touch press had
  // NO_CONSUMER, this press event should reach the renderer.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

TEST_F(TouchEventQueueTest, ConsumerIgnoreMultiFinger) {
  // Interleave three pointer press, move and release events.
  PressTouchPoint(1, 1);
  MoveTouchPoint(0, 5, 5);
  PressTouchPoint(10, 10);
  MoveTouchPoint(1, 15, 15);
  PressTouchPoint(20, 20);
  MoveTouchPoint(2, 25, 25);
  ReleaseTouchPoint(2);
  MoveTouchPoint(1, 20, 20);
  ReleaseTouchPoint(1);
  MoveTouchPoint(0, 10, 10);
  ReleaseTouchPoint(0);

  // Since the first touch-press is still pending ACK, no other event should
  // have been sent to the renderer.
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(11U, queued_event_count());

  // ACK the first press as CONSUMED. This should cause the first touch-move of
  // the first touch-point to be dispatched.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(10U, queued_event_count());

  // ACK the first move as CONSUMED.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(9U, queued_event_count());

  // ACK the second press as NO_CONSUMER_EXISTS. The second pointer's touchmove
  // should still be forwarded, despite lacking a direct consumer.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(8U, queued_event_count());

  // ACK the coalesced move as NOT_CONSUMED.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(7U, queued_event_count());

  // All remaining touch events should be forwarded, even if the third pointer
  // press also reports no consumer.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(6U, queued_event_count());

  while (queued_event_count())
    SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  EXPECT_EQ(6U, GetAndResetSentEventCount());
}

// Tests that touch-event's enqueued via a touch ack are properly handled.
TEST_F(TouchEventQueueTest, AckWithFollowupEvents) {
  // Queue a touch down.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Create a touch event that will be queued synchronously by a touch ack.
  // Note, this will be triggered by all subsequent touch acks.
  WebTouchEvent followup_event;
  followup_event.type = WebInputEvent::TouchMove;
  followup_event.touchesLength = 1;
  followup_event.touches[0].id = 0;
  followup_event.touches[0].state = WebTouchPoint::StateMoved;
  SetFollowupEvent(followup_event);

  // Receive an ACK for the press. This should cause the followup touch-move to
  // be sent to the renderer.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_CONSUMED, acked_event_state());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);

  // Queue another event.
  MoveTouchPoint(0, 2, 2);
  EXPECT_EQ(2U, queued_event_count());

  // Receive an ACK for the touch-move followup event. This should cause the
  // subsequent touch move event be sent to the renderer.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that touch-events can be synchronously ack'ed.
TEST_F(TouchEventQueueTest, SynchronousAcks) {
  // TouchStart
  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  PressTouchPoint(1, 1);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove
  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  MoveTouchPoint(0, 2, 2);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchEnd
  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  ReleaseTouchPoint(0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchCancel (first inserting a TouchStart so the TouchCancel will be sent)
  PressTouchPoint(1, 1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  CancelTouchPoint(0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that followup events triggered by an immediate ack from
// TouchEventQueue::QueueEvent() are properly handled.
TEST_F(TouchEventQueueTest, ImmediateAckWithFollowupEvents) {
  // Create a touch event that will be queued synchronously by a touch ack.
  WebTouchEvent followup_event;
  followup_event.type = WebInputEvent::TouchStart;
  followup_event.touchesLength = 1;
  followup_event.touches[0].id = 1;
  followup_event.touches[0].state = WebTouchPoint::StatePressed;
  SetFollowupEvent(followup_event);

  // Now, enqueue a stationary touch that will not be forwarded.  This should be
  // immediately ack'ed with "NO_CONSUMER_EXISTS".  The followup event should
  // then be enqueued and immediately sent to the renderer.
  WebTouchEvent stationary_event;
  stationary_event.touchesLength = 1;
  stationary_event.type = WebInputEvent::TouchMove;
  stationary_event.touches[0].id = 1;
  stationary_event.touches[0].state = WebTouchPoint::StateStationary;
  SendTouchEvent(stationary_event);

  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);
}

// Tests basic TouchEvent forwarding suppression.
TEST_F(TouchEventQueueTest, NoTouchBasic) {
  // Disable TouchEvent forwarding.
  OnHasTouchEventHandlers(false);
  PressTouchPoint(30, 5);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove should not be sent to renderer.
  MoveTouchPoint(0, 65, 10);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchEnd should not be sent to renderer.
  ReleaseTouchPoint(0);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Enable TouchEvent forwarding.
  OnHasTouchEventHandlers(true);

  PressTouchPoint(80, 10);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 80, 20);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  ReleaseTouchPoint(0);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that IsTouchStartPendingAck works correctly.
TEST_F(TouchEventQueueTest, PendingStart) {

  EXPECT_FALSE(IsPendingAckTouchStart());

  // Send the touchstart for one point (#1).
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_TRUE(IsPendingAckTouchStart());

  // Send a touchmove for that point (#2).
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_TRUE(IsPendingAckTouchStart());

  // Ack the touchstart (#1).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_FALSE(IsPendingAckTouchStart());

  // Send a touchstart for another point (#3).
  PressTouchPoint(10, 10);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_FALSE(IsPendingAckTouchStart());

  // Ack the touchmove (#2).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_TRUE(IsPendingAckTouchStart());

  // Send a touchstart for a third point (#4).
  PressTouchPoint(15, 15);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_TRUE(IsPendingAckTouchStart());

  // Ack the touchstart for the second point (#3).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_TRUE(IsPendingAckTouchStart());

  // Ack the touchstart for the third point (#4).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(IsPendingAckTouchStart());
}

// Tests that the touch timeout is started when sending certain touch types.
TEST_F(TouchEventQueueTest, TouchTimeoutTypes) {
  SetUpForTimeoutTesting();

  // Sending a TouchStart will start the timeout.
  PressTouchPoint(0, 1);
  EXPECT_TRUE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());

  // A TouchMove should start the timeout.
  MoveTouchPoint(0, 5, 5);
  EXPECT_TRUE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());

  // A TouchEnd should not start the timeout.
  ReleaseTouchPoint(0);
  EXPECT_FALSE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());

  // A TouchCancel should not start the timeout.
  PressTouchPoint(0, 1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_FALSE(IsTimeoutRunning());
  CancelTouchPoint(0);
  EXPECT_FALSE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
}

// Tests that a delayed TouchEvent ack will trigger a TouchCancel timeout,
// disabling touch forwarding until the next TouchStart is received after
// the timeout events are ack'ed.
TEST_F(TouchEventQueueTest, TouchTimeoutBasic) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  GetAndResetSentEventCount();
  GetAndResetAckedEventCount();
  PressTouchPoint(0, 1);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_TRUE(IsTimeoutRunning());

  // Delay the ack.
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);

  // The timeout should have fired, synthetically ack'ing the timed-out event.
  // TouchEvent forwarding is disabled until the ack is received for the
  // timed-out event and the future cancel event.
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Ack'ing the original event should trigger a cancel event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(WebInputEvent::TouchCancel, sent_event().type);
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Touch events should not be forwarded until we receive the cancel acks.
  MoveTouchPoint(0, 1, 1);
  ASSERT_EQ(0U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  ReleaseTouchPoint(0);
  ASSERT_EQ(0U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // The synthetic TouchCancel ack should not reach the client, but should
  // resume touch forwarding.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Subsequent events should be handled normally.
  PressTouchPoint(0, 1);
  EXPECT_EQ(WebInputEvent::TouchStart, sent_event().type);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that the timeout is never started if the renderer consumes
// a TouchEvent from the current touch sequence.
TEST_F(TouchEventQueueTest, NoTouchTimeoutIfRendererIsConsumingGesture) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  ASSERT_TRUE(IsTimeoutRunning());

  // Mark the event as consumed. This should prevent the timeout from
  // being activated on subsequent TouchEvents in this gesture.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());

  // A TouchMove should not start the timeout.
  MoveTouchPoint(0, 5, 5);
  EXPECT_FALSE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // A secondary TouchStart should not start the timeout.
  PressTouchPoint(1, 0);
  EXPECT_FALSE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // A TouchEnd should not start the timeout.
  ReleaseTouchPoint(1);
  EXPECT_FALSE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // A TouchCancel should not start the timeout.
  CancelTouchPoint(0);
  EXPECT_FALSE(IsTimeoutRunning());
}

// Tests that the timeout is never started if the renderer consumes
// a TouchEvent from the current touch sequence.
TEST_F(TouchEventQueueTest, NoTouchTimeoutIfDisabledAfterTouchStart) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  ASSERT_TRUE(IsTimeoutRunning());

  // Send the ack immediately. The timeout should not have fired.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now explicitly disable the timeout.
  SetAckTimeoutDisabled();
  EXPECT_FALSE(IsTimeoutRunning());

  // A TouchMove should not start or trigger the timeout.
  MoveTouchPoint(0, 5, 5);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that the timeout is never started if the ack is synchronous.
TEST_F(TouchEventQueueTest, NoTouchTimeoutIfAckIsSynchronous) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  ASSERT_FALSE(IsTimeoutRunning());
  PressTouchPoint(0, 1);
  EXPECT_FALSE(IsTimeoutRunning());
}

// Tests that the timeout does not fire if explicitly disabled while an event
// is in-flight.
TEST_F(TouchEventQueueTest, NoTouchTimeoutIfDisabledWhileTimerIsActive) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  ASSERT_TRUE(IsTimeoutRunning());

  // Verify that disabling the timeout also turns off the timer.
  SetAckTimeoutDisabled();
  EXPECT_FALSE(IsTimeoutRunning());
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that the timeout does not fire if the delay is zero.
TEST_F(TouchEventQueueTest, NoTouchTimeoutIfTimeoutDelayIsZero) {
  SetUpForTimeoutTesting(base::TimeDelta(), base::TimeDelta());

  // As the delay is zero, timeout behavior should be disabled.
  PressTouchPoint(0, 1);
  EXPECT_FALSE(IsTimeoutRunning());
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that timeout delays for mobile sites take effect when appropriate.
TEST_F(TouchEventQueueTest, TouchTimeoutConfiguredForMobile) {
  base::TimeDelta desktop_delay = DefaultTouchTimeoutDelay();
  base::TimeDelta mobile_delay = base::TimeDelta();
  SetUpForTimeoutTesting(desktop_delay, mobile_delay);

  // The desktop delay is non-zero, allowing timeout behavior.
  SetIsMobileOptimizedSite(false);

  PressTouchPoint(0, 1);
  ASSERT_TRUE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  ReleaseTouchPoint(0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, GetAndResetAckedEventCount());
  ASSERT_FALSE(IsTimeoutRunning());

  // The mobile delay is zero, preventing timeout behavior.
  SetIsMobileOptimizedSite(true);

  PressTouchPoint(0, 1);
  EXPECT_FALSE(IsTimeoutRunning());
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that a TouchCancel timeout plays nice when the timed out touch stream
// turns into a scroll gesture sequence.
TEST_F(TouchEventQueueTest, TouchTimeoutWithFollowupGesture) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  EXPECT_TRUE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The cancelled sequence may turn into a scroll gesture.
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);

  // Delay the ack.
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);

  // The timeout should have fired, disabling touch forwarding until both acks
  // are received, acking the timed out event.
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Ack the original event, triggering a TouchCancel.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Ack the cancel event.  Normally, this would resume touch forwarding,
  // but we're still within a scroll gesture so it remains disabled.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Try to forward touch events for the current sequence.
  GetAndResetSentEventCount();
  GetAndResetAckedEventCount();
  MoveTouchPoint(0, 1, 1);
  ReleaseTouchPoint(0);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, GetAndResetAckedEventCount());

  // Now end the scroll sequence, resuming touch handling.
  SendGestureEvent(blink::WebInputEvent::GestureScrollEnd);
  PressTouchPoint(0, 1);
  EXPECT_TRUE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that a TouchCancel timeout plays nice when the timed out touch stream
// turns into a scroll gesture sequence, but the original event acks are
// significantly delayed.
TEST_F(TouchEventQueueTest, TouchTimeoutWithFollowupGestureAndDelayedAck) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  EXPECT_TRUE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The cancelled sequence may turn into a scroll gesture.
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);

  // Delay the ack.
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);

  // The timeout should have fired, disabling touch forwarding until both acks
  // are received and acking the timed out event.
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Try to forward a touch event.
  GetAndResetSentEventCount();
  GetAndResetAckedEventCount();
  MoveTouchPoint(0, 1, 1);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now end the scroll sequence.  Events will not be forwarded until the two
  // outstanding touch acks are received.
  SendGestureEvent(blink::WebInputEvent::GestureScrollEnd);
  MoveTouchPoint(0, 2, 2);
  ReleaseTouchPoint(0);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, GetAndResetAckedEventCount());

  // Ack the original event, triggering a cancel.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Ack the cancel event, resuming touch forwarding.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  PressTouchPoint(0, 1);
  EXPECT_TRUE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
}

// Tests that a delayed TouchEvent ack will not trigger a TouchCancel timeout if
// the timed-out event had no consumer.
TEST_F(TouchEventQueueTest, NoCancelOnTouchTimeoutWithoutConsumer) {
  SetUpForTimeoutTesting();

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_TRUE(IsTimeoutRunning());

  // Delay the ack.
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);

  // The timeout should have fired, synthetically ack'ing the timed out event.
  // TouchEvent forwarding is disabled until the original ack is received.
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Touch events should not be forwarded until we receive the original ack.
  MoveTouchPoint(0, 1, 1);
  ReleaseTouchPoint(0);
  ASSERT_EQ(0U, GetAndResetSentEventCount());
  ASSERT_EQ(2U, GetAndResetAckedEventCount());

  // Ack'ing the original event should not trigger a cancel event, as the
  // TouchStart had no consumer.  However, it should re-enable touch forwarding.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Subsequent events should be handled normally.
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that TouchMove's are dropped if within the boundary-inclusive slop
// suppression region for an unconsumed TouchStart.
TEST_F(TouchEventQueueTest, TouchMoveSuppressionIncludingSlopBoundary) {
  SetUpForTouchMoveSlopTesting(kSlopLengthDips);

  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove's within the region should be suppressed.
  MoveTouchPoint(0, 0, kHalfSlopLengthDips);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, acked_event_state());

  MoveTouchPoint(0, kHalfSlopLengthDips, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, acked_event_state());

  MoveTouchPoint(0, -kHalfSlopLengthDips, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, acked_event_state());

  MoveTouchPoint(0, -kSlopLengthDips, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, acked_event_state());

  MoveTouchPoint(0, 0, kSlopLengthDips);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, acked_event_state());

  // As soon as a TouchMove exceeds the (Euclidean) distance, no more
  // TouchMove's should be suppressed.
  const float kFortyFiveDegreeSlopLengthXY =
      kSlopLengthDips * std::sqrt(2.f) / 2;
  MoveTouchPoint(0, kFortyFiveDegreeSlopLengthXY + .2f,
                    kFortyFiveDegreeSlopLengthXY + .2f);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Even TouchMove's within the original slop region should now be forwarded.
  MoveTouchPoint(0, 0, 0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // A new touch sequence should reset suppression.
  ReleaseTouchPoint(0);
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(2U, GetAndResetSentEventCount());
  ASSERT_EQ(2U, GetAndResetAckedEventCount());
  ASSERT_EQ(0U, queued_event_count());

  // The slop region is boundary-inclusive.
  MoveTouchPoint(0, kSlopLengthDips - 1, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, kSlopLengthDips, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that TouchMove's are not dropped within the slop suppression region if
// the touchstart was consumed.
TEST_F(TouchEventQueueTest, NoTouchMoveSuppressionAfterTouchConsumed) {
  SetUpForTouchMoveSlopTesting(kSlopLengthDips);

  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove's within the region should not be suppressed, as a touch was
  // consumed.
  MoveTouchPoint(0, 0, kHalfSlopLengthDips);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that even very small TouchMove's are not suppressed when suppression is
// disabled.
TEST_F(TouchEventQueueTest, NoTouchMoveSuppressionIfDisabled) {
  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // Small TouchMove's should not be suppressed.
  MoveTouchPoint(0, 0.001f, 0.001f);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that TouchMove's are not dropped if a secondary pointer is present
// during any movement.
TEST_F(TouchEventQueueTest, NoTouchMoveSuppressionAfterMultiTouch) {
  SetUpForTouchMoveSlopTesting(kSlopLengthDips);

  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove's within the region should be suppressed.
  MoveTouchPoint(0, 0, kHalfSlopLengthDips);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Simulate a secondary pointer press.
  PressTouchPoint(kSlopLengthDips, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove with a secondary pointer should not be suppressed.
  MoveTouchPoint(1, kSlopLengthDips+1, 0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Release the secondary pointer.
  ReleaseTouchPoint(0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove's should not should be suppressed, even with the original
  // unmoved pointer.
  MoveTouchPoint(0, 0, 0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that secondary touch points can be forwarded even if the primary touch
// point had no consumer.
TEST_F(TouchEventQueueTest, SecondaryTouchForwardedAfterPrimaryHadNoConsumer) {
  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // Events should not be forwarded, as the point had no consumer.
  MoveTouchPoint(0, 0, 15);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Simulate a secondary pointer press.
  PressTouchPoint(20, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove with a secondary pointer should not be suppressed.
  MoveTouchPoint(1, 25, 0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that secondary touch points can be forwarded after scrolling begins
// while first touch point has no consumer.
TEST_F(TouchEventQueueTest, NoForwardingAfterScrollWithNoTouchConsumers) {
  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  MoveTouchPoint(0, 20, 5);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  // The secondary pointer press should be forwarded.
  PressTouchPoint(20, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove with a secondary pointer should also be forwarded.
  MoveTouchPoint(1, 25, 0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

TEST_F(TouchEventQueueTest, AsyncTouch) {
  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  for (int i = 0; i < 3; ++i) {
   SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                       INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

   MoveTouchPoint(0, 10, 5+i);
   SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
   EXPECT_FALSE(HasPendingAsyncTouchMove());
   EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
   EXPECT_EQ(0U, queued_event_count());
   EXPECT_EQ(1U, GetAndResetSentEventCount());

   // Consuming a scroll event will throttle subsequent touchmoves.
   SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                       INPUT_EVENT_ACK_STATE_CONSUMED);
   MoveTouchPoint(0, 10, 7+i);
   EXPECT_TRUE(HasPendingAsyncTouchMove());
   EXPECT_EQ(0U, queued_event_count());
   EXPECT_EQ(0U, GetAndResetSentEventCount());
  }
}

// Ensure that touchmove's are appropriately throttled during a typical
// scroll sequences that transitions between scrolls consumed and unconsumed.
TEST_F(TouchEventQueueTest, AsyncTouchThrottledAfterScroll) {
  // Process a TouchStart
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now send the first touch move and associated GestureScrollBegin.
  MoveTouchPoint(0, 0, 5);
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  SendGestureEventAck(WebInputEvent::GestureScrollBegin,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Send the second touch move and associated GestureScrollUpdate, but don't
  // ACK the gesture event yet.
  MoveTouchPoint(0, 0, 50);
  followup_scroll.type = WebInputEvent::GestureScrollUpdate;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now queue a second touchmove and verify it's not (yet) dispatched.
  MoveTouchPoint(0, 0, 100);
  SetFollowupEvent(followup_scroll);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Queuing the final touchend should flush the pending async touchmove. In
  // this case, we will first dispatch an async touchmove and then a touchend.
  // For the async touchmove, we will not send ack again.
  ReleaseTouchPoint(0);
  followup_scroll.type = WebInputEvent::GestureScrollEnd;
  SetFollowupEvent(followup_scroll);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_EQ(WebInputEvent::TouchMove, all_sent_events()[0].type);
  EXPECT_NE(WebInputEvent::Blocking, all_sent_events()[0].dispatchType);
  EXPECT_EQ(WebInputEvent::TouchEnd, all_sent_events()[1].type);
  EXPECT_NE(WebInputEvent::Blocking, all_sent_events()[1].dispatchType);
  EXPECT_EQ(2U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Ack the touchend.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now mark the scrolls as not consumed (which would cause future touchmoves
  // in the active sequence to be sent if there was one).
  SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Start a new touch sequence and verify that throttling has been reset.
  // Touch moves after the start of scrolling will again be throttled.
  PressTouchPoint(0, 0);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  MoveTouchPoint(0, 0, 5);
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 0, 6);
  followup_scroll.type = WebInputEvent::GestureScrollUpdate;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 0, 10);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Subsequent touchmove's should be deferred.
  MoveTouchPoint(0, 0, 25);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, uncancelable_touch_moves_pending_ack_count());

  // The pending touchmove should be flushed with the the new touchmove if
  // sufficient time has passed and ack to the client.
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  MoveTouchPoint(0, 0, 15);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Non-touchmove events should always flush any pending touchmove events. In
  // this case, we will first dispatch an async touchmove and then a
  // touchstart. For the async touchmove, we will not send ack again.
  MoveTouchPoint(0, 0, 25);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  PressTouchPoint(30, 30);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_EQ(WebInputEvent::TouchMove, all_sent_events()[0].type);
  EXPECT_NE(WebInputEvent::Blocking, all_sent_events()[0].dispatchType);
  EXPECT_EQ(WebInputEvent::TouchStart, all_sent_events()[1].type);
  EXPECT_EQ(WebInputEvent::Blocking, all_sent_events()[1].dispatchType);
  EXPECT_EQ(2U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Ack the touchstart.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Send a secondary touchmove.
  MoveTouchPoint(1, 0, 25);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // An unconsumed scroll should resume synchronous touch handling.
  SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // The pending touchmove should be coalesced with the next (now synchronous)
  // touchmove.
  MoveTouchPoint(0, 0, 26);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(WebInputEvent::TouchMove, sent_event().type);
  EXPECT_EQ(WebTouchPoint::StateMoved, sent_event().touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, sent_event().touches[1].state);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Subsequent touches will queue until the preceding, synchronous touches are
  // ack'ed.
  ReleaseTouchPoint(1);
  EXPECT_EQ(2U, queued_event_count());
  ReleaseTouchPoint(0);
  EXPECT_EQ(3U, queued_event_count());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(WebInputEvent::TouchEnd, sent_event().type);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(WebInputEvent::TouchEnd, sent_event().type);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

TEST_F(TouchEventQueueTest, AsyncTouchFlushedByTouchEnd) {
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Initiate async touchmove dispatch after the start of a scroll sequence.
  MoveTouchPoint(0, 0, 5);
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 0, 10);
  followup_scroll.type = WebInputEvent::GestureScrollUpdate;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now queue a second touchmove and verify it's not (yet) dispatched.
  MoveTouchPoint(0, 0, 100);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Return the touch sequence to the original touchstart position. Note that
  // this (0, 0) touchmove will coalesce with the previous (0, 100) touchmove.
  MoveTouchPoint(0, 0, 0);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Queuing the final touchend should flush the pending, async touchmove. In
  // this case, we will first dispatch an async touchmove and then a touchend.
  // For the async touchmove, we will not send ack again.
  ReleaseTouchPoint(0);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_EQ(WebInputEvent::TouchMove, all_sent_events()[0].type);
  EXPECT_NE(WebInputEvent::Blocking, all_sent_events()[0].dispatchType);
  EXPECT_EQ(0, all_sent_events()[0].touches[0].position.x);
  EXPECT_EQ(0, all_sent_events()[0].touches[0].position.y);
  EXPECT_EQ(WebInputEvent::TouchEnd, all_sent_events()[1].type);
  EXPECT_NE(WebInputEvent::Blocking, all_sent_events()[1].dispatchType);
  EXPECT_EQ(2U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Ensure that async touch dispatch and touch ack timeout interactions work
// appropriately.
TEST_F(TouchEventQueueTest, AsyncTouchWithAckTimeout) {
  SetUpForTimeoutTesting();

  // The touchstart should start the timeout.
  PressTouchPoint(0, 0);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_TRUE(IsTimeoutRunning());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_FALSE(IsTimeoutRunning());

  // The start of a scroll gesture should trigger async touch event dispatch.
  MoveTouchPoint(0, 1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_TRUE(IsTimeoutRunning());
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_CONSUMED);

  // An async touch should fire after the throttling interval has expired, but
  // it should not start the touch ack timeout.
  MoveTouchPoint(0, 5, 5);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  MoveTouchPoint(0, 5, 5);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // An unconsumed scroll event will resume synchronous touchmoves, which are
  // subject to the ack timeout.
  SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  MoveTouchPoint(0, 20, 5);
  EXPECT_TRUE(IsTimeoutRunning());
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The timeout should fire, disabling touch forwarding until both acks are
  // received and acking the timed out event.
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Ack'ing the original event should trigger a cancel event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Subsequent touchmove's should not be forwarded, even as the scroll gesture
  // goes from unconsumed to consumed.
  SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  MoveTouchPoint(0, 20, 5);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  MoveTouchPoint(0, 25, 5);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
}

// Ensure that if the touch ack for an async touchmove triggers a follow-up
// touch event, that follow-up touch will be forwarded appropriately.
TEST_F(TouchEventQueueTest, AsyncTouchWithTouchCancelAfterAck) {
  PressTouchPoint(0, 0);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // The start of a scroll gesture should trigger async touch event dispatch.
  MoveTouchPoint(0, 1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  SendGestureEvent(WebInputEvent::GestureScrollUpdate);

  // The async touchmove should be ack'ed immediately, but not forwarded.
  // However, because the ack triggers a touchcancel, both the pending touch and
  // the queued touchcancel should be flushed.
  WebTouchEvent followup_cancel;
  followup_cancel.type = WebInputEvent::TouchCancel;
  followup_cancel.touchesLength = 1;
  followup_cancel.touches[0].state = WebTouchPoint::StateCancelled;
  SetFollowupEvent(followup_cancel);
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(2U, all_sent_events().size());
  EXPECT_EQ(WebInputEvent::TouchMove, all_sent_events()[0].type);
  EXPECT_NE(WebInputEvent::Blocking, all_sent_events()[0].dispatchType);
  EXPECT_EQ(WebInputEvent::TouchCancel, all_sent_events()[1].type);
  EXPECT_NE(WebInputEvent::Blocking, all_sent_events()[1].dispatchType);
  EXPECT_EQ(2U, GetAndResetSentEventCount());
  // Sending the ack is because the async touchmove is not ready for
  // dispatching send the ack immediately.
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchCancel, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
}

// Ensure that the async touch is fully reset if the touch sequence restarts
// without properly terminating.
TEST_F(TouchEventQueueTest, AsyncTouchWithHardTouchStartReset) {
  PressTouchPoint(0, 0);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Trigger async touchmove dispatch.
  MoveTouchPoint(0, 1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());
  SendGestureEvent(WebInputEvent::GestureScrollUpdate);

  // The async touchmove should be immediately ack'ed but delivery is deferred.
  MoveTouchPoint(0, 2, 2);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);

  // The queue should be robust to hard touch restarts with a new touch
  // sequence. In this case, the deferred async touch should not be flushed
  // by the new touch sequence.
  SendGestureEvent(WebInputEvent::GestureScrollEnd);
  ResetTouchEvent();

  PressTouchPoint(0, 0);
  EXPECT_EQ(WebInputEvent::TouchStart, sent_event().type);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Ensure that even when the interval expires, we still need to wait for the
// ack sent back from render to send the next async touchmove once the scroll
// starts.
TEST_F(TouchEventQueueTest, SendNextThrottledAsyncTouchMoveAfterAck) {
  // Process a TouchStart
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Initiate async touchmove dispatch after the start of a scroll sequence.
  MoveTouchPoint(0, 0, 5);
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 0, 10);
  followup_scroll.type = WebInputEvent::GestureScrollUpdate;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // We set the next touch event time to be after the throttled interval.
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  // Dispatch the touch move event when sufficient time has passed.
  MoveTouchPoint(0, 0, 40);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  // When we dispatch an async touchmove, we do not put it back to the queue
  // any more and we will ack to client right away.
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, uncancelable_touch_moves_pending_ack_count());

  // Do not dispatch the event until throttledTouchmoves intervals expires and
  // receive an ack from render.
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  MoveTouchPoint(0, 0, 50);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, uncancelable_touch_moves_pending_ack_count());

  // Send pending_async_touch_move_ when we receive an ack back from render,
  // but we will not send an ack for pending_async_touch_move_ becasue it is
  // been acked before.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, uncancelable_touch_moves_pending_ack_count());
}

// Ensure that even when we receive the ack from render, we still need to wait
// for the interval expires to send the next async touchmove once the scroll
// starts.
TEST_F(TouchEventQueueTest, SendNextAsyncTouchMoveAfterAckAndTimeExpire) {
  // Process a TouchStart
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Initiate async touchmove dispatch after the start of a scroll sequence.
  MoveTouchPoint(0, 0, 5);
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 0, 10);
  followup_scroll.type = WebInputEvent::GestureScrollUpdate;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Dispatch the touch move event when sufficient time has passed.
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  MoveTouchPoint(0, 0, 40);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  // When we dispatch an async touchmove, we do not put it back to the queue
  // any more and we will ack to client right away.
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, uncancelable_touch_moves_pending_ack_count());

  // We receive an ack back from render but the time interval is not expired,
  // so we do not dispatch the touch move event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, uncancelable_touch_moves_pending_ack_count());
  MoveTouchPoint(0, 0, 50);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Dispatch the touch move when sufficient time has passed.
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  MoveTouchPoint(0, 0, 50);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(WebInputEvent::TouchMove, sent_event().type);
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, uncancelable_touch_moves_pending_ack_count());
}

TEST_F(TouchEventQueueTest, AsyncTouchFlushedByNonTouchMove) {
  // Process a TouchStart
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Initiate async touchmove dispatch after the start of a scroll sequence.
  MoveTouchPoint(0, 0, 5);
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 0, 10);
  followup_scroll.type = WebInputEvent::GestureScrollUpdate;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Dispatch the touch move when sufficient time has passed.
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  MoveTouchPoint(0, 0, 40);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  // When we dispatch an async touchmove, we do not put it back to the queue
  // any more and we will ack to client right away.
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, uncancelable_touch_moves_pending_ack_count());

  for (int i = 0; i < 3; ++i) {
    // We throttle the touchmoves, put it in the pending_async_touch_move_,
    // do not dispatch it.
    MoveTouchPoint(0, 10 + 10 * i, 10 + 10 * i);
    EXPECT_TRUE(HasPendingAsyncTouchMove());
    EXPECT_EQ(0U, queued_event_count());
    EXPECT_EQ(0U, GetAndResetSentEventCount());
    EXPECT_EQ(1U, GetAndResetAckedEventCount());
    EXPECT_EQ(static_cast<size_t>(i + 1),
              uncancelable_touch_moves_pending_ack_count());

    // Send touchstart will flush pending_async_touch_move_, and increase the
    // count. In this case, we will first dispatch an async touchmove and
    // then a touchstart. For the async touchmove, we will not send ack again.
    PressTouchPoint(30, 30);
    EXPECT_FALSE(HasPendingAsyncTouchMove());
    EXPECT_EQ(2U, all_sent_events().size());
    EXPECT_EQ(WebInputEvent::TouchMove, all_sent_events()[0].type);
    EXPECT_NE(WebInputEvent::Blocking, all_sent_events()[0].dispatchType);
    EXPECT_EQ(10 + 10 * i, all_sent_events()[0].touches[0].position.x);
    EXPECT_EQ(10 + 10 * i, all_sent_events()[0].touches[0].position.y);
    EXPECT_EQ(static_cast<size_t>(i + 2),
              uncancelable_touch_moves_pending_ack_count());
    EXPECT_EQ(WebInputEvent::TouchStart, all_sent_events()[1].type);
    EXPECT_EQ(WebInputEvent::Blocking, all_sent_events()[1].dispatchType);
    EXPECT_EQ(2U, GetAndResetSentEventCount());
    EXPECT_EQ(0U, GetAndResetAckedEventCount());

    SendTouchEventAckWithID(INPUT_EVENT_ACK_STATE_NOT_CONSUMED,
                            GetUniqueTouchEventID());
    EXPECT_EQ(0U, queued_event_count());
    EXPECT_FALSE(HasPendingAsyncTouchMove());
    EXPECT_EQ(0U, GetAndResetSentEventCount());
    EXPECT_EQ(1U, GetAndResetAckedEventCount());
  }

  EXPECT_EQ(4U, uncancelable_touch_moves_pending_ack_count());

  // When we receive an ack from render we decrease the count.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(3U, uncancelable_touch_moves_pending_ack_count());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_FALSE(HasPendingAsyncTouchMove());

  // Do not dispatch the next uncancelable touchmove when we have not received
  // all the acks back from render.
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  MoveTouchPoint(0, 20, 30);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(3U, uncancelable_touch_moves_pending_ack_count());

  // Once we receive the ack from render, we do not dispatch the
  // pending_async_touchmove_ until the count is 0.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(2U, uncancelable_touch_moves_pending_ack_count());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_TRUE(HasPendingAsyncTouchMove());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // When we receive this ack from render, and the count is 0, so we can
  // dispatch the pending_async_touchmove_.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, uncancelable_touch_moves_pending_ack_count());
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchMove, sent_event().type);
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Ensure that even when we receive the ack from render, we still need to wait
// for the interval expires to send the next async touchmove once the scroll
// starts.
TEST_F(TouchEventQueueTest, DoNotIncreaseIfClientConsumeAsyncTouchMove) {
  // Process a TouchStart
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Initiate async touchmove dispatch after the start of a scroll sequence.
  MoveTouchPoint(0, 0, 5);
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 0, 10);
  followup_scroll.type = WebInputEvent::GestureScrollUpdate;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Dispatch the touch move event when sufficient time has passed.
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  MoveTouchPoint(0, 0, 40);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  // When we dispatch an async touchmove, we do not put it back to the queue
  // any more and we will ack to client right away.
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, uncancelable_touch_moves_pending_ack_count());

  // We receive an ack back from render but the time interval is not expired,
  // so we do not dispatch the touch move event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, uncancelable_touch_moves_pending_ack_count());
  MoveTouchPoint(0, 0, 50);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Dispatch the touch move when sufficient time has passed. Becasue the event
  // is consumed by client already, we would not increase the count and ack to
  // client again.
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  MoveTouchPoint(0, 0, 50);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(WebInputEvent::TouchMove, sent_event().type);
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, uncancelable_touch_moves_pending_ack_count());
}

TEST_F(TouchEventQueueTest, TouchAbsorptionWithConsumedFirstMove) {
  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 20, 5);
  SendGestureEvent(blink::WebInputEvent::GestureScrollBegin);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(2U, GetAndResetSentEventCount());

  // Even if the first touchmove event was consumed, subsequent unconsumed
  // touchmove events should trigger scrolling.
  MoveTouchPoint(0, 60, 5);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  MoveTouchPoint(0, 20, 5);
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollUpdate;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Touch move event is throttled.
  MoveTouchPoint(0, 60, 5);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
}

TEST_F(TouchEventQueueTest, TouchStartCancelableDuringScroll) {
  // Queue a touchstart and touchmove that go unconsumed, transitioning to an
  // active scroll sequence.
  PressTouchPoint(0, 1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  MoveTouchPoint(0, 20, 5);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  SendGestureEvent(blink::WebInputEvent::GestureScrollBegin);
  SendGestureEvent(blink::WebInputEvent::GestureScrollUpdate);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  // Even though scrolling has begun, touchstart events should be cancelable,
  // allowing, for example, customized pinch processing.
  PressTouchPoint(10, 11);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  // As the touch start was consumed, touchmoves should no longer be throttled.
  MoveTouchPoint(1, 11, 11);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  // With throttling disabled, touchend and touchmove events should also be
  // cancelable.
  MoveTouchPoint(1, 12, 12);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ReleaseTouchPoint(1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  // If subsequent touchmoves aren't consumed, the generated scroll events
  // will restore async touch dispatch.
  MoveTouchPoint(0, 25, 5);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendGestureEvent(blink::WebInputEvent::GestureScrollUpdate);
  EXPECT_EQ(WebInputEvent::Blocking, sent_event().dispatchType);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  MoveTouchPoint(0, 30, 5);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  ASSERT_EQ(1U, GetAndResetSentEventCount());

  // The touchend will be uncancelable during an active scroll sequence.
  ReleaseTouchPoint(0);
  EXPECT_NE(WebInputEvent::Blocking, sent_event().dispatchType);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
}

TEST_F(TouchEventQueueTest, UnseenTouchPointerIdsNotForwarded) {
  SyntheticWebTouchEvent event;
  event.PressPoint(0, 0);
  SendTouchEvent(event);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Give the touchmove a previously unseen pointer id; it should not be sent.
  int press_id = event.touches[0].id;
  event.MovePoint(0, 1, 1);
  event.touches[0].id = 7;
  SendTouchEvent(event);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Give the touchmove a valid id; it should be sent.
  event.touches[0].id = press_id;
  SendTouchEvent(event);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Do the same for release.
  event.ReleasePoint(0);
  event.touches[0].id = 11;
  SendTouchEvent(event);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Give the touchmove a valid id; it should be sent.
  event.touches[0].id = press_id;
  SendTouchEvent(event);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that touch points states are correct in TouchMove events.
TEST_F(TouchEventQueueTest, PointerStatesInTouchMove) {
  PressTouchPoint(1, 1);
  PressTouchPoint(2, 2);
  PressTouchPoint(3, 3);
  PressTouchPoint(4, 4);
  EXPECT_EQ(4U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Receive ACK for the first three touch-events.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, queued_event_count());

  // Test current touches state before sending TouchMoves.
  const WebTouchEvent& event1 = sent_event();
  EXPECT_EQ(WebInputEvent::TouchStart, event1.type);
  EXPECT_EQ(WebTouchPoint::StateStationary, event1.touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateStationary, event1.touches[1].state);
  EXPECT_EQ(WebTouchPoint::StateStationary, event1.touches[2].state);
  EXPECT_EQ(WebTouchPoint::StatePressed, event1.touches[3].state);

  // Move x-position for 1st touch, y-position for 2nd touch
  // and do not move other touches.
  MoveTouchPoints(0, 1.1f, 1.f, 1, 2.f, 20.001f);
  MoveTouchPoints(2, 3.f, 3.f, 3, 4.f, 4.f);
  EXPECT_EQ(2U, queued_event_count());

  // Receive an ACK for the last TouchPress event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // 1st TouchMove is sent. Test for touches state.
  const WebTouchEvent& event2 = sent_event();
  EXPECT_EQ(WebInputEvent::TouchMove, event2.type);
  EXPECT_EQ(WebTouchPoint::StateMoved, event2.touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, event2.touches[1].state);
  EXPECT_EQ(WebTouchPoint::StateStationary, event2.touches[2].state);
  EXPECT_EQ(WebTouchPoint::StateStationary, event2.touches[3].state);

  // Move only 4th touch but not others.
  MoveTouchPoints(0, 1.1f, 1.f, 1, 2.f, 20.001f);
  MoveTouchPoints(2, 3.f, 3.f, 3, 4.1f, 4.1f);

  // Receive an ACK for previous (1st) TouchMove.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // 2nd TouchMove is sent. Test for touches state.
  const WebTouchEvent& event3 = sent_event();
  EXPECT_EQ(WebInputEvent::TouchMove, event3.type);
  EXPECT_EQ(WebTouchPoint::StateStationary, event3.touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateStationary, event3.touches[1].state);
  EXPECT_EQ(WebTouchPoint::StateStationary, event3.touches[2].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, event3.touches[3].state);
}

// Tests that touch point state is correct in TouchMove events
// when point properties other than position changed.
TEST_F(TouchEventQueueTest, PointerStatesWhenOtherThanPositionChanged) {
  PressTouchPoint(1, 1);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Default initial radiusX/Y is (1.f, 1.f).
  // Default initial rotationAngle is 1.f.
  // Default initial force is 1.f.

  // Change touch point radius only.
  ChangeTouchPointRadius(0, 1.5f, 1.f);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // TouchMove is sent. Test for pointer state.
  const WebTouchEvent& event1 = sent_event();
  EXPECT_EQ(WebInputEvent::TouchMove, event1.type);
  EXPECT_EQ(WebTouchPoint::StateMoved, event1.touches[0].state);

  // Change touch point force.
  ChangeTouchPointForce(0, 0.9f);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // TouchMove is sent. Test for pointer state.
  const WebTouchEvent& event2 = sent_event();
  EXPECT_EQ(WebInputEvent::TouchMove, event2.type);
  EXPECT_EQ(WebTouchPoint::StateMoved, event2.touches[0].state);

  // Change touch point rotationAngle.
  ChangeTouchPointRotationAngle(0, 1.1f);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // TouchMove is sent. Test for pointer state.
  const WebTouchEvent& event3 = sent_event();
  EXPECT_EQ(WebInputEvent::TouchMove, event3.type);
  EXPECT_EQ(WebTouchPoint::StateMoved, event3.touches[0].state);

  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(4U, GetAndResetSentEventCount());
  EXPECT_EQ(4U, GetAndResetAckedEventCount());
}

// Tests that TouchMoves are filtered when none of the points are changed.
TEST_F(TouchEventQueueTest, FilterTouchMovesWhenNoPointerChanged) {
  PressTouchPoint(1, 1);
  PressTouchPoint(2, 2);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(2U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, GetAndResetAckedEventCount());

  // Move 1st touch point.
  MoveTouchPoint(0, 10, 10);
  EXPECT_EQ(1U, queued_event_count());

  // TouchMove should be allowed and test for touches state.
  const WebTouchEvent& event1 = sent_event();
  EXPECT_EQ(WebInputEvent::TouchMove, event1.type);
  EXPECT_EQ(WebTouchPoint::StateMoved, event1.touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateStationary, event1.touches[1].state);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Do not really move any touch points, but use previous values.
  MoveTouchPoint(0, 10, 10);
  ChangeTouchPointRadius(1, 1, 1);
  MoveTouchPoint(1, 2, 2);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Receive an ACK for 1st TouchMove.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  // Tries to forward TouchMove but should be filtered
  // when none of the touch points have changed.
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(4U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  // Move 2nd touch point.
  MoveTouchPoint(1, 3, 3);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());

  // TouchMove should be allowed and test for touches state.
  const WebTouchEvent& event2 = sent_event();
  EXPECT_EQ(WebInputEvent::TouchMove, event2.type);
  EXPECT_EQ(WebTouchPoint::StateStationary, event2.touches[0].state);
  EXPECT_EQ(WebTouchPoint::StateMoved, event2.touches[1].state);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that touch-scroll-notification is not pushed into an empty queue.
TEST_F(TouchEventQueueTest, TouchScrollNotificationOrder_EmptyQueue) {
  PrependTouchScrollNotification();

  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
}

// Tests touch-scroll-notification firing order when the event is placed at the
// end of touch queue because of a pending ack for the head of the queue.
TEST_F(TouchEventQueueTest, TouchScrollNotificationOrder_EndOfQueue) {
  PressTouchPoint(1, 1);

  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Send the touch-scroll-notification when 3 events are in the queue.
  PrependTouchScrollNotification();

  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Receive an ACK for the touchstart.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);
  EXPECT_EQ(1U, queued_event_count());

  // Receive an ACK for the touch-scroll-notification.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_IGNORED);

  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  EXPECT_EQ(WebInputEvent::TouchStart, all_sent_events()[0].type);
  EXPECT_EQ(WebInputEvent::TouchScrollStarted, all_sent_events()[1].type);
  EXPECT_EQ(2U, GetAndResetSentEventCount());
}

// Tests touch-scroll-notification firing order when the event is placed in the
// 2nd position in the touch queue between two events.
TEST_F(TouchEventQueueTest, TouchScrollNotificationOrder_SecondPosition) {
  PressTouchPoint(1, 1);
  MoveTouchPoint(0, 5, 5);
  ReleaseTouchPoint(0);

  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(3U, queued_event_count());

  // Send the touch-scroll-notification when 3 events are in the queue.
  PrependTouchScrollNotification();

  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(4U, queued_event_count());

  // Receive an ACK for the touchstart.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);
  EXPECT_EQ(3U, queued_event_count());

  // Receive an ACK for the touch-scroll-notification.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_IGNORED);

  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Receive an ACK for the touchmove.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);
  EXPECT_EQ(1U, queued_event_count());

  // Receive an ACK for the touchend.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);

  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchEnd, acked_event().type);
  EXPECT_EQ(0U, queued_event_count());

  EXPECT_EQ(WebInputEvent::TouchStart, all_sent_events()[0].type);
  EXPECT_EQ(WebInputEvent::TouchScrollStarted, all_sent_events()[1].type);
  EXPECT_EQ(WebInputEvent::TouchMove, all_sent_events()[2].type);
  EXPECT_EQ(WebInputEvent::TouchEnd, all_sent_events()[3].type);
  EXPECT_EQ(4U, GetAndResetSentEventCount());
}

}  // namespace content
