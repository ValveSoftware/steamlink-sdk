// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorScrollOffsetAnimationCurve.h"
#include "platform/animation/TimingFunction.h"

#include "cc/animation/scroll_offset_animation_curve.h"
#include "cc/animation/timing_function.h"

using blink::CompositorScrollOffsetAnimationCurve;
using DurationBehavior = cc::ScrollOffsetAnimationCurve::DurationBehavior;

namespace blink {

static DurationBehavior GetDurationBehavior(CompositorScrollOffsetAnimationCurve::ScrollDurationBehavior webDurationBehavior)
{
    switch (webDurationBehavior) {
    case CompositorScrollOffsetAnimationCurve::ScrollDurationDeltaBased:
        return DurationBehavior::DELTA_BASED;

    case CompositorScrollOffsetAnimationCurve::ScrollDurationConstant:
        return DurationBehavior::CONSTANT;

    case CompositorScrollOffsetAnimationCurve::ScrollDurationInverseDelta:
        return DurationBehavior::INVERSE_DELTA;
    }
    NOTREACHED();
    return DurationBehavior::DELTA_BASED;
}

CompositorScrollOffsetAnimationCurve::CompositorScrollOffsetAnimationCurve(
    FloatPoint targetValue,
    ScrollDurationBehavior durationBehavior)
    : m_curve(cc::ScrollOffsetAnimationCurve::Create(
        gfx::ScrollOffset(targetValue.x(), targetValue.y()),
        cc::CubicBezierTimingFunction::CreatePreset(CubicBezierTimingFunction::EaseType::EASE_OUT_NATURAL),
        GetDurationBehavior(durationBehavior)))
{
}

CompositorScrollOffsetAnimationCurve::CompositorScrollOffsetAnimationCurve(
    cc::ScrollOffsetAnimationCurve* curve)
    : m_curve(curve->CloneToScrollOffsetAnimationCurve())
{
}

CompositorScrollOffsetAnimationCurve::~CompositorScrollOffsetAnimationCurve()
{
}

void CompositorScrollOffsetAnimationCurve::setInitialValue(FloatPoint initialValue)
{
    m_curve->SetInitialValue(gfx::ScrollOffset(initialValue.x(), initialValue.y()));
}

FloatPoint CompositorScrollOffsetAnimationCurve::getValue(double time) const
{
    gfx::ScrollOffset value = m_curve->GetValue(base::TimeDelta::FromSecondsD(time));
    return FloatPoint(value.x(), value.y());
}

void CompositorScrollOffsetAnimationCurve::applyAdjustment(IntSize adjustment)
{
    m_curve->ApplyAdjustment(gfx::Vector2dF(adjustment.width(), adjustment.height()));
}

double CompositorScrollOffsetAnimationCurve::duration() const
{
    return m_curve->Duration().InSecondsF();
}

FloatPoint CompositorScrollOffsetAnimationCurve::targetValue() const
{
    gfx::ScrollOffset target = m_curve->target_value();
    return FloatPoint(target.x(), target.y());
}

void CompositorScrollOffsetAnimationCurve::updateTarget(double time, FloatPoint newTarget)
{
    m_curve->UpdateTarget(time, gfx::ScrollOffset(newTarget.x(), newTarget.y()));
}

std::unique_ptr<cc::AnimationCurve> CompositorScrollOffsetAnimationCurve::cloneToAnimationCurve() const
{
    return m_curve->Clone();
}

} // namespace blink
