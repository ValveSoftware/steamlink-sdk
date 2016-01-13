// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/compositor_bindings/web_animation_curve_common.h"

#include "cc/animation/timing_function.h"

namespace content {

scoped_ptr<cc::TimingFunction> CreateTimingFunction(
    blink::WebAnimationCurve::TimingFunctionType type) {
  switch (type) {
    case blink::WebAnimationCurve::TimingFunctionTypeEase:
      return cc::EaseTimingFunction::Create();
    case blink::WebAnimationCurve::TimingFunctionTypeEaseIn:
      return cc::EaseInTimingFunction::Create();
    case blink::WebAnimationCurve::TimingFunctionTypeEaseOut:
      return cc::EaseOutTimingFunction::Create();
    case blink::WebAnimationCurve::TimingFunctionTypeEaseInOut:
      return cc::EaseInOutTimingFunction::Create();
    case blink::WebAnimationCurve::TimingFunctionTypeLinear:
      return scoped_ptr<cc::TimingFunction>();
  }
  return scoped_ptr<cc::TimingFunction>();
}

}  // namespace content

