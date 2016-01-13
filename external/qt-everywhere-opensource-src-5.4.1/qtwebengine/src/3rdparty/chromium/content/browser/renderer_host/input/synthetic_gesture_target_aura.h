// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_AURA_H_

#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target_base.h"
#include "content/common/input/synthetic_gesture_params.h"

namespace aura {
class Window;
class WindowEventDispatcher;

namespace client {
class ScreenPositionClient;
}
}  // namespace aura

namespace content {

// SyntheticGestureTarget implementation for aura
class SyntheticGestureTargetAura : public SyntheticGestureTargetBase {
 public:
  explicit SyntheticGestureTargetAura(RenderWidgetHostImpl* host);

  // SyntheticGestureTargetBase:
  virtual void DispatchWebTouchEventToPlatform(
      const blink::WebTouchEvent& web_touch,
      const ui::LatencyInfo& latency_info) OVERRIDE;
  virtual void DispatchWebMouseWheelEventToPlatform(
      const blink::WebMouseWheelEvent& web_wheel,
      const ui::LatencyInfo& latency_info) OVERRIDE;
  virtual void DispatchWebMouseEventToPlatform(
      const blink::WebMouseEvent& web_mouse,
      const ui::LatencyInfo& latency_info) OVERRIDE;

  // SyntheticGestureTarget:
  virtual SyntheticGestureParams::GestureSourceType
      GetDefaultSyntheticGestureSourceType() const OVERRIDE;

  virtual float GetTouchSlopInDips() const OVERRIDE;

  virtual float GetMinScalingSpanInDips() const OVERRIDE;

 private:
  aura::Window* GetWindow() const;

  DISALLOW_COPY_AND_ASSIGN(SyntheticGestureTargetAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SYNTHETIC_GESTURE_TARGET_AURA_H_
