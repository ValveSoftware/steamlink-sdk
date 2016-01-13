// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_provider.h"

#include <cmath>

#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/motion_event.h"

namespace ui {
namespace {

// Double-tap drag zoom sensitivity (speed).
const float kDoubleTapDragZoomSpeed = 0.005f;

const char* GetMotionEventActionName(MotionEvent::Action action) {
  switch(action) {
    case MotionEvent::ACTION_POINTER_DOWN: return "ACTION_POINTER_DOWN";
    case MotionEvent::ACTION_POINTER_UP:   return "ACTION_POINTER_UP";
    case MotionEvent::ACTION_DOWN:         return "ACTION_DOWN";
    case MotionEvent::ACTION_UP:           return "ACTION_UP";
    case MotionEvent::ACTION_CANCEL:       return "ACTION_CANCEL";
    case MotionEvent::ACTION_MOVE:         return "ACTION_MOVE";
  }
  return "";
}

gfx::RectF GetBoundingBox(const MotionEvent& event) {
  gfx::RectF bounds;
  for (size_t i = 0; i < event.GetPointerCount(); ++i) {
    float diameter = event.GetTouchMajor(i);
    bounds.Union(gfx::RectF(event.GetX(i) - diameter / 2,
                            event.GetY(i) - diameter / 2,
                            diameter,
                            diameter));
  }
  return bounds;
}

GestureEventData CreateGesture(const GestureEventDetails& details,
                               int motion_event_id,
                               base::TimeTicks time,
                               float x,
                               float y,
                               float raw_x,
                               float raw_y,
                               size_t touch_point_count,
                               const gfx::RectF& bounding_box) {
  return GestureEventData(details,
                          motion_event_id,
                          time,
                          x,
                          y,
                          raw_x,
                          raw_y,
                          touch_point_count,
                          bounding_box);
}

GestureEventData CreateGesture(EventType type,
                               int motion_event_id,
                               base::TimeTicks time,
                               float x,
                               float y,
                               float raw_x,
                               float raw_y,
                               size_t touch_point_count,
                               const gfx::RectF& bounding_box) {
  return GestureEventData(GestureEventDetails(type, 0, 0),
                          motion_event_id,
                          time,
                          x,
                          y,
                          raw_x,
                          raw_y,
                          touch_point_count,
                          bounding_box);
}

GestureEventData CreateGesture(const GestureEventDetails& details,
                               const MotionEvent& event) {
  return GestureEventData(details,
                          event.GetId(),
                          event.GetEventTime(),
                          event.GetX(),
                          event.GetY(),
                          event.GetRawX(),
                          event.GetRawY(),
                          event.GetPointerCount(),
                          GetBoundingBox(event));
}

GestureEventData CreateGesture(EventType type, const MotionEvent& event) {
  return CreateGesture(GestureEventDetails(type, 0, 0), event);
}

GestureEventDetails CreateTapGestureDetails(EventType type) {
  // Set the tap count to 1 even for ET_GESTURE_DOUBLE_TAP, in order to be
  // consistent with double tap behavior on a mobile viewport. See
  // crbug.com/234986 for context.
  GestureEventDetails tap_details(type, 1, 0);
  return tap_details;
}

}  // namespace

// GestureProvider:::Config

GestureProvider::Config::Config()
    : display(gfx::Display::kInvalidDisplayID, gfx::Rect(1, 1)),
      disable_click_delay(false),
      gesture_begin_end_types_enabled(false),
      min_gesture_bounds_length(0) {}

GestureProvider::Config::~Config() {}

// GestureProvider::ScaleGestureListener

class GestureProvider::ScaleGestureListenerImpl
    : public ScaleGestureDetector::ScaleGestureListener {
 public:
  ScaleGestureListenerImpl(const ScaleGestureDetector::Config& config,
                           GestureProvider* provider)
      : scale_gesture_detector_(config, this),
        provider_(provider),
        ignore_multitouch_events_(false),
        pinch_event_sent_(false),
        min_pinch_update_span_delta_(config.min_pinch_update_span_delta) {}

  bool OnTouchEvent(const MotionEvent& event) {
    // TODO: Need to deal with multi-touch transition.
    const bool in_scale_gesture = IsScaleGestureDetectionInProgress();
    bool handled = scale_gesture_detector_.OnTouchEvent(event);
    if (!in_scale_gesture &&
        (event.GetAction() == MotionEvent::ACTION_UP ||
         event.GetAction() == MotionEvent::ACTION_CANCEL)) {
      return false;
    }
    return handled;
  }

  // ScaleGestureDetector::ScaleGestureListener implementation.
  virtual bool OnScaleBegin(const ScaleGestureDetector& detector,
                            const MotionEvent& e) OVERRIDE {
    if (ignore_multitouch_events_ && !detector.InDoubleTapMode())
      return false;
    pinch_event_sent_ = false;
    return true;
  }

  virtual void OnScaleEnd(const ScaleGestureDetector& detector,
                          const MotionEvent& e) OVERRIDE {
    if (!pinch_event_sent_)
      return;
    provider_->Send(CreateGesture(ET_GESTURE_PINCH_END, e));
    pinch_event_sent_ = false;
  }

  virtual bool OnScale(const ScaleGestureDetector& detector,
                       const MotionEvent& e) OVERRIDE {
    if (ignore_multitouch_events_ && !detector.InDoubleTapMode())
      return false;
    if (!pinch_event_sent_) {
      pinch_event_sent_ = true;
      provider_->Send(CreateGesture(ET_GESTURE_PINCH_BEGIN,
                                    e.GetId(),
                                    detector.GetEventTime(),
                                    detector.GetFocusX(),
                                    detector.GetFocusY(),
                                    detector.GetFocusX() + e.GetRawOffsetX(),
                                    detector.GetFocusY() + e.GetRawOffsetY(),
                                    e.GetPointerCount(),
                                    GetBoundingBox(e)));
    }

    if (std::abs(detector.GetCurrentSpan() - detector.GetPreviousSpan()) <
        min_pinch_update_span_delta_) {
      return false;
    }

    float scale = detector.GetScaleFactor();
    if (scale == 1)
      return true;

    if (detector.InDoubleTapMode()) {
      // Relative changes in the double-tap scale factor computed by |detector|
      // diminish as the touch moves away from the original double-tap focus.
      // For historical reasons, Chrome has instead adopted a scale factor
      // computation that is invariant to the focal distance, where
      // the scale delta remains constant if the touch velocity is constant.
      float dy =
          (detector.GetCurrentSpanY() - detector.GetPreviousSpanY()) * 0.5f;
      scale = std::pow(scale > 1 ? 1.0f + kDoubleTapDragZoomSpeed
                                 : 1.0f - kDoubleTapDragZoomSpeed,
                       std::abs(dy));
    }
    GestureEventDetails pinch_details(ET_GESTURE_PINCH_UPDATE, scale, 0);
    provider_->Send(CreateGesture(pinch_details,
                                  e.GetId(),
                                  detector.GetEventTime(),
                                  detector.GetFocusX(),
                                  detector.GetFocusY(),
                                  detector.GetFocusX() + e.GetRawOffsetX(),
                                  detector.GetFocusY() + e.GetRawOffsetY(),
                                  e.GetPointerCount(),
                                  GetBoundingBox(e)));
    return true;
  }

  void SetDoubleTapEnabled(bool enabled) {
    DCHECK(!IsDoubleTapInProgress());
    scale_gesture_detector_.SetQuickScaleEnabled(enabled);
  }

  void SetMultiTouchEnabled(bool enabled) {
    // Note that returning false from OnScaleBegin / OnScale makes the
    // gesture detector not to emit further scaling notifications
    // related to this gesture. Thus, if detector events are enabled in
    // the middle of the gesture, we don't need to do anything.
    ignore_multitouch_events_ = !enabled;
  }

  bool IsDoubleTapInProgress() const {
    return IsScaleGestureDetectionInProgress() && InDoubleTapMode();
  }

  bool IsScaleGestureDetectionInProgress() const {
    return scale_gesture_detector_.IsInProgress();
  }

 private:
  bool InDoubleTapMode() const {
    return scale_gesture_detector_.InDoubleTapMode();
  }

  ScaleGestureDetector scale_gesture_detector_;

  GestureProvider* const provider_;

  // Completely silence multi-touch (pinch) scaling events. Used in WebView when
  // zoom support is turned off.
  bool ignore_multitouch_events_;

  // Whether any pinch zoom event has been sent to native.
  bool pinch_event_sent_;

  // The minimum change in span required before this is considered a pinch. See
  // crbug.com/373318.
  float min_pinch_update_span_delta_;

  DISALLOW_COPY_AND_ASSIGN(ScaleGestureListenerImpl);
};

// GestureProvider::GestureListener

class GestureProvider::GestureListenerImpl
    : public GestureDetector::GestureListener,
      public GestureDetector::DoubleTapListener {
 public:
  GestureListenerImpl(
      const gfx::Display& display,
      const GestureDetector::Config& gesture_detector_config,
      bool disable_click_delay,
      GestureProvider* provider)
      : gesture_detector_(gesture_detector_config, this, this),
        snap_scroll_controller_(display),
        provider_(provider),
        disable_click_delay_(disable_click_delay),
        touch_slop_(gesture_detector_config.touch_slop),
        double_tap_timeout_(gesture_detector_config.double_tap_timeout),
        ignore_single_tap_(false),
        seen_first_scroll_event_(false) {}

  virtual ~GestureListenerImpl() {}

  bool OnTouchEvent(const MotionEvent& e,
                    bool is_scale_gesture_detection_in_progress) {
    snap_scroll_controller_.SetSnapScrollingMode(
        e, is_scale_gesture_detection_in_progress);

    if (is_scale_gesture_detection_in_progress)
      SetIgnoreSingleTap(true);

    if (e.GetAction() == MotionEvent::ACTION_DOWN)
      gesture_detector_.set_longpress_enabled(true);

    return gesture_detector_.OnTouchEvent(e);
  }

  // GestureDetector::GestureListener implementation.
  virtual bool OnDown(const MotionEvent& e) OVERRIDE {
    current_down_time_ = e.GetEventTime();
    ignore_single_tap_ = false;
    seen_first_scroll_event_ = false;

    GestureEventDetails tap_details(ET_GESTURE_TAP_DOWN, 0, 0);
    provider_->Send(CreateGesture(tap_details, e));

    // Return true to indicate that we want to handle touch.
    return true;
  }

  virtual bool OnScroll(const MotionEvent& e1,
                        const MotionEvent& e2,
                        float raw_distance_x,
                        float raw_distance_y) OVERRIDE {
    float distance_x = raw_distance_x;
    float distance_y = raw_distance_y;
    if (!seen_first_scroll_event_) {
      // Remove the touch slop region from the first scroll event to avoid a
      // jump.
      seen_first_scroll_event_ = true;
      double distance =
          std::sqrt(distance_x * distance_x + distance_y * distance_y);
      double epsilon = 1e-3;
      if (distance > epsilon) {
        double ratio = std::max(0., distance - touch_slop_) / distance;
        distance_x *= ratio;
        distance_y *= ratio;
      }
    }
    snap_scroll_controller_.UpdateSnapScrollMode(distance_x, distance_y);
    if (snap_scroll_controller_.IsSnappingScrolls()) {
      if (snap_scroll_controller_.IsSnapHorizontal()) {
        distance_y = 0;
      } else {
        distance_x = 0;
      }
    }

    if (!provider_->IsScrollInProgress()) {
      // Note that scroll start hints are in distance traveled, where
      // scroll deltas are in the opposite direction.
      GestureEventDetails scroll_details(
          ET_GESTURE_SCROLL_BEGIN, -raw_distance_x, -raw_distance_y);

      // Use the co-ordinates from the touch down, as these co-ordinates are
      // used to determine which layer the scroll should affect.
      provider_->Send(CreateGesture(scroll_details,
                                    e2.GetId(),
                                    e2.GetEventTime(),
                                    e1.GetX(),
                                    e1.GetY(),
                                    e1.GetRawX(),
                                    e1.GetRawY(),
                                    e2.GetPointerCount(),
                                    GetBoundingBox(e2)));
    }

    if (distance_x || distance_y) {
      const gfx::RectF bounding_box = GetBoundingBox(e2);
      const gfx::PointF center = bounding_box.CenterPoint();
      const gfx::PointF raw_center =
          center + gfx::Vector2dF(e2.GetRawOffsetX(), e2.GetRawOffsetY());
      GestureEventDetails scroll_details(
          ET_GESTURE_SCROLL_UPDATE, -distance_x, -distance_y);
      provider_->Send(CreateGesture(scroll_details,
                                    e2.GetId(),
                                    e2.GetEventTime(),
                                    center.x(),
                                    center.y(),
                                    raw_center.x(),
                                    raw_center.y(),
                                    e2.GetPointerCount(),
                                    bounding_box));
    }

    return true;
  }

  virtual bool OnFling(const MotionEvent& e1,
                       const MotionEvent& e2,
                       float velocity_x,
                       float velocity_y) OVERRIDE {
    if (snap_scroll_controller_.IsSnappingScrolls()) {
      if (snap_scroll_controller_.IsSnapHorizontal()) {
        velocity_y = 0;
      } else {
        velocity_x = 0;
      }
    }

    provider_->Fling(e2, velocity_x, velocity_y);
    return true;
  }

  virtual bool OnSwipe(const MotionEvent& e1,
                       const MotionEvent& e2,
                       float velocity_x,
                       float velocity_y) OVERRIDE {
    GestureEventDetails swipe_details(ET_GESTURE_SWIPE, velocity_x, velocity_y);
    provider_->Send(CreateGesture(swipe_details, e2));
    return true;
  }

  virtual bool OnTwoFingerTap(const MotionEvent& e1,
                              const MotionEvent& e2) OVERRIDE {
    // The location of the two finger tap event should be the location of the
    // primary pointer.
    GestureEventDetails two_finger_tap_details(ET_GESTURE_TWO_FINGER_TAP,
                                               e1.GetTouchMajor(),
                                               e1.GetTouchMajor());
    provider_->Send(CreateGesture(two_finger_tap_details,
                                  e2.GetId(),
                                  e2.GetEventTime(),
                                  e1.GetX(),
                                  e1.GetY(),
                                  e1.GetRawX(),
                                  e1.GetRawY(),
                                  e2.GetPointerCount(),
                                  GetBoundingBox(e2)));
    return true;
  }

  virtual void OnShowPress(const MotionEvent& e) OVERRIDE {
    GestureEventDetails show_press_details(ET_GESTURE_SHOW_PRESS, 0, 0);
    provider_->Send(CreateGesture(show_press_details, e));
  }

  virtual bool OnSingleTapUp(const MotionEvent& e) OVERRIDE {
    // This is a hack to address the issue where user hovers
    // over a link for longer than double_tap_timeout_, then
    // OnSingleTapConfirmed() is not triggered. But we still
    // want to trigger the tap event at UP. So we override
    // OnSingleTapUp() in this case. This assumes singleTapUp
    // gets always called before singleTapConfirmed.
    if (!ignore_single_tap_) {
      if (e.GetEventTime() - current_down_time_ > double_tap_timeout_) {
        return OnSingleTapConfirmed(e);
      } else if (!IsDoubleTapEnabled() || disable_click_delay_) {
        // If double-tap has been disabled, there is no need to wait
        // for the double-tap timeout.
        return OnSingleTapConfirmed(e);
      } else {
        // Notify Blink about this tapUp event anyway, when none of the above
        // conditions applied.
        provider_->Send(CreateGesture(
            CreateTapGestureDetails(ET_GESTURE_TAP_UNCONFIRMED), e));
      }
    }

    return provider_->SendLongTapIfNecessary(e);
  }

  // GestureDetector::DoubleTapListener implementation.
  virtual bool OnSingleTapConfirmed(const MotionEvent& e) OVERRIDE {
    // Long taps in the edges of the screen have their events delayed by
    // ContentViewHolder for tab swipe operations. As a consequence of the delay
    // this method might be called after receiving the up event.
    // These corner cases should be ignored.
    if (ignore_single_tap_)
      return true;

    ignore_single_tap_ = true;

    provider_->Send(CreateGesture(CreateTapGestureDetails(ET_GESTURE_TAP), e));
    return true;
  }

  virtual bool OnDoubleTap(const MotionEvent& e) OVERRIDE { return false; }

  virtual bool OnDoubleTapEvent(const MotionEvent& e) OVERRIDE {
    switch (e.GetAction()) {
      case MotionEvent::ACTION_DOWN:
        gesture_detector_.set_longpress_enabled(false);
        break;

      case MotionEvent::ACTION_UP:
        if (!provider_->IsPinchInProgress() &&
            !provider_->IsScrollInProgress()) {
          provider_->Send(
              CreateGesture(CreateTapGestureDetails(ET_GESTURE_DOUBLE_TAP), e));
          return true;
        }
        break;
      default:
        break;
    }
    return false;
  }

  virtual void OnLongPress(const MotionEvent& e) OVERRIDE {
    DCHECK(!IsDoubleTapInProgress());
    SetIgnoreSingleTap(true);

    GestureEventDetails long_press_details(ET_GESTURE_LONG_PRESS, 0, 0);
    provider_->Send(CreateGesture(long_press_details, e));
  }

  void SetDoubleTapEnabled(bool enabled) {
    DCHECK(!IsDoubleTapInProgress());
    gesture_detector_.SetDoubleTapListener(enabled ? this : NULL);
  }

  bool IsDoubleTapInProgress() const {
    return gesture_detector_.is_double_tapping();
  }

 private:
  void SetIgnoreSingleTap(bool value) { ignore_single_tap_ = value; }

  bool IsDoubleTapEnabled() const {
    return gesture_detector_.has_doubletap_listener();
  }

  GestureDetector gesture_detector_;
  SnapScrollController snap_scroll_controller_;

  GestureProvider* const provider_;

  // Whether the click delay should always be disabled by sending clicks for
  // double-tap gestures.
  const bool disable_click_delay_;

  const float touch_slop_;

  const base::TimeDelta double_tap_timeout_;

  base::TimeTicks current_down_time_;

  // TODO(klobag): This is to avoid a bug in GestureDetector. With multi-touch,
  // always_in_tap_region_ is not reset. So when the last finger is up,
  // OnSingleTapUp() will be mistakenly fired.
  bool ignore_single_tap_;

  // Used to remove the touch slop from the initial scroll event in a scroll
  // gesture.
  bool seen_first_scroll_event_;

  DISALLOW_COPY_AND_ASSIGN(GestureListenerImpl);
};

// GestureProvider

GestureProvider::GestureProvider(const Config& config,
                                 GestureProviderClient* client)
    : client_(client),
      touch_scroll_in_progress_(false),
      pinch_in_progress_(false),
      double_tap_support_for_page_(true),
      double_tap_support_for_platform_(true),
      gesture_begin_end_types_enabled_(config.gesture_begin_end_types_enabled),
      min_gesture_bounds_length_(config.min_gesture_bounds_length) {
  DCHECK(client);
  InitGestureDetectors(config);
}

GestureProvider::~GestureProvider() {}

bool GestureProvider::OnTouchEvent(const MotionEvent& event) {
  TRACE_EVENT1("input", "GestureProvider::OnTouchEvent",
               "action", GetMotionEventActionName(event.GetAction()));

  DCHECK_NE(0u, event.GetPointerCount());

  if (!CanHandle(event))
    return false;

  const bool in_scale_gesture =
      scale_gesture_listener_->IsScaleGestureDetectionInProgress();

  OnTouchEventHandlingBegin(event);
  gesture_listener_->OnTouchEvent(event, in_scale_gesture);
  scale_gesture_listener_->OnTouchEvent(event);
  OnTouchEventHandlingEnd(event);
  return true;
}

void GestureProvider::SetMultiTouchZoomSupportEnabled(bool enabled) {
  scale_gesture_listener_->SetMultiTouchEnabled(enabled);
}

void GestureProvider::SetDoubleTapSupportForPlatformEnabled(bool enabled) {
  if (double_tap_support_for_platform_ == enabled)
    return;
  double_tap_support_for_platform_ = enabled;
  UpdateDoubleTapDetectionSupport();
}

void GestureProvider::SetDoubleTapSupportForPageEnabled(bool enabled) {
  if (double_tap_support_for_page_ == enabled)
    return;
  double_tap_support_for_page_ = enabled;
  UpdateDoubleTapDetectionSupport();
}

bool GestureProvider::IsScrollInProgress() const {
  // TODO(wangxianzhu): Also return true when fling is active once the UI knows
  // exactly when the fling ends.
  return touch_scroll_in_progress_;
}

bool GestureProvider::IsPinchInProgress() const { return pinch_in_progress_; }

bool GestureProvider::IsDoubleTapInProgress() const {
  return gesture_listener_->IsDoubleTapInProgress() ||
         scale_gesture_listener_->IsDoubleTapInProgress();
}

void GestureProvider::InitGestureDetectors(const Config& config) {
  TRACE_EVENT0("input", "GestureProvider::InitGestureDetectors");
  gesture_listener_.reset(
      new GestureListenerImpl(config.display,
                              config.gesture_detector_config,
                              config.disable_click_delay,
                              this));

  scale_gesture_listener_.reset(
      new ScaleGestureListenerImpl(config.scale_gesture_detector_config, this));

  UpdateDoubleTapDetectionSupport();
}

bool GestureProvider::CanHandle(const MotionEvent& event) const {
  return event.GetAction() == MotionEvent::ACTION_DOWN || current_down_event_;
}

void GestureProvider::Fling(const MotionEvent& event,
                            float velocity_x,
                            float velocity_y) {
  if (!velocity_x && !velocity_y) {
    EndTouchScrollIfNecessary(event, true);
    return;
  }

  if (!touch_scroll_in_progress_) {
    // The native side needs a ET_GESTURE_SCROLL_BEGIN before
    // ET_SCROLL_FLING_START to send the fling to the correct target. Send if it
    // has not sent.  The distance traveled in one second is a reasonable scroll
    // start hint.
    GestureEventDetails scroll_details(
        ET_GESTURE_SCROLL_BEGIN, velocity_x, velocity_y);
    Send(CreateGesture(scroll_details, event));
  }
  EndTouchScrollIfNecessary(event, false);

  GestureEventDetails fling_details(
      ET_SCROLL_FLING_START, velocity_x, velocity_y);
  Send(CreateGesture(fling_details, event));
}

void GestureProvider::Send(GestureEventData gesture) {
  DCHECK(!gesture.time.is_null());
  // The only valid events that should be sent without an active touch sequence
  // are SHOW_PRESS and TAP, potentially triggered by the double-tap
  // delay timing out.
  DCHECK(current_down_event_ || gesture.type() == ET_GESTURE_TAP ||
         gesture.type() == ET_GESTURE_SHOW_PRESS);

  // TODO(jdduke): Provide a way of skipping this clamping for stylus and/or
  // mouse-based input, perhaps by exposing the source type on MotionEvent.
  const gfx::RectF& gesture_bounds = gesture.details.bounding_box_f();
  gesture.details.set_bounding_box(gfx::RectF(
      gesture_bounds.x(),
      gesture_bounds.y(),
      std::max(min_gesture_bounds_length_, gesture_bounds.width()),
      std::max(min_gesture_bounds_length_, gesture_bounds.height())));

  switch (gesture.type()) {
    case ET_GESTURE_LONG_PRESS:
      DCHECK(!scale_gesture_listener_->IsScaleGestureDetectionInProgress());
      current_longpress_time_ = gesture.time;
      break;
    case ET_GESTURE_LONG_TAP:
      current_longpress_time_ = base::TimeTicks();
      break;
    case ET_GESTURE_SCROLL_BEGIN:
      DCHECK(!touch_scroll_in_progress_);
      touch_scroll_in_progress_ = true;
      break;
    case ET_GESTURE_SCROLL_END:
      DCHECK(touch_scroll_in_progress_);
      if (pinch_in_progress_)
        Send(GestureEventData(ET_GESTURE_PINCH_END, gesture));
      touch_scroll_in_progress_ = false;
      break;
    case ET_GESTURE_PINCH_BEGIN:
      DCHECK(!pinch_in_progress_);
      if (!touch_scroll_in_progress_)
        Send(GestureEventData(ET_GESTURE_SCROLL_BEGIN, gesture));
      pinch_in_progress_ = true;
      break;
    case ET_GESTURE_PINCH_END:
      DCHECK(pinch_in_progress_);
      pinch_in_progress_ = false;
      break;
    case ET_GESTURE_SHOW_PRESS:
      // It's possible that a double-tap drag zoom (from ScaleGestureDetector)
      // will start before the press gesture fires (from GestureDetector), in
      // which case the press should simply be dropped.
      if (pinch_in_progress_ || touch_scroll_in_progress_)
        return;
    default:
      break;
  };

  client_->OnGestureEvent(gesture);
}

bool GestureProvider::SendLongTapIfNecessary(const MotionEvent& event) {
  if (event.GetAction() == MotionEvent::ACTION_UP &&
      !current_longpress_time_.is_null() &&
      !scale_gesture_listener_->IsScaleGestureDetectionInProgress()) {
    GestureEventDetails long_tap_details(ET_GESTURE_LONG_TAP, 0, 0);
    Send(CreateGesture(long_tap_details, event));
    return true;
  }
  return false;
}

void GestureProvider::EndTouchScrollIfNecessary(const MotionEvent& event,
                                                bool send_scroll_end_event) {
  if (!touch_scroll_in_progress_)
    return;
  if (send_scroll_end_event)
    Send(CreateGesture(ET_GESTURE_SCROLL_END, event));
  touch_scroll_in_progress_ = false;
}

void GestureProvider::OnTouchEventHandlingBegin(const MotionEvent& event) {
  switch (event.GetAction()) {
    case MotionEvent::ACTION_DOWN:
      current_down_event_ = event.Clone();
      touch_scroll_in_progress_ = false;
      pinch_in_progress_ = false;
      current_longpress_time_ = base::TimeTicks();
      if (gesture_begin_end_types_enabled_)
        Send(CreateGesture(ET_GESTURE_BEGIN, event));
      break;
    case MotionEvent::ACTION_POINTER_DOWN:
      if (gesture_begin_end_types_enabled_) {
        const int action_index = event.GetActionIndex();
        Send(CreateGesture(ET_GESTURE_BEGIN,
                           event.GetId(),
                           event.GetEventTime(),
                           event.GetX(action_index),
                           event.GetY(action_index),
                           event.GetRawX(action_index),
                           event.GetRawY(action_index),
                           event.GetPointerCount(),
                           GetBoundingBox(event)));
      }
      break;
    case MotionEvent::ACTION_POINTER_UP:
    case MotionEvent::ACTION_UP:
    case MotionEvent::ACTION_CANCEL:
    case MotionEvent::ACTION_MOVE:
      break;
  }
}

void GestureProvider::OnTouchEventHandlingEnd(const MotionEvent& event) {
  switch (event.GetAction()) {
    case MotionEvent::ACTION_UP:
    case MotionEvent::ACTION_CANCEL: {
      // Note: This call will have no effect if a fling was just generated, as
      // |Fling()| will have already signalled an end to touch-scrolling.
      EndTouchScrollIfNecessary(event, true);

      const gfx::RectF bounding_box = GetBoundingBox(event);

      if (gesture_begin_end_types_enabled_) {
        for (size_t i = 0; i < event.GetPointerCount(); ++i) {
          Send(CreateGesture(ET_GESTURE_END,
                             event.GetId(),
                             event.GetEventTime(),
                             event.GetX(i),
                             event.GetY(i),
                             event.GetRawX(i),
                             event.GetRawY(i),
                             event.GetPointerCount() - i,
                             bounding_box));
        }
      }

      current_down_event_.reset();

      UpdateDoubleTapDetectionSupport();
      break;
    }
    case MotionEvent::ACTION_POINTER_UP:
      if (gesture_begin_end_types_enabled_)
        Send(CreateGesture(ET_GESTURE_END, event));
      break;
    case MotionEvent::ACTION_DOWN:
    case MotionEvent::ACTION_POINTER_DOWN:
    case MotionEvent::ACTION_MOVE:
      break;
  }
}

void GestureProvider::UpdateDoubleTapDetectionSupport() {
  // The GestureDetector requires that any provided DoubleTapListener remain
  // attached to it for the duration of a touch sequence. Defer any potential
  // null'ing of the listener until the sequence has ended.
  if (current_down_event_)
    return;

  const bool double_tap_enabled = double_tap_support_for_page_ &&
                                  double_tap_support_for_platform_;
  gesture_listener_->SetDoubleTapEnabled(double_tap_enabled);
  scale_gesture_listener_->SetDoubleTapEnabled(double_tap_enabled);
}

}  //  namespace ui
