// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mock_input_router_client.h"

#include "content/browser/renderer_host/input/input_router.h"
#include "content/common/input/input_event.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

namespace content {

MockInputRouterClient::MockInputRouterClient()
    : input_router_(NULL),
      in_flight_event_count_(0),
      has_touch_handler_(false),
      filter_state_(INPUT_EVENT_ACK_STATE_NOT_CONSUMED),
      filter_input_event_called_(false),
      did_flush_called_count_(0) {
}

MockInputRouterClient::~MockInputRouterClient() {}

InputEventAckState MockInputRouterClient::FilterInputEvent(
    const WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info) {
  filter_input_event_called_ = true;
  last_filter_event_.reset(new InputEvent(input_event, latency_info));
  return filter_state_;
}

void MockInputRouterClient::IncrementInFlightEventCount() {
  ++in_flight_event_count_;
}

void MockInputRouterClient::DecrementInFlightEventCount() {
  --in_flight_event_count_;
}

void MockInputRouterClient::OnHasTouchEventHandlers(
    bool has_handlers)  {
  has_touch_handler_ = has_handlers;
}

void MockInputRouterClient::DidFlush() {
  ++did_flush_called_count_;
}

void MockInputRouterClient::DidOverscroll(const DidOverscrollParams& params) {
  overscroll_ = params;
}

void MockInputRouterClient::DidStopFlinging() {
}

void MockInputRouterClient::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& gesture_event,
    const ui::LatencyInfo& latency_info) {
  if (input_router_)
    input_router_->SendGestureEvent(
        GestureEventWithLatencyInfo(gesture_event, latency_info));
}

bool MockInputRouterClient::GetAndResetFilterEventCalled() {
  bool filter_input_event_called = filter_input_event_called_;
  filter_input_event_called_ = false;
  return filter_input_event_called;
}

size_t MockInputRouterClient::GetAndResetDidFlushCount() {
  size_t did_flush_called_count = did_flush_called_count_;
  did_flush_called_count_ = 0;
  return did_flush_called_count;
}

DidOverscrollParams MockInputRouterClient::GetAndResetOverscroll() {
  DidOverscrollParams overscroll;
  std::swap(overscroll_, overscroll);
  return overscroll;
}

}  // namespace content
