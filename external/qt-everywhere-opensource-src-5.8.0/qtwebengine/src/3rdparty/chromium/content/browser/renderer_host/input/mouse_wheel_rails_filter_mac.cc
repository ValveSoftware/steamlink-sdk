// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mouse_wheel_rails_filter_mac.h"

using blink::WebInputEvent;
using blink::WebMouseWheelEvent;

namespace content {

MouseWheelRailsFilterMac::MouseWheelRailsFilterMac() {
}

MouseWheelRailsFilterMac::~MouseWheelRailsFilterMac() {
}

WebInputEvent::RailsMode MouseWheelRailsFilterMac::UpdateRailsMode(
    const WebMouseWheelEvent& event) {
  // A somewhat-arbitrary decay constant for hysteresis.
  const float kDecayConstant = 0.8f;

  if (event.phase == WebMouseWheelEvent::PhaseBegan) {
    decayed_delta_ = gfx::Vector2dF();
  }
  if (event.deltaX == 0 && event.deltaY == 0)
    return WebInputEvent::RailsModeFree;

  decayed_delta_.Scale(kDecayConstant);
  decayed_delta_ +=
      gfx::Vector2dF(std::abs(event.deltaX), std::abs(event.deltaY));

  if (decayed_delta_.y() >= decayed_delta_.x())
    return WebInputEvent::RailsModeVertical;
  return WebInputEvent::RailsModeHorizontal;
}

}  // namespace content
