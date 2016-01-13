// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/compositor_bindings/web_scroll_offset_animation_curve_impl.h"

#if WEB_SCROLL_OFFSET_ANIMATION_CURVE_IS_DEFINED

#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "content/renderer/compositor_bindings/web_animation_curve_common.h"

using blink::WebFloatPoint;

namespace content {

WebScrollOffsetAnimationCurveImpl::WebScrollOffsetAnimationCurveImpl(
    WebFloatPoint target_value,
    TimingFunctionType timing_function)
    : curve_(cc::ScrollOffsetAnimationCurve::Create(
          gfx::Vector2dF(target_value.x, target_value.y),
          CreateTimingFunction(timing_function))) {
}

WebScrollOffsetAnimationCurveImpl::~WebScrollOffsetAnimationCurveImpl() {
}

blink::WebAnimationCurve::AnimationCurveType
WebScrollOffsetAnimationCurveImpl::type() const {
  return WebAnimationCurve::AnimationCurveTypeScrollOffset;
}

void WebScrollOffsetAnimationCurveImpl::setInitialValue(
    WebFloatPoint initial_value) {
  curve_->SetInitialValue(gfx::Vector2dF(initial_value.x, initial_value.y));
}

WebFloatPoint WebScrollOffsetAnimationCurveImpl::getValue(double time) const {
  gfx::Vector2dF value = curve_->GetValue(time);
  return WebFloatPoint(value.x(), value.y());
}

double WebScrollOffsetAnimationCurveImpl::duration() const {
  return curve_->Duration();
}

scoped_ptr<cc::AnimationCurve>
WebScrollOffsetAnimationCurveImpl::CloneToAnimationCurve() const {
  return curve_->Clone();
}

}  // namespace content

#endif  // WEB_SCROLL_OFFSET_ANIMATION_CURVE_IS_DEFINED

