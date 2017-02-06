// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <new>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;

namespace content {
namespace {

const int kTestRoutingID = 13;
}

class MainThreadEventQueueTest : public testing::Test,
                                 public MainThreadEventQueueClient {
 public:
  MainThreadEventQueueTest() : queue_(kTestRoutingID, this) {}

  void SendEventToMainThread(int routing_id,
                             const blink::WebInputEvent* event,
                             const ui::LatencyInfo& latency,
                             InputEventDispatchType type) override {
    ASSERT_EQ(kTestRoutingID, routing_id);
    const unsigned char* eventPtr =
        reinterpret_cast<const unsigned char*>(event);
    last_event_.assign(eventPtr, eventPtr + event->size);
  }

  void SendInputEventAck(int routing_id,
                         blink::WebInputEvent::Type type,
                         InputEventAckState ack_result,
                         uint32_t touch_event_id) override {
    additional_acked_events_.push_back(touch_event_id);
  }

  WebInputEventQueue<PendingMouseWheelEvent>& wheel_event_queue() {
    return queue_.wheel_events_;
  }

  WebInputEventQueue<PendingTouchEvent>& touch_event_queue() {
    return queue_.touch_events_;
  }

 protected:
  MainThreadEventQueue queue_;
  std::vector<unsigned char> last_event_;
  std::vector<uint32_t> additional_acked_events_;
};

TEST_F(MainThreadEventQueueTest, NonBlockingWheel) {
  WebMouseWheelEvent kEvents[4] = {
      SyntheticWebMouseWheelEventBuilder::Build(10, 10, 0, 53, 0, false),
      SyntheticWebMouseWheelEventBuilder::Build(20, 20, 0, 53, 0, false),
      SyntheticWebMouseWheelEventBuilder::Build(30, 30, 0, 53, 1, false),
      SyntheticWebMouseWheelEventBuilder::Build(30, 30, 0, 53, 1, false),
  };

  ASSERT_EQ(WebInputEventQueueState::ITEM_NOT_PENDING,
            wheel_event_queue().state());
  queue_.HandleEvent(&kEvents[0], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  ASSERT_EQ(WebInputEventQueueState::ITEM_PENDING, wheel_event_queue().state());
  queue_.HandleEvent(&kEvents[1], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  ASSERT_EQ(kEvents[0].size, last_event_.size());
  kEvents[0].dispatchType =
      WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  ASSERT_TRUE(memcmp(&last_event_[0], &kEvents[0], kEvents[0].size) == 0);
  queue_.EventHandled(blink::WebInputEvent::MouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(kEvents[1].size, last_event_.size());
  kEvents[1].dispatchType =
      WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  ASSERT_TRUE(memcmp(&last_event_[0], &kEvents[1], kEvents[1].size) == 0);
  ASSERT_EQ(WebInputEventQueueState::ITEM_PENDING, wheel_event_queue().state());
  queue_.EventHandled(blink::WebInputEvent::MouseWheel,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(WebInputEventQueueState::ITEM_NOT_PENDING,
            wheel_event_queue().state());

  // Ensure that coalescing takes place.
  queue_.HandleEvent(&kEvents[0], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_.HandleEvent(&kEvents[2], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_.HandleEvent(&kEvents[3], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  ASSERT_EQ(1u, wheel_event_queue().size());
  ASSERT_EQ(WebInputEventQueueState::ITEM_PENDING, wheel_event_queue().state());
}

TEST_F(MainThreadEventQueueTest, NonBlockingTouch) {
  SyntheticWebTouchEvent kEvents[4];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].modifiers = 1;
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].MovePoint(0, 30, 30);
  kEvents[3].PressPoint(10, 10);
  kEvents[3].MovePoint(0, 35, 35);

  ASSERT_EQ(WebInputEventQueueState::ITEM_NOT_PENDING,
            touch_event_queue().state());
  queue_.HandleEvent(&kEvents[0], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  ASSERT_EQ(WebInputEventQueueState::ITEM_PENDING, touch_event_queue().state());
  queue_.HandleEvent(&kEvents[1], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  ASSERT_EQ(kEvents[0].size, last_event_.size());
  kEvents[0].dispatchType =
      WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  ASSERT_TRUE(memcmp(&last_event_[0], &kEvents[0], kEvents[0].size) == 0);
  queue_.EventHandled(blink::WebInputEvent::TouchStart,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(kEvents[1].size, last_event_.size());
  kEvents[1].dispatchType =
      WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  ASSERT_TRUE(memcmp(&last_event_[0], &kEvents[1], kEvents[1].size) == 0);
  ASSERT_EQ(WebInputEventQueueState::ITEM_PENDING, touch_event_queue().state());
  queue_.EventHandled(blink::WebInputEvent::TouchMove,
                      INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(WebInputEventQueueState::ITEM_NOT_PENDING,
            touch_event_queue().state());

  // Ensure that coalescing takes place.
  queue_.HandleEvent(&kEvents[0], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_.HandleEvent(&kEvents[2], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_.HandleEvent(&kEvents[3], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  ASSERT_EQ(1u, touch_event_queue().size());
  ASSERT_EQ(WebInputEventQueueState::ITEM_PENDING, touch_event_queue().state());
}

TEST_F(MainThreadEventQueueTest, BlockingTouch) {
  SyntheticWebTouchEvent kEvents[4];
  kEvents[0].PressPoint(10, 10);
  kEvents[1].PressPoint(10, 10);
  kEvents[1].MovePoint(0, 20, 20);
  kEvents[2].PressPoint(10, 10);
  kEvents[2].MovePoint(0, 30, 30);
  kEvents[3].PressPoint(10, 10);
  kEvents[3].MovePoint(0, 35, 35);

  ASSERT_EQ(WebInputEventQueueState::ITEM_NOT_PENDING,
            touch_event_queue().state());
  // Ensure that coalescing takes place.
  queue_.HandleEvent(&kEvents[0], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_SET_NON_BLOCKING);
  queue_.HandleEvent(&kEvents[1], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  queue_.HandleEvent(&kEvents[2], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  queue_.HandleEvent(&kEvents[3], ui::LatencyInfo(), DISPATCH_TYPE_BLOCKING,
                     INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(1u, touch_event_queue().size());
  ASSERT_EQ(WebInputEventQueueState::ITEM_PENDING, touch_event_queue().state());
  queue_.EventHandled(kEvents[0].type, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(0u, touch_event_queue().size());
  queue_.EventHandled(kEvents[1].type, INPUT_EVENT_ACK_STATE_NOT_CONSUMED);
  ASSERT_EQ(2u, additional_acked_events_.size());
  ASSERT_EQ(kEvents[2].uniqueTouchEventId, additional_acked_events_.at(0));
  ASSERT_EQ(kEvents[3].uniqueTouchEventId, additional_acked_events_.at(1));
}

}  // namespace content
