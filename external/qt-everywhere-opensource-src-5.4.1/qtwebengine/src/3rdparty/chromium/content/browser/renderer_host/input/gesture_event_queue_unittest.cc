// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/gesture_event_queue.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using base::TimeDelta;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {

class GestureEventQueueTest : public testing::Test,
                              public GestureEventQueueClient,
                              public TouchpadTapSuppressionControllerClient {
 public:
  GestureEventQueueTest()
      : acked_gesture_event_count_(0),
        sent_gesture_event_count_(0) {}

  virtual ~GestureEventQueueTest() {}

  // testing::Test
  virtual void SetUp() OVERRIDE {
    queue_.reset(new GestureEventQueue(this, this, DefaultConfig()));
  }

  virtual void TearDown() OVERRIDE {
    // Process all pending tasks to avoid leaks.
    RunUntilIdle();
    queue_.reset();
  }

  // GestureEventQueueClient
  virtual void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& event) OVERRIDE {
    ++sent_gesture_event_count_;
    if (sync_ack_result_) {
      scoped_ptr<InputEventAckState> ack_result = sync_ack_result_.Pass();
      SendInputEventACK(event.event.type, *ack_result);
    }
  }

  virtual void OnGestureEventAck(
      const GestureEventWithLatencyInfo& event,
      InputEventAckState ack_result) OVERRIDE {
    ++acked_gesture_event_count_;
    last_acked_event_ = event.event;
    if (sync_followup_event_)
      SimulateGestureEvent(*sync_followup_event_.Pass());
  }

  // TouchpadTapSuppressionControllerClient
  virtual void SendMouseEventImmediately(
      const MouseEventWithLatencyInfo& event) OVERRIDE {
  }

 protected:
  static GestureEventQueue::Config DefaultConfig() {
    return GestureEventQueue::Config();
  }

  void SetUpForDebounce(int interval_ms) {
    queue()->set_debounce_interval_time_ms_for_testing(interval_ms);
  }

  // Returns the result of |GestureEventQueue::ShouldForward()|.
  bool SimulateGestureEvent(const WebGestureEvent& gesture) {
    GestureEventWithLatencyInfo gesture_with_latency(gesture,
                                                     ui::LatencyInfo());
    if (queue()->ShouldForward(gesture_with_latency)) {
      SendGestureEventImmediately(gesture_with_latency);
      return true;
    }
    return false;
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
                                       int modifiers) {
    SimulateGestureEvent(SyntheticWebGestureEventBuilder::BuildPinchUpdate(
        scale,
        anchorX,
        anchorY,
        modifiers,
        blink::WebGestureDeviceTouchscreen));
  }

  void SimulateGestureFlingStartEvent(float velocityX,
                                      float velocityY,
                                      WebGestureDevice sourceDevice) {
    SimulateGestureEvent(
        SyntheticWebGestureEventBuilder::BuildFling(velocityX,
                                                    velocityY,
                                                    sourceDevice));
  }

  void SendInputEventACK(WebInputEvent::Type type,
                         InputEventAckState ack) {
    queue()->ProcessGestureAck(ack, type, ui::LatencyInfo());
  }

  void RunUntilIdle() {
    base::MessageLoop::current()->RunUntilIdle();
  }

  size_t GetAndResetSentGestureEventCount() {
    size_t count = sent_gesture_event_count_;
    sent_gesture_event_count_ = 0;
    return count;
  }

  size_t GetAndResetAckedGestureEventCount() {
    size_t count = acked_gesture_event_count_;
    acked_gesture_event_count_ = 0;
    return count;
  }

  const WebGestureEvent& last_acked_event() const {
    return last_acked_event_;
  }

  void set_synchronous_ack(InputEventAckState ack_result) {
    sync_ack_result_.reset(new InputEventAckState(ack_result));
  }

  void set_sync_followup_event(WebInputEvent::Type type,
                               WebGestureDevice sourceDevice) {
    sync_followup_event_.reset(new WebGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice)));
  }

  unsigned GestureEventQueueSize() {
    return queue()->coalesced_gesture_events_.size();
  }

  WebGestureEvent GestureEventSecondFromLastQueueEvent() {
    return queue()->coalesced_gesture_events_.at(
        GestureEventQueueSize() - 2).event;
  }

  WebGestureEvent GestureEventLastQueueEvent() {
    return queue()->coalesced_gesture_events_.back().event;
  }

  unsigned GestureEventDebouncingQueueSize() {
    return queue()->debouncing_deferral_queue_.size();
  }

  WebGestureEvent GestureEventQueueEventAt(int i) {
    return queue()->coalesced_gesture_events_.at(i).event;
  }

  bool ScrollingInProgress() {
    return queue()->scrolling_in_progress_;
  }

  bool FlingInProgress() {
    return queue()->fling_in_progress_;
  }

  bool WillIgnoreNextACK() {
    return queue()->ignore_next_ack_;
  }

  GestureEventQueue* queue() const {
    return queue_.get();
  }

 private:
  scoped_ptr<GestureEventQueue> queue_;
  size_t acked_gesture_event_count_;
  size_t sent_gesture_event_count_;
  WebGestureEvent last_acked_event_;
  scoped_ptr<InputEventAckState> sync_ack_result_;
  scoped_ptr<WebGestureEvent> sync_followup_event_;
  base::MessageLoopForUI message_loop_;
};

