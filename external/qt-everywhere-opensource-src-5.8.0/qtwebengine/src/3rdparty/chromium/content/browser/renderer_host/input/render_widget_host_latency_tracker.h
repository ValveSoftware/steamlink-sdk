// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_RENDER_WIDGET_HOST_LATENCY_TRACKER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_RENDER_WIDGET_HOST_LATENCY_TRACKER_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/common/input/input_event_ack_state.h"
#include "ui/events/latency_info.h"

namespace content {

// Utility class for tracking the latency of events passing through
// a given RenderWidgetHost.
class CONTENT_EXPORT RenderWidgetHostLatencyTracker {
 public:
  RenderWidgetHostLatencyTracker();
  ~RenderWidgetHostLatencyTracker();

  // Associates the latency tracker with a given route and process.
  // Called once after the RenderWidgetHost is fully initialized.
  void Initialize(int routing_id, int process_id);

  void ComputeInputLatencyHistograms(blink::WebInputEvent::Type type,
                                     int64_t latency_component_id,
                                     const ui::LatencyInfo& latency,
                                     InputEventAckState ack_result);

  // Populates the LatencyInfo with relevant entries for latency tracking.
  // Called when an event is received by the RenderWidgetHost, prior to
  // that event being forwarded to the renderer (via the InputRouter).
  void OnInputEvent(const blink::WebInputEvent& event,
                    ui::LatencyInfo* latency);

  // Populates the LatencyInfo with relevant entries for latency tracking, also
  // terminating latency tracking for events that did not trigger rendering and
  // performing relevant UMA latency reporting. Called when an event is ack'ed
  // to the RenderWidgetHost (from the InputRouter).
  void OnInputEventAck(const blink::WebInputEvent& event,
                       ui::LatencyInfo* latency,
                       InputEventAckState ack_result);

  // Populates renderer-created LatencyInfo entries with the appropriate latency
  // component id. Called when the RenderWidgetHost receives a compositor swap
  // update from the renderer.
  void OnSwapCompositorFrame(std::vector<ui::LatencyInfo>* latencies);

  // Terminates latency tracking for events that triggered rendering, also
  // performing relevant UMA latency reporting.
  // Called when the RenderWidgetHost receives a swap update from the GPU.
  void OnFrameSwapped(const ui::LatencyInfo& latency);

  // WebInputEvent coordinates are in DPIs, while LatencyInfo expects
  // coordinates in device pixels.
  void set_device_scale_factor(float device_scale_factor) {
    device_scale_factor_ = device_scale_factor;
  }

  // Returns the ID that uniquely describes this component to the latency
  // subsystem.
  int64_t latency_component_id() const { return latency_component_id_; }

 private:
  int64_t last_event_id_;
  int64_t latency_component_id_;
  float device_scale_factor_;
  bool has_seen_first_gesture_scroll_update_;
  // Whether the current stream of touch events has ever included more than one
  // touch point.
  bool multi_finger_gesture_;
  // Whether the touch start for the current stream of touch events had its
  // default action prevented. Only valid for single finger gestures.
  bool touch_start_default_prevented_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostLatencyTracker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_RENDER_WIDGET_HOST_LATENCY_TRACKER_H_
