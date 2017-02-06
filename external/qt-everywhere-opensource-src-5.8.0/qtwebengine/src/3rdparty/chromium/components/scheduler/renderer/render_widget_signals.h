// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_RENDERER_RENDER_WIDGET_SIGNALS_H_
#define COMPONENTS_SCHEDULER_RENDERER_RENDER_WIDGET_SIGNALS_H_

#include <memory>

#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class RenderWidgetSchedulingState;

class SCHEDULER_EXPORT RenderWidgetSignals {
 public:
  class SCHEDULER_EXPORT Observer {
   public:
    virtual ~Observer() {}

    // If |hidden| is true then all render widgets managed by this renderer
    // process have been hidden.
    // If |hidden| is false at least one render widget managed by this renderer
    // process has become visible and the renderer is no longer hidden.
    // Will be called on the main thread.
    virtual void SetAllRenderWidgetsHidden(bool hidden) = 0;

    // Tells the observer whether or not we have at least one touch handler on
    // a visible render widget. Will be called on the main thread.
    virtual void SetHasVisibleRenderWidgetWithTouchHandler(
        bool has_visible_render_widget_with_touch_handler) = 0;
  };

  explicit RenderWidgetSignals(Observer* observer);

  std::unique_ptr<RenderWidgetSchedulingState> NewRenderWidgetSchedulingState();

  void AsValueInto(base::trace_event::TracedValue* state) const;

 private:
  friend class RenderWidgetSchedulingState;

  void IncNumVisibleRenderWidgets();
  void DecNumVisibleRenderWidgets();
  void IncNumVisibleRenderWidgetsWithTouchHandlers();
  void DecNumVisibleRenderWidgetsWithTouchHandlers();

  Observer* observer_;  // NOT OWNED
  int num_visible_render_widgets_;
  int num_visible_render_widgets_with_touch_handlers_;
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_RENDERER_RENDER_WIDGET_SIGNALS_H_
