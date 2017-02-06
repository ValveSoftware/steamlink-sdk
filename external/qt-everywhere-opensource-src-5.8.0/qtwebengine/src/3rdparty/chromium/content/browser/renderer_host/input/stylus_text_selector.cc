// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_text_selector.h"

#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_detector.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/gesture_detection/motion_event.h"

using ui::GestureDetector;
using ui::MotionEvent;

namespace content {
namespace {
std::unique_ptr<GestureDetector> CreateGestureDetector(
    ui::GestureListener* listener) {
  GestureDetector::Config config =
      ui::GetGestureProviderConfig(
          ui::GestureProviderConfigType::CURRENT_PLATFORM)
          .gesture_detector_config;

  ui::DoubleTapListener* null_double_tap_listener = nullptr;

  // Doubletap, showpress and longpress detection are not required, and
  // should be explicitly disabled for efficiency.
  std::unique_ptr<ui::GestureDetector> detector(
      new ui::GestureDetector(config, listener, null_double_tap_listener));
  detector->set_longpress_enabled(false);
  detector->set_showpress_enabled(false);

  return detector;
}

}  // namespace

StylusTextSelector::StylusTextSelector(StylusTextSelectorClient* client)
    : client_(client),
      text_selection_triggered_(false),
      secondary_button_pressed_(false),
      dragging_(false),
      dragged_(false),
      anchor_x_(0.0f),
      anchor_y_(0.0f) {
  DCHECK(client);
}

StylusTextSelector::~StylusTextSelector() {
}

bool StylusTextSelector::OnTouchEvent(const MotionEvent& event) {
  // Only trigger selection on ACTION_DOWN to prevent partial touch or gesture
  // sequences from being forwarded.
  if (event.GetAction() == MotionEvent::ACTION_DOWN)
    text_selection_triggered_ = ShouldStartTextSelection(event);

  if (!text_selection_triggered_)
    return false;

  secondary_button_pressed_ =
      event.GetButtonState() == MotionEvent::BUTTON_SECONDARY;

  switch (event.GetAction()) {
    case MotionEvent::ACTION_DOWN:
      dragging_ = false;
      dragged_ = false;
      anchor_x_ = event.GetX();
      anchor_y_ = event.GetY();
      break;

    case MotionEvent::ACTION_MOVE:
      if (!secondary_button_pressed_) {
        dragging_ = false;
        anchor_x_ = event.GetX();
        anchor_y_ = event.GetY();
      }
      break;

    case MotionEvent::ACTION_UP:
    case MotionEvent::ACTION_CANCEL:
      if (dragged_)
        client_->OnStylusSelectEnd();
      dragged_ = false;
      dragging_ = false;
      break;

    case MotionEvent::ACTION_POINTER_UP:
    case MotionEvent::ACTION_POINTER_DOWN:
      break;
    case MotionEvent::ACTION_NONE:
      NOTREACHED();
      break;
  }

  if (!gesture_detector_)
    gesture_detector_ = CreateGestureDetector(this);

  gesture_detector_->OnTouchEvent(event);

  // Always return true, even if |gesture_detector_| technically doesn't
  // consume the event. This prevents forwarding of a partial touch stream.
  return true;
}

bool StylusTextSelector::OnSingleTapUp(const MotionEvent& e, int tap_count) {
  DCHECK(text_selection_triggered_);
  DCHECK(!dragging_);
  client_->OnStylusSelectTap(e.GetEventTime(), e.GetX(), e.GetY());
  return true;
}

bool StylusTextSelector::OnScroll(const MotionEvent& e1,
                                  const MotionEvent& e2,
                                  float distance_x,
                                  float distance_y) {
  DCHECK(text_selection_triggered_);

  // Return if Stylus button is not pressed.
  if (!secondary_button_pressed_)
    return true;

  if (!dragging_) {
    dragging_ = true;
    dragged_ = true;
    client_->OnStylusSelectBegin(anchor_x_, anchor_y_, e2.GetX(), e2.GetY());
  } else {
    client_->OnStylusSelectUpdate(e2.GetX(), e2.GetY());
  }

  return true;
}

// static
bool StylusTextSelector::ShouldStartTextSelection(const MotionEvent& event) {
  DCHECK_GT(event.GetPointerCount(), 0u);
  // Currently we are supporting stylus-only cases.
  const bool is_stylus = event.GetToolType(0) == MotionEvent::TOOL_TYPE_STYLUS;
  const bool is_only_secondary_button_pressed =
      event.GetButtonState() == MotionEvent::BUTTON_SECONDARY;
  return is_stylus && is_only_secondary_button_pressed;
}

}  // namespace content