#if GTEST_HAS_PARAM_TEST
// This is for tests that are to be run for all source devices.
class GestureEventQueueWithSourceTest
    : public GestureEventQueueTest,
      public testing::WithParamInterface<WebGestureDevice> {};
#endif  // GTEST_HAS_PARAM_TEST

TEST_F(GestureEventQueueTest, CoalescesScrollGestureEvents) {
  // Test coalescing of only GestureScrollUpdate events.
  // Simulate gesture events.

  // Sent.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -5, 0);

  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // Coalesced.
  SimulateGestureScrollUpdateEvent(8, -6, 0);

  // Check that coalescing updated the correct values.
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(0, merged_event.modifiers);
  EXPECT_EQ(16, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-11, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -7, 1);

  // Check that we didn't wrongly coalesce.
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // Different.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       blink::WebGestureDeviceTouchscreen);

  // Check that only the first event was sent.
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Ack for queued coalesced event.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Ack for queued uncoalesced event.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // After the final ack, the queue should be empty.
  SendInputEventACK(WebInputEvent::GestureScrollEnd,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
}

TEST_F(GestureEventQueueTest,
       DoesNotCoalesceScrollGestureEventsFromDifferentDevices) {
  // Test that GestureScrollUpdate events from Touchscreen and Touchpad do not
  // coalesce.

  // Sent.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -5, 0);

  // Make sure that the queue contains what we think it should.
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen,
            GestureEventLastQueueEvent().sourceDevice);

  // Coalesced.
  SimulateGestureScrollUpdateEvent(8, -6, 0);
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen,
            GestureEventLastQueueEvent().sourceDevice);

  // Enqueued.
  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchpad);
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(blink::WebGestureDeviceTouchpad,
            GestureEventLastQueueEvent().sourceDevice);

  // Coalesced.
  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchpad);
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(blink::WebGestureDeviceTouchpad,
            GestureEventLastQueueEvent().sourceDevice);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -7, 0);
  EXPECT_EQ(4U, GestureEventQueueSize());
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen,
            GestureEventLastQueueEvent().sourceDevice);
}

