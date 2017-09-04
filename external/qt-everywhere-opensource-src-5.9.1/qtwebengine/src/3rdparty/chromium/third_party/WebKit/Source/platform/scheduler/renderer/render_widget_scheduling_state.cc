// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/scheduler/renderer/render_widget_scheduling_state.h"

#include "platform/scheduler/renderer/render_widget_signals.h"

namespace blink {
namespace scheduler {

RenderWidgetSchedulingState::RenderWidgetSchedulingState(
    RenderWidgetSignals* render_widget_scheduling_signals)
    : render_widget_signals_(render_widget_scheduling_signals),
      hidden_(false),
      has_touch_handler_(false) {
  render_widget_signals_->IncNumVisibleRenderWidgets();
}

RenderWidgetSchedulingState::~RenderWidgetSchedulingState() {
  if (hidden_)
    return;

  render_widget_signals_->DecNumVisibleRenderWidgets();

  if (has_touch_handler_) {
    render_widget_signals_->DecNumVisibleRenderWidgetsWithTouchHandlers();
  }
}

void RenderWidgetSchedulingState::SetHidden(bool hidden) {
  if (hidden_ == hidden)
    return;

  hidden_ = hidden;

  if (hidden_) {
    render_widget_signals_->DecNumVisibleRenderWidgets();
    if (has_touch_handler_) {
      render_widget_signals_->DecNumVisibleRenderWidgetsWithTouchHandlers();
    }
  } else {
    render_widget_signals_->IncNumVisibleRenderWidgets();
    if (has_touch_handler_) {
      render_widget_signals_->IncNumVisibleRenderWidgetsWithTouchHandlers();
    }
  }
}

void RenderWidgetSchedulingState::SetHasTouchHandler(bool has_touch_handler) {
  if (has_touch_handler_ == has_touch_handler)
    return;

  has_touch_handler_ = has_touch_handler;

  if (hidden_)
    return;

  if (has_touch_handler_) {
    render_widget_signals_->IncNumVisibleRenderWidgetsWithTouchHandlers();
  } else {
    render_widget_signals_->DecNumVisibleRenderWidgetsWithTouchHandlers();
  }
}

}  // namespace scheduler
}  // namespace blink
