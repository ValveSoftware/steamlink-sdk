// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_RENDER_WIDGET_SCHEDULING_STATE_H_
#define COMPONENTS_SCHEDULER_RENDERER_RENDER_WIDGET_SCHEDULING_STATE_H_

#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class RenderWidgetSignals;

class SCHEDULER_EXPORT RenderWidgetSchedulingState {
 public:
  void SetHidden(bool hidden);
  void SetHasTouchHandler(bool has_touch_handler);

  ~RenderWidgetSchedulingState();

 private:
  friend class RenderWidgetSignals;

  explicit RenderWidgetSchedulingState(
      RenderWidgetSignals* render_widget_scheduling_signals);

  RenderWidgetSignals* render_widget_signals_;  // NOT OWNED
  bool hidden_;
  bool has_touch_handler_;
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_RENDER_WIDGET_SCHEDULING_STATE_H_
