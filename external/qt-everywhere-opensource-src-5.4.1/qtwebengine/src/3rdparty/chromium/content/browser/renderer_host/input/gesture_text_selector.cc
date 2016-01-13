// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/gesture_text_selector.h"

#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace content {

GestureTextSelector::GestureTextSelector(GestureTextSelectorClient* client)
    : client_(client),
      text_selection_triggered_(false),
      anchor_x_(0.0f),
      anchor_y_(0.0f) {
}

GestureTextSelector::~GestureTextSelector() {
}

bool GestureTextSelector::OnTouchEvent(const ui::MotionEvent& event) {
  if (event.GetAction() == ui::MotionEvent::ACTION_DOWN) {
    // Only trigger selection on ACTION_DOWN to prevent partial touch or gesture
    // sequences from being forwarded.
    text_selection_triggered_ = ShouldStartTextSelection(event);
  }
  return text_selection_triggered_;
}

bool GestureTextSelector::OnGestureEvent(const ui::GestureEventData& gesture) {
  if (!text_selection_triggered_)
    return false;

  switch (gesture.type()) {
    case ui::ET_GESTURE_TAP: {
      client_->LongPress(gesture.time, gesture.x, gesture.y);
      break;
    }
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      client_->Unselect();
      anchor_x_ = gesture.x;
      anchor_y_ = gesture.y;
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      // TODO(changwan): check if we can show handles on ET_GESTURE_SCROLL_END
      // instead. Currently it is not possible as ShowSelectionHandles should
      // be called before we change the selection.
      client_->ShowSelectionHandlesAutomatically();
      client_->SelectRange(anchor_x_, anchor_y_, gesture.x, gesture.y);
      break;
    }
    default:
      // Suppress all other gestures when we are selecting text.
      break;
  }
  return true;
}

// static
bool GestureTextSelector::ShouldStartTextSelection(
    const ui::MotionEvent& event) {
  DCHECK_GT(event.GetPointerCount(), 0u);
  // Currently we are supporting stylus-only cases.
  const bool is_stylus =
      event.GetToolType(0) == ui::MotionEvent::TOOL_TYPE_STYLUS;
  const bool is_only_secondary_button_pressed =
      event.GetButtonState() == ui::MotionEvent::BUTTON_SECONDARY;
  return is_stylus && is_only_secondary_button_pressed;
}

}  // namespace content