TEST_F(GestureEventQueueTest, CoalescesScrollAndPinchEvents) {
  // Test coalescing of only GestureScrollUpdate events.
  // Simulate gesture events.

  // Sent.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);

  // Sent.
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       blink::WebGestureDeviceTouchscreen);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(8, -4, 1);

  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(8, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-4, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -3, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // Check that only the first event was sent.
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -6, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(3U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(13, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-7, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // At this point ACKs shouldn't be getting ignored.
  EXPECT_FALSE(WillIgnoreNextACK());

  // Check that the ACK sends both scroll and pinch updates.
  SendInputEventACK(WebInputEvent::GesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());

  // The next ACK should be getting ignored.
  EXPECT_TRUE(WillIgnoreNextACK());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(1, -1, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(3U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(1, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-1, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(2, -2, 1);

  // Coalescing scrolls should still work.
  EXPECT_EQ(3U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-3, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(6, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // Enqueued.
  SimulateGesturePinchUpdateEvent(0.5, 60, 60, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(0.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(3, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-3, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // Check that the ACK gets ignored.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);
  RunUntilIdle();
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  // The flag should have been flipped back to false.
  EXPECT_FALSE(WillIgnoreNextACK());

  // Enqueued.
  SimulateGestureScrollUpdateEvent(2, -2, 2);

  // Shouldn't coalesce with different modifiers.
  EXPECT_EQ(4U, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(2, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-2, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(2, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(0.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  EXPECT_EQ(blink::WebGestureDeviceTouchscreen, merged_event.sourceDevice);

  // Check that the ACK sends the next scroll pinch pair.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  RunUntilIdle();
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second message.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);
  RunUntilIdle();
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());

  // Check that the ACK sends the second event.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  // Check that the queue is empty after ACK and no events get sent.
  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);
  RunUntilIdle();
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueTest, CoalescesMultiplePinchEventSequences) {
  // Simulate a pinch sequence.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       blink::WebGestureDeviceTouchscreen);

  SimulateGestureScrollUpdateEvent(8, -4, 1);
  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  size_t expected_events_in_queue = 3;
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(++expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(8, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-4, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -3, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Now start another sequence before the previous sequence has been ack'ed.
  SimulateGestureEvent(WebInputEvent::GesturePinchEnd,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       blink::WebGestureDeviceTouchscreen);

  SimulateGestureScrollUpdateEvent(8, -4, 1);
  // Make sure that the queue contains what we think it should.
  expected_events_in_queue += 5;
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 30, 30, 1);
  EXPECT_EQ(++expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(8, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-4, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);

  // Enqueued.
  SimulateGestureScrollUpdateEvent(6, -3, 1);

  // Check whether coalesced correctly.
  EXPECT_EQ(expected_events_in_queue, GestureEventQueueSize());
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, merged_event.type);
  EXPECT_EQ(1.5, merged_event.data.pinchUpdate.scale);
  EXPECT_EQ(1, merged_event.modifiers);
  merged_event = GestureEventSecondFromLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);
  EXPECT_EQ(12, merged_event.data.scrollUpdate.deltaX);
  EXPECT_EQ(-6, merged_event.data.scrollUpdate.deltaY);
  EXPECT_EQ(1, merged_event.modifiers);
}

TEST_F(GestureEventQueueTest, CoalescesPinchSequencesWithEarlyAck) {
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       blink::WebGestureDeviceTouchscreen);
  SendInputEventACK(WebInputEvent::GesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  // ScrollBegin and PinchBegin have been sent
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(5, 5, 1);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate,
            GestureEventLastQueueEvent().type);
  EXPECT_EQ(1U, GestureEventQueueSize());


  SimulateGesturePinchUpdateEvent(2, 60, 60, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate,
            GestureEventLastQueueEvent().type);
  EXPECT_EQ(2U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(3, 60, 60, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate,
            GestureEventLastQueueEvent().type);
  EXPECT_EQ(2U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(5, 5, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  // The coalesced pinch/scroll pair will have been re-arranged, with the
  // pinch following the scroll.
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate,
            GestureEventLastQueueEvent().type);
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(4, 60, 60, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);

  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  EXPECT_EQ(2.f * 3.f * 4.f, last_acked_event().data.pinchUpdate.scale);

  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueTest,
       DoesNotCoalescePinchGestureEventsWithDifferentModifiers) {
  // Insert an event to force queueing of gestures.
  SimulateGestureEvent(WebInputEvent::GestureTapCancel,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(5, 5, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(3, 60, 60, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(10, 15, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(4, 60, 60, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  // Using different modifiers should prevent coalescing.
  SimulateGesturePinchUpdateEvent(5, 60, 60, 2);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(4U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(6, 60, 60, 3);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(5U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::GestureTapCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(4U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);
  EXPECT_EQ(3U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  EXPECT_EQ(3.f * 4.f, last_acked_event().data.pinchUpdate.scale);
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  EXPECT_EQ(5.f, last_acked_event().data.pinchUpdate.scale);
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  EXPECT_EQ(6.f, last_acked_event().data.pinchUpdate.scale);
  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueTest, CoalescesScrollAndPinchEventsIdentity) {
  // Insert an event to force queueing of gestures.
  SimulateGestureEvent(WebInputEvent::GestureTapCancel,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());

  // Ensure that coalescing yields an identity transform for any pinch/scroll
  // pair combined with its inverse.
  SimulateGestureScrollUpdateEvent(5, 5, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(5, 10, 10, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(.2f, 10, 10, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(-5, -5, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::GestureTapCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);
  EXPECT_EQ(0.f, last_acked_event().data.scrollUpdate.deltaX);
  EXPECT_EQ(0.f, last_acked_event().data.scrollUpdate.deltaY);

  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  EXPECT_EQ(1.f, last_acked_event().data.pinchUpdate.scale);
  EXPECT_EQ(0U, GestureEventQueueSize());

  // Insert an event to force queueing of gestures.
  SimulateGestureEvent(WebInputEvent::GestureTapCancel,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());

  // Ensure that coalescing yields an identity transform for any pinch/scroll
  // pair combined with its inverse.
  SimulateGesturePinchUpdateEvent(2, 10, 10, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(20, 20, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGesturePinchUpdateEvent(0.5f, 20, 20, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SimulateGestureScrollUpdateEvent(-5, -5, 1);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::GestureTapCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::GestureScrollUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);
  EXPECT_EQ(0.f, last_acked_event().data.scrollUpdate.deltaX);
  EXPECT_EQ(0.f, last_acked_event().data.scrollUpdate.deltaY);

  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  EXPECT_EQ(1.f, last_acked_event().data.pinchUpdate.scale);
}

// Tests a single event with an synchronous ack.
TEST_F(GestureEventQueueTest, SimpleSyncAck) {
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
}

// Tests an event with an synchronous ack which enqueues an additional event.
TEST_F(GestureEventQueueTest, SyncAckQueuesEvent) {
  scoped_ptr<WebGestureEvent> queued_event;
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  set_sync_followup_event(WebInputEvent::GestureShowPress,
                          blink::WebGestureDeviceTouchscreen);
  // This event enqueues the show press event.
  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());

  SendInputEventACK(WebInputEvent::GestureShowPress,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
}

// Tests an event with an async ack followed by an event with a sync ack.
TEST_F(GestureEventQueueTest, AsyncThenSyncAck) {
  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       blink::WebGestureDeviceTouchscreen);

  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetAckedGestureEventCount());

  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetAckedGestureEventCount());

  SendInputEventACK(WebInputEvent::GestureTapDown,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(2U, GetAndResetAckedGestureEventCount());
}

TEST_F(GestureEventQueueTest, CoalescesScrollAndPinchEventWithSyncAck) {
  // Simulate a pinch sequence.
  SimulateGestureEvent(WebInputEvent::GestureScrollBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  SimulateGestureEvent(WebInputEvent::GesturePinchBegin,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());

  SimulateGestureScrollUpdateEvent(8, -4, 1);
  // Make sure that the queue contains what we think it should.
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, merged_event.type);

  // Coalesced without changing event order. Note anchor at (60, 60). Anchoring
  // from a point that is not the origin should still give us the right scroll.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(4U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::GestureScrollBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());

  // Ack the PinchBegin, and schedule a synchronous ack for GestureScrollUpdate.
  set_synchronous_ack(INPUT_EVENT_ACK_STATE_CONSUMED);
  SendInputEventACK(WebInputEvent::GesturePinchBegin,
                    INPUT_EVENT_ACK_STATE_CONSUMED);

  // Both GestureScrollUpdate and GesturePinchUpdate should have been sent.
  EXPECT_EQ(WebInputEvent::GestureScrollUpdate, last_acked_event().type);
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());

  // Ack the final GesturePinchUpdate.
  SendInputEventACK(WebInputEvent::GesturePinchUpdate,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  EXPECT_EQ(WebInputEvent::GesturePinchUpdate, last_acked_event().type);
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
}

#if GTEST_HAS_PARAM_TEST
TEST_P(GestureEventQueueWithSourceTest, GestureFlingCancelsFiltered) {
  WebGestureDevice source_device = GetParam();

  // GFC without previous GFS is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());

  // GFC after previous GFS is dispatched and acked.
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_TRUE(FlingInProgress());
  SendInputEventACK(WebInputEvent::GestureFlingStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  SendInputEventACK(WebInputEvent::GestureFlingCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());

  // GFC before previous GFS is acked.
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_TRUE(FlingInProgress());
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());

  // Advance state realistically.
  SendInputEventACK(WebInputEvent::GestureFlingStart,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  SendInputEventACK(WebInputEvent::GestureFlingCancel,
                    INPUT_EVENT_ACK_STATE_CONSUMED);
  RunUntilIdle();
  EXPECT_EQ(2U, GetAndResetAckedGestureEventCount());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());

  // GFS is added to the queue if another event is pending
  SimulateGestureScrollUpdateEvent(8, -7, 0);
  SimulateGestureFlingStartEvent(0, -10, source_device);
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  WebGestureEvent merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingStart, merged_event.type);
  EXPECT_TRUE(FlingInProgress());
  EXPECT_EQ(2U, GestureEventQueueSize());

  // GFS in queue means that a GFC is added to the queue
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event =GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(3U, GestureEventQueueSize());

  // Adding a second GFC is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(3U, GestureEventQueueSize());

  // Adding another GFS will add it to the queue.
  SimulateGestureFlingStartEvent(0, -10, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingStart, merged_event.type);
  EXPECT_TRUE(FlingInProgress());
  EXPECT_EQ(4U, GestureEventQueueSize());

  // GFS in queue means that a GFC is added to the queue
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(5U, GestureEventQueueSize());

  // Adding another GFC with a GFC already there is dropped.
  SimulateGestureEvent(WebInputEvent::GestureFlingCancel, source_device);
  merged_event = GestureEventLastQueueEvent();
  EXPECT_EQ(WebInputEvent::GestureFlingCancel, merged_event.type);
  EXPECT_FALSE(FlingInProgress());
  EXPECT_EQ(5U, GestureEventQueueSize());
}

INSTANTIATE_TEST_CASE_P(AllSources,
                        GestureEventQueueWithSourceTest,
                        testing::Values(blink::WebGestureDeviceTouchscreen,
                                        blink::WebGestureDeviceTouchpad));
#endif  // GTEST_HAS_PARAM_TEST

// Test that a GestureScrollEnd | GestureFlingStart are deferred during the
// debounce interval, that Scrolls are not and that the deferred events are
// sent after that timer fires.
TEST_F(GestureEventQueueTest, DebounceDefersFollowingGestureEvents) {
  SetUpForDebounce(3);

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureFlingStartEvent(0, 10, blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(2U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::GestureTapDown,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(3U, GestureEventDebouncingQueueSize());

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::MessageLoop::QuitClosure(),
      TimeDelta::FromMilliseconds(5));
  base::MessageLoop::current()->Run();

  // The deferred events are correctly queued in coalescing queue.
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(5U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_FALSE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollEnd,
      WebInputEvent::GestureFlingStart};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
      i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.type);
  }
}

// Test that non-scroll events are deferred while scrolling during the debounce
// interval and are discarded if a GestureScrollUpdate event arrives before the
// interval end.
TEST_F(GestureEventQueueTest, DebounceDropsDeferredEvents) {
  SetUpForDebounce(3);

  EXPECT_FALSE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // This event should get discarded.
  SimulateGestureEvent(WebInputEvent::GestureScrollEnd,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::GestureScrollUpdate,
                       blink::WebGestureDeviceTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {
      WebInputEvent::GestureScrollUpdate,
      WebInputEvent::GestureScrollUpdate};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
      i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.type);
  }
}

}  // namespace content
