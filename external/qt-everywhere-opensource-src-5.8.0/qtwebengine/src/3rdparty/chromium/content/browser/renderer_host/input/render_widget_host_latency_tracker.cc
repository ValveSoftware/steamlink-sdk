// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/render_widget_host_latency_tracker.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using ui::LatencyInfo;

namespace content {
namespace {

void UpdateLatencyCoordinatesImpl(const blink::WebTouchEvent& touch,
                                  LatencyInfo* latency,
                                  float device_scale_factor) {
  for (uint32_t i = 0; i < touch.touchesLength; ++i) {
    gfx::PointF coordinate(touch.touches[i].position.x * device_scale_factor,
                           touch.touches[i].position.y * device_scale_factor);
    if (!latency->AddInputCoordinate(coordinate))
      break;
  }
}

void UpdateLatencyCoordinatesImpl(const WebGestureEvent& gesture,
                                  LatencyInfo* latency,
                                  float device_scale_factor) {
  latency->AddInputCoordinate(gfx::PointF(gesture.x * device_scale_factor,
                                          gesture.y * device_scale_factor));
}

void UpdateLatencyCoordinatesImpl(const WebMouseEvent& mouse,
                                  LatencyInfo* latency,
                                  float device_scale_factor) {
  latency->AddInputCoordinate(gfx::PointF(mouse.x * device_scale_factor,
                                          mouse.y * device_scale_factor));
}

void UpdateLatencyCoordinatesImpl(const WebMouseWheelEvent& wheel,
                                  LatencyInfo* latency,
                                  float device_scale_factor) {
  latency->AddInputCoordinate(gfx::PointF(wheel.x * device_scale_factor,
                                          wheel.y * device_scale_factor));
}

void UpdateLatencyCoordinates(const WebInputEvent& event,
                              float device_scale_factor,
                              LatencyInfo* latency) {
  if (WebInputEvent::isMouseEventType(event.type)) {
    UpdateLatencyCoordinatesImpl(static_cast<const WebMouseEvent&>(event),
                                 latency, device_scale_factor);
  } else if (WebInputEvent::isGestureEventType(event.type)) {
    UpdateLatencyCoordinatesImpl(static_cast<const WebGestureEvent&>(event),
                                 latency, device_scale_factor);
  } else if (WebInputEvent::isTouchEventType(event.type)) {
    UpdateLatencyCoordinatesImpl(static_cast<const WebTouchEvent&>(event),
                                 latency, device_scale_factor);
  } else if (event.type == WebInputEvent::MouseWheel) {
    UpdateLatencyCoordinatesImpl(static_cast<const WebMouseWheelEvent&>(event),
                                 latency, device_scale_factor);
  }
}

// Touch to scroll latency that is mostly under 1 second.
#define UMA_HISTOGRAM_TOUCH_TO_SCROLL_LATENCY(name, start, end)               \
  UMA_HISTOGRAM_CUSTOM_COUNTS(                                                \
      name, (end.event_time - start.event_time).InMicroseconds(), 1, 1000000, \
      100)

// Long scroll latency component that is mostly under 200ms.
#define UMA_HISTOGRAM_SCROLL_LATENCY_LONG(name, start, end)        \
  UMA_HISTOGRAM_CUSTOM_COUNTS(                                     \
    name,                                                          \
    (end.event_time - start.event_time).InMicroseconds(),          \
    1000, 200000, 50)

// Short scroll latency component that is mostly under 50ms.
#define UMA_HISTOGRAM_SCROLL_LATENCY_SHORT(name, start, end)       \
  UMA_HISTOGRAM_CUSTOM_COUNTS(                                     \
    name,                                                          \
    (end.event_time - start.event_time).InMicroseconds(),          \
    1, 50000, 50)

void ComputeScrollLatencyHistograms(
    const LatencyInfo::LatencyComponent& gpu_swap_begin_component,
    const LatencyInfo::LatencyComponent& gpu_swap_end_component,
    int64_t latency_component_id,
    const LatencyInfo& latency) {
  DCHECK(!latency.coalesced());
  if (latency.coalesced())
    return;

  DCHECK(!gpu_swap_begin_component.event_time.is_null());
  DCHECK(!gpu_swap_end_component.event_time.is_null());
  LatencyInfo::LatencyComponent original_component;
  if (latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          latency_component_id, &original_component)) {
    // This UMA metric tracks the time between the final frame swap for the
    // first scroll event in a sequence and the original timestamp of that
    // scroll event's underlying touch event.
    for (size_t i = 0; i < original_component.event_count; i++) {
      UMA_HISTOGRAM_TOUCH_TO_SCROLL_LATENCY(
          "Event.Latency.TouchToFirstScrollUpdateSwapBegin",
          original_component, gpu_swap_begin_component);
    }
  } else if (!latency.FindLatency(
                 ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
                 latency_component_id, &original_component)) {
    return;
  }

