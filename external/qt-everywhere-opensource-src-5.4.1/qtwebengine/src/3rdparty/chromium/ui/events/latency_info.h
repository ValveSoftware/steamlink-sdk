// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_LATENCY_INFO_H_
#define UI_EVENTS_LATENCY_INFO_H_

#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/small_map.h"
#include "base/time/time.h"
#include "ui/events/events_base_export.h"

namespace ui {

enum LatencyComponentType {
  // ---------------------------BEGIN COMPONENT-------------------------------
  // BEGIN COMPONENT is when we show the latency begin in chrome://tracing.
  // Timestamp when the input event is sent from RenderWidgetHost to renderer.
  INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT,
  // Timestamp when the input event is received in plugin.
  INPUT_EVENT_LATENCY_BEGIN_PLUGIN_COMPONENT,
  // ---------------------------NORMAL COMPONENT-------------------------------
  // Timestamp when the scroll update gesture event is sent from RWH to
  // renderer. In Aura, touch event's LatencyInfo is carried over to the gesture
  // event. So gesture event's INPUT_EVENT_LATENCY_RWH_COMPONENT is the
  // timestamp when its original touch events is sent from RWH to renderer.
  // In non-aura platform, INPUT_EVENT_LATENCY_SCROLL_UPDATE_RWH_COMPONENT
  // is the same as INPUT_EVENT_LATENCY_RWH_COMPONENT.
  INPUT_EVENT_LATENCY_SCROLL_UPDATE_RWH_COMPONENT,
  // The original timestamp of the touch event which converts to scroll update.
  INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT,
  // Original timestamp for input event (e.g. timestamp from kernel).
  INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
  // Timestamp when the UI event is created.
  INPUT_EVENT_LATENCY_UI_COMPONENT,
  // This is special component indicating there is rendering scheduled for
  // the event associated with this LatencyInfo.
  INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_COMPONENT,
  // Timestamp when the touch event is acked.
  INPUT_EVENT_LATENCY_ACKED_TOUCH_COMPONENT,
  // Frame number when a window snapshot was requested. The snapshot
  // is taken when the rendering results actually reach the screen.
  WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT,
  // ---------------------------TERMINAL COMPONENT-----------------------------
  // TERMINAL COMPONENT is when we show the latency end in chrome://tracing.
  // Timestamp when the mouse event is acked from renderer and it does not
  // cause any rendering scheduled.
  INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT,
  // Timestamp when the touch event is acked from renderer and it does not
  // cause any rendering schedueld and does not generate any gesture event.
  INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT,
  // Timestamp when the gesture event is acked from renderer, and it does not
  // cause any rendering schedueld.
  INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT,
  // Timestamp when the frame is swapped (i.e. when the rendering caused by
  // input event actually takes effect).
  INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT,
  // This component indicates that the input causes a commit to be scheduled
  // but the commit failed.
  INPUT_EVENT_LATENCY_TERMINATED_COMMIT_FAILED_COMPONENT,
  // This component indicates that the input causes a swap to be scheduled
  // but the swap failed.
  INPUT_EVENT_LATENCY_TERMINATED_SWAP_FAILED_COMPONENT,
  // This component indicates that the cached LatencyInfo number exceeds the
  // maximal allowed size.
  LATENCY_INFO_LIST_TERMINATED_OVERFLOW_COMPONENT,
  // Timestamp when the input event is considered not cause any rendering
  // damage in plugin and thus terminated.
  INPUT_EVENT_LATENCY_TERMINATED_PLUGIN_COMPONENT,
  LATENCY_COMPONENT_TYPE_LAST = INPUT_EVENT_LATENCY_TERMINATED_PLUGIN_COMPONENT
};

struct EVENTS_BASE_EXPORT LatencyInfo {
  struct LatencyComponent {
    // Nondecreasing number that can be used to determine what events happened
    // in the component at the time this struct was sent on to the next
    // component.
    int64 sequence_number;
    // Average time of events that happened in this component.
    base::TimeTicks event_time;
    // Count of events that happened in this component
    uint32 event_count;
  };

  // Empirically determined constant based on a typical scroll sequence.
  enum { kTypicalMaxComponentsPerLatencyInfo = 6 };

  // Map a Latency Component (with a component-specific int64 id) to a
  // component info.
  typedef base::SmallMap<
      std::map<std::pair<LatencyComponentType, int64>, LatencyComponent>,
      kTypicalMaxComponentsPerLatencyInfo> LatencyMap;

  LatencyInfo();

  ~LatencyInfo();

  // Returns true if the vector |latency_info| is valid. Returns false
  // if it is not valid and log the |referring_msg|.
  // This function is mainly used to check the latency_info vector that
  // is passed between processes using IPC message has reasonable size
  // so that we are confident the IPC message is not corrupted/compromised.
  // This check will go away once the IPC system has better built-in scheme
  // for corruption/compromise detection.
  static bool Verify(const std::vector<LatencyInfo>& latency_info,
                     const char* referring_msg);

  // Copy LatencyComponents with type |type| from |other| into |this|.
  void CopyLatencyFrom(const LatencyInfo& other, LatencyComponentType type);

  // Add LatencyComponents that are in |other| but not in |this|.
  void AddNewLatencyFrom(const LatencyInfo& other);

  // Modifies the current sequence number for a component, and adds a new
  // sequence number with the current timestamp.
  void AddLatencyNumber(LatencyComponentType component,
                        int64 id,
                        int64 component_sequence_number);

  // Modifies the current sequence number and adds a certain number of events
  // for a specific component.
  void AddLatencyNumberWithTimestamp(LatencyComponentType component,
                                     int64 id,
                                     int64 component_sequence_number,
                                     base::TimeTicks time,
                                     uint32 event_count);

  // Returns true if the a component with |type| and |id| is found in
  // the latency_components and the component is stored to |output| if
  // |output| is not NULL. Returns false if no such component is found.
  bool FindLatency(LatencyComponentType type,
                   int64 id,
                   LatencyComponent* output) const;

  void RemoveLatency(LatencyComponentType type);

  void Clear();

  // Records the |event_type| in trace buffer as TRACE_EVENT_ASYNC_STEP.
  void TraceEventType(const char* event_type);

  LatencyMap latency_components;
  // The unique id for matching the ASYNC_BEGIN/END trace event.
  int64 trace_id;
  // Whether a terminal component has been added.
  bool terminated;
};

}  // namespace ui

#endif  // UI_EVENTS_LATENCY_INFO_H_
