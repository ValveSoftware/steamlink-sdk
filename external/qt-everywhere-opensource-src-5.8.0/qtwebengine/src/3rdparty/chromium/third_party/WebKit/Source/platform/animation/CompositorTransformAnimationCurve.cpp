// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/animation/CompositorTransformAnimationCurve.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/animation/transform_operations.h"
#include "platform/animation/CompositorTransformOperations.h"

using blink::CompositorTransformKeyframe;

namespace blink {

CompositorTransformAnimationCurve::CompositorTransformAnimationCurve()
    : m_curve(cc::KeyframedTransformAnimationCurve::Create())
{
}

CompositorTransformAnimationCurve::~CompositorTransformAnimationCurve()
{
}

void CompositorTransformAnimationCurve::addLinearKeyframe(const CompositorTransformKeyframe& keyframe)
{
    const cc::TransformOperations& transformOperations = keyframe.value().asTransformOperations();
    m_curve->AddKeyframe(cc::TransformKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), transformOperations, nullptr));
}

void CompositorTransformAnimationCurve::addCubicBezierKeyframe(const CompositorTransformKeyframe& keyframe, CubicBezierTimingFunction::EaseType easeType)
{
    const cc::TransformOperations& transformOperations = keyframe.value().asTransformOperations();
    m_curve->AddKeyframe(cc::TransformKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), transformOperations,
        cc::CubicBezierTimingFunction::CreatePreset(easeType)));
}

void CompositorTransformAnimationCurve::addCubicBezierKeyframe(const CompositorTransformKeyframe& keyframe,  double x1, double y1, double x2, double y2)
{
    const cc::TransformOperations& transformOperations = keyframe.value().asTransformOperations();
    m_curve->AddKeyframe(cc::TransformKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), transformOperations,
        cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2)));
}

void CompositorTransformAnimationCurve::addStepsKeyframe(const CompositorTransformKeyframe& keyframe, int steps, StepsTimingFunction::StepPosition stepPosition)
{
    const cc::TransformOperations& transformOperations = keyframe.value().asTransformOperations();
    m_curve->AddKeyframe(cc::TransformKeyframe::Create(
        base::TimeDelta::FromSecondsD(keyframe.time()), transformOperations,
        cc::StepsTimingFunction::Create(steps, stepPosition)));
}

void CompositorTransformAnimationCurve::setLinearTimingFunction()
{
    m_curve->SetTimingFunction(nullptr);
}

void CompositorTransformAnimationCurve::setCubicBezierTimingFunction(CubicBezierTimingFunction::EaseType easeType)
{
    m_curve->SetTimingFunction(cc::CubicBezierTimingFunction::CreatePreset(easeType));
}

void CompositorTransformAnimationCurve::setCubicBezierTimingFunction(double x1, double y1, double x2, double y2)
{
    m_curve->SetTimingFunction(
        cc::CubicBezierTimingFunction::Create(x1, y1, x2, y2));
}

void CompositorTransformAnimationCurve::setStepsTimingFunction(int numberOfSteps, StepsTimingFunction::StepPosition stepPosition)
{
    m_curve->SetTimingFunction(cc::StepsTimingFunction::Create(numberOfSteps, stepPosition));
}

std::unique_ptr<cc::AnimationCurve> CompositorTransformAnimationCurve::cloneToAnimationCurve() const
{
    return m_curve->Clone();
}

} // namespace blink
