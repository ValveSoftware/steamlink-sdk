// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_GESTURE_CONFIGURATION_H_
#define UI_EVENTS_GESTURES_GESTURE_CONFIGURATION_H_

#include "base/basictypes.h"
#include "ui/events/events_base_export.h"

namespace ui {

// TODO: Expand this design to support multiple OS configuration
// approaches (windows, chrome, others).  This would turn into an
// abstract base class.

class EVENTS_BASE_EXPORT GestureConfiguration {
 public:
  // Number of parameters in the array of parameters for the fling acceleration
  // curve.
  static const int NumAccelParams = 4;

  // Ordered alphabetically ignoring underscores, to align with the
  // associated list of prefs in gesture_prefs_aura.cc.
  static int default_radius() {
    return default_radius_;
  }
  static void set_default_radius(int radius) { default_radius_ = radius; }
  static int fling_max_cancel_to_down_time_in_ms() {
    return fling_max_cancel_to_down_time_in_ms_;
  }
  static void set_fling_max_cancel_to_down_time_in_ms(int val) {
    fling_max_cancel_to_down_time_in_ms_ = val;
  }
  static int fling_max_tap_gap_time_in_ms() {
    return fling_max_tap_gap_time_in_ms_;
  }
  static void set_fling_max_tap_gap_time_in_ms(int val) {
    fling_max_tap_gap_time_in_ms_ = val;
  }
  static double long_press_time_in_seconds() {
    return long_press_time_in_seconds_;
  }
  static double semi_long_press_time_in_seconds() {
    return semi_long_press_time_in_seconds_;
  }
  static double max_distance_for_two_finger_tap_in_pixels() {
    return max_distance_for_two_finger_tap_in_pixels_;
  }
  static void set_max_distance_for_two_finger_tap_in_pixels(double val) {
    max_distance_for_two_finger_tap_in_pixels_ = val;
  }
  static int max_radius() {
    return max_radius_;
  }
  static void set_long_press_time_in_seconds(double val) {
    long_press_time_in_seconds_ = val;
  }
  static void set_semi_long_press_time_in_seconds(double val) {
    semi_long_press_time_in_seconds_ = val;
  }
  static double max_seconds_between_double_click() {
    return max_seconds_between_double_click_;
  }
  static void set_max_seconds_between_double_click(double val) {
    max_seconds_between_double_click_ = val;
  }
  static int max_separation_for_gesture_touches_in_pixels() {
    return max_separation_for_gesture_touches_in_pixels_;
  }
  static void set_max_separation_for_gesture_touches_in_pixels(int val) {
    max_separation_for_gesture_touches_in_pixels_ = val;
  }
  static double max_swipe_deviation_ratio() {
    return max_swipe_deviation_ratio_;
  }
  static void set_max_swipe_deviation_ratio(double val) {
    max_swipe_deviation_ratio_ = val;
  }
  static double max_touch_down_duration_in_seconds_for_click() {
    return max_touch_down_duration_in_seconds_for_click_;
  }
  static void set_max_touch_down_duration_in_seconds_for_click(double val) {
    max_touch_down_duration_in_seconds_for_click_ = val;
  }
  static double max_touch_move_in_pixels_for_click() {
    return max_touch_move_in_pixels_for_click_;
  }
  static void set_max_touch_move_in_pixels_for_click(double val) {
    max_touch_move_in_pixels_for_click_ = val;
  }
  static double max_distance_between_taps_for_double_tap() {
    return max_distance_between_taps_for_double_tap_;
  }
  static void set_max_distance_between_taps_for_double_tap(double val) {
    max_distance_between_taps_for_double_tap_ = val;
  }
  static double min_distance_for_pinch_scroll_in_pixels() {
    return min_distance_for_pinch_scroll_in_pixels_;
  }
  static void set_min_distance_for_pinch_scroll_in_pixels(double val) {
    min_distance_for_pinch_scroll_in_pixels_ = val;
  }
  static double min_flick_speed_squared() {
    return min_flick_speed_squared_;
  }
  static void set_min_flick_speed_squared(double val) {
    min_flick_speed_squared_ = val;
  }
  static double min_pinch_update_distance_in_pixels() {
    return min_pinch_update_distance_in_pixels_;
  }
  static void set_min_pinch_update_distance_in_pixels(double val) {
    min_pinch_update_distance_in_pixels_ = val;
  }
  static double min_rail_break_velocity() {
    return min_rail_break_velocity_;
  }
  static void set_min_rail_break_velocity(double val) {
    min_rail_break_velocity_ = val;
  }
  static double min_scroll_delta_squared() {
    return min_scroll_delta_squared_;
  }
  static void set_min_scroll_delta_squared(double val) {
    min_scroll_delta_squared_ = val;
  }
  static float min_scroll_velocity() {
    return min_scroll_velocity_;
  }
  static void set_min_scroll_velocity(float val) {
    min_scroll_velocity_ = val;
  }
  static double min_swipe_speed() {
    return min_swipe_speed_;
  }
  static void set_min_swipe_speed(double val) {
    min_swipe_speed_ = val;
  }
  static double min_touch_down_duration_in_seconds_for_click() {
    return min_touch_down_duration_in_seconds_for_click_;
  }
  static void set_min_touch_down_duration_in_seconds_for_click(double val) {
    min_touch_down_duration_in_seconds_for_click_ = val;
  }

