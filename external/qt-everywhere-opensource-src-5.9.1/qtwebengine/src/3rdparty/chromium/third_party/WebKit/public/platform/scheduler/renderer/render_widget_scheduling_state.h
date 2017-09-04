// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_RENDERER_RENDER_WIDGET_SCHEDULING_STATE_H_
#define THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_RENDERER_RENDER_WIDGET_SCHEDULING_STATE_H_

#include "public/platform/WebCommon.h"

namespace blink {
namespace scheduler {

class RenderWidgetSignals;

class BLINK_PLATFORM_EXPORT RenderWidgetSchedulingState {
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
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_RENDERER_RENDER_WIDGET_SCHEDULING_STATE_H_