  // This UMA metric tracks the time from when the original touch event is
  // created to when the scroll gesture results in final frame swap.
  for (size_t i = 0; i < original_component.event_count; i++) {
    UMA_HISTOGRAM_TOUCH_TO_SCROLL_LATENCY(
        "Event.Latency.TouchToScrollUpdateSwapBegin", original_component,
        gpu_swap_begin_component);
  }

  // TODO(miletus): Add validation for making sure the following components
  // are present and their event times are legit.
  LatencyInfo::LatencyComponent rendering_scheduled_component;
  bool rendering_scheduled_on_main = latency.FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT,
      0, &rendering_scheduled_component);

  if (!rendering_scheduled_on_main) {
    if (!latency.FindLatency(
            ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT,
            0, &rendering_scheduled_component))
      return;
  }

  if (rendering_scheduled_on_main) {
    UMA_HISTOGRAM_SCROLL_LATENCY_LONG(
        "Event.Latency.ScrollUpdate.TouchToHandled_Main",
        original_component, rendering_scheduled_component);
  } else {
    UMA_HISTOGRAM_SCROLL_LATENCY_LONG(
        "Event.Latency.ScrollUpdate.TouchToHandled_Impl",
        original_component, rendering_scheduled_component);
  }

  LatencyInfo::LatencyComponent renderer_swap_component;
  if (!latency.FindLatency(ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT,
                           0, &renderer_swap_component))
    return;

  if (rendering_scheduled_on_main) {
    UMA_HISTOGRAM_SCROLL_LATENCY_LONG(
        "Event.Latency.ScrollUpdate.HandledToRendererSwap_Main",
        rendering_scheduled_component, renderer_swap_component);
  } else {
    UMA_HISTOGRAM_SCROLL_LATENCY_LONG(
        "Event.Latency.ScrollUpdate.HandledToRendererSwap_Impl",
        rendering_scheduled_component, renderer_swap_component);
  }

  LatencyInfo::LatencyComponent browser_received_swap_component;
  if (!latency.FindLatency(
          ui::INPUT_EVENT_BROWSER_RECEIVED_RENDERER_SWAP_COMPONENT,
          0, &browser_received_swap_component))
    return;

  UMA_HISTOGRAM_SCROLL_LATENCY_SHORT(
      "Event.Latency.ScrollUpdate.RendererSwapToBrowserNotified",
      renderer_swap_component, browser_received_swap_component);

  UMA_HISTOGRAM_SCROLL_LATENCY_LONG(
      "Event.Latency.ScrollUpdate.BrowserNotifiedToBeforeGpuSwap",
      browser_received_swap_component, gpu_swap_begin_component);

  UMA_HISTOGRAM_SCROLL_LATENCY_SHORT("Event.Latency.ScrollUpdate.GpuSwap",
                                     gpu_swap_begin_component,
                                     gpu_swap_end_component);
}

// LatencyComponents generated in the renderer must have component IDs
// provided to them by the browser process. This function adds the correct
// component ID where necessary.
void AddLatencyInfoComponentIds(LatencyInfo* latency,
                                int64_t latency_component_id) {
  std::vector<std::pair<ui::LatencyComponentType, int64_t>> new_components_key;
  std::vector<LatencyInfo::LatencyComponent> new_components_value;
  for (const auto& lc : latency->latency_components()) {
    ui::LatencyComponentType component_type = lc.first.first;
    if (component_type == ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT) {
      // Generate a new component entry with the correct component ID
      new_components_key.push_back(std::make_pair(component_type,
                                                  latency_component_id));
      new_components_value.push_back(lc.second);
    }
  }

  // Remove the entries with invalid component IDs.
  latency->RemoveLatency(ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT);

  // Add newly generated components into the latency info
  for (size_t i = 0; i < new_components_key.size(); i++) {
    latency->AddLatencyNumberWithTimestamp(
        new_components_key[i].first,
        new_components_key[i].second,
        new_components_value[i].sequence_number,
        new_components_value[i].event_time,
        new_components_value[i].event_count);
  }
}

}  // namespace

