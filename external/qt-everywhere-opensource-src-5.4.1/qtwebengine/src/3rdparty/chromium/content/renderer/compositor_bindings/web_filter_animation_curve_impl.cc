// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/compositor_bindings/web_filter_animation_curve_impl.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/output/filter_operations.h"
#include "content/renderer/compositor_bindings/web_animation_curve_common.h"
#include "content/renderer/compositor_bindings/web_filter_operations_impl.h"

using blink::WebFilterKeyframe;

namespace content {

WebFilterAnimationCurveImpl::WebFilterAnimationCurveImpl()
    : curve_(cc::KeyframedFilterAnimationCurve::Create()) {
}

WebFilterAnimationCurveImpl::~WebFilterAnimationCurveImpl() {
}

blink::WebAnimationCurve::AnimationCurveType WebFilterAnimationCurveImpl::type()
    const {
  return WebAnimationCurve::AnimationCurveTypeFilter;
}

void WebFilterAnimationCurveImpl::add(const WebFilterKeyframe& keyframe,
                                      TimingFunctionType type) {
  const cc::FilterOperations& filter_operations =
      static_cast<const WebFilterOperationsImpl&>(keyframe.value())
          .AsFilterOperations();
  curve_->AddKeyframe(cc::FilterKeyframe::Create(
      keyframe.time(), filter_operations, CreateTimingFunction(type)));
}

void WebFilterAnimationCurveImpl::add(const WebFilterKeyframe& keyframe,
                                      double x1,
                                      double y1,
                                      double x2,
                                      double y2) {
  const cc::FilterOperations& filter_operations =
      static_cast<const WebFilterOperationsImpl&>(keyframe.value())
          .AsFilterOperations();
  curve_->AddKeyframe(cc::FilterKeyframe::Create(
      keyframe.time(),
      filter_operations,
      cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2)
          .PassAs<cc::TimingFunction>()));
}

scoped_ptr<cc::AnimationCurve>
WebFilterAnimationCurveImpl::CloneToAnimationCurve() const {
  return curve_->Clone();
}

}  // namespace content

