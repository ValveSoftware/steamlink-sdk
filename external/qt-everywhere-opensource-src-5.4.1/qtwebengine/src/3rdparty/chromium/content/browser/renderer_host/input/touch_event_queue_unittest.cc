// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/renderer_host/input/timeout_monitor.h"
#include "content/browser/renderer_host/input/touch_event_queue.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {
namespace {

const double kMinSecondsBetweenThrottledTouchmoves = 0.2;

base::TimeDelta DefaultTouchTimeoutDelay() {
  return base::TimeDelta::FromMilliseconds(1);
}
}  // namespace

class TouchEventQueueTest : public testing::Test,
                            public TouchEventQueueClient {
 public:
  TouchEventQueueTest()
      : sent_event_count_(0),
        acked_event_count_(0),
        last_acked_event_state_(INPUT_EVENT_ACK_STATE_UNKNOWN),
        slop_length_dips_(0),
        slop_includes_boundary_(true),
        touch_scrolling_mode_(TouchEventQueue::TOUCH_SCROLLING_MODE_DEFAULT) {}

  virtual ~TouchEventQueueTest() {}

  // testing::Test
  virtual void SetUp() OVERRIDE { ResetQueueWithConfig(CreateConfig()); }

  virtual void TearDown() OVERRIDE {
    queue_.reset();
  }

  // TouchEventQueueClient
  virtual void SendTouchEventImmediately(
      const TouchEventWithLatencyInfo& event) OVERRIDE {
    ++sent_event_count_;
    last_sent_event_ = event.event;
    if (sync_ack_result_)
      SendTouchEventAck(*sync_ack_result_.Pass());
  }

  virtual void OnTouchEventAck(
      const TouchEventWithLatencyInfo& event,
      InputEventAckState ack_result) OVERRIDE {
    ++acked_event_count_;
    last_acked_event_ = event.event;
    last_acked_event_state_ = ack_result;
    if (followup_touch_event_) {
      scoped_ptr<WebTouchEvent> followup_touch_event =
          followup_touch_event_.Pass();
      SendTouchEvent(*followup_touch_event);
    }
    if (followup_gesture_event_) {
      scoped_ptr<WebGestureEvent> followup_gesture_event =
          followup_gesture_event_.Pass();
      queue_->OnGestureScrollEvent(
          GestureEventWithLatencyInfo(*followup_gesture_event,
                                      ui::LatencyInfo()));
    }
  }

 protected:
  TouchEventQueue::Config CreateConfig() {
    TouchEventQueue::Config config;
    config.touch_scrolling_mode = touch_scrolling_mode_;
    config.touchmove_slop_suppression_length_dips = slop_length_dips_;
    config.touchmove_slop_suppression_region_includes_boundary =
        slop_includes_boundary_;
    return config;
  }

  void SetTouchScrollingMode(TouchEventQueue::TouchScrollingMode mode) {
    touch_scrolling_mode_ = mode;
    ResetQueueWithConfig(CreateConfig());
  }

  void SetUpForTouchMoveSlopTesting(double slop_length_dips,
                                    bool slop_includes_boundary) {
    slop_length_dips_ = slop_length_dips;
    slop_includes_boundary_ = slop_includes_boundary;
    ResetQueueWithConfig(CreateConfig());
  }

  void SetUpForTimeoutTesting(base::TimeDelta timeout_delay) {
    TouchEventQueue::Config config = CreateConfig();
    config.touch_ack_timeout_delay = timeout_delay;
    config.touch_ack_timeout_supported = true;
    ResetQueueWithConfig(config);
  }

  void SendTouchEvent(const WebTouchEvent& event) {
    queue_->QueueEvent(TouchEventWithLatencyInfo(event, ui::LatencyInfo()));
  }

  void SendGestureEvent(WebInputEvent::Type type) {
    WebGestureEvent event;
    event.type = type;
    queue_->OnGestureScrollEvent(
        GestureEventWithLatencyInfo(event, ui::LatencyInfo()));
  }

  void SendTouchEventAck(InputEventAckState ack_result) {
    queue_->ProcessTouchAck(ack_result, ui::LatencyInfo());
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

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
    SendTouchEvent();
  }

  void CancelTouchPoint(int index) {
    touch_event_.CancelPoint(index);
    SendTouchEvent();
  }

  void AdvanceTouchTime(double seconds) {
    touch_event_.timeStampSeconds += seconds;
  }

  size_t GetAndResetAckedEventCount() {
    size_t count = acked_event_count_;
    acked_event_count_ = 0;
    return count;
  }

  size_t GetAndResetSentEventCount() {
    size_t count = sent_event_count_;
    sent_event_count_ = 0;
    return count;
  }

  bool IsPendingAckTouchStart() const {
    return queue_->IsPendingAckTouchStart();
  }

  void OnHasTouchEventHandlers(bool has_handlers) {
    queue_->OnHasTouchEventHandlers(has_handlers);
  }

  void SetAckTimeoutDisabled() { queue_->SetAckTimeoutEnabled(false); }

  bool IsTimeoutEnabled() const { return queue_->ack_timeout_enabled(); }

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
    return last_sent_event_;
  }

  InputEventAckState acked_event_state() const {
    return last_acked_event_state_;
  }

  static void RunTasksAndWait(base::TimeDelta delay) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, base::MessageLoop::QuitClosure(), delay);
    base::MessageLoop::current()->Run();
  }

 private:
  void SendTouchEvent() {
    SendTouchEvent(touch_event_);
    touch_event_.ResetPoints();
  }

  void ResetQueueWithConfig(const TouchEventQueue::Config& config) {
    queue_.reset(new TouchEventQueue(this, config));
    queue_->OnHasTouchEventHandlers(true);
  }

  scoped_ptr<TouchEventQueue> queue_;
  size_t sent_event_count_;
  size_t acked_event_count_;
  WebTouchEvent last_sent_event_;
  WebTouchEvent last_acked_event_;
  InputEventAckState last_acked_event_state_;
  SyntheticWebTouchEvent touch_event_;
  scoped_ptr<WebTouchEvent> followup_touch_event_;
  scoped_ptr<WebGestureEvent> followup_gesture_event_;
  scoped_ptr<InputEventAckState> sync_ack_result_;
  double slop_length_dips_;
  bool slop_includes_boundary_;
  TouchEventQueue::TouchScrollingMode touch_scrolling_mode_;
  base::MessageLoopForUI message_loop_;
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
  EXPECT_TRUE(acked_event().cancelable);

  // Receive an ACK for the second touch-event.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);
  EXPECT_TRUE(acked_event().cancelable);
}

