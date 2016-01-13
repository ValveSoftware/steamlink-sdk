// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_settings.h"

#include "cc/trees/layer_tree_settings.h"

namespace cc {

SchedulerSettings::SchedulerSettings()
    : begin_frame_scheduling_enabled(true),
      main_frame_before_draw_enabled(true),
      main_frame_before_activation_enabled(false),
      impl_side_painting(false),
      timeout_and_draw_when_animation_checkerboards(true),
      maximum_number_of_failed_draws_before_draw_is_forced_(3),
      using_synchronous_renderer_compositor(false),
      throttle_frame_production(true) {
}

SchedulerSettings::SchedulerSettings(const LayerTreeSettings& settings)
    : begin_frame_scheduling_enabled(settings.begin_frame_scheduling_enabled),
      main_frame_before_draw_enabled(settings.main_frame_before_draw_enabled),
      main_frame_before_activation_enabled(
          settings.main_frame_before_activation_enabled),
      impl_side_painting(settings.impl_side_painting),
      timeout_and_draw_when_animation_checkerboards(
          settings.timeout_and_draw_when_animation_checkerboards),
      maximum_number_of_failed_draws_before_draw_is_forced_(
          settings.maximum_number_of_failed_draws_before_draw_is_forced_),
      using_synchronous_renderer_compositor(
          settings.using_synchronous_renderer_compositor),
      throttle_frame_production(settings.throttle_frame_production) {
}

SchedulerSettings::~SchedulerSettings() {}

scoped_ptr<base::Value> SchedulerSettings::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);
  state->SetBoolean("begin_frame_scheduling_enabled",
                    begin_frame_scheduling_enabled);
  state->SetBoolean("main_frame_before_draw_enabled",
                    main_frame_before_draw_enabled);
  state->SetBoolean("main_frame_before_activation_enabled",
                    main_frame_before_activation_enabled);
  state->SetBoolean("impl_side_painting", impl_side_painting);
  state->SetBoolean("timeout_and_draw_when_animation_checkerboards",
                    timeout_and_draw_when_animation_checkerboards);
  state->SetInteger("maximum_number_of_failed_draws_before_draw_is_forced_",
                    maximum_number_of_failed_draws_before_draw_is_forced_);
  state->SetBoolean("using_synchronous_renderer_compositor",
                    using_synchronous_renderer_compositor);
  state->SetBoolean("throttle_frame_production", throttle_frame_production);
  return state.PassAs<base::Value>();
}

}  // namespace cc