RenderWidgetHostLatencyTracker::RenderWidgetHostLatencyTracker()
    : last_event_id_(0),
      latency_component_id_(0),
      device_scale_factor_(1),
      has_seen_first_gesture_scroll_update_(false),
      multi_finger_gesture_(false),
      touch_start_default_prevented_(false) {}

RenderWidgetHostLatencyTracker::~RenderWidgetHostLatencyTracker() {}

void RenderWidgetHostLatencyTracker::Initialize(int routing_id,
                                                int process_id) {
  DCHECK_EQ(0, last_event_id_);
  DCHECK_EQ(0, latency_component_id_);
  last_event_id_ = static_cast<int64_t>(process_id) << 32;
  latency_component_id_ = routing_id | last_event_id_;
}

void RenderWidgetHostLatencyTracker::ComputeInputLatencyHistograms(
    WebInputEvent::Type type,
    int64_t latency_component_id,
    const LatencyInfo& latency,
    InputEventAckState ack_result) {
  if (latency.coalesced())
    return;

  LatencyInfo::LatencyComponent rwh_component;
  if (!latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                           latency_component_id, &rwh_component)) {
    return;
  }
  DCHECK_EQ(rwh_component.event_count, 1u);

  LatencyInfo::LatencyComponent ui_component;
  if (latency.FindLatency(ui::INPUT_EVENT_LATENCY_UI_COMPONENT, 0,
                          &ui_component)) {
    DCHECK_EQ(ui_component.event_count, 1u);
    base::TimeDelta ui_delta =
        rwh_component.event_time - ui_component.event_time;

    if (type == blink::WebInputEvent::MouseWheel) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.Browser.WheelUI",
                                  ui_delta.InMicroseconds(), 1, 20000, 100);

    } else {
      DCHECK(WebInputEvent::isTouchEventType(type));
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.Browser.TouchUI",
                                  ui_delta.InMicroseconds(), 1, 20000, 100);
    }
  }

  // Both tap and scroll gestures depend on the disposition of the touch start
  // and the current touch. For touch start, touch_start_default_prevented_ ==
  // (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED).
  bool action_prevented = touch_start_default_prevented_ ||
                          ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;

  LatencyInfo::LatencyComponent main_component;
  if (latency.FindLatency(ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT, 0,
                          &main_component)) {
    DCHECK_EQ(main_component.event_count, 1u);
    base::TimeDelta queueing_delta =
        main_component.event_time - rwh_component.event_time;

    if (!multi_finger_gesture_) {
      if (action_prevented) {
        switch (type) {
          case WebInputEvent::TouchStart:
            UMA_HISTOGRAM_TIMES(
              "Event.Latency.QueueingTime.TouchStartDefaultPrevented",
              queueing_delta);
            break;
          case WebInputEvent::TouchMove:
            UMA_HISTOGRAM_TIMES(
                "Event.Latency.QueueingTime.TouchMoveDefaultPrevented",
                queueing_delta);
            break;
          case WebInputEvent::TouchEnd:
            UMA_HISTOGRAM_TIMES(
                "Event.Latency.QueueingTime.TouchEndDefaultPrevented",
                queueing_delta);
            break;
          default:
            break;
        }
      } else {
        switch (type) {
          case WebInputEvent::TouchStart:
            UMA_HISTOGRAM_TIMES(
                "Event.Latency.QueueingTime.TouchStartDefaultAllowed",
                queueing_delta);
            break;
          case WebInputEvent::TouchMove:
            UMA_HISTOGRAM_TIMES(
                "Event.Latency.QueueingTime.TouchMoveDefaultAllowed",
                queueing_delta);
            break;
          case WebInputEvent::TouchEnd:
            UMA_HISTOGRAM_TIMES(
                "Event.Latency.QueueingTime.TouchEndDefaultAllowed",
                queueing_delta);
            break;
          default:
            break;
        }
      }
    }
  }

  LatencyInfo::LatencyComponent acked_component;
  if (latency.FindLatency(ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0,
                          &acked_component)) {
    DCHECK_EQ(acked_component.event_count, 1u);
    base::TimeDelta acked_delta =
        acked_component.event_time - rwh_component.event_time;
    if (type == blink::WebInputEvent::MouseWheel) {
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.Browser.WheelAcked",
                                  acked_delta.InMicroseconds(), 1, 1000000,
                                  100);
    } else {
      DCHECK(WebInputEvent::isTouchEventType(type));
      UMA_HISTOGRAM_CUSTOM_COUNTS("Event.Latency.Browser.TouchAcked",
                                  acked_delta.InMicroseconds(), 1, 1000000,
                                  100);
    }

    if (!multi_finger_gesture_ &&
        main_component.event_time != base::TimeTicks()) {
      base::TimeDelta blocking_delta;
      blocking_delta = acked_component.event_time - main_component.event_time;

      if (action_prevented) {
        switch (type) {
          case WebInputEvent::TouchStart:
            UMA_HISTOGRAM_TIMES(
                "Event.Latency.BlockingTime.TouchStartDefaultPrevented",
                blocking_delta);
            break;
          case WebInputEvent::TouchMove:
            UMA_HISTOGRAM_TIMES(
                "Event.Latency.BlockingTime.TouchMoveDefaultPrevented",
                blocking_delta);
            break;
          case WebInputEvent::TouchEnd:
            UMA_HISTOGRAM_TIMES(
                "Event.Latency.BlockingTime.TouchEndDefaultPrevented",
                blocking_delta);
            break;
          default:
            break;
        }
      } else {
        switch (type) {
          case WebInputEvent::TouchStart:
            UMA_HISTOGRAM_TIMES(
                "Event.Latency.BlockingTime.TouchStartDefaultAllowed",
                blocking_delta);
            break;
          case WebInputEvent::TouchMove:
            UMA_HISTOGRAM_TIMES(
                "Event.Latency.BlockingTime.TouchMoveDefaultAllowed",
                blocking_delta);
            break;
          case WebInputEvent::TouchEnd:
            UMA_HISTOGRAM_TIMES(
                "Event.Latency.BlockingTime.TouchEndDefaultAllowed",
                blocking_delta);
            break;
          default:
            break;
        }
      }
    }
  }
}