// Tests that the touch-queue is emptied if a page stops listening for touch
// events.
TEST_F(TouchEventQueueTest, QueueFlushedWhenHandlersRemoved) {
  OnHasTouchEventHandlers(true);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Send a touch-press event.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  ReleaseTouchPoint(0);

  // Events will be queued until the first sent event is ack'ed.
  for (int i = 5; i < 15; ++i) {
    PressTouchPoint(1, 1);
    MoveTouchPoint(0, i, i);
    ReleaseTouchPoint(0);
  }
  EXPECT_EQ(32U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  // Receive an ACK for the first touch-event. One of the queued touch-event
  // should be forwarded.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(31U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);

  // Flush the queue. The touch-event queue should now be emptied, but none of
  // the queued touch-events should be sent to the renderer.
  OnHasTouchEventHandlers(false);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(31U, GetAndResetAckedEventCount());
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

  // Touch handle deregistration should flush the queue.
  OnHasTouchEventHandlers(false);
  EXPECT_EQ(2U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // The ack should be ignored as the touch queue is now empty.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
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

// Tests that touch-events are coalesced properly in the queue.
TEST_F(TouchEventQueueTest, Coalesce) {
  // Send a touch-press event.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Send a few touch-move events, followed by a touch-release event. All the
  // touch-move events should be coalesced into a single event.
  for (int i = 5; i < 15; ++i)
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
  for (int i = 5; i < 15; ++i)
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

// Tests that if a touch-event queue is destroyed in response to a touch-event
// in the renderer, then there is no crash when the ACK for that touch-event
// comes back.
TEST_F(TouchEventQueueTest, AckAfterQueueFlushed) {
  // Send some touch-events to the renderer.
  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  MoveTouchPoint(0, 10, 10);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Receive an ACK for the press. This should cause the queued touch-move to
  // be sent to the renderer.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  OnHasTouchEventHandlers(false);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Now receive an ACK for the move.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());
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
  // Press two touch points and move them around a bit. The renderer consumes
  // the events for the first touch point, but returns NO_CONSUMER_EXISTS for
  // the second touch point.

  PressTouchPoint(1, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  MoveTouchPoint(0, 5, 5);

  PressTouchPoint(10, 10);

  MoveTouchPoint(0, 2, 2);

  MoveTouchPoint(1, 4, 10);

  MoveTouchPoints(0, 10, 10, 1, 20, 20);

  // Since the first touch-press is still pending ACK, no other event should
  // have been sent to the renderer.
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  // The queue includes the two presses, the first touch-move of the first
  // point, and a coalesced touch-move of both points.
  EXPECT_EQ(4U, queued_event_count());

  // ACK the first press as CONSUMED. This should cause the first touch-move of
  // the first touch-point to be dispatched.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(3U, queued_event_count());

  // ACK the first move as CONSUMED.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // ACK the second press as NO_CONSUMER_EXISTS. This will dequeue the coalesced
  // touch-move event (which contains both touch points). Although the second
  // touch-point does not need to be sent to the renderer, the first touch-point
  // did move, and so the coalesced touch-event will be sent to the renderer.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // ACK the coalesced move as NOT_CONSUMED.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Move just the second touch point. Because the first touch point did not
  // move, this event should not reach the renderer.
  MoveTouchPoint(1, 30, 30);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Move just the first touch point. This should reach the renderer.
  MoveTouchPoint(0, 10, 10);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Move both fingers. This event should reach the renderer (after the ACK of
  // the previous move event is received), because the first touch point did
  // move.
  MoveTouchPoints(0, 15, 15, 1, 25, 25);
  EXPECT_EQ(0U, GetAndResetSentEventCount());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());

  // Release the first finger. Then move the second finger around some, then
  // press another finger. Once the release event is ACKed, the move events of
  // the second finger should be immediately released to the view, and the
  // touch-press event should be dispatched to the renderer.
  ReleaseTouchPoint(0);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, queued_event_count());

  MoveTouchPoint(1, 40, 40);

  MoveTouchPoint(1, 50, 50);

  PressTouchPoint(1, 1);

  MoveTouchPoint(1, 30, 30);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(4U, queued_event_count());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);

  // ACK the press with NO_CONSUMED_EXISTS. This should release the queued
  // touch-move events to the view.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);

  ReleaseTouchPoint(2);
  ReleaseTouchPoint(1);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, queued_event_count());
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
  followup_event.type = WebInputEvent::TouchStart;
  followup_event.touchesLength = 1;
  followup_event.touches[0].id = 1;
  followup_event.touches[0].state = WebTouchPoint::StatePressed;
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

// Tests that no TouchEvents are sent to renderer during scrolling.
TEST_F(TouchEventQueueTest, TouchCancelOnScroll) {
  SetTouchScrollingMode(TouchEventQueue::TOUCH_SCROLLING_MODE_TOUCHCANCEL);
  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, 20, 5);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // Queue another TouchStart.
  PressTouchPoint(20, 20);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, latest_event().type);

  // GestureScrollBegin inserts a synthetic TouchCancel before the TouchStart.
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchCancel, sent_event().type);
  EXPECT_FALSE(sent_event().cancelable);
  EXPECT_EQ(WebInputEvent::TouchStart, latest_event().type);

  // Acking the TouchCancel will result in dispatch of the next TouchStart.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  // The synthetic TouchCancel should not reach client, only the TouchStart.
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, acked_event().type);

  // TouchMove should not be sent to the renderer.
  MoveTouchPoint(0, 30, 5);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  // GestureScrollUpdates should not change affect touch forwarding.
  SendGestureEvent(WebInputEvent::GestureScrollUpdate);

  // TouchEnd should not be sent to the renderer.
  ReleaseTouchPoint(0);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  ReleaseTouchPoint(0);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  // Touch events from a new gesture sequence should be forwarded normally.
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

