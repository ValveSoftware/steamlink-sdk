// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_INPUT_EVENT_ACK_H_
#define CONTENT_COMMON_INPUT_INPUT_EVENT_ACK_H_

#include <stdint.h>

#include <memory>

#include "content/common/content_export.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/common/input/input_event_ack_state.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/latency_info.h"

namespace content {

// InputEventAck.
struct CONTENT_EXPORT InputEventAck {
  InputEventAck(blink::WebInputEvent::Type type,
                InputEventAckState state,
                const ui::LatencyInfo& latency,
                std::unique_ptr<content::DidOverscrollParams> overscroll,
                uint32_t unique_touch_event_id);
  InputEventAck(blink::WebInputEvent::Type type,
                InputEventAckState state,
                const ui::LatencyInfo& latency,
                uint32_t unique_touch_event_id);
  InputEventAck(blink::WebInputEvent::Type type,
                InputEventAckState state,
                uint32_t unique_touch_event_id);
  InputEventAck(blink::WebInputEvent::Type type, InputEventAckState state);
  InputEventAck();
  ~InputEventAck();

  blink::WebInputEvent::Type type;
  InputEventAckState state;
  ui::LatencyInfo latency;
  std::unique_ptr<content::DidOverscrollParams> overscroll;
  uint32_t unique_touch_event_id;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_INPUT_EVENT_ACK_H_