void RenderWidgetHostLatencyTracker::OnInputEvent(
    const blink::WebInputEvent& event,
    LatencyInfo* latency) {
  DCHECK(latency);
  if (latency->FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                           latency_component_id_, NULL)) {
    return;
  }

  if (event.timeStampSeconds &&
      !latency->FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
                            0, NULL)) {
    base::TimeTicks timestamp_now = base::TimeTicks::Now();
    base::TimeTicks timestamp_original = base::TimeTicks() +
        base::TimeDelta::FromSecondsD(event.timeStampSeconds);

    // Timestamp from platform input can wrap, e.g. 32 bits timestamp
    // for Xserver and Window MSG time will wrap about 49.6 days. Do a
    // sanity check here and if wrap does happen, use TimeTicks::Now()
    // as the timestamp instead.
    if ((timestamp_now - timestamp_original).InDays() > 0)
      timestamp_original = timestamp_now;

    latency->AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
        0,
        0,
        timestamp_original,
        1);
  }

  latency->AddLatencyNumberWithTraceName(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
      latency_component_id_, ++last_event_id_,
      WebInputEventTraits::GetName(event.type));

  UpdateLatencyCoordinates(event, device_scale_factor_, latency);

  if (event.type == blink::WebInputEvent::GestureScrollBegin) {
    has_seen_first_gesture_scroll_update_ = false;
  } else if (event.type == blink::WebInputEvent::GestureScrollUpdate) {
    // Make a copy of the INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT with a
    // different name INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT.
    // So we can track the latency specifically for scroll update events.
    LatencyInfo::LatencyComponent original_component;
    if (latency->FindLatency(ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT, 0,
                             &original_component)) {
      latency->AddLatencyNumberWithTimestamp(
          has_seen_first_gesture_scroll_update_
              ? ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT
              : ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT,
          latency_component_id_, original_component.sequence_number,
          original_component.event_time, original_component.event_count);
    }

    has_seen_first_gesture_scroll_update_ = true;
  }
}