// Tests that a scroll event will not insert a synthetic TouchCancel if there
// was no consumer for the current touch sequence.
TEST_F(TouchEventQueueTest, NoTouchCancelOnScrollIfNoConsumer) {
  SetTouchScrollingMode(TouchEventQueue::TOUCH_SCROLLING_MODE_TOUCHCANCEL);

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, sent_event().type);

  // Queue a TouchMove that turns into a GestureScrollBegin.
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  MoveTouchPoint(0, 20, 5);

  // The TouchMove has no consumer, and should be ack'ed immediately. However,
  // *no* synthetic TouchCancel should be inserted as the touch sequence
  // had no consumer.
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchStart, sent_event().type);

  // Subsequent TouchMove's should not be sent to the renderer.
  MoveTouchPoint(0, 30, 5);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  // TouchEnd should not be sent to the renderer.
  ReleaseTouchPoint(0);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  // Touch events from a new gesture sequence should be forwarded normally.
  PressTouchPoint(80, 10);
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
  SetUpForTimeoutTesting(DefaultTouchTimeoutDelay());

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
  SetUpForTimeoutTesting(DefaultTouchTimeoutDelay());

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
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(WebInputEvent::TouchCancel, sent_event().type);
  EXPECT_FALSE(sent_event().cancelable);

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
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(WebInputEvent::TouchStart, sent_event().type);
  EXPECT_TRUE(sent_event().cancelable);
}

