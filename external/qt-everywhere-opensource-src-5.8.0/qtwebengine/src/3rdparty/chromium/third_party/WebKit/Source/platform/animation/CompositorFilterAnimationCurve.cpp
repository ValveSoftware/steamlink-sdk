// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorFilterAnimationCurve.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/output/filter_operations.h"
#include "platform/graphics/CompositorFilterOperations.h"

using blink::CompositorFilterKeyframe;

namespace blink {

CompositorFilterAnimationCurve::CompositorFilterAnimationCurve()
    : m_curve(cc::KeyframedFilterAnimationCurve::Create())
{
}

CompositorFilterAnimationCurve::~CompositorFilterAnimationCurve()
{
}

void CompositorFilterAnimationCurve::addLinearKeyframe(const CompositorFilterKeyframe& keyframe)
{
    const cc::FilterOperations& filterOperations = keyframe.value().asFilterOperations();
    m_curve->AddKeyframe(cc::FilterKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), filterOperations, nullptr));

}

void CompositorFilterAnimationCurve::addCubicBezierKeyframe(const CompositorFilterKeyframe& keyframe, CubicBezierTimingFunction::EaseType easeType)
{
    const cc::FilterOperations& filterOperations = keyframe.value().asFilterOperations();
    m_curve->AddKeyframe(cc::FilterKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), filterOperations,
        cc::CubicBezierTimingFunction::CreatePreset(easeType)));
}

void CompositorFilterAnimationCurve::addCubicBezierKeyframe(const CompositorFilterKeyframe& keyframe, double x1, double y1, double x2, double y2)
{
    const cc::FilterOperations& filterOperations = keyframe.value().asFilterOperations();
    m_curve->AddKeyframe(cc::FilterKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), filterOperations,
        cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2)));
}

void CompositorFilterAnimationCurve::addStepsKeyframe(const CompositorFilterKeyframe& keyframe, int steps, StepsTimingFunction::StepPosition stepPosition)
{
    const cc::FilterOperations& filterOperations = keyframe.value().asFilterOperations();
    m_curve->AddKeyframe(cc::FilterKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), filterOperations,
        cc::StepsTimingFunction::Create(steps, stepPosition)));
}

void CompositorFilterAnimationCurve::setLinearTimingFunction()
{
    m_curve->SetTimingFunction(nullptr);
}

void CompositorFilterAnimationCurve::setCubicBezierTimingFunction(CubicBezierTimingFunction::EaseType easeType)
{
    m_curve->SetTimingFunction(cc::CubicBezierTimingFunction::CreatePreset(easeType));
}

void CompositorFilterAnimationCurve::setCubicBezierTimingFunction(double x1, double y1, double x2, double y2)
{
    m_curve->SetTimingFunction(cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2));
}

void CompositorFilterAnimationCurve::setStepsTimingFunction(int numberOfSteps, StepsTimingFunction::StepPosition stepPosition)
{
    m_curve->SetTimingFunction(cc::StepsTimingFunction::Create(numberOfSteps, stepPosition));
}

std::unique_ptr<cc::AnimationCurve> CompositorFilterAnimationCurve::cloneToAnimationCurve() const
{
    return m_curve->Clone();
}

} // namespace blink
