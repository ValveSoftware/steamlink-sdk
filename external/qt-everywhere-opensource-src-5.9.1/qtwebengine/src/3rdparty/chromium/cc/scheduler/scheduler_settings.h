// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_SETTINGS_H_
#define CC_SCHEDULER_SCHEDULER_SETTINGS_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/values.h"
#include "cc/base/cc_export.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace cc {

class CC_EXPORT SchedulerSettings {
 public:
  SchedulerSettings();
  SchedulerSettings(const SchedulerSettings& other);
  ~SchedulerSettings();

  bool use_external_begin_frame_source = false;
  bool main_frame_while_submit_frame_throttled_enabled = false;
  bool main_frame_before_activation_enabled = false;
  bool commit_to_active_tree = false;
  bool timeout_and_draw_when_animation_checkerboards = true;
  bool using_synchronous_renderer_compositor = false;
  bool abort_commit_before_compositor_frame_sink_creation = true;
  bool enable_latency_recovery = true;

  int maximum_number_of_failed_draws_before_draw_is_forced = 3;
  base::TimeDelta background_frame_interval = base::TimeDelta::FromSeconds(1);

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue() const;
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_SETTINGS_H_
