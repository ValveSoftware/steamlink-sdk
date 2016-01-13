// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_SETTINGS_H_
#define CC_SCHEDULER_SCHEDULER_SETTINGS_H_

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "cc/base/cc_export.h"

namespace cc {
class LayerTreeSettings;

class CC_EXPORT SchedulerSettings {
 public:
  SchedulerSettings();
  explicit SchedulerSettings(const LayerTreeSettings& settings);
  ~SchedulerSettings();

  bool begin_frame_scheduling_enabled;
  bool main_frame_before_draw_enabled;
  bool main_frame_before_activation_enabled;
  bool impl_side_painting;
  bool timeout_and_draw_when_animation_checkerboards;
  int maximum_number_of_failed_draws_before_draw_is_forced_;
  bool using_synchronous_renderer_compositor;
  bool throttle_frame_production;

  scoped_ptr<base::Value> AsValue() const;
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_SETTINGS_H_