  static int min_scaling_span_in_pixels() {
    return min_scaling_span_in_pixels_;
  };

  static void set_min_scaling_span_in_pixels(int val) {
    min_scaling_span_in_pixels_ = val;
  }

  static int points_buffered_for_velocity() {
    return points_buffered_for_velocity_;
  }
  static void set_points_buffered_for_velocity(int val) {
    points_buffered_for_velocity_ = val;
  }
  static double rail_break_proportion() {
    return rail_break_proportion_;
  }
  static void set_rail_break_proportion(double val) {
    rail_break_proportion_ = val;
  }
  static double rail_start_proportion() {
    return rail_start_proportion_;
  }
  static void set_rail_start_proportion(double val) {
    rail_start_proportion_ = val;
  }
  static double scroll_prediction_seconds() {
    return scroll_prediction_seconds_;
  }
  static void set_scroll_prediction_seconds(double val) {
    scroll_prediction_seconds_ = val;
  }
  static int show_press_delay_in_ms() {
    return show_press_delay_in_ms_;
  }
  static int set_show_press_delay_in_ms(int val) {
    return show_press_delay_in_ms_ = val;
  }
  static int scroll_debounce_interval_in_ms() {
    return scroll_debounce_interval_in_ms_;
  }
  static int set_scroll_debounce_interval_in_ms(int val) {
    return scroll_debounce_interval_in_ms_ = val;
  }
  static void set_fling_acceleration_curve_coefficients(int i, float val) {
    fling_acceleration_curve_coefficients_[i] = val;
  }
  static float fling_acceleration_curve_coefficients(int i) {
    return fling_acceleration_curve_coefficients_[i];
  }
  static float fling_velocity_cap() {
    return fling_velocity_cap_;
  }
  static void set_fling_velocity_cap(float val) {
    fling_velocity_cap_ = val;
  }
  // TODO(davemoore): Move into chrome/browser/ui.
  static int tab_scrub_activation_delay_in_ms() {
    return tab_scrub_activation_delay_in_ms_;
  }
  static void set_tab_scrub_activation_delay_in_ms(int val) {
    tab_scrub_activation_delay_in_ms_ = val;
  }

 private:
  // These are listed in alphabetical order ignoring underscores, to
  // align with the associated list of preferences in
  // gesture_prefs_aura.cc. These two lists should be kept in sync.

  // The default touch radius length used when the only information given
  // by the device is the touch center.
  static int default_radius_;

  // The maximum allowed distance between two fingers for a two finger tap. If
  // the distance between two fingers is greater than this value, we will not
  // recognize a two finger tap.
  static double max_distance_for_two_finger_tap_in_pixels_;

  // The maximum allowed size for the radius of a touch region used in
  // forming an ET_GESTURE_TAP event.
  static int max_radius_;

  // Maximum time between a GestureFlingCancel and a mousedown such that the
  // mousedown is considered associated with the cancel event.
  static int fling_max_cancel_to_down_time_in_ms_;

  // Maxium time between a mousedown/mouseup pair that is considered to be a
  // suppressable tap.
  static int fling_max_tap_gap_time_in_ms_;

  static double long_press_time_in_seconds_;
  static double semi_long_press_time_in_seconds_;
  static double max_seconds_between_double_click_;
  static double max_separation_for_gesture_touches_in_pixels_;
  static double max_swipe_deviation_ratio_;
  static double max_touch_down_duration_in_seconds_for_click_;
  static double max_touch_move_in_pixels_for_click_;
  static double max_distance_between_taps_for_double_tap_;
  static double min_distance_for_pinch_scroll_in_pixels_;
  static double min_flick_speed_squared_;
  static double min_pinch_update_distance_in_pixels_;
  static double min_rail_break_velocity_;
  static double min_scroll_delta_squared_;
  static float min_scroll_velocity_;
  static double min_swipe_speed_;
  static double min_touch_down_duration_in_seconds_for_click_;
  static int min_scaling_span_in_pixels_;
  static int points_buffered_for_velocity_;
  static double rail_break_proportion_;
  static double rail_start_proportion_;
  static double scroll_prediction_seconds_;
  static int show_press_delay_in_ms_;
  static int scroll_debounce_interval_in_ms_;

  static float fling_acceleration_curve_coefficients_[NumAccelParams];
  static float fling_velocity_cap_;
  // TODO(davemoore): Move into chrome/browser/ui.
  static int tab_scrub_activation_delay_in_ms_;

  DISALLOW_COPY_AND_ASSIGN(GestureConfiguration);
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_GESTURE_CONFIGURATION_H_
