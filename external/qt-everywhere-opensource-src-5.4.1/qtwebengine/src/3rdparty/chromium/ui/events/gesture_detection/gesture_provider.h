// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"
#include "ui/events/gesture_detection/gesture_detector.h"
#include "ui/events/gesture_detection/gesture_event_data.h"
#include "ui/events/gesture_detection/scale_gesture_detector.h"
#include "ui/events/gesture_detection/snap_scroll_controller.h"
#include "ui/gfx/display.h"

namespace ui {

class GESTURE_DETECTION_EXPORT GestureProviderClient {
 public:
  virtual ~GestureProviderClient() {}
  virtual void OnGestureEvent(const GestureEventData& gesture) = 0;
};

// Given a stream of |MotionEvent|'s, provides gesture detection and gesture
// event dispatch.
class GESTURE_DETECTION_EXPORT GestureProvider {
 public:
  struct GESTURE_DETECTION_EXPORT Config {
    Config();
    ~Config();
    gfx::Display display;
    GestureDetector::Config gesture_detector_config;
    ScaleGestureDetector::Config scale_gesture_detector_config;

    // If |disable_click_delay| is true and double-tap support is disabled,
    // there will be no delay before tap events. When double-tap support is
    // enabled, there will always be a delay before a tap event is fired, in
    // order to allow the double tap gesture to occur without firing any tap
    // events.
    bool disable_click_delay;

    // If |gesture_begin_end_types_enabled| is true, fire an ET_GESTURE_BEGIN
    // event for every added touch point, and an ET_GESTURE_END event for every
    // removed touch point. Defaults to false.
    bool gesture_begin_end_types_enabled;

    // The minimum size (both length and width, in dips) of the generated
    // bounding box for all gesture types. This is useful for touch streams
    // that may report zero or unreasonably small touch sizes.
    // Defaults to 0.
    float min_gesture_bounds_length;
  };

  GestureProvider(const Config& config, GestureProviderClient* client);
  ~GestureProvider();

  // Handle the incoming MotionEvent, returning false if the event could not
  // be handled.
  bool OnTouchEvent(const MotionEvent& event);

  // Update whether multi-touch pinch zoom is supported by the platform.
  void SetMultiTouchZoomSupportEnabled(bool enabled);

  // Update whether double-tap gestures are supported by the platform.
  void SetDoubleTapSupportForPlatformEnabled(bool enabled);

  // Update whether double-tap gesture detection should be suppressed, e.g.,
  // if the page scale is fixed or the page has a mobile viewport. This disables
  // the tap delay, allowing rapid and responsive single-tap gestures.
  void SetDoubleTapSupportForPageEnabled(bool enabled);

  // Whether a scroll gesture is in-progress.
  bool IsScrollInProgress() const;

  // Whether a pinch gesture is in-progress (i.e. a pinch update has been
  // forwarded and detection is still active).
  bool IsPinchInProgress() const;

  // Whether a double-tap gesture is in-progress (either double-tap or
  // double-tap drag zoom).
  bool IsDoubleTapInProgress() const;

  // May be NULL if there is no currently active touch sequence.
  const ui::MotionEvent* current_down_event() const {
    return current_down_event_.get();
  }

 private:
  void InitGestureDetectors(const Config& config);

  bool CanHandle(const MotionEvent& event) const;

  void Fling(const MotionEvent& e, float velocity_x, float velocity_y);
  void Send(GestureEventData gesture);
  bool SendLongTapIfNecessary(const MotionEvent& event);
  void EndTouchScrollIfNecessary(const MotionEvent& event,
                                 bool send_scroll_end_event);
  void OnTouchEventHandlingBegin(const MotionEvent& event);
  void OnTouchEventHandlingEnd(const MotionEvent& event);
  void UpdateDoubleTapDetectionSupport();

  GestureProviderClient* const client_;

  class GestureListenerImpl;
  friend class GestureListenerImpl;
  scoped_ptr<GestureListenerImpl> gesture_listener_;

  class ScaleGestureListenerImpl;
  friend class ScaleGestureListenerImpl;
  scoped_ptr<ScaleGestureListenerImpl> scale_gesture_listener_;

  scoped_ptr<MotionEvent> current_down_event_;

  // Whether the respective {SCROLL,PINCH}_BEGIN gestures have been terminated
  // with a {SCROLL,PINCH}_END.
  bool touch_scroll_in_progress_;
  bool pinch_in_progress_;

  // Whether double-tap gesture detection is currently supported.
  bool double_tap_support_for_page_;
  bool double_tap_support_for_platform_;

  // Keeps track of the current GESTURE_LONG_PRESS event. If a context menu is
  // opened after a GESTURE_LONG_PRESS, this is used to insert a
  // GESTURE_TAP_CANCEL for removing any ::active styling.
  base::TimeTicks current_longpress_time_;

  const bool gesture_begin_end_types_enabled_;

  const float min_gesture_bounds_length_;
};

}  //  namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_PROVIDER_H_