// Tests that the timeout is never started if the renderer consumes
// a TouchEvent from the current touch sequence.
TEST_F(TouchEventQueueTest, NoTouchTimeoutIfRendererIsConsumingGesture) {
  SetUpForTimeoutTesting(DefaultTouchTimeoutDelay());

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
  SetUpForTimeoutTesting(DefaultTouchTimeoutDelay());

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  ASSERT_TRUE(IsTimeoutRunning());

  // Send the ack immediately. The timeout should not have fired.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_TRUE(IsTimeoutEnabled());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now explicitly disable the timeout.
  SetAckTimeoutDisabled();
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_FALSE(IsTimeoutEnabled());

  // A TouchMove should not start or trigger the timeout.
  MoveTouchPoint(0, 5, 5);
  EXPECT_FALSE(IsTimeoutRunning());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that the timeout is never started if the ack is synchronous.
TEST_F(TouchEventQueueTest, NoTouchTimeoutIfAckIsSynchronous) {
  SetUpForTimeoutTesting(DefaultTouchTimeoutDelay());

  // Queue a TouchStart.
  SetSyncAckResult(INPUT_EVENT_ACK_STATE_CONSUMED);
  ASSERT_FALSE(IsTimeoutRunning());
  PressTouchPoint(0, 1);
  EXPECT_FALSE(IsTimeoutRunning());
}

// Tests that the timeout is disabled if the touch handler disappears.
TEST_F(TouchEventQueueTest, NoTouchTimeoutIfTouchHandlerRemoved) {
  SetUpForTimeoutTesting(DefaultTouchTimeoutDelay());

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  ASSERT_TRUE(IsTimeoutRunning());

  // Unload the touch handler.
  OnHasTouchEventHandlers(false);
  EXPECT_FALSE(IsTimeoutRunning());
}

// Tests that the timeout does not fire if explicitly disabled while an event
// is in-flight.
TEST_F(TouchEventQueueTest, NoTouchTimeoutIfDisabledWhileTimerIsActive) {
  SetUpForTimeoutTesting(DefaultTouchTimeoutDelay());

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  ASSERT_TRUE(IsTimeoutRunning());

  // Verify that disabling the timeout also turns off the timer.
  SetAckTimeoutDisabled();
  EXPECT_FALSE(IsTimeoutRunning());
  RunTasksAndWait(DefaultTouchTimeoutDelay() * 2);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that a TouchCancel timeout plays nice when the timed out touch stream
// turns into a scroll gesture sequence.
TEST_F(TouchEventQueueTest, TouchTimeoutWithFollowupGesture) {
  SetUpForTimeoutTesting(DefaultTouchTimeoutDelay());

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
  SetUpForTimeoutTesting(DefaultTouchTimeoutDelay());

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
  SetUpForTimeoutTesting(DefaultTouchTimeoutDelay());

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
  const double kSlopLengthDips = 10.;
  const double kHalfSlopLengthDips = kSlopLengthDips / 2;
  const bool slop_includes_boundary = true;
  SetUpForTouchMoveSlopTesting(kSlopLengthDips, slop_includes_boundary);

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
  const double kFortyFiveDegreeSlopLengthXY =
      kSlopLengthDips * std::sqrt(2.) / 2.;
  MoveTouchPoint(0, kFortyFiveDegreeSlopLengthXY + .2,
                    kFortyFiveDegreeSlopLengthXY + .2);
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
  MoveTouchPoint(0, kSlopLengthDips - 1., 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, kSlopLengthDips, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that TouchMove's are dropped if within the boundary-exclusive slop
// suppression region for an unconsumed TouchStart.
TEST_F(TouchEventQueueTest, TouchMoveSuppressionExcludingSlopBoundary) {
  const double kSlopLengthDips = 10.;
  const double kHalfSlopLengthDips = kSlopLengthDips / 2;
  const bool slop_includes_boundary = false;
  SetUpForTouchMoveSlopTesting(kSlopLengthDips, slop_includes_boundary);

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

  MoveTouchPoint(0, kSlopLengthDips - 0.2f, 0);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, acked_event_state());

  // As soon as a TouchMove reaches the (Euclidean) slop distance, no more
  // TouchMove's should be suppressed.
  MoveTouchPoint(0, kSlopLengthDips, 0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  MoveTouchPoint(0, kHalfSlopLengthDips, 0);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Tests that TouchMove's are not dropped within the slop suppression region if
// the touchstart was consumed.
TEST_F(TouchEventQueueTest, NoTouchMoveSuppressionAfterTouchConsumed) {
  const double kSlopLengthDips = 10.;
  const double kHalfSlopLengthDips = kSlopLengthDips / 2;
  const bool slop_includes_boundary = true;
  SetUpForTouchMoveSlopTesting(kSlopLengthDips, slop_includes_boundary);

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

// Tests that TouchMove's are not dropped due to incorrect handling of DPI
// scaling.
TEST_F(TouchEventQueueTest, TouchMoveSuppressionWithDIPScaling) {
  const float kSlopLengthPixels = 7.f;
  const float kDPIScale = 3.f;
  const bool slop_includes_boundary = true;
  SetUpForTouchMoveSlopTesting(kSlopLengthPixels / kDPIScale,
                               slop_includes_boundary);

  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove's along the slop boundary should be suppresed.
  MoveTouchPoint(0, 0, kSlopLengthPixels / kDPIScale);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Reset the touch sequence.
  ReleaseTouchPoint(0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  GetAndResetSentEventCount();
  GetAndResetAckedEventCount();

  // Queue a TouchStart.
  PressTouchPoint(0, 0);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(1U, GetAndResetSentEventCount());
  ASSERT_EQ(1U, GetAndResetAckedEventCount());

  // TouchMove's outside the region should not be suppressed.
  const float kPixelCoordOutsideSlopRegion = kSlopLengthPixels + 0.5f;
  MoveTouchPoint(0, 0, kPixelCoordOutsideSlopRegion / kDPIScale);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
}

// Tests that TouchMove's are not dropped if a secondary pointer is present
// during any movement.
TEST_F(TouchEventQueueTest, NoTouchMoveSuppressionAfterMultiTouch) {
  const double kSlopLengthDips = 10.;
  const double kHalfSlopLengthDips = kSlopLengthDips / 2;
  const bool slop_includes_boundary = true;
  SetUpForTouchMoveSlopTesting(kSlopLengthDips, slop_includes_boundary);

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
  MoveTouchPoint(1, kSlopLengthDips, 0);
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

// Tests that no touch points will be forwarded after scrolling begins while no
// touch points have a consumer.
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

  // The secondary pointer press should not be forwarded.
  PressTouchPoint(20, 0);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());

  // Neither should any further touchmoves be forwarded.
  MoveTouchPoint(1, 25, 0);
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, acked_event_state());
}

TEST_F(TouchEventQueueTest, SyncTouchMoveDoesntCancelTouchOnScroll) {
 SetTouchScrollingMode(TouchEventQueue::TOUCH_SCROLLING_MODE_SYNC_TOUCHMOVE);
 // Queue a TouchStart.
 PressTouchPoint(0, 1);
 EXPECT_EQ(1U, GetAndResetSentEventCount());
 SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
 EXPECT_EQ(1U, GetAndResetAckedEventCount());

 MoveTouchPoint(0, 20, 5);
 EXPECT_EQ(1U, queued_event_count());
 EXPECT_EQ(1U, GetAndResetSentEventCount());

 // GestureScrollBegin doesn't insert a synthetic TouchCancel.
 WebGestureEvent followup_scroll;
 followup_scroll.type = WebInputEvent::GestureScrollBegin;
 SetFollowupEvent(followup_scroll);
 SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
 EXPECT_EQ(0U, GetAndResetSentEventCount());
 EXPECT_EQ(1U, GetAndResetAckedEventCount());
 EXPECT_EQ(0U, queued_event_count());
}

TEST_F(TouchEventQueueTest, AsyncTouch) {
  SetTouchScrollingMode(TouchEventQueue::TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE);

  // Queue a TouchStart.
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  for (int i = 0; i < 3; ++i) {
   SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                       INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

   MoveTouchPoint(0, 10, 5);
   SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
   EXPECT_FALSE(HasPendingAsyncTouchMove());
   EXPECT_TRUE(sent_event().cancelable);
   EXPECT_EQ(0U, queued_event_count());
   EXPECT_EQ(1U, GetAndResetSentEventCount());

   // Consuming a scroll event will throttle subsequent touchmoves.
   SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                       INPUT_EVENT_ACK_STATE_CONSUMED);
   MoveTouchPoint(0, 10, 5);
   SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
   EXPECT_TRUE(HasPendingAsyncTouchMove());
   EXPECT_EQ(0U, queued_event_count());
   EXPECT_EQ(0U, GetAndResetSentEventCount());
  }
}

// Ensure that touchmove's are appropriately throttled during a typical
// scroll sequences that transitions between scrolls consumed and unconsumed.
TEST_F(TouchEventQueueTest, AsyncTouchThrottledAfterScroll) {
  SetTouchScrollingMode(TouchEventQueue::TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE);

  // Process a TouchStart
  PressTouchPoint(0, 1);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now send the first touch move and associated GestureScrollBegin,
  // but don't ACK the gesture event yet.
  MoveTouchPoint(0, 0, 5);
  WebGestureEvent followup_scroll;
  followup_scroll.type = WebInputEvent::GestureScrollBegin;
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now queue a second touchmove and verify it's not (yet) dispatched.
  MoveTouchPoint(0, 0, 10);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Queuing the final touchend should flush the pending, async touchmove.
  ReleaseTouchPoint(0);
  EXPECT_EQ(WebInputEvent::TouchMove, sent_event().type);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_FALSE(sent_event().cancelable);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Ack the flushed, async touchmove. The ack should not reach the client, but
  // it should trigger sending of the (now non-cancelable) touchend.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(WebInputEvent::TouchEnd, sent_event().type);
  EXPECT_FALSE(sent_event().cancelable);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, queued_event_count());

  // Ack the touchend.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Now mark the scroll as not consumed (which would cause future
  // touchmoves in the active sequence to be sent if there was one).
  SendGestureEventAck(WebInputEvent::GestureScrollBegin,
      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);

  // Start a new touch sequence and verify that throttling has been reset.
  // Touch moves after the start of scrolling will again be throttled.
  PressTouchPoint(0, 0);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  MoveTouchPoint(0, 0, 5);
  SetFollowupEvent(followup_scroll);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  MoveTouchPoint(0, 0, 10);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // As soon as a touchmove exceeds the outer slop region it will be forwarded
  // immediately.
  MoveTouchPoint(0, 0, 20);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_FALSE(sent_event().cancelable);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Subsequent touchmove's should be deferred.
  MoveTouchPoint(0, 0, 25);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // The pending touchmove should be flushed with the the new touchmove if
  // sufficient time has passed.
  AdvanceTouchTime(kMinSecondsBetweenThrottledTouchmoves + 0.1);
  MoveTouchPoint(0, 0, 15);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_FALSE(sent_event().cancelable);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  // Non-touchmove events should always flush any pending touchmove events.
  MoveTouchPoint(0, 0, 25);
  EXPECT_TRUE(HasPendingAsyncTouchMove());
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  PressTouchPoint(30, 30);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_FALSE(sent_event().cancelable);
  EXPECT_EQ(WebInputEvent::TouchMove, sent_event().type);
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(2U, queued_event_count());

  // Ack'ing the flushed, async touchmove will dispatch the touchstart. Note
  // that the flushed touchmove's ack will not reach the client (its
  // constituent events have already been ack'ed).
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(sent_event().cancelable);
  EXPECT_EQ(WebInputEvent::TouchStart, sent_event().type);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(0U, GetAndResetAckedEventCount());

  // Ack the touchstart.
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
  MoveTouchPoint(0, 0, 25);
  EXPECT_TRUE(sent_event().cancelable);
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
  EXPECT_TRUE(sent_event().cancelable);
  EXPECT_EQ(WebInputEvent::TouchEnd, sent_event().type);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_TRUE(sent_event().cancelable);
  EXPECT_EQ(WebInputEvent::TouchEnd, sent_event().type);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(1U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
}

// Ensure that async touch dispatch and touch ack timeout interactions work
// appropriately.
TEST_F(TouchEventQueueTest, AsyncTouchWithAckTimeout) {
  SetTouchScrollingMode(TouchEventQueue::TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE);
  SetUpForTimeoutTesting(DefaultTouchTimeoutDelay());

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
  EXPECT_FALSE(sent_event().cancelable);
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // An unconsumed scroll event will resume synchronous touchmoves, which are
  // subject to the ack timeout.
  SendGestureEventAck(WebInputEvent::GestureScrollUpdate,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  MoveTouchPoint(0, 20, 5);
  EXPECT_TRUE(IsTimeoutRunning());
  EXPECT_TRUE(sent_event().cancelable);
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
  EXPECT_FALSE(sent_event().cancelable);
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
  SetTouchScrollingMode(TouchEventQueue::TOUCH_SCROLLING_MODE_ASYNC_TOUCHMOVE);

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

  // The async touchmove should be ack'ed immediately, but not forwarded.
  // However, because the ack triggers a touchcancel, both the pending touch and
  // the queued touchcancel should be flushed.
  WebTouchEvent followup_cancel;
  followup_cancel.type = WebInputEvent::TouchCancel;
  followup_cancel.touchesLength = 1;
  followup_cancel.touches[0].state = WebTouchPoint::StateCancelled;
  SetFollowupEvent(followup_cancel);
  MoveTouchPoint(0, 5, 5);
  EXPECT_EQ(2U, queued_event_count());
  EXPECT_FALSE(sent_event().cancelable);
  EXPECT_FALSE(HasPendingAsyncTouchMove());
  EXPECT_EQ(WebInputEvent::TouchMove, acked_event().type);
  EXPECT_EQ(WebInputEvent::TouchMove, sent_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  // The ack for the asnc touchmove should not reach the client, as it has
  // already been ack'ed.
  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_FALSE(sent_event().cancelable);
  EXPECT_EQ(1U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchCancel, sent_event().type);
  EXPECT_EQ(0U, GetAndResetAckedEventCount());
  EXPECT_EQ(1U, GetAndResetSentEventCount());

  SendTouchEventAck(INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  EXPECT_EQ(0U, queued_event_count());
  EXPECT_EQ(WebInputEvent::TouchCancel, acked_event().type);
  EXPECT_EQ(1U, GetAndResetAckedEventCount());
  EXPECT_EQ(0U, GetAndResetSentEventCount());
}

}  // namespace content