void RenderWidgetHostLatencyTracker::OnInputEventAck(
    const blink::WebInputEvent& event,
    LatencyInfo* latency, InputEventAckState ack_result) {
  DCHECK(latency);

  // Latency ends when it is acked but does not cause render scheduling.
  bool rendering_scheduled = latency->FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT, 0, nullptr);
  rendering_scheduled |= latency->FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT, 0, nullptr);

  if (WebInputEvent::isGestureEventType(event.type)) {
    if (!rendering_scheduled) {
      latency->AddLatencyNumber(
          ui::INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT, 0, 0);
      // TODO(jdduke): Consider exposing histograms for gesture event types.
    }
    return;
  }

  if (WebInputEvent::isTouchEventType(event.type)) {
    const WebTouchEvent& touch_event =
        *static_cast<const WebTouchEvent*>(&event);
    if (event.type == WebInputEvent::TouchStart) {
      DCHECK(touch_event.touchesLength >= 1);
      multi_finger_gesture_ = touch_event.touchesLength != 1;
      touch_start_default_prevented_ =
          ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;
    }

    latency->AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0);

    if (!rendering_scheduled) {
      latency->AddLatencyNumber(
          ui::INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT, 0, 0);
    }
    ComputeInputLatencyHistograms(event.type, latency_component_id_, *latency,
                                  ack_result);
    return;
  }

  if (event.type == WebInputEvent::MouseWheel) {
    latency->AddLatencyNumber(ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT, 0, 0);
    if (!rendering_scheduled) {
      latency->AddLatencyNumber(
          ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_WHEEL_COMPONENT, 0, 0);
    }
    ComputeInputLatencyHistograms(event.type, latency_component_id_, *latency,
                                  ack_result);
    return;
  }

  if (WebInputEvent::isMouseEventType(event.type) && !rendering_scheduled) {
      latency->AddLatencyNumber(
          ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT, 0, 0);
    return;
  }

  if (WebInputEvent::isKeyboardEventType(event.type) && !rendering_scheduled) {
      latency->AddLatencyNumber(
          ui::INPUT_EVENT_LATENCY_TERMINATED_KEYBOARD_COMPONENT, 0, 0);
    return;
  }
}

void RenderWidgetHostLatencyTracker::OnSwapCompositorFrame(
    std::vector<LatencyInfo>* latencies) {
  DCHECK(latencies);
  for (LatencyInfo& latency : *latencies) {
    AddLatencyInfoComponentIds(&latency, latency_component_id_);
    latency.AddLatencyNumber(
        ui::INPUT_EVENT_BROWSER_RECEIVED_RENDERER_SWAP_COMPONENT, 0, 0);
  }
}

void RenderWidgetHostLatencyTracker::OnFrameSwapped(
    const LatencyInfo& latency) {
  // Don't report frame latency on wheel events. Previously they were only
  // reported on touch metrics and we need to be consistent across reporting
  // metrics.
  LatencyInfo::LatencyComponent mouse_wheel_scroll_update_component;
  if (latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_GENERATE_SCROLL_UPDATE_FROM_MOUSE_WHEEL, 0,
          &mouse_wheel_scroll_update_component)) {
    return;
  }

  LatencyInfo::LatencyComponent gpu_swap_end_component;
  if (!latency.FindLatency(
          ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT, 0,
          &gpu_swap_end_component)) {
    return;
  }

  LatencyInfo::LatencyComponent gpu_swap_begin_component;
  if (!latency.FindLatency(ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, 0,
                           &gpu_swap_begin_component)) {
    return;
  }

  LatencyInfo::LatencyComponent tab_switch_component;
  if (latency.FindLatency(ui::TAB_SHOW_COMPONENT, latency_component_id_,
                          &tab_switch_component)) {
    base::TimeDelta delta =
        gpu_swap_end_component.event_time - tab_switch_component.event_time;
    for (size_t i = 0; i < tab_switch_component.event_count; i++) {
      UMA_HISTOGRAM_TIMES("MPArch.RWH_TabSwitchPaintDuration", delta);
    }
  }

  if (!latency.FindLatency(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
                           latency_component_id_, nullptr)) {
    return;
  }

  ComputeScrollLatencyHistograms(gpu_swap_begin_component,
                                 gpu_swap_end_component, latency_component_id_,
                                 latency);
}

}  // namespace content
